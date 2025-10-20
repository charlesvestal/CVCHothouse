#pragma once

#include "MiniFDNReverb.h"

// Mini-FDN reverb - feedback delay network for stability with modulated input
class ReverbModule
{
  public:
    int Init(float sampleRate);
    void SetMix(float mix);
    void SetDecayTime(float seconds);
    void SetFeedbackLowpassCutoff(float cutoffHz);
    void SetPostFilterCutoff(float cutoffHz);
    void SetModulation(float depthSec, float rateHz);
    void UpdateControls();
    void Process(float** in, float** out, size_t size);

  private:
    float sampleRate_ = 48000.0f;
    float mix_ = 0.0f;
    float targetMix_ = 0.0f;
    float wetGainComp_ = 0.3f;

    MiniFDNReverb reverb_;

    static constexpr float kMixSmooth = 0.01f;
};
