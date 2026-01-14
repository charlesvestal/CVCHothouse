#pragma once

#include <cmath>
#include <memory>
#include <cstdint>

class WowFlutterModule
{
  public:
    void Init(float sampleRate);
    void SetAmount(float amount);

    // Set depth multiplier for tape age/condition (1.0 = normal)
    void SetDepthMultiplier(float multiplier);
    void SetModeParameters(float wowDepthScale, float flutterDepthScale,
                           float wowRateMinHz, float wowRateMaxHz,
                           float flutterRateMinHz, float flutterRateMaxHz);
    void SetBaselineMotion(float ageAmount, float speedAmount);
    // Allows host to cap the maximum deviation of the virtual tape delay.
    // Typical ranges: 0.003-0.005 (tape), 0.015-0.030 (pedal/vibrato).
    void SetMaxDeviation(float seconds);
    // 0 = dual-mono (fully linked L/R), 1 = original stereo offsets
    void SetStereoBlend(float blend);

    void UpdateControls();
    void Process(float** in, float** out, size_t size);

  private:
    float InterpolateLinear(const float* buf, size_t size, float index);
    float InterpolateCubic(const float* buf, size_t size, float index);

    float sampleRate_ = 48000.0f;
    float invSampleRate_ = 1.0f;

    float targetAmount_   = 0.0f;
    float smoothedAmount_ = 0.0f;
    float depthShape_     = 0.0f;  // nonlinear depth mapping (amt^2)
    float activationRamp_ = 0.0f;  // smooth enable ramp so knob at 0 stays off
    float crossfadeGain_ = 0.0f;  // per-sample smoothed dry/wet crossfade

    // Multiplier for tape age/condition
    float depthMultiplier_ = 1.0f;

    // Randomness/turbulence factors (tied to tape age)
    float wowRandomnessFactor_ = 0.4f;      // 0-1, introduces random variation to wow
    float flutterTurbulenceFactor_ = 0.2f;  // 0-1, adds turbulence to flutter

    float wowAmount_    = 0.0f;
    float wowRateHz_    = 0.1f;
    float flutterAmount_= 0.0f;
    float flutterRateHz_= 4.0f;
    float wowDepthScale_ = 1.0f;
    float flutterDepthScale_ = 1.0f;
    float wowRateMinHz_ = 0.25f;
    float wowRateMaxHz_ = 0.5f;
    float flutterRateMinHz_ = 2.0f;
    float flutterRateMaxHz_ = 5.0f;
    float maxDevSec_ = 0.0042f;   // runtime clamp on pitch deviation
    float slewAlpha_ = 0.9f;      // smoothing toward read target (updated with amount)

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
    float wowNoiseStateL_ = 0.0f;
    float wowNoiseStateR_ = 0.0f;
    float flutterNoiseStateL_ = 0.0f;
    float flutterNoiseStateR_ = 0.0f;
    float stereoBlend_ = 1.0f;

    // Flutter burst state
    float burstTimerL_ = 0.0f;
    float burstTimerR_ = 0.0f;
    float burstAmountL_ = 1.0f;
    float burstAmountR_ = 1.0f;
    float burstCountdownL_ = 0.0f;
    float burstCountdownR_ = 0.0f;

    // Slow LFO phases for smooth parameter modulation (no discontinuities)
    float wowDepthLFOPhase_ = 0.0f;
    float wowRateLFOPhase_ = 0.0f;
    float flutterDepthLFOPhase_ = 0.0f;
    float flutterRateLFOPhase_ = 0.0f;

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

    float baseAgeDriftDepthSec_ = 0.0f;
    float baseSpeedFlutterDepthSec_ = 0.0f;
    float ageDriftAlpha_ = 0.0f;
    float flutterDriftAlpha_ = 0.0f;

    static constexpr float kMaxAgeDriftDepthSec = 0.0015f;
    static constexpr float kMaxSpeedFlutterDepthSec = 0.0006f;


    static constexpr float kAmountSmooth = 0.3f;  // Near-instant for plugin (was 0.005f hardware)
    static constexpr float kDriftSmooth  = 0.0008f;
    static constexpr float kDriftCoeff  = 0.00015f;  // 10× slower drift
    static constexpr float kDriftScale  = 0.0006f;
    static constexpr float kJitterAlpha = 0.0004f;   // 10× slower jitter (50ms time constant)
    static constexpr float kMaxDeltaSamples = 0.75f;
    static constexpr float kMinReadSec  = 0.0002f;
    static constexpr float kWowNoiseAlpha = 0.00018f;
    static constexpr float kFlutterNoiseAlpha = 0.00025f;
    static constexpr float kActivationSmooth = 0.4f;
    uint32_t randState_ = 0x1234567u;

    float NextRand();
    float NextRandRange(float min, float max);
    static inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    float NextRandCentered();
    float TriangleWave(float phase);  // phase in 0-1 range
};
