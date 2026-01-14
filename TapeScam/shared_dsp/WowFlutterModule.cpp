#include "WowFlutterModule.h"

#include <algorithm>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
constexpr float kTwoPi = 6.283185307179586476925286766559f;
// Base delay must be > max deviation to allow pitch modulation in both directions.
// Max wow deviation is 35ms * wowAmount (typically << 1.0), clamped by maxDevSec_ (~4.2ms).
// Using 5ms base provides headroom while minimizing perceptible latency.
constexpr float kBaseDelayMsL = 5.0f;
constexpr float kBaseDelayMsR = 5.5f;
// Derivative limiter: max rate of change (samples per sample)
constexpr float kMaxDeltaSamples = 0.35f;  // Reduced from 0.9 to prevent aliasing
}

void WowFlutterModule::Init(float sampleRate)
{
    sampleRate_    = sampleRate;
    invSampleRate_ = (sampleRate_ > 0.0f) ? 1.0f / sampleRate_ : 0.0f;
    targetAmount_  = 0.0f;
    smoothedAmount_= 0.0f;

    wowAmount_     = 0.0f;
    wowRateHz_     = 0.2f;
    flutterAmount_ = 0.0f;
    flutterRateHz_ = 6.0f;

    wowPhaseL_ = 0.0f;
    wowPhaseR_ = 0.25f;  // 90° offset for wider stereo motion
    fltPhaseL_ = 0.0f;
    fltPhaseR_ = 0.33f;  // ~120° offset
    driftL_ = driftR_ = 0.0f;
    flutterJitL_ = flutterJitR_ = 0.0f;
    wowNoiseStateL_ = wowNoiseStateR_ = 0.0f;
    flutterNoiseStateL_ = flutterNoiseStateR_ = 0.0f;

    delayBufSize_ = kMaxDelaySamples;
    delayBufL_.reset(new float[delayBufSize_]);
    delayBufR_.reset(new float[delayBufSize_]);
    std::fill(delayBufL_.get(), delayBufL_.get() + delayBufSize_, 0.0f);
    std::fill(delayBufR_.get(), delayBufR_.get() + delayBufSize_, 0.0f);

    writeIndex_ = 0;

    baseDelaySamplesL_ = std::min(sampleRate_ * (kBaseDelayMsL * 0.001f), static_cast<float>(delayBufSize_) - 4.0f);
    baseDelaySamplesR_ = std::min(sampleRate_ * (kBaseDelayMsR * 0.001f), static_cast<float>(delayBufSize_) - 4.0f);
    readStateL_ = baseDelaySamplesL_;
    readStateR_ = baseDelaySamplesR_;

    prevReadPosL_ = 0.0f;
    prevReadPosR_ = 0.0f;

    maxDeltaL_ = 0.0f;
    maxDeltaR_ = 0.0f;
    jumpCountL_ = 0;
    jumpCountR_ = 0;
    burstCountL_ = 0;
    burstCountR_ = 0;
    maxReadDeltaSamplesL_ = 0.0f;
    maxReadDeltaSamplesR_ = 0.0f;

    // Initialize burst state
    burstTimerL_ = 0.0f;
    burstTimerR_ = 0.0f;
    burstAmountL_ = 1.0f;
    burstAmountR_ = 1.0f;
    burstCountdownL_ = 0.0f;
    burstCountdownR_ = 0.0f;

    randState_ ^= static_cast<uint32_t>(sampleRate_);
}

void WowFlutterModule::SetAmount(float amount)
{
    targetAmount_ = Clamp(amount, 0.0f, 1.0f);
}

void WowFlutterModule::SetDepthMultiplier(float multiplier)
{
    // Ensure multiplier is positive and within reasonable range
    depthMultiplier_ = Clamp(multiplier, 0.5f, 2.0f);

    // Tie randomness/turbulence factors to tape age
    // New tape (0.8): less randomness
    // Used tape (1.0): moderate randomness
    // Worn tape (1.3): high randomness
    if(depthMultiplier_ <= 0.85f) {
        wowRandomnessFactor_ = 0.2f;
        flutterTurbulenceFactor_ = 0.1f;
    } else if(depthMultiplier_ <= 1.15f) {
        wowRandomnessFactor_ = 0.4f;
        flutterTurbulenceFactor_ = 0.2f;
    } else {
        wowRandomnessFactor_ = 0.75f;
        flutterTurbulenceFactor_ = 0.45f;
    }
}

void WowFlutterModule::SetModeParameters(float wowDepthScale, float flutterDepthScale,
                                         float wowRateMinHz, float wowRateMaxHz,
                                         float flutterRateMinHz, float flutterRateMaxHz)
{
    wowDepthScale_ = Clamp(wowDepthScale, 0.2f, 3.0f);
    flutterDepthScale_ = Clamp(flutterDepthScale, 0.2f, 3.0f);
    wowRateMinHz_ = std::max(0.05f, wowRateMinHz);
    wowRateMaxHz_ = std::max(wowRateMinHz_, wowRateMaxHz);
    flutterRateMinHz_ = std::max(0.5f, flutterRateMinHz);
    flutterRateMaxHz_ = std::max(flutterRateMinHz_, flutterRateMaxHz);
}

void WowFlutterModule::SetBaselineMotion(float ageAmount, float speedAmount)
{
    const float ageAmt = Clamp(ageAmount, 0.0f, 1.0f);
    const float speedAmt = Clamp(speedAmount, 0.0f, 1.0f);

    baseAgeDriftDepthSec_ = ageAmt * kMaxAgeDriftDepthSec;
    baseSpeedFlutterDepthSec_ = speedAmt * kMaxSpeedFlutterDepthSec;

    ageDriftAlpha_ = (baseAgeDriftDepthSec_ > 0.0f)
        ? (0.0002f + 0.0012f * ageAmt)
        : 0.0f;

    flutterDriftAlpha_ = (baseSpeedFlutterDepthSec_ > 0.0f)
        ? (0.0008f + 0.0025f * speedAmt)
        : 0.0f;
}

void WowFlutterModule::SetMaxDeviation(float seconds)
{
    maxDevSec_ = Clamp(seconds, 0.0008f, 0.05f);
}

void WowFlutterModule::SetStereoBlend(float blend)
{
    stereoBlend_ = Clamp(blend, 0.0f, 1.0f);
}

void WowFlutterModule::UpdateControls()
{
    smoothedAmount_ += (targetAmount_ - smoothedAmount_) * kAmountSmooth;

    const float targetActive = (targetAmount_ > 0.0005f) ? 1.0f : 0.0f;
    activationRamp_ += (targetActive - activationRamp_) * kActivationSmooth;

    const float amt = smoothedAmount_ * activationRamp_;
    depthShape_ = amt * amt;

    const float depthBlendWow = 0.05f + 0.95f * depthShape_;
    const float depthBlendFlutter = 0.03f + 0.97f * depthShape_;

    const float rateShape = std::sqrt(std::max(amt, 0.0f));
    const float rawWowRate = wowRateMinHz_ + (wowRateMaxHz_ - wowRateMinHz_) * rateShape;
    const float rawFlutterRate = flutterRateMinHz_ + (flutterRateMaxHz_ - flutterRateMinHz_) * rateShape;

    const float targetSlew = (amt <= 0.0005f)
        ? 0.97f
        : Clamp(0.94f - 0.28f * depthShape_, 0.7f, 0.95f);
    slewAlpha_ += (targetSlew - slewAlpha_) * 0.2f;

    wowAmount_ = depthBlendWow * depthMultiplier_ * wowDepthScale_ * activationRamp_;
    flutterAmount_ = depthBlendFlutter * depthMultiplier_ * flutterDepthScale_ * activationRamp_;

    wowRateHz_ = wowRateMinHz_ + (rawWowRate - wowRateMinHz_) * activationRamp_;
    flutterRateHz_ = flutterRateMinHz_ + (rawFlutterRate - flutterRateMinHz_) * activationRamp_;
}

float WowFlutterModule::InterpolateLinear(const float* buf, size_t size, float index)
{
    int idx0 = static_cast<int>(std::floor(index));
    int idx1 = idx0 + 1;
    float frac = index - static_cast<float>(idx0);
    while(idx0 < 0) idx0 += static_cast<int>(size);
    while(idx1 < 0) idx1 += static_cast<int>(size);
    idx0 %= static_cast<int>(size);
    idx1 %= static_cast<int>(size);
    return buf[idx0] + frac * (buf[idx1] - buf[idx0]);
}

float WowFlutterModule::InterpolateCubic(const float* buf, size_t size, float index)
{
    // 4-point cubic interpolation (Hermite)
    int idx1 = static_cast<int>(std::floor(index));
    float frac = index - static_cast<float>(idx1);

    int idx0 = idx1 - 1;
    int idx2 = idx1 + 1;
    int idx3 = idx1 + 2;

    // Wrap indices
    while(idx0 < 0) idx0 += static_cast<int>(size);
    while(idx1 < 0) idx1 += static_cast<int>(size);
    while(idx2 < 0) idx2 += static_cast<int>(size);
    while(idx3 < 0) idx3 += static_cast<int>(size);
    idx0 %= static_cast<int>(size);
    idx1 %= static_cast<int>(size);
    idx2 %= static_cast<int>(size);
    idx3 %= static_cast<int>(size);

    float y0 = buf[idx0];
    float y1 = buf[idx1];
    float y2 = buf[idx2];
    float y3 = buf[idx3];

    // Hermite interpolation
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void WowFlutterModule::Process(float** in, float** out, size_t size)
{
    const float wowAmt        = wowAmount_;
    const float flutterAmt    = flutterAmount_;

    const float invSr         = invSampleRate_;
    const float baseSecL_raw  = baseDelaySamplesL_ * invSr;
    const float baseSecR_raw  = baseDelaySamplesR_ * invSr;
    const float baseMidSec    = 0.5f * (baseSecL_raw + baseSecR_raw);
    const float baseSecL      = baseMidSec + stereoBlend_ * (baseSecL_raw - baseMidSec);
    const float baseSecR      = baseMidSec + stereoBlend_ * (baseSecR_raw - baseMidSec);

    const bool bypassDelay = activationRamp_ < 1.0e-4f
                             && baseAgeDriftDepthSec_ <= 0.0f
                             && baseSpeedFlutterDepthSec_ <= 0.0f
                             && wowAmt <= 1.0e-5f
                             && flutterAmt <= 1.0e-5f;

    if (bypassDelay)
    {
        for (size_t i = 0; i < size; ++i)
        {
            const float xL = in[0][i];
            const float xR = in[1][i];
            if (out)
            {
                out[0][i] = xL;
                out[1][i] = xR;
            }

            delayBufL_[writeIndex_] = xL;
            delayBufR_[writeIndex_] = xR;
            writeIndex_++;
            if (writeIndex_ >= delayBufSize_)
                writeIndex_ = 0;
        }
        readStateL_ = baseDelaySamplesL_;
        readStateR_ = baseDelaySamplesR_;
        prevReadPosL_ = baseDelaySamplesL_;
        prevReadPosR_ = baseDelaySamplesR_;
        return;
    }

    // Simplified depth control - musical and clear
    const float maxWowDepthSec = 0.035f;       // 35ms max wow (deeper drift)
    const float maxFlutterDepthSec = 0.002f;  // 2ms max flutter (tape speed variation)

    // Simple, direct modulation - no cross-modulation complexity
    wowDepthL_actual_ = maxWowDepthSec * wowAmt;
    wowDepthR_actual_ = maxWowDepthSec * wowAmt * 1.03f;
    wowRateL_actual_ = wowRateHz_;
    wowRateR_actual_ = wowRateHz_ * 1.03f;  // Slight L/R offset for width

    flutterDepthL_actual_ = maxFlutterDepthSec * flutterAmt;
    flutterDepthR_actual_ = maxFlutterDepthSec * flutterAmt * 0.97f;
    flutterRateL_actual_ = flutterRateHz_;
    flutterRateR_actual_ = flutterRateHz_ * 0.97f;  // Slight L/R offset

    const bool ageDriftActive = (baseAgeDriftDepthSec_ > 0.0f) && (ageDriftAlpha_ > 0.0f);
    const bool speedDriftActive = (baseSpeedFlutterDepthSec_ > 0.0f) && (flutterDriftAlpha_ > 0.0f);
    const float wowDepthBaseL = wowDepthL_actual_;
    const float wowDepthBaseR = wowDepthR_actual_;
    const float flutterDepthBaseL = flutterDepthL_actual_;
    const float flutterDepthBaseR = flutterDepthR_actual_;

    // Per-sample crossfade smoothing coefficient (~5ms fade at 48kHz)
    const float crossfadeAlpha = 0.004f;
    float crossfadeGain = crossfadeGain_;

    for(size_t i = 0; i < size; ++i)
    {
        float xL = in[0][i];
        float xR = in[1][i];

        // Smooth crossfade gain toward activation ramp target
        crossfadeGain += (activationRamp_ - crossfadeGain) * crossfadeAlpha;

        wowNoiseStateL_ += (NextRandCentered() - wowNoiseStateL_) * kWowNoiseAlpha;
        wowNoiseStateR_ += (NextRandCentered() - wowNoiseStateR_) * kWowNoiseAlpha;
        flutterNoiseStateL_ += (NextRandCentered() - flutterNoiseStateL_) * kFlutterNoiseAlpha;
        flutterNoiseStateR_ += (NextRandCentered() - flutterNoiseStateR_) * kFlutterNoiseAlpha;

        const float wowRateL_now = wowRateL_actual_;
        const float wowRateR_now = wowRateR_actual_;
        const float flutterRateL_now = flutterRateL_actual_;
        const float flutterRateR_now = flutterRateR_actual_;
        const float wowDepthL_now = wowDepthBaseL;
        const float wowDepthR_now = wowDepthBaseR;
        const float flutterDepthL_now = flutterDepthBaseL;
        const float flutterDepthR_now = flutterDepthBaseR;

        if(ageDriftActive)
        {
            const float shared = NextRandCentered();
            const float slightOffset = NextRandCentered() * 0.12f;
            const float targetL = shared * baseAgeDriftDepthSec_;
            const float targetR = (shared + slightOffset) * baseAgeDriftDepthSec_;
            driftL_ += (targetL - driftL_) * ageDriftAlpha_;
            driftR_ += (targetR - driftR_) * ageDriftAlpha_;
        }
        else
        {
            driftL_ *= 0.9996f;
            driftR_ *= 0.9996f;
        }

        if(speedDriftActive)
        {
            const float shared = NextRandCentered();
            const float targetL = shared * baseSpeedFlutterDepthSec_;
            const float targetR = (shared - 0.08f * NextRandCentered()) * baseSpeedFlutterDepthSec_;
            const float slowAlpha = flutterDriftAlpha_ * 0.5f;
            flutterJitL_ += (targetL - flutterJitL_) * slowAlpha;
            flutterJitR_ += (targetR - flutterJitR_) * slowAlpha;
        }
        else
        {
            flutterJitL_ *= 0.9997f;
            flutterJitR_ *= 0.9997f;
        }

        // --- LEFT CHANNEL ---

        // Update wow phase
        wowPhaseL_ += wowRateL_now * invSr;
        if(wowPhaseL_ >= 1.0f) wowPhaseL_ -= 1.0f;

        // Wow waveform: sine blended with filtered noise for organic motion
        float wowSineL = std::sin(kTwoPi * wowPhaseL_);

        // Update flutter phase
        fltPhaseL_ += flutterRateL_now * invSr;
        if(fltPhaseL_ >= 1.0f) fltPhaseL_ -= 1.0f;

        // Flutter waveform: pure sine
        float flutterSineL = std::sin(kTwoPi * fltPhaseL_);

        const float wowNoiseMixBase = Clamp(wowRandomnessFactor_, 0.0f, 0.6f);
        const float flutterNoiseMixBase = Clamp(flutterTurbulenceFactor_, 0.0f, 0.6f);
        const float depthSlow = std::sqrt(std::max(depthShape_, 0.0f));
        const float wowNoiseMix = Clamp(wowNoiseMixBase * (0.10f + 0.45f * depthSlow), 0.0f, 0.65f);
        const float flutterNoiseMix = Clamp(flutterNoiseMixBase * (0.12f + 0.45f * depthSlow), 0.0f, 0.7f);
        const float wowSineMix = 1.0f - wowNoiseMix;
        const float flutterSineMix = 1.0f - flutterNoiseMix;

        const float wowShapeL = wowSineMix * wowSineL + wowNoiseMix * wowNoiseStateL_;
        const float flutterShapeL = flutterSineMix * flutterSineL + flutterNoiseMix * flutterNoiseStateL_;

        // Combine modulations (simple addition - wow + flutter)
        const float flutterContribution = 0.4f + 0.5f * depthShape_;
        float devSecL = driftL_;
        devSecL += wowDepthL_now * wowShapeL;
        devSecL += flutterDepthL_now * flutterShapeL;
        devSecL += flutterJitL_ * flutterContribution;

        // --- RIGHT CHANNEL ---

        // Update wow phase
        wowPhaseR_ += wowRateR_now * invSr;
        if(wowPhaseR_ >= 1.0f) wowPhaseR_ -= 1.0f;

        float wowSineR = std::sin(kTwoPi * wowPhaseR_);

        // Update flutter phase
        fltPhaseR_ += flutterRateR_now * invSr;
        if(fltPhaseR_ >= 1.0f) fltPhaseR_ -= 1.0f;

        float flutterSineR = std::sin(kTwoPi * fltPhaseR_);

        const float wowShapeR = wowSineMix * wowSineR + wowNoiseMix * wowNoiseStateR_;
        const float flutterShapeR = flutterSineMix * flutterSineR + flutterNoiseMix * flutterNoiseStateR_;

        // Combine modulations (simple addition - wow + flutter)
        float devSecR = driftR_;
        devSecR += wowDepthR_now * wowShapeR;
        devSecR += flutterDepthR_now * flutterShapeR;
        devSecR += flutterJitR_ * flutterContribution;

        if(stereoBlend_ < 0.999f)
        {
            const float monoDev = 0.5f * (devSecL + devSecR);
            devSecL = monoDev + stereoBlend_ * (devSecL - monoDev);
            devSecR = monoDev + stereoBlend_ * (devSecR - monoDev);
        }

        devSecL = Clamp(devSecL, -maxDevSec_, maxDevSec_);
        devSecR = Clamp(devSecR, -maxDevSec_, maxDevSec_);

        float readSecL = std::max(kMinReadSec, baseSecL + devSecL);
        float readSecR = std::max(kMinReadSec, baseSecR + devSecR);

        float targetSamplesL = readSecL * sampleRate_;
        float targetSamplesR = readSecR * sampleRate_;

        // Gentle slew-rate limiter: smoothly approach target to prevent clicks
        // Lower alpha = more responsive, higher = more smoothing
        const float slewAlpha = slewAlpha_;
        float smoothTargetL = slewAlpha * readStateL_ + (1.0f - slewAlpha) * targetSamplesL;
        float smoothTargetR = slewAlpha * readStateR_ + (1.0f - slewAlpha) * targetSamplesR;

        float deltaL = smoothTargetL - readStateL_;
        float deltaR = smoothTargetR - readStateR_;

        // Track max read delta for debugging
        float absDeltaL = fabsf(deltaL);
        float absDeltaR = fabsf(deltaR);
        if(absDeltaL > maxReadDeltaSamplesL_) maxReadDeltaSamplesL_ = absDeltaL;
        if(absDeltaR > maxReadDeltaSamplesR_) maxReadDeltaSamplesR_ = absDeltaR;

        // Apply smooth delta (no hard clamp - already limited by slew)
        readStateL_ = smoothTargetL;
        readStateR_ = smoothTargetR;

        float readIndexL = static_cast<float>(writeIndex_) - readStateL_;
        float readIndexR = static_cast<float>(writeIndex_) - readStateR_;
        if(readIndexL < 0.0f) readIndexL += static_cast<float>(delayBufSize_);
        if(readIndexR < 0.0f) readIndexR += static_cast<float>(delayBufSize_);

        // Guard against large jumps in read position that cause impulses/clicks
        // This prevents buffer wrap discontinuities that sound like bit-crushing
        const float maxAllowedJump = 4.0f;  // samples

        // Check left channel for discontinuities
        float jumpDeltaL = readIndexL - prevReadPosL_;
        // Handle wrap-around: if delta is huge, we wrapped
        if(jumpDeltaL > static_cast<float>(delayBufSize_) / 2.0f) {
            jumpDeltaL -= static_cast<float>(delayBufSize_);
        } else if(jumpDeltaL < -static_cast<float>(delayBufSize_) / 2.0f) {
            jumpDeltaL += static_cast<float>(delayBufSize_);
        }

        // Track maximum delta for debugging
        float absJumpL = fabsf(jumpDeltaL);
        if(absJumpL > maxDeltaL_) maxDeltaL_ = absJumpL;

        if(absJumpL > maxAllowedJump) {
            jumpCountL_++;
            readIndexL = prevReadPosL_ + (jumpDeltaL > 0.0f ? maxAllowedJump : -maxAllowedJump);
            if(readIndexL < 0.0f) readIndexL += static_cast<float>(delayBufSize_);
            if(readIndexL >= static_cast<float>(delayBufSize_)) readIndexL -= static_cast<float>(delayBufSize_);
        }
        prevReadPosL_ = readIndexL;

        // Check right channel for discontinuities
        float jumpDeltaR = readIndexR - prevReadPosR_;
        if(jumpDeltaR > static_cast<float>(delayBufSize_) / 2.0f) {
            jumpDeltaR -= static_cast<float>(delayBufSize_);
        } else if(jumpDeltaR < -static_cast<float>(delayBufSize_) / 2.0f) {
            jumpDeltaR += static_cast<float>(delayBufSize_);
        }

        // Track maximum delta for debugging
        float absJumpR = fabsf(jumpDeltaR);
        if(absJumpR > maxDeltaR_) maxDeltaR_ = absJumpR;

        if(absJumpR > maxAllowedJump) {
            jumpCountR_++;
            readIndexR = prevReadPosR_ + (jumpDeltaR > 0.0f ? maxAllowedJump : -maxAllowedJump);
            if(readIndexR < 0.0f) readIndexR += static_cast<float>(delayBufSize_);
            if(readIndexR >= static_cast<float>(delayBufSize_)) readIndexR -= static_cast<float>(delayBufSize_);
        }
        prevReadPosR_ = readIndexR;

        float delayedL = InterpolateCubic(delayBufL_.get(), delayBufSize_, readIndexL);
        float delayedR = InterpolateCubic(delayBufR_.get(), delayBufSize_, readIndexR);

        delayBufL_[writeIndex_] = xL;
        delayBufR_[writeIndex_] = xR;

        writeIndex_++;
        if(writeIndex_ >= delayBufSize_)
            writeIndex_ = 0;

        // Crossfade between dry and delayed signal based on smoothed activation
        // This prevents clicks when engaging/disengaging wow/flutter
        const float wet = crossfadeGain;
        const float dry = 1.0f - wet;
        out[0][i] = dry * xL + wet * delayedL;
        out[1][i] = dry * xR + wet * delayedR;
    }

    // Save crossfade state for next block
    crossfadeGain_ = crossfadeGain;
}

float WowFlutterModule::NextRand()
{
    randState_ = randState_ * 1664525u + 1013904223u;
    return static_cast<float>(randState_) * 2.3283064365386963e-10f;
}

float WowFlutterModule::NextRandCentered()
{
    return NextRand() * 2.0f - 1.0f;
}

float WowFlutterModule::NextRandRange(float min, float max)
{
    return min + NextRand() * (max - min);
}

float WowFlutterModule::TriangleWave(float phase)
{
    // phase in 0-1 range
    // Triangle: rises 0->1 in first half, falls 1->0 in second half
    if(phase < 0.5f) {
        return 4.0f * phase - 1.0f;  // -1 to +1
    } else {
        return 3.0f - 4.0f * phase;  // +1 to -1
    }
}
