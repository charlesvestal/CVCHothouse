#pragma once

#include <cmath>
#include <memory>
#include <cstdint>
#include "daisysp.h"

class WowFlutterModule
{
  public:
    void Init(float sampleRate);
    void SetAmount(float amount);

    // Set depth multiplier for tape age/condition (1.0 = normal)
    void SetDepthMultiplier(float multiplier);

    void UpdateControls();
    void Process(float** in, float** out, size_t size);

  private:
    float InterpolateLinear(const float* buf, size_t size, float index);

    float sampleRate_ = 48000.0f;
    float invSampleRate_ = 1.0f;

    float targetAmount_   = 0.0f;
    float smoothedAmount_ = 0.0f;

    // Multiplier for tape age/condition
    float depthMultiplier_ = 1.0f;

    float wowAmount_    = 0.0f;
    float wowRateHz_    = 0.1f;
    float flutterAmount_= 0.0f;
    float flutterRateHz_= 4.0f;

    float wowPhaseL_    = 0.0f;
    float wowPhaseR_    = 0.0f;
    float fltPhaseL_    = 0.0f;
    float fltPhaseR_    = 0.0f;
    float driftL_       = 0.0f;
    float driftR_       = 0.0f;
    float readStateL_   = 0.0f;
    float readStateR_   = 0.0f;
    float flutterJitL_  = 0.0f;
    float flutterJitR_  = 0.0f;
    static constexpr size_t kMaxDelaySamples = 48000;
    std::unique_ptr<float[]> delayBufL_;
    std::unique_ptr<float[]> delayBufR_;
    size_t delayBufSize_ = kMaxDelaySamples;
    size_t writeIndex_ = 0;
    float baseDelaySamplesL_ = 0.0f;
    float baseDelaySamplesR_ = 0.0f;

    static constexpr float kAmountSmooth = 0.005f;
    static constexpr float kDriftSmooth  = 0.0008f;
    static constexpr float kDriftCoeff  = 0.0015f;
    static constexpr float kDriftScale  = 0.0006f;
    static constexpr float kJitterAlpha = 0.004f;
    static constexpr float kMaxDevSec   = 0.0018f;
    static constexpr float kMaxDeltaSamples = 0.75f;
    static constexpr float kMinReadSec  = 0.0002f;
    uint32_t randState_ = 0x1234567u;

    float NextRand();
    static inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    float NextRandCentered();
};

