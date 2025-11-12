#include "TapeSatModule.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kMinRollOffHz = 14000.0f;  // Raised from 12kHz for cleaner highs
constexpr float kMaxRollOffHz = 20000.0f;
constexpr float kMaxDriveDb  = 24.0f;
constexpr float kBiasRange   = 0.5f;   // up to +50% bias gain
constexpr float kCompMin     = 1.0f;
constexpr float kCompRange   = 3.0f;   // up to 4:1
constexpr float kKneeStart   = 0.6f;   // Soft knee start
constexpr float kKneeEnd     = 0.9f;   // Soft knee end (was hard threshold at 0.8)
constexpr float kDriveBypass = 0.01f;  // below this we bypass processing
}

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
    asymmetryAmountHigh_ = 0.0f;
    bassCompressionRatio_ = 1.0f;
    highSaturationLimit_ = 1.0f;

    compEnvelopeL_ = 0.0f;
    compEnvelopeR_ = 0.0f;

    // Initialize oversampling history
    bassL_prev_ = 0.0f;
    bassR_prev_ = 0.0f;
    highL_prev_ = 0.0f;
    highR_prev_ = 0.0f;

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
    float shaped = Clamp(normalizedDrive, 0.0f, 1.0f);
    shaped = std::pow(shaped, 0.8f); // more impact earlier in the knob travel
    targetDrive_ = shaped;
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
    tapeSaturationFactor_ = Clamp(drive * 1.35f * driveMultiplier_, 0.0f, 2.0f);

    // High-frequency compression is tighter (faster, more aggressive)
    tapeCompressionRatio_ = kCompMin + drive * kCompRange;

    // Bass compression is looser (more tape-like, slower)
    bassCompressionRatio_ = kCompMin + drive * (kCompRange * 0.6f);

    // Asymmetry creates even-order harmonics (tape hysteresis)
    // Bass band: Quadratic mapping for smooth growth, up to 0.4 asymmetry
    asymmetryAmount_ = drive * drive * 0.55f;

    // High band: Dead zone below 0.1 drive, then gentler asymmetry growth
    // This reduces high-frequency harmonic generation to minimize aliasing
    if(drive < 0.1f)
    {
        asymmetryAmountHigh_ = 0.0f;  // No high-band asymmetry at low drive
    }
    else
    {
        // Quadratic mapping: (drive - 0.1)^2 scaled to 0.0 → 0.25 (less than bass)
        float driveScaled = (drive - 0.1f) / 0.9f;  // Normalize to 0-1
        asymmetryAmountHigh_ = driveScaled * driveScaled * 0.35f;
    }

    // High-band saturation limiter: reduce saturation strength in highs to prevent aliasing
    // At drive=0: moderate saturation (0.6x)
    // At drive=1: very limited to 0.3x saturation strength
    highSaturationLimit_ = 0.7f - drive * 0.4f;  // 0.7 → 0.3

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
    // Drive-dependent: 18kHz at drive=0 → 10kHz at drive=1
    // Very aggressive to eliminate aliasing artifacts
    float postSatCutoff = 18000.0f - drive * 8000.0f;  // 18kHz → 10kHz
    for(auto& svf : hfRollOff_)
    {
        svf.SetFreq(postSatCutoff);
        svf.SetRes(0.707f);
        svf.SetDrive(1.0f);
    }
}

float TapeSatModule::SoftKneeCompressor(float inSample, float ratio, float kneeStart, float kneeEnd) const
{
    const float absIn = fabsf(inSample);

    if(absIn <= kneeStart)
    {
        // Below knee start - no compression
        return inSample;
    }
    else if(absIn < kneeEnd)
    {
        // In the knee region - gradual transition from 1:1 to full ratio
        // This creates a smooth curve that reduces harsh harmonic generation
        float kneePosition = (absIn - kneeStart) / (kneeEnd - kneeStart);  // 0.0 → 1.0
        float currentRatio = 1.0f + kneePosition * (ratio - 1.0f);  // Smoothly increase ratio

        float excess = absIn - kneeStart;
        float compressed = kneeStart + (excess / currentRatio);

        return inSample >= 0.0f ? compressed : -compressed;
    }
    else
    {
        // Above knee end - full compression ratio
        float excess = absIn - kneeEnd;
        float compressed = kneeEnd + (excess / ratio);
        if(compressed > 1.0f)
            compressed = 1.0f;

        return inSample >= 0.0f ? compressed : -compressed;
    }
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

float TapeSatModule::TapeSaturationCurve(float input, float satAmount, float asymmetry) const
{
    // Pure smooth tanh - absolutely no discontinuities
    // Asymmetry parameter currently unused - all character from drive + filtering

    if(satAmount < 0.01f)
    {
        // Bypass saturation entirely at very low settings
        return input;
    }

    // Simple tanh saturation - mathematically smooth everywhere
    // Very conservative drive multiplier
    float drive = 1.0f + satAmount * 0.8f;  // Max drive = 1.8x
    return tanhf(input * drive);
}


void TapeSatModule::Process(float** buffers, size_t size)
{
    if(smoothedDrive_ < kDriveBypass)
    {
        return;  // Bypass if drive is very low
    }

    const float satAmount = tapeSaturationFactor_;

    if(satAmount < 0.01f)
    {
        return;  // Bypass if saturation is off
    }

    // Tape saturation using smooth tanh curve
    const float drive = 1.0f + satAmount * 2.5f;

    for(size_t i = 0; i < size; ++i)
    {
        buffers[0][i] = tanhf(buffers[0][i] * drive);
        buffers[1][i] = tanhf(buffers[1][i] * drive);
    }
}
