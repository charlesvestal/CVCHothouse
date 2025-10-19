#include "ReverbModule.h"
#include <cmath>

// Convert decay time (seconds) to feedback coefficient (0-1)
static float DecayTimeToFeedback(float decaySeconds)
{
    if(decaySeconds < 0.1f)
        return 0.0f;
    if(decaySeconds > 10.0f)
        decaySeconds = 10.0f;

    // Map decay time to feedback
    // 1.0s -> 0.7, 3.0s -> 0.85, 5.0s -> 0.9
    float feedback = 0.6f + (decaySeconds / 10.0f) * 0.3f;
    return feedback < 0.92f ? feedback : 0.92f;
}

int ReverbModule::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    mix_ = 0.0f;
    targetMix_ = 0.0f;

    reverb_.Init(sampleRate_);
    reverb_.SetFeedback(0.7f);
    reverb_.SetDamping(0.4f);  // Some HF damping

    return 0;  // Simple reverb always succeeds
}

void ReverbModule::SetMix(float mix)
{
    targetMix_ = mix < 0.0f ? 0.0f : (mix > 1.0f ? 1.0f : mix);
}

void ReverbModule::SetDecayTime(float seconds)
{
    float feedback = DecayTimeToFeedback(seconds);
    reverb_.SetFeedback(feedback);

    // Adjust damping based on decay time (longer = less damping)
    float damping = 0.6f - (seconds / 10.0f) * 0.3f;
    reverb_.SetDamping(damping);
}

void ReverbModule::UpdateControls()
{
    // Smooth mix parameter
    mix_ += (targetMix_ - mix_) * kMixSmooth;
    if(fabsf(targetMix_ - mix_) < 0.001f)
    {
        mix_ = targetMix_;
    }
}

void ReverbModule::Process(float** in, float** out, size_t size)
{
    const float currentMix = mix_;

    // Bypass if mix is essentially zero
    if(currentMix < 0.001f)
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

    const float dryMix = 1.0f - currentMix;
    const float wetMix = currentMix;

    for(size_t i = 0; i < size; ++i)
    {
        float inL = in[0][i];
        float inR = in[1][i];

        // Process through simple reverb
        float reverbL, reverbR;
        reverb_.Process(inL, inR, &reverbL, &reverbR);

        // Mix dry and wet signals with gain compensation
        const float wetGain = 0.25f;  // SimpleReverb outputs hot signal
        out[0][i] = inL * dryMix + reverbL * wetMix * wetGain;
        out[1][i] = inR * dryMix + reverbR * wetMix * wetGain;
    }
}
