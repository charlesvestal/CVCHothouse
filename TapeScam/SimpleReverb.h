#pragma once

#include <memory>

// Very simple stereo reverb using comb + allpass filters
// Designed to be stable with modulated input signals
class SimpleReverb
{
  public:
    void Init(float sampleRate);
    void SetFeedback(float fb);  // 0-1
    void SetDamping(float damp);  // 0-1
    void Process(float inL, float inR, float* outL, float* outR);

  private:
    float sampleRate_ = 48000.0f;
    float feedback_ = 0.5f;
    float damping_ = 0.5f;

    // Comb filter delays (in samples at 48kHz)
    static constexpr int kCombDelays[4] = {1557, 1617, 1491, 1422};
    static constexpr int kAllpassDelays[2] = {225, 341};

    // Simple comb filter state
    struct CombFilter {
        std::unique_ptr<float[]> buffer;
        int bufferSize;
        int writePos;
        float filterState;
    };

    // Simple allpass filter state
    struct AllpassFilter {
        std::unique_ptr<float[]> buffer;
        int bufferSize;
        int writePos;
    };

    CombFilter combsL_[4];
    CombFilter combsR_[4];
    AllpassFilter allpassL_[2];
    AllpassFilter allpassR_[2];
};
