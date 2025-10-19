#pragma once

#include <cmath>
#include "daisysp.h"

class TapeSatModule
{
  public:
    void Init(float sampleRate);

    // Set target drive (0…1) and mark parameters dirty
    void SetDrive(float normalizedDrive);

    // Set drive multiplier for tape age/condition (1.0 = normal)
    void SetDriveMultiplier(float multiplier);

    // Set HF rolloff cutoff frequency for tape speed mode (Hz)
    void SetHFRolloffCutoff(float cutoffHz);

    // Call each control refresh to smooth and update filter coefficients
    void UpdateControls();

    // Process audio in place (stereo non-interleaved buffers)
    void Process(float** buffers, size_t size);

  private:
    static inline float Clamp(float v, float lo, float hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    float dBToLin(float dB) const { return powf(10.0f, dB / 20.0f); }
    float SimpleCompressor(float inSample, float ratio) const;

    float sampleRate_ = 48000.0f;

    // Target drive (0..1) from UI
    float targetDrive_ = 0.0f;
    float smoothedDrive_ = 0.0f;

    // Multiplier for tape age/condition
    float driveMultiplier_ = 1.0f;

    // HF rolloff override for tape speed mode (0 = use drive-based calculation)
    float hfRolloffOverride_ = 0.0f;

    // Derived parameters
    float tapeDrive_dB_ = 0.0f;
    float tapeDriveLin_ = 1.0f;
    float tapeSaturationFactor_ = 0.0f;
    float tapeCompressionRatio_ = 1.0f;
    float hfRollOffCutoff_ = 20000.0f;
    float biasGain_ = 1.0f;

    // Biquad per channel for HF roll-off
    daisysp::Svf hfRollOff_[2];

    // Smoothing coefficient for drive parameter
    float driveSmoothCoeff_ = 0.005f;

    bool paramsDirty_ = true;
};
