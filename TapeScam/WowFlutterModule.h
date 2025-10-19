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
    float InterpolateCubic(const float* buf, size_t size, float index);

    float sampleRate_ = 48000.0f;
    float invSampleRate_ = 1.0f;

    float targetAmount_   = 0.0f;
    float smoothedAmount_ = 0.0f;

    // Multiplier for tape age/condition
    float depthMultiplier_ = 1.0f;

    // Randomness/turbulence factors (tied to tape age)
    float wowRandomnessFactor_ = 0.4f;      // 0-1, introduces random variation to wow
    float flutterTurbulenceFactor_ = 0.2f;  // 0-1, adds turbulence to flutter

    float wowAmount_    = 0.0f;
    float wowRateHz_    = 0.1f;
    float flutterAmount_= 0.0f;
    float flutterRateHz_= 4.0f;

    // Actual modulated rates/depths (updated at slow rate, not per sample)
    float wowRateL_actual_ = 0.1f;
    float wowRateR_actual_ = 0.1f;
    float wowDepthL_actual_ = 0.0f;
    float wowDepthR_actual_ = 0.0f;
    float flutterRateL_actual_ = 4.0f;
    float flutterRateR_actual_ = 4.0f;
    float flutterDepthL_actual_ = 0.0f;
    float flutterDepthR_actual_ = 0.0f;

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

    // Flutter burst state
    float burstTimerL_ = 0.0f;
    float burstTimerR_ = 0.0f;
    float burstAmountL_ = 1.0f;
    float burstAmountR_ = 1.0f;
    float burstCountdownL_ = 0.0f;
    float burstCountdownR_ = 0.0f;

    // Randomness update timer (update at ~20 Hz, not per sample)
    float randomUpdateTimer_ = 0.0f;

    // Previous read positions for discontinuity detection
    float prevReadPosL_ = 0.0f;
    float prevReadPosR_ = 0.0f;

    // Debug: track maximum deltas per channel
    float maxDeltaL_ = 0.0f;
    float maxDeltaR_ = 0.0f;
    uint32_t jumpCountL_ = 0;
    uint32_t jumpCountR_ = 0;
    uint32_t burstCountL_ = 0;
    uint32_t burstCountR_ = 0;
    float maxReadDeltaSamplesL_ = 0.0f;
    float maxReadDeltaSamplesR_ = 0.0f;
    static constexpr size_t kMaxDelaySamples = 2048;  // ~42ms at 48kHz, plenty for wow/flutter
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
    float NextRandRange(float min, float max);
    static inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    float NextRandCentered();
    float TriangleWave(float phase);  // phase in 0-1 range
};

