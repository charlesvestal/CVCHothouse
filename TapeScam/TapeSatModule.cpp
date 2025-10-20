#include "TapeSatModule.h"

#include <algorithm>

namespace
{
constexpr float kMinRollOffHz = 12000.0f;
constexpr float kMaxRollOffHz = 20000.0f;
constexpr float kMaxDriveDb  = 18.0f;
constexpr float kBiasRange   = 0.5f;   // up to +50% bias gain
constexpr float kCompMin     = 1.0f;
constexpr float kCompRange   = 3.0f;   // up to 4:1
constexpr float kThreshold   = 0.8f;   // compressor knee
constexpr float kDriveBypass = 0.01f;  // below this we bypass processing
}

using daisysp::Biquad;

void TapeSatModule::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    targetDrive_ = 0.0f;
    smoothedDrive_ = 0.0f;

    tapeDrive_dB_ = 0.0f;
    tapeDriveLin_ = 1.0f;
    tapeSaturationFactor_ = 0.0f;
    tapeCompressionRatio_ = 1.0f;
    hfRollOffCutoff_ = kMaxRollOffHz;
    biasGain_ = 1.0f;
    asymmetryAmount_ = 0.0f;
    bassCompressionRatio_ = 1.0f;

    compEnvelopeL_ = 0.0f;
    compEnvelopeR_ = 0.0f;

    // Initialize pre-saturation anti-aliasing filters
    // Band-limits input before nonlinear saturation to reduce aliasing
    for(auto& svf : preSatLPF_)
    {
        svf.Init(sampleRate_);
        svf.SetFreq(preSatLPFCutoff_);
        svf.SetRes(0.707f);
        svf.SetDrive(1.0f);
    }

    // Initialize HF rolloff filters
    for(auto& svf : hfRollOff_)
    {
        svf.Init(sampleRate_);
        svf.SetFreq(hfRollOffCutoff_);
        svf.SetRes(0.707f);
        svf.SetDrive(1.0f);
    }

    // Initialize crossover filters for frequency-dependent saturation
    // Split at ~400Hz (bass/mids-highs)
    for(auto& svf : crossover_)
    {
        svf.Init(sampleRate_);
        svf.SetFreq(400.0f);
        svf.SetRes(0.707f);
        svf.SetDrive(1.0f);
    }

    paramsDirty_ = true;
}

void TapeSatModule::SetDrive(float normalizedDrive)
{
    targetDrive_ = Clamp(normalizedDrive, 0.0f, 1.0f);
    paramsDirty_ = true;
}

void TapeSatModule::SetDriveMultiplier(float multiplier)
{
    // Ensure multiplier is positive and within reasonable range
    // Expanded range to allow more extreme speed/age combinations
    driveMultiplier_ = Clamp(multiplier, 0.3f, 3.0f);
    paramsDirty_ = true;
}

void TapeSatModule::SetHFRolloffCutoff(float cutoffHz)
{
    // Clamp to reasonable range for audio filters (8kHz to 22kHz)
    // 0 means use drive-based calculation
    if(cutoffHz > 0.0f)
    {
        hfRolloffOverride_ = Clamp(cutoffHz, 8000.0f, 22000.0f);

        // Set pre-saturation filter based on tape speed mode
        // Slower tape (lower cutoff) = more limited HF response before saturation
        // This reduces aliasing by band-limiting input before nonlinearity
        if(cutoffHz <= 8000.0f) {
            preSatLPFCutoff_ = 16000.0f;  // SPEED_LOFI
        } else if(cutoffHz <= 14000.0f) {
            preSatLPFCutoff_ = 20000.0f;  // SPEED_STANDARD
        } else {
            preSatLPFCutoff_ = 24000.0f;  // SPEED_HIGH
        }
    }
    else
    {
        hfRolloffOverride_ = 0.0f;  // Disable override
        preSatLPFCutoff_ = 20000.0f;  // Default
    }
    paramsDirty_ = true;
}

void TapeSatModule::UpdateControls()
{
    if(!paramsDirty_)
        return;

    // Smooth drive transitions to avoid zippering
    const float delta = targetDrive_ - smoothedDrive_;
    smoothedDrive_ += delta * driveSmoothCoeff_;

    if(fabsf(targetDrive_ - smoothedDrive_) < 1e-4f)
    {
        smoothedDrive_ = targetDrive_;
        paramsDirty_ = false;
    }

    const float drive = smoothedDrive_;
    tapeDrive_dB_         = drive * kMaxDriveDb;
    tapeDriveLin_         = dBToLin(tapeDrive_dB_);

    // Apply drive multiplier to saturation factor
    tapeSaturationFactor_ = drive * driveMultiplier_;

    // High-frequency compression is tighter (faster, more aggressive)
    tapeCompressionRatio_ = kCompMin + drive * kCompRange;

    // Bass compression is looser (more tape-like, slower)
    bassCompressionRatio_ = kCompMin + drive * (kCompRange * 0.6f);

    // Asymmetry creates even-order harmonics (tape hysteresis)
    // Dead zone below 0.1 drive = symmetric (linear tape region)
    // Above 0.1: quadratic mapping for smooth growth
    if(drive < 0.1f)
    {
        asymmetryAmount_ = 0.0f;  // Clean, symmetric response
    }
    else
    {
        // Quadratic mapping: (drive - 0.1)^2 scaled to 0.0 → 0.35
        float driveScaled = (drive - 0.1f) / 0.9f;  // Normalize to 0-1
        asymmetryAmount_ = driveScaled * driveScaled * 0.35f;  // Up to 35% asymmetry
    }

    // Use override cutoff if set, otherwise calculate based on drive
    if(hfRolloffOverride_ > 0.0f)
    {
        hfRollOffCutoff_ = hfRolloffOverride_;
    }
    else
    {
        hfRollOffCutoff_ = kMaxRollOffHz - drive * (kMaxRollOffHz - kMinRollOffHz);
    }

    biasGain_             = 1.0f + drive * kBiasRange;

    // Update pre-saturation anti-aliasing filter
    for(auto& svf : preSatLPF_)
    {
        svf.SetFreq(preSatLPFCutoff_);
        svf.SetRes(0.707f);
        svf.SetDrive(1.0f);
    }

    // Update post-saturation HF rolloff (main anti-aliasing defense)
    // Drive-dependent: 20kHz at drive=0 → 12kHz at drive=1
    // This kills high-order harmonics from tanh() before they alias
    float postSatCutoff = 20000.0f - drive * 8000.0f;  // 20kHz → 12kHz
    for(auto& svf : hfRollOff_)
    {
        svf.SetFreq(postSatCutoff);
        svf.SetRes(0.707f);
        svf.SetDrive(1.0f);
    }
}

float TapeSatModule::SimpleCompressor(float inSample, float ratio) const
{
    const float absIn = fabsf(inSample);
    if(absIn <= kThreshold)
    {
        return inSample;
    }

    float excess = absIn - kThreshold;
    float compressed = kThreshold + (excess / ratio);
        if(compressed > 1.0f)
            compressed = 1.0f;
    return inSample >= 0.0f ? compressed : -compressed;
}

float TapeSatModule::AsymmetricSaturation(float input, float asymmetry) const
{
    // Asymmetric transfer function creates even-order harmonics
    // Tape has magnetic hysteresis which is inherently asymmetric
    // Positive/negative excursions saturate differently

    if(input > 0.0f)
    {
        // Positive side saturates harder (like hitting tape saturation)
        return tanhf(input * (1.0f + asymmetry));
    }
    else
    {
        // Negative side saturates softer (tape bias effect)
        return tanhf(input * (1.0f - asymmetry * 0.5f));
    }
}

float TapeSatModule::TapeSaturationCurve(float input, float satAmount) const
{
    // Original multi-stage tape saturation (restored for authentic sound):
    // 1. Soft knee compression (low levels)
    // 2. Asymmetric saturation (mid levels) - creates even harmonics
    // 3. Hard limiting (high levels)

    const float absIn = fabsf(input);

    if(absIn < 0.3f)
    {
        // Low level - mostly clean with slight compression
        return input * (1.0f + satAmount * 0.1f);
    }
    else if(absIn < 0.7f)
    {
        // Mid level - asymmetric saturation zone
        float asymSat = AsymmetricSaturation(input, asymmetryAmount_);
        // Blend based on saturation amount
        return input * (1.0f - satAmount) + asymSat * satAmount;
    }
    else
    {
        // High level - hard tape saturation
        return tanhf(input * (1.0f + satAmount * 2.0f));
    }
}

void TapeSatModule::Process(float** buffers, size_t size)
{
    if(smoothedDrive_ < kDriveBypass)
    {
        // effectively bypass: nothing to do, assume buffers already contain input
        return;
    }

    const float driveLin      = tapeDriveLin_;
    const float satAmount     = tapeSaturationFactor_;
    const float hfRatio       = tapeCompressionRatio_;
    const float bassRatio     = bassCompressionRatio_;
    const float bias          = biasGain_;
    const float attackCoeff   = compAttackCoeff_;
    const float releaseCoeff  = compReleaseCoeff_;

    for(size_t i = 0; i < size; ++i)
    {
        // 1. Pre-saturation anti-aliasing filter (band-limit input before nonlinearity)
        // Removes ultra-HF content to prevent aliasing from tanh() harmonics
        preSatLPF_[0].Process(buffers[0][i]);
        preSatLPF_[1].Process(buffers[1][i]);
        float xL = preSatLPF_[0].Low() * driveLin * bias;
        float xR = preSatLPF_[1].Low() * driveLin * bias;

        // 2. Split into bass and mids/highs for frequency-dependent processing
        crossover_[0].Process(xL);
        crossover_[1].Process(xR);

        float bassL = crossover_[0].Low();
        float highL = crossover_[0].High();
        float bassR = crossover_[1].Low();
        float highR = crossover_[1].High();

        // --- BASS PROCESSING (slower, looser compression) ---
        // Slow envelope follower for tape-like compression
        float envL = fabsf(bassL);
        if(envL > compEnvelopeL_)
            compEnvelopeL_ += (envL - compEnvelopeL_) * attackCoeff;
        else
            compEnvelopeL_ += (envL - compEnvelopeL_) * releaseCoeff;

        float envR = fabsf(bassR);
        if(envR > compEnvelopeR_)
            compEnvelopeR_ += (envR - compEnvelopeR_) * attackCoeff;
        else
            compEnvelopeR_ += (envR - compEnvelopeR_) * releaseCoeff;

        // Apply slow compression to bass (tape-like)
        float bassGainL = 1.0f;
        if(compEnvelopeL_ > kThreshold)
        {
            bassGainL = kThreshold / (compEnvelopeL_ * bassRatio + kThreshold * (1.0f - bassRatio));
        }

        float bassGainR = 1.0f;
        if(compEnvelopeR_ > kThreshold)
        {
            bassGainR = kThreshold / (compEnvelopeR_ * bassRatio + kThreshold * (1.0f - bassRatio));
        }

        bassL *= bassGainL;
        bassR *= bassGainR;

        // Asymmetric saturation on bass (warmth, even harmonics)
        bassL = TapeSaturationCurve(bassL, satAmount * 0.8f);
        bassR = TapeSaturationCurve(bassR, satAmount * 0.8f);

        // --- MIDS/HIGHS PROCESSING (faster, tighter compression) ---
        // Apply standard compression to highs
        highL = SimpleCompressor(highL, hfRatio);
        highR = SimpleCompressor(highR, hfRatio);

        // Asymmetric saturation on highs (tape characteristic)
        highL = TapeSaturationCurve(highL, satAmount);
        highR = TapeSaturationCurve(highR, satAmount);

        // Recombine bass and highs
        xL = bassL + highL;
        xR = bassR + highR;

        // Final HF rolloff (tape head response)
        hfRollOff_[0].Process(xL);
        hfRollOff_[1].Process(xR);
        xL = hfRollOff_[0].Low();
        xR = hfRollOff_[1].Low();

        buffers[0][i] = xL;
        buffers[1][i] = xR;
    }
}
