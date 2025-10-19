#pragma once

#include <cmath>
#include <memory>
#include "daisysp.h"

class WowFlutterModule
{
  public:
    void Init(float sampleRate);
    void SetAmount(float amount);
    void UpdateControls();
    void Process(float** in, float** out, size_t size);

  private:
    float InterpolateLinear(const float* buf, size_t size, float index);

    float sampleRate_ = 48000.0f;
    float invSampleRate_ = 1.0f;

    float targetAmount_   = 0.0f;
    float smoothedAmount_ = 0.0f;

    float wowDepth_    = 0.0f;
    float wowRate_     = 0.1f;
    float flutterDepth_= 0.0f;
    float flutterRate_ = 4.0f;

    float phaseWow_     = 0.0f;
    float phaseFlutter_ = 0.0f;

    static constexpr size_t kMaxDelaySamples = 48000;
    std::unique_ptr<float[]> delayBufL_;
    std::unique_ptr<float[]> delayBufR_;
    size_t delayBufSize_ = kMaxDelaySamples;
    size_t writeIndex_ = 0;
    float baseDelaySamples_ = 0.0f;

    static constexpr float kAmountSmooth = 0.005f;
};

