#include "SimpleReverb.h"
#include <algorithm>
#include <cstring>

// Define static arrays
constexpr int SimpleReverb::kCombDelays[4];
constexpr int SimpleReverb::kAllpassDelays[2];

void SimpleReverb::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    feedback_ = 0.5f;
    damping_ = 0.5f;

    // Initialize comb filters for left channel
    for(int i = 0; i < 4; i++) {
        combsL_[i].bufferSize = kCombDelays[i];
        combsL_[i].buffer.reset(new float[combsL_[i].bufferSize]);
        std::fill(combsL_[i].buffer.get(),
                  combsL_[i].buffer.get() + combsL_[i].bufferSize, 0.0f);
        combsL_[i].writePos = 0;
        combsL_[i].filterState = 0.0f;
    }

    // Initialize comb filters for right channel (slightly different sizes for stereo)
    for(int i = 0; i < 4; i++) {
        combsR_[i].bufferSize = kCombDelays[i] + 23;  // offset for stereo width
        combsR_[i].buffer.reset(new float[combsR_[i].bufferSize]);
        std::fill(combsR_[i].buffer.get(),
                  combsR_[i].buffer.get() + combsR_[i].bufferSize, 0.0f);
        combsR_[i].writePos = 0;
        combsR_[i].filterState = 0.0f;
    }

    // Initialize allpass filters for left channel
    for(int i = 0; i < 2; i++) {
        allpassL_[i].bufferSize = kAllpassDelays[i];
        allpassL_[i].buffer.reset(new float[allpassL_[i].bufferSize]);
        std::fill(allpassL_[i].buffer.get(),
                  allpassL_[i].buffer.get() + allpassL_[i].bufferSize, 0.0f);
        allpassL_[i].writePos = 0;
    }

    // Initialize allpass filters for right channel
    for(int i = 0; i < 2; i++) {
        allpassR_[i].bufferSize = kAllpassDelays[i] + 11;  // offset for stereo
        allpassR_[i].buffer.reset(new float[allpassR_[i].bufferSize]);
        std::fill(allpassR_[i].buffer.get(),
                  allpassR_[i].buffer.get() + allpassR_[i].bufferSize, 0.0f);
        allpassR_[i].writePos = 0;
    }
}

void SimpleReverb::SetFeedback(float fb)
{
    feedback_ = fb < 0.0f ? 0.0f : (fb > 0.95f ? 0.95f : fb);
}

void SimpleReverb::SetDamping(float damp)
{
    damping_ = damp < 0.0f ? 0.0f : (damp > 1.0f ? 1.0f : damp);
}

void SimpleReverb::Process(float inL, float inR, float* outL, float* outR)
{
    // Process through comb filters
    float combOutL = 0.0f;
    float combOutR = 0.0f;

    const float damp1 = damping_;
    const float damp2 = 1.0f - damping_;

    // Left channel combs
    for(int i = 0; i < 4; i++) {
        CombFilter& comb = combsL_[i];

        // Read delayed sample
        float delayed = comb.buffer[comb.writePos];

        // Damped feedback
        comb.filterState = delayed * damp2 + comb.filterState * damp1;

        // Write input + feedback
        comb.buffer[comb.writePos] = inL + comb.filterState * feedback_;

        // Advance write position
        comb.writePos++;
        if(comb.writePos >= comb.bufferSize) comb.writePos = 0;

        combOutL += delayed;
    }

    // Right channel combs
    for(int i = 0; i < 4; i++) {
        CombFilter& comb = combsR_[i];

        float delayed = comb.buffer[comb.writePos];
        comb.filterState = delayed * damp2 + comb.filterState * damp1;
        comb.buffer[comb.writePos] = inR + comb.filterState * feedback_;

        comb.writePos++;
        if(comb.writePos >= comb.bufferSize) comb.writePos = 0;

        combOutR += delayed;
    }

    // Process through allpass filters (diffusion)
    const float allpassGain = 0.5f;

    // Left channel allpass chain
    float apOutL = combOutL;
    for(int i = 0; i < 2; i++) {
        AllpassFilter& ap = allpassL_[i];

        float delayed = ap.buffer[ap.writePos];
        float input = apOutL;

        apOutL = -input + delayed;
        ap.buffer[ap.writePos] = input + delayed * allpassGain;

        ap.writePos++;
        if(ap.writePos >= ap.bufferSize) ap.writePos = 0;
    }

    // Right channel allpass chain
    float apOutR = combOutR;
    for(int i = 0; i < 2; i++) {
        AllpassFilter& ap = allpassR_[i];

        float delayed = ap.buffer[ap.writePos];
        float input = apOutR;

        apOutR = -input + delayed;
        ap.buffer[ap.writePos] = input + delayed * allpassGain;

        ap.writePos++;
        if(ap.writePos >= ap.bufferSize) ap.writePos = 0;
    }

    *outL = apOutL;
    *outR = apOutR;
}
