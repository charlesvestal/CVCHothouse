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

    // Soft-knee compression for smoother transitions (reduces harsh harmonics)
    float SoftKneeCompressor(float inSample, float ratio, float kneeStart, float kneeEnd) const;

    // Enhanced tape saturation characteristics
    float TapeSaturationCurve(float input, float satAmount, float asymmetry) const;
    float AsymmetricSaturation(float input, float asymmetry) const;

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
    float preSatLPFCutoff_ = 20000.0f;  // Pre-sat anti-alias filter cutoff
    float hfRollOffCutoff_ = 20000.0f;
    float biasGain_ = 1.0f;
    float asymmetryAmount_ = 0.0f;  // Even-order harmonic generation (bass band)
    float asymmetryAmountHigh_ = 0.0f;  // Even-order harmonic generation (high band)
    float bassCompressionRatio_ = 1.0f;  // Frequency-dependent compression
    float highSaturationLimit_ = 1.0f;  // Limits high-band saturation to reduce aliasing

    // Pre-saturation anti-aliasing filter (band-limits input before nonlinearity)
    daisysp::Svf preSatLPF_[2];  // Removes ultra-HF content to reduce aliasing

    // Biquad per channel for HF roll-off
    daisysp::Svf hfRollOff_[2];

    // Split-band processing for frequency-dependent saturation
    daisysp::Svf crossover_[2];  // Low/high split at ~400Hz

    // Slow compression envelope followers (for tape-like compression)
    float compEnvelopeL_ = 0.0f;
    float compEnvelopeR_ = 0.0f;
    float compAttackCoeff_ = 0.01f;   // ~10ms at 48kHz
    float compReleaseCoeff_ = 0.001f; // ~100ms at 48kHz

    // Smoothing coefficient for drive parameter
    float driveSmoothCoeff_ = 0.005f;

    // History for linear interpolation in oversampling
    float bassL_prev_ = 0.0f;
    float bassR_prev_ = 0.0f;
    float highL_prev_ = 0.0f;
    float highR_prev_ = 0.0f;

    bool paramsDirty_ = true;
};
