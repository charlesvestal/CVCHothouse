#pragma once

#include <cstddef>
#include <vector>
#include <cmath>

class ToneModule
{
  public:
    void Init(float sampleRate, size_t numChannels);
    void SetAmount(float amount);
    void UpdateControls();
    void Process(float** in, float** out, size_t size);

  private:
    struct BiquadState
    {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
    };

    void UpdateCoefficients();
    void ComputeShelf(BiquadState& state, float gainDb, float cutoffHz, bool highShelf);
    static float DbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
    static float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    float sampleRate_ = 48000.0f;
    size_t numChannels_ = 2;

    float targetAmount_   = 0.5f;
    float smoothedAmount_ = 0.5f;

    float bassGainDb_   = 0.0f;
    float trebleGainDb_ = 0.0f;

    float bassCutoff_   = 120.0f;
    float trebleCutoff_ = 6000.0f;

    static constexpr float kSmooth = 0.5f;  // Near-instant for plugin (was 0.01f hardware)

    std::vector<BiquadState> lowShelf_;
    std::vector<BiquadState> highShelf_;
};
