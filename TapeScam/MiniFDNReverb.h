#pragma once

#include <memory>
#include <cstdint>

// Mini-FDN Reverb - Feedback Delay Network with 5 delay lines
// Designed for stability with modulated input signals
// Alias-mitigation edition with multi-stage filtering
class MiniFDNReverb
{
  public:
    void Init(float sampleRate);
    void SetDecayTime(float seconds);
    void SetDamping(float coeff);  // 0-1, higher = more damping
    void SetModulation(float depthSec, float rateHz);
    void SetFeedbackLowpassCutoff(float cutoffHz);  // Anti-alias filter on feedback path
    void SetPostFilterCutoff(float cutoffHz);       // Anti-alias filter on output
    void Process(float inL, float inR, float* outL, float* outR);

    // Diagnostics
    float GetMaxReadPointerDelta() const { return maxReadPointerDeltaSamples_; }
    uint32_t GetAliasingEventCount() const { return aliasingEventCount_; }

  private:
    static constexpr int kNumDelayLines = 5;
    static constexpr int kMaxDelaySamples = 3200;  // ~67ms at 48kHz
    static constexpr float kMaxDelayDeltaSamplesPerFrame = 2.0f;  // Reduced for reverb tail

    float sampleRate_ = 48000.0f;
    float invSampleRate_ = 1.0f / 48000.0f;

    // Delay lines
    std::unique_ptr<float[]> delayBuffers_[kNumDelayLines];
    int delayLengths_[kNumDelayLines];
    int writePos_[kNumDelayLines];
    float prevReadPos_[kNumDelayLines];  // Track for delta detection

    // Base delay times (prime-ish in samples at 48kHz)
    static constexpr float kBaseDelaySec[kNumDelayLines] = {
        0.0197f,  // ~945 samples
        0.0293f,  // ~1406 samples
        0.0373f,  // ~1790 samples
        0.0491f,  // ~2357 samples
        0.0631f   // ~3029 samples
    };

    // Feedback matrix (Householder, unitary, energy-preserving)
    float feedbackMatrix_[kNumDelayLines][kNumDelayLines];

    // Multi-stage damping filters (one-pole lowpass per line)
    float dampingState_[kNumDelayLines];
    float dampingCoeff_ = 0.5f;

    // Feedback path anti-alias filter (2-pole Butterworth per line)
    float feedbackFilter1State_[kNumDelayLines];  // First pole
    float feedbackFilter2State_[kNumDelayLines];  // Second pole
    float feedbackFilterCoeff_ = 0.5f;

    // Post-FDN anti-alias filter (stereo output)
    float postFilterStateL1_ = 0.0f;
    float postFilterStateL2_ = 0.0f;
    float postFilterStateR1_ = 0.0f;
    float postFilterStateR2_ = 0.0f;
    float postFilterCoeff_ = 0.5f;

    // Modulation bandwidth limiting filter (per line)
    float modLFOState_[kNumDelayLines];
    float modLFOCoeff_ = 0.9f;  // Heavy smoothing on modulation signal

    // Feedback gain
    float feedbackGain_ = 0.7f;

    // Modulation (with constraints)
    float modDepthSec_ = 0.0f;
    float modRateHz_ = 0.0f;
    float modPhase_[kNumDelayLines];

    // Diagnostics
    float maxReadPointerDeltaSamples_ = 0.0f;
    uint32_t aliasingEventCount_ = 0;
    uint32_t instrumentationCounter_ = 0;

    void InitFeedbackMatrix();
    float InterpolateLinear(const float* buffer, int bufferSize, float readPos);
    float OnePoleFilter(float input, float state, float coeff);
};
