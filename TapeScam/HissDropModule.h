#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
#include <cmath>

class HissDropModule
{
  public:
    void Init(float sampleRate, size_t numChannels);
    void SetAmount(float amount);
    void UpdateControls();
    void Process(float** in, float** out, size_t size);

  private:
    struct PinkState
    {
        float b0 = 0.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float b3 = 0.0f;
        float b4 = 0.0f;
        float b5 = 0.0f;
        float b6 = 0.0f;
    };

    float NextRand();
    float NextRandCentered();
    float GenerateNoise(size_t channel);
    float ProcessPink(size_t channel, float white);
    void  ResetPink();
    float ComputeNextDropInterval();

    static constexpr float kAmountSmooth    = 0.01f;
    static constexpr float kBypassThreshold = 0.0005f;
    static constexpr float kDropoutSlew     = 0.0025f;
    static constexpr float kMinDropoutGain  = 0.02f;

    float sampleRate_      = 48000.0f;
    size_t numChannels_    = 2;

    float targetAmount_    = 0.0f;
    float smoothedAmount_  = 0.0f;

    float hissLevelDb_     = -60.0f;
    float hissLevelLin_    = 0.0f;
    float noiseColorFactor_= 0.0f;

    float dropoutRate_             = 0.0f;
    float dropoutDepth_            = 0.0f;
    float dropoutDurationSamples_  = 0.0f;
    float avgDropSamples_          = std::numeric_limits<float>::infinity();
    float samplesUntilNextDrop_    = std::numeric_limits<float>::infinity();
    bool  inDropout_               = false;
    float dropoutSamplesRemaining_ = 0.0f;
    float dropoutGain_             = 1.0f;
    float dropoutTargetGain_       = 1.0f;

    uint32_t randState_ = 0x1234567u;
    std::vector<PinkState> pinkStates_;

    static inline float DbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
    static inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
};
