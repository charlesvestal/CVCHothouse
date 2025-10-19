#pragma once

#include "daisysp.h"

// Minimal reverb wrapper - just DaisySP ReverbSc, no pre-delay or extras
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
    float decayTime_ = 1.0f;

    daisysp::ReverbSc reverb_;
};
