#include "ReverbModule.h"
#include <cmath>

// Convert decay time (seconds) to feedback coefficient (0-1)
static float DecayTimeToFeedback(float decaySeconds)
{
    if(decaySeconds < 0.1f)
        return 0.0f;
    if(decaySeconds > 10.0f)
        decaySeconds = 10.0f;

    // Map decay time to feedback - very conservative
    // 1.0s -> 0.65, 3.0s -> 0.75
    float feedback = 0.55f + (decaySeconds / 10.0f) * 0.2f;
    return feedback < 0.75f ? feedback : 0.75f;
}

int ReverbModule::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    mix_ = 0.0f;
    targetMix_ = 0.0f;

    // Initialize DaisySP reverb - returns 0 on success, 1 on failure
    int result = reverb_.Init(sampleRate_);
    if(result == 0)
    {
        reverb_.SetFeedback(0.6f);   // Lower default to reduce artifact amplification
        reverb_.SetLpFreq(8000.0f);  // More aggressive internal damping
    }
    return result;
}

void ReverbModule::SetMix(float mix)
{
    targetMix_ = mix < 0.0f ? 0.0f : (mix > 1.0f ? 1.0f : mix);
}

void ReverbModule::SetDecayTime(float seconds)
{
    float feedback = DecayTimeToFeedback(seconds);
    reverb_.SetFeedback(feedback);
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

        // Simple hard clip to ±0.9 to prevent extreme signals
        inL = fmaxf(-0.9f, fminf(0.9f, inL));
        inR = fmaxf(-0.9f, fminf(0.9f, inR));

        // Process through reverb
        float reverbL = 0.0f, reverbR = 0.0f;
        reverb_.Process(inL, inR, &reverbL, &reverbR);

        // Denormal protection
        if(fabsf(reverbL) < 1e-10f) reverbL = 0.0f;
        if(fabsf(reverbR) < 1e-10f) reverbR = 0.0f;

        // Mix dry and wet signals
        out[0][i] = inL * dryMix + reverbL * wetMix;
        out[1][i] = inR * dryMix + reverbR * wetMix;
    }
}
