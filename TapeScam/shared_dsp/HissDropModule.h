#pragma once

#include <vector>
#include <cstdint>

class HissDropModule
{
  public:
    void Init(float sampleRate, size_t numChannels);
    void SetAmount(float amount); // 0..1 noise level

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
    float ProcessPink(size_t channel, float white);

    static constexpr float kAmountSmooth    = 0.5f;
    static constexpr float kBypassThreshold = 0.0005f;
    static constexpr float kMinHissDb       = -60.0f;
    static constexpr float kMaxHissDb       = -6.0f;

    float sampleRate_      = 48000.0f;
    size_t numChannels_    = 2;

    float targetAmount_    = 0.0f;
    float smoothedAmount_  = 0.0f;

    float hissLevelDb_     = kMinHissDb;
    float hissLevelLin_    = 0.0f;
    float noiseColorFactor_= 0.0f;

    uint32_t randState_ = 0x1234567u;
    std::vector<PinkState> pinkStates_;

    static inline float DbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
};
