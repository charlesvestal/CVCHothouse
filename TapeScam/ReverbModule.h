#pragma once

#include "daisysp.h"

// Minimal reverb wrapper - just DaisySP ReverbSc, no extra buffers
class ReverbModule
{
  public:
    int Init(float sampleRate);
    void SetMix(float mix);
    void SetDecayTime(float seconds);
    void UpdateControls();
    void Process(float** in, float** out, size_t size);

  private:
    float sampleRate_ = 48000.0f;
    float mix_ = 0.0f;
    float targetMix_ = 0.0f;

    daisysp::ReverbSc reverb_;

    // Pre-filter states to remove high-frequency artifacts before reverb
    float preFilterL1_ = 0.0f;
    float preFilterL2_ = 0.0f;
    float preFilterR1_ = 0.0f;
    float preFilterR2_ = 0.0f;

    static constexpr float kMixSmooth = 0.01f;
};
