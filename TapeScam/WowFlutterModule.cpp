#include "WowFlutterModule.h"

#include <algorithm>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
constexpr float kTwoPi = 6.283185307179586476925286766559f;
constexpr float kBaseDelayMsL = 12.0f;
constexpr float kBaseDelayMsR = 15.0f;
constexpr float kMaxDeltaSamples = 0.9f;
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

    wowPhaseL_ = wowPhaseR_ = 0.0f;
    fltPhaseL_ = fltPhaseR_ = 0.0f;
    driftL_ = driftR_ = 0.0f;
    flutterJitL_ = flutterJitR_ = 0.0f;

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
        wowRandomnessFactor_ = 0.7f;
        flutterTurbulenceFactor_ = 0.4f;
    }
}

void WowFlutterModule::UpdateControls()
{
    smoothedAmount_ += (targetAmount_ - smoothedAmount_) * kAmountSmooth;

    const float amt = smoothedAmount_;
    const float shaped = Clamp(amt * amt, 0.0f, 1.0f);

    // Apply depth multiplier to wow and flutter amounts
    wowAmount_     = shaped * depthMultiplier_;
    wowRateHz_     = 0.015f + shaped * 0.085f;    // 0.015 – 0.100 Hz (slightly slower lower bound)
    flutterAmount_ = 0.35f * Clamp(amt, 0.0f, 1.0f) * depthMultiplier_;
    flutterRateHz_ = 4.5f + flutterAmount_ * 8.0f; // 4.5 – ~7.3 Hz
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
    const float amount = smoothedAmount_;
    if(amount < 0.0005f)
    {
        if(in != out)
        {
            for(size_t i = 0; i < size; ++i)
            {
                out[0][i] = in[0][i];
                out[1][i] = in[1][i];
            }
        }
        return;
    }

    const float wowAmt        = wowAmount_;
    const float flutterAmt    = flutterAmount_;

    const float invSr         = invSampleRate_;
    const float baseSecL      = baseDelaySamplesL_ * invSr;
    const float baseSecR      = baseDelaySamplesR_ * invSr;

    const float maxWowDepthSec = 0.020f;  // 20ms max

    // Update burst timers (runs once per buffer)
    const float burstIntervalSec = 3.0f;  // Average time between bursts
    burstTimerL_ += static_cast<float>(size) * invSr;
    burstTimerR_ += static_cast<float>(size) * invSr;

    // Update randomness/turbulence values at ~20 Hz to avoid aliasing
    // CRITICAL: Don't update these per-sample or you get bitcrushing!
    const float randomUpdateInterval = 0.05f;  // 20 Hz
    randomUpdateTimer_ += static_cast<float>(size) * invSr;
    if(randomUpdateTimer_ >= randomUpdateInterval) {
        randomUpdateTimer_ = 0.0f;

        // Update wow rate and depth with randomness (LEFT)
        wowRateL_actual_ = wowRateHz_ * 1.0f * (1.0f + wowRandomnessFactor_ * NextRandRange(-0.2f, 0.2f));
        wowDepthL_actual_ = maxWowDepthSec * wowAmt * (1.0f + wowRandomnessFactor_ * NextRandRange(-0.3f, 0.3f));

        // Update wow rate and depth with randomness (RIGHT)
        wowRateR_actual_ = wowRateHz_ * 1.035f * (1.0f + wowRandomnessFactor_ * NextRandRange(-0.2f, 0.2f));
        wowDepthR_actual_ = maxWowDepthSec * wowAmt * (1.0f + wowRandomnessFactor_ * NextRandRange(-0.3f, 0.3f));

        // Update flutter with turbulence (LEFT)
        flutterRateL_actual_ = flutterRateHz_ * 1.0f + flutterTurbulenceFactor_ * NextRandRange(-1.0f, 1.0f);
        flutterDepthL_actual_ = 0.0010f * std::sqrt(std::max(flutterAmt, 0.0f));
        flutterDepthL_actual_ *= (1.0f + flutterTurbulenceFactor_ * NextRandRange(-0.5f, 0.5f));

        // Update flutter with turbulence (RIGHT)
        flutterRateR_actual_ = flutterRateHz_ * 0.96f + flutterTurbulenceFactor_ * NextRandRange(-1.0f, 1.0f);
        flutterDepthR_actual_ = 0.0010f * std::sqrt(std::max(flutterAmt, 0.0f));
        flutterDepthR_actual_ *= (1.0f + flutterTurbulenceFactor_ * NextRandRange(-0.5f, 0.5f));
    }

    for(size_t i = 0; i < size; ++i)
    {
        float xL = in[0][i];
        float xR = in[1][i];

        // --- LEFT CHANNEL ---

        // Update wow phase (using pre-calculated rate)
        wowPhaseL_ += wowRateL_actual_ * invSr;
        if(wowPhaseL_ >= 1.0f) wowPhaseL_ -= 1.0f;

        // Wow waveform: pure sine (triangle has sharp corners = harmonics = aliasing)
        float wowL = std::sin(kTwoPi * wowPhaseL_);

        // Update flutter phase (using pre-calculated rate)
        fltPhaseL_ += flutterRateL_actual_ * invSr;
        if(fltPhaseL_ >= 1.0f) fltPhaseL_ -= 1.0f;

        // Flutter waveform (no per-sample noise!)
        flutterJitL_ = (1.0f - kJitterAlpha) * flutterJitL_ + kJitterAlpha * NextRandCentered();
        float flutterL = std::sin(kTwoPi * fltPhaseL_);

        // Flutter burst logic
        if(burstCountdownL_ > 0.0f) {
            burstCountdownL_ -= invSr;
            if(burstCountdownL_ <= 0.0f) {
                burstAmountL_ = 1.0f;  // End burst
            }
        } else if(burstTimerL_ > burstIntervalSec) {
            // Trigger new burst with some randomness
            if(NextRand() < (invSr * static_cast<float>(size) / burstIntervalSec)) {
                burstCountdownL_ = NextRandRange(0.1f, 0.3f);
                burstAmountL_ = 2.0f;  // Double flutter during burst
                burstCountL_++;
                burstTimerL_ = 0.0f;
            }
        }

        // Drift
        float driftTargetL = kDriftScale * NextRandCentered();
        driftL_ += kDriftCoeff * (driftTargetL - driftL_);

        // Combine modulations (using pre-calculated depths)
        float devSecL = wowDepthL_actual_ * wowL
                      + flutterDepthL_actual_ * burstAmountL_ * (flutterL + 0.35f * flutterJitL_)
                      + driftL_;

        // --- RIGHT CHANNEL ---

        // Update wow phase (using pre-calculated rate)
        wowPhaseR_ += wowRateR_actual_ * invSr;
        if(wowPhaseR_ >= 1.0f) wowPhaseR_ -= 1.0f;

        // Wow waveform: pure sine (triangle has sharp corners = harmonics = aliasing)
        float wowR = std::sin(kTwoPi * wowPhaseR_);

        // Update flutter phase (using pre-calculated rate)
        fltPhaseR_ += flutterRateR_actual_ * invSr;
        if(fltPhaseR_ >= 1.0f) fltPhaseR_ -= 1.0f;

        // Flutter waveform (no per-sample noise!)
        flutterJitR_ = (1.0f - kJitterAlpha) * flutterJitR_ + kJitterAlpha * NextRandCentered();
        float flutterR = std::sin(kTwoPi * fltPhaseR_);

        // Flutter burst logic
        if(burstCountdownR_ > 0.0f) {
            burstCountdownR_ -= invSr;
            if(burstCountdownR_ <= 0.0f) {
                burstAmountR_ = 1.0f;
            }
        } else if(burstTimerR_ > burstIntervalSec) {
            if(NextRand() < (invSr * static_cast<float>(size) / burstIntervalSec)) {
                burstCountdownR_ = NextRandRange(0.1f, 0.3f);
                burstAmountR_ = 2.0f;
                burstCountR_++;
                burstTimerR_ = 0.0f;
            }
        }

        // Drift
        float driftTargetR = kDriftScale * NextRandCentered();
        driftR_ += kDriftCoeff * (driftTargetR - driftR_);

        // Combine modulations (using pre-calculated depths)
        float devSecR = wowDepthR_actual_ * wowR
                      + flutterDepthR_actual_ * burstAmountR_ * (flutterR + 0.35f * flutterJitR_)
                      + driftR_;

        devSecL = Clamp(devSecL, -kMaxDevSec, kMaxDevSec);
        devSecR = Clamp(devSecR, -kMaxDevSec, kMaxDevSec);

        float readSecL = std::max(kMinReadSec, baseSecL + devSecL);
        float readSecR = std::max(kMinReadSec, baseSecR + devSecR);

        float targetSamplesL = readSecL * sampleRate_;
        float targetSamplesR = readSecR * sampleRate_;

        float deltaL = targetSamplesL - readStateL_;
        float deltaR = targetSamplesR - readStateR_;

        // Track max read delta for debugging
        float absDeltaL = fabsf(deltaL);
        float absDeltaR = fabsf(deltaR);
        if(absDeltaL > maxReadDeltaSamplesL_) maxReadDeltaSamplesL_ = absDeltaL;
        if(absDeltaR > maxReadDeltaSamplesR_) maxReadDeltaSamplesR_ = absDeltaR;

        if(deltaL > kMaxDeltaSamples) deltaL = kMaxDeltaSamples;
        else if(deltaL < -kMaxDeltaSamples) deltaL = -kMaxDeltaSamples;
        if(deltaR > kMaxDeltaSamples) deltaR = kMaxDeltaSamples;
        else if(deltaR < -kMaxDeltaSamples) deltaR = -kMaxDeltaSamples;
        readStateL_ += deltaL;
        readStateR_ += deltaR;

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

        out[0][i] = delayedL;
        out[1][i] = delayedR;
    }
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
