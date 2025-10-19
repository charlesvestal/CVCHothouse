#pragma once

#include "SimpleReverb.h"

// Simple reverb - stable and artifact-free
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

    SimpleReverb reverb_;

    static constexpr float kMixSmooth = 0.01f;
};
