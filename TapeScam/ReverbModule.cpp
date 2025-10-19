#include "ReverbModule.h"
#include <algorithm>

namespace
{
// Convert decay time (seconds) to feedback coefficient (0-1)
// Using exponential decay formula: feedback = exp(-6.91 / (decayTime * sampleRate))
// Simplified approximation for musical decay times
float DecayTimeToFeedback(float decaySeconds)
{
    if(decaySeconds < 0.1f)
        return 0.0f;
    if(decaySeconds > 10.0f)
        decaySeconds = 10.0f;

    // Map decay time to feedback: 0.5s -> 0.7, 1.0s -> 0.8, 3.0s -> 0.9, 5.0s -> 0.95
    float feedback = 0.5f + (decaySeconds / 10.0f) * 0.45f;
    return feedback < 0.95f ? feedback : 0.95f;  // Prevent infinite tail
}
}

void ReverbModule::Init(float sampleRate)
{
    sampleRate_    = sampleRate;
    targetMix_     = 0.0f;
    smoothedMix_   = 0.0f;
    targetDecay_   = 1.0f;
    smoothedDecay_ = 1.0f;
    targetPreDelayMs_ = 0.0f;
    preDelaySamples_ = 0;
    preDelayWriteIdx_ = 0;

    // Initialize DaisySP reverb
    reverb_.Init(sampleRate_);
    reverb_.SetFeedback(DecayTimeToFeedback(1.0f));
    reverb_.SetLpFreq(8000.0f);  // Damping for natural sound

    // Allocate pre-delay buffers
    preDelayBufL_.reset(new float[kMaxPreDelaySamples]);
    preDelayBufR_.reset(new float[kMaxPreDelaySamples]);
    std::fill(preDelayBufL_.get(), preDelayBufL_.get() + kMaxPreDelaySamples, 0.0f);
    std::fill(preDelayBufR_.get(), preDelayBufR_.get() + kMaxPreDelaySamples, 0.0f);

    paramsDirty_ = true;
}

void ReverbModule::SetMix(float mix)
{
    targetMix_ = Clamp(mix, 0.0f, 1.0f);
    paramsDirty_ = true;
}

void ReverbModule::SetDecayTime(float seconds)
{
    targetDecay_ = Clamp(seconds, 0.1f, 10.0f);
    paramsDirty_ = true;
}

void ReverbModule::SetPreDelay(float ms)
{
    targetPreDelayMs_ = Clamp(ms, 0.0f, 100.0f);
    // Convert to samples
    size_t newDelaySamples = static_cast<size_t>((ms / 1000.0f) * sampleRate_);
    if(newDelaySamples > kMaxPreDelaySamples)
        newDelaySamples = kMaxPreDelaySamples;
    preDelaySamples_ = newDelaySamples;
    paramsDirty_ = true;
}

void ReverbModule::UpdateControls()
{
    if(!paramsDirty_)
        return;

    // Smooth mix parameter
    smoothedMix_ += (targetMix_ - smoothedMix_) * kMixSmooth;
    if(fabsf(targetMix_ - smoothedMix_) < 0.001f)
    {
        smoothedMix_ = targetMix_;
    }

    // Smooth decay parameter
    smoothedDecay_ += (targetDecay_ - smoothedDecay_) * kDecaySmooth;
    if(fabsf(targetDecay_ - smoothedDecay_) < 0.01f)
    {
        smoothedDecay_ = targetDecay_;
        paramsDirty_ = false;
    }

    // Update reverb engine feedback based on decay time
    float feedback = DecayTimeToFeedback(smoothedDecay_);
    reverb_.SetFeedback(feedback);
}

void ReverbModule::Process(float** in, float** out, size_t size)
{
    const float mix = smoothedMix_;

    // Bypass if mix is essentially zero
    if(mix < 0.001f)
    {
        if(in != out)
        {
            for(size_t i = 0; i < size; ++i)
            {
                out[0][i] = in[0][i];
                out[1][i] = in[1][i];
            }
        }
        return;
    }

    const float dryMix = 1.0f - mix;
    const float wetMix = mix;

    for(size_t i = 0; i < size; ++i)
    {
        float inL = in[0][i];
        float inR = in[1][i];

        // Apply pre-delay if enabled
        float delayedL = inL;
        float delayedR = inR;

        if(preDelaySamples_ > 0)
        {
            // Read from delay buffer
            size_t readIdx = (preDelayWriteIdx_ + kMaxPreDelaySamples - preDelaySamples_) % kMaxPreDelaySamples;
            delayedL = preDelayBufL_[readIdx];
            delayedR = preDelayBufR_[readIdx];

            // Write current sample to delay buffer
            preDelayBufL_[preDelayWriteIdx_] = inL;
            preDelayBufR_[preDelayWriteIdx_] = inR;
            preDelayWriteIdx_ = (preDelayWriteIdx_ + 1) % kMaxPreDelaySamples;
        }

        // Process through reverb
        float reverbL, reverbR;
        reverb_.Process(delayedL, delayedR, &reverbL, &reverbR);

        // Mix dry and wet signals
        out[0][i] = inL * dryMix + reverbL * wetMix;
        out[1][i] = inR * dryMix + reverbR * wetMix;
    }
}
