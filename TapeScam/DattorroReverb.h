#pragma once

#include <memory>

// Dattorro Plate Reverb - smooth, musical reverb
// Based on "Effect Design, Part 1: Reverberator and Other Filters" by Jon Dattorro
// Uses two parallel tank sections with cross-feedback for natural stereo image
class DattorroReverb
{
  public:
    void Init(float sampleRate);
    void SetFeedback(float fb);    // 0-1, controls decay time
    void SetDamping(float damp);   // 0-1, HF damping
    void Process(float inL, float inR, float* outL, float* outR);

  private:
    float sampleRate_ = 48000.0f;
    float feedback_ = 0.7f;
    float damping_ = 0.5f;

    // Pre-delay
    static constexpr int kPreDelaySize = 2400;  // ~50ms at 48kHz
    std::unique_ptr<float[]> preDelayBuf_;
    int preDelayWrite_ = 0;

    // Input diffusion (4 allpass filters)
    static constexpr int kInputAPDelays[4] = {142, 107, 379, 277};
    struct AllpassFilter {
        std::unique_ptr<float[]> buffer;
        int size;
        int writePos;
    };
    AllpassFilter inputAP_[4];

    // Tank structure - two parallel delay lines with cross-feedback
    // Tank 1 (Left)
    static constexpr int kTank1Delay1 = 672;
    static constexpr int kTank1AP1 = 908;
    static constexpr int kTank1Delay2 = 4453;
    static constexpr int kTank1AP2 = 1800;

    // Tank 2 (Right) - slightly different for stereo decorrelation
    static constexpr int kTank2Delay1 = 908;
    static constexpr int kTank2AP1 = 672;
    static constexpr int kTank2Delay2 = 4217;
    static constexpr int kTank2AP2 = 2656;

    std::unique_ptr<float[]> tank1Delay1Buf_;
    std::unique_ptr<float[]> tank1AP1Buf_;
    std::unique_ptr<float[]> tank1Delay2Buf_;
    std::unique_ptr<float[]> tank1AP2Buf_;

    std::unique_ptr<float[]> tank2Delay1Buf_;
    std::unique_ptr<float[]> tank2AP1Buf_;
    std::unique_ptr<float[]> tank2Delay2Buf_;
    std::unique_ptr<float[]> tank2AP2Buf_;

    int tank1Delay1Write_ = 0;
    int tank1AP1Write_ = 0;
    int tank1Delay2Write_ = 0;
    int tank1AP2Write_ = 0;

    int tank2Delay1Write_ = 0;
    int tank2AP1Write_ = 0;
    int tank2Delay2Write_ = 0;
    int tank2AP2Write_ = 0;

    // One-pole lowpass filters for damping
    float tank1Damp_ = 0.0f;
    float tank2Damp_ = 0.0f;

    // Helper: allpass filter
    float ProcessAllpass(float input, float* buffer, int size, int& writePos, float gain);
};
