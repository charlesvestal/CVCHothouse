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

    for(auto& svf : hfRollOff_)
    {
        svf.Init(sampleRate_);
        svf.SetFreq(hfRollOffCutoff_);
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
    }
    else
    {
        hfRolloffOverride_ = 0.0f;  // Disable override
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
    tapeCompressionRatio_ = kCompMin + drive * kCompRange;

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

    for(auto& svf : hfRollOff_)
    {
        svf.SetFreq(hfRollOffCutoff_);
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

void TapeSatModule::Process(float** buffers, size_t size)
{
    if(smoothedDrive_ < kDriveBypass)
    {
        // effectively bypass: nothing to do, assume buffers already contain input
        return;
    }

    const float driveLin   = tapeDriveLin_;
    const float satFactor  = 1.0f + tapeSaturationFactor_ * 2.0f;
    const float ratio      = tapeCompressionRatio_;
    const float bias       = biasGain_;

    for(size_t i = 0; i < size; ++i)
    {
        float xL = buffers[0][i] * driveLin * bias;
        float xR = buffers[1][i] * driveLin * bias;

        xL = tanhf(xL * satFactor);
        xR = tanhf(xR * satFactor);

        xL = SimpleCompressor(xL, ratio);
        xR = SimpleCompressor(xR, ratio);

        hfRollOff_[0].Process(xL);
        hfRollOff_[1].Process(xR);
        xL = hfRollOff_[0].Low();
        xR = hfRollOff_[1].Low();

        buffers[0][i] = xL;
        buffers[1][i] = xR;
    }
}
