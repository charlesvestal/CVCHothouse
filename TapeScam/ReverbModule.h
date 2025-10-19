#pragma once

#include <cmath>
#include <memory>
#include "daisysp.h"

class ReverbModule
{
  public:
    void Init(float sampleRate);

    // Set reverb parameters
    void SetMix(float mix);          // 0.0 = dry, 1.0 = fully wet
    void SetDecayTime(float seconds); // Reverb tail length in seconds
    void SetPreDelay(float ms);       // Pre-delay in milliseconds

    // Call each control refresh to smooth parameters
    void UpdateControls();

    // Process audio (stereo non-interleaved buffers)
    void Process(float** in, float** out, size_t size);

  private:
    static inline float Clamp(float v, float lo, float hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    float sampleRate_ = 48000.0f;

    // Target parameters from UI
    float targetMix_     = 0.0f;
    float targetDecay_   = 1.0f;
    float targetPreDelayMs_ = 0.0f;

    // Smoothed parameters
    float smoothedMix_   = 0.0f;
    float smoothedDecay_ = 1.0f;

    // Pre-delay buffer
    static constexpr size_t kMaxPreDelaySamples = 4800; // 100ms at 48kHz
    std::unique_ptr<float[]> preDelayBufL_;
    std::unique_ptr<float[]> preDelayBufR_;
    size_t preDelayWriteIdx_ = 0;
    size_t preDelaySamples_  = 0;

    // DaisySP reverb engine
    daisysp::ReverbSc reverb_;

    // Smoothing coefficients
    static constexpr float kMixSmooth = 0.01f;
    static constexpr float kDecaySmooth = 0.005f;

    bool paramsDirty_ = true;
};
