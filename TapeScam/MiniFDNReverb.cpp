#include "MiniFDNReverb.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

constexpr float MiniFDNReverb::kBaseDelaySec[kNumDelayLines];

void MiniFDNReverb::InitFeedbackMatrix()
{
    // Householder feedback matrix for energy preservation
    // Based on unitary matrix design for FDN stability
    const float a = 2.0f / static_cast<float>(kNumDelayLines);

    for(int i = 0; i < kNumDelayLines; ++i) {
        for(int j = 0; j < kNumDelayLines; ++j) {
            if(i == j) {
                feedbackMatrix_[i][j] = a - 1.0f;
            } else {
                feedbackMatrix_[i][j] = a;
            }
        }
    }
}

void MiniFDNReverb::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    invSampleRate_ = 1.0f / sampleRate_;

    // Allocate and clear delay buffers
    for(int i = 0; i < kNumDelayLines; ++i) {
        delayLengths_[i] = static_cast<int>(kBaseDelaySec[i] * sampleRate_);
        if(delayLengths_[i] > kMaxDelaySamples) {
            delayLengths_[i] = kMaxDelaySamples;
        }

        delayBuffers_[i].reset(new float[delayLengths_[i]]);
        std::fill(delayBuffers_[i].get(), delayBuffers_[i].get() + delayLengths_[i], 0.0f);

        writePos_[i] = 0;
        prevReadPos_[i] = 0.0f;
        dampingState_[i] = 0.0f;
        feedbackFilter1State_[i] = 0.0f;
        feedbackFilter2State_[i] = 0.0f;
        modLFOState_[i] = 0.0f;
        modPhase_[i] = static_cast<float>(i) / static_cast<float>(kNumDelayLines);  // Phase offset per line
    }

    InitFeedbackMatrix();

    feedbackGain_ = 0.7f;
    dampingCoeff_ = 0.5f;
    modDepthSec_ = 0.0f;
    modRateHz_ = 0.0f;

    // Default anti-alias filter settings (10kHz cutoff at 48kHz)
    SetFeedbackLowpassCutoff(10000.0f);
    SetPostFilterCutoff(12000.0f);

    postFilterStateL1_ = 0.0f;
    postFilterStateL2_ = 0.0f;
    postFilterStateR1_ = 0.0f;
    postFilterStateR2_ = 0.0f;

    maxReadPointerDeltaSamples_ = 0.0f;
    aliasingEventCount_ = 0;
    instrumentationCounter_ = 0;
}

void MiniFDNReverb::SetDecayTime(float seconds)
{
    // Map decay time to feedback gain
    // Longer decay = higher feedback (but must stay stable)
    if(seconds < 0.1f) {
        feedbackGain_ = 0.0f;
    } else if(seconds < 1.0f) {
        feedbackGain_ = 0.5f;
    } else if(seconds < 2.0f) {
        feedbackGain_ = 0.7f;
    } else if(seconds < 3.5f) {
        feedbackGain_ = 0.75f;
    } else {
        feedbackGain_ = 0.78f;  // Max safe value for 5-line Householder
    }
}

void MiniFDNReverb::SetDamping(float coeff)
{
    // Higher damping = more HF roll-off
    dampingCoeff_ = coeff < 0.0f ? 0.0f : (coeff > 0.95f ? 0.95f : coeff);
}

void MiniFDNReverb::SetModulation(float depthSec, float rateHz)
{
    // Constrain modulation depth to prevent excessive aliasing
    // Max delta per sample at max rate should be < kMaxDelayDeltaSamplesPerFrame
    modDepthSec_ = depthSec;
    modRateHz_ = rateHz;

    // Calculate max modulation bandwidth limit
    // modLFOCoeff controls how fast the modulation LFO can change
    // Higher coeff = more smoothing = less aliasing but less modulation character
    float modBandwidthHz = rateHz * 2.0f;  // Nyquist-limited modulation bandwidth
    if(modBandwidthHz > 1.0f) {
        // One-pole filter coefficient from cutoff frequency
        float omega = 2.0f * M_PI * modBandwidthHz * invSampleRate_;
        modLFOCoeff_ = 1.0f - omega;
        if(modLFOCoeff_ < 0.5f) modLFOCoeff_ = 0.5f;  // Min smoothing
        if(modLFOCoeff_ > 0.98f) modLFOCoeff_ = 0.98f;  // Max smoothing
    } else {
        modLFOCoeff_ = 0.95f;  // Heavy smoothing for very slow modulation
    }
}

void MiniFDNReverb::SetFeedbackLowpassCutoff(float cutoffHz)
{
    // 2-pole Butterworth approximation using cascaded one-pole filters
    // Each pole has same coefficient for simplicity
    if(cutoffHz < 100.0f) cutoffHz = 100.0f;
    if(cutoffHz > sampleRate_ * 0.45f) cutoffHz = sampleRate_ * 0.45f;

    float omega = 2.0f * M_PI * cutoffHz * invSampleRate_;
    feedbackFilterCoeff_ = 1.0f - omega;
    if(feedbackFilterCoeff_ < 0.1f) feedbackFilterCoeff_ = 0.1f;
    if(feedbackFilterCoeff_ > 0.95f) feedbackFilterCoeff_ = 0.95f;
}

void MiniFDNReverb::SetPostFilterCutoff(float cutoffHz)
{
    // Post-processing anti-alias filter
    if(cutoffHz < 100.0f) cutoffHz = 100.0f;
    if(cutoffHz > sampleRate_ * 0.45f) cutoffHz = sampleRate_ * 0.45f;

    float omega = 2.0f * M_PI * cutoffHz * invSampleRate_;
    postFilterCoeff_ = 1.0f - omega;
    if(postFilterCoeff_ < 0.1f) postFilterCoeff_ = 0.1f;
    if(postFilterCoeff_ > 0.95f) postFilterCoeff_ = 0.95f;
}

float MiniFDNReverb::OnePoleFilter(float input, float state, float coeff)
{
    return coeff * state + (1.0f - coeff) * input;
}

float MiniFDNReverb::InterpolateLinear(const float* buffer, int bufferSize, float readPos)
{
    // Ensure readPos is within bounds
    while(readPos < 0.0f) readPos += static_cast<float>(bufferSize);
    while(readPos >= static_cast<float>(bufferSize)) readPos -= static_cast<float>(bufferSize);

    int idx0 = static_cast<int>(readPos);
    int idx1 = (idx0 + 1) % bufferSize;
    float frac = readPos - static_cast<float>(idx0);

    return buffer[idx0] + frac * (buffer[idx1] - buffer[idx0]);
}

void MiniFDNReverb::Process(float inL, float inR, float* outL, float* outR)
{
    // Mono input for reverb (sum L+R)
    float input = (inL + inR) * 0.5f;

    // Read from all delay lines
    float delayOut[kNumDelayLines];

    for(int i = 0; i < kNumDelayLines; ++i) {
        // Calculate read position with optional modulation
        float readDelay = static_cast<float>(delayLengths_[i]);

        if(modDepthSec_ > 0.0f && modRateHz_ > 0.0f) {
            // Update modulation phase
            modPhase_[i] += modRateHz_ * invSampleRate_;
            if(modPhase_[i] >= 1.0f) modPhase_[i] -= 1.0f;

            // Generate raw modulation LFO
            float rawModLFO = std::sin(2.0f * M_PI * modPhase_[i]);

            // Apply bandwidth-limiting filter to modulation signal
            modLFOState_[i] = OnePoleFilter(rawModLFO, modLFOState_[i], modLFOCoeff_);

            // Apply modulation to read position with depth constraint
            float modSamples = modDepthSec_ * sampleRate_ * modLFOState_[i];

            // Clamp modulation to prevent excessive jumps
            if(modSamples > kMaxDelayDeltaSamplesPerFrame) {
                modSamples = kMaxDelayDeltaSamplesPerFrame;
            }
            if(modSamples < -kMaxDelayDeltaSamplesPerFrame) {
                modSamples = -kMaxDelayDeltaSamplesPerFrame;
            }

            readDelay += modSamples;
        }

        // Read with wraparound
        float readPos = static_cast<float>(writePos_[i]) - readDelay;
        if(readPos < 0.0f) readPos += static_cast<float>(delayLengths_[i]);

        // Track read pointer delta for diagnostics
        float readDelta = fabsf(readPos - prevReadPos_[i]);
        if(readDelta > maxReadPointerDeltaSamples_) {
            maxReadPointerDeltaSamples_ = readDelta;
        }
        if(readDelta > kMaxDelayDeltaSamplesPerFrame) {
            aliasingEventCount_++;
        }
        prevReadPos_[i] = readPos;

        delayOut[i] = InterpolateLinear(delayBuffers_[i].get(), delayLengths_[i], readPos);
    }

    // Apply feedback matrix and write to delay lines
    for(int j = 0; j < kNumDelayLines; ++j) {
        // Mix outputs through feedback matrix
        float sum = 0.0f;
        for(int i = 0; i < kNumDelayLines; ++i) {
            sum += feedbackMatrix_[j][i] * delayOut[i];
        }

        // Apply damping filter (one-pole lowpass)
        dampingState_[j] = OnePoleFilter(sum, dampingState_[j], dampingCoeff_);

        // Apply 2-pole feedback anti-alias filter
        feedbackFilter1State_[j] = OnePoleFilter(dampingState_[j], feedbackFilter1State_[j], feedbackFilterCoeff_);
        feedbackFilter2State_[j] = OnePoleFilter(feedbackFilter1State_[j], feedbackFilter2State_[j], feedbackFilterCoeff_);

        // Combine input with feedback (use filtered signal)
        float delayIn = input * 0.2f + feedbackGain_ * feedbackFilter2State_[j];

        // Write to delay buffer
        delayBuffers_[j][writePos_[j]] = delayIn;

        // Advance write position
        writePos_[j]++;
        if(writePos_[j] >= delayLengths_[j]) {
            writePos_[j] = 0;
        }
    }

    // Sum delay outputs for reverb tail (stereo spread)
    float reverbL = 0.0f;
    float reverbR = 0.0f;

    // Alternate panning for stereo width
    for(int i = 0; i < kNumDelayLines; ++i) {
        if(i % 2 == 0) {
            reverbL += delayOut[i];
        } else {
            reverbR += delayOut[i];
        }
    }

    // Normalize
    reverbL *= 0.4f;
    reverbR *= 0.4f;

    // Apply post-FDN anti-alias filter (2-pole per channel)
    postFilterStateL1_ = OnePoleFilter(reverbL, postFilterStateL1_, postFilterCoeff_);
    postFilterStateL2_ = OnePoleFilter(postFilterStateL1_, postFilterStateL2_, postFilterCoeff_);
    postFilterStateR1_ = OnePoleFilter(reverbR, postFilterStateR1_, postFilterCoeff_);
    postFilterStateR2_ = OnePoleFilter(postFilterStateR1_, postFilterStateR2_, postFilterCoeff_);

    // Output filtered reverb tail
    *outL = postFilterStateL2_;
    *outR = postFilterStateR2_;

    instrumentationCounter_++;
}
