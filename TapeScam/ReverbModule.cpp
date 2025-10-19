#include "ReverbModule.h"
#include <cmath>

// Convert decay time (seconds) to feedback coefficient (0-1)
static float DecayTimeToFeedback(float decaySeconds)
{
    if(decaySeconds < 0.1f)
        return 0.0f;
    if(decaySeconds > 10.0f)
        decaySeconds = 10.0f;

    // Map decay time to feedback - conservative to prevent instability
    // 1.0s -> 0.7, 3.0s -> 0.85
    float feedback = 0.6f + (decaySeconds / 10.0f) * 0.25f;
    return feedback < 0.85f ? feedback : 0.85f;
}

int ReverbModule::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    mix_ = 0.0f;
    targetMix_ = 0.0f;

    preFilterL1_ = 0.0f;
    preFilterL2_ = 0.0f;
    preFilterR1_ = 0.0f;
    preFilterR2_ = 0.0f;

    // Initialize DaisySP reverb - returns 0 on success, 1 on failure
    int result = reverb_.Init(sampleRate_);
    if(result == 0)
    {
        reverb_.SetFeedback(0.7f);   // Conservative default
        reverb_.SetLpFreq(10000.0f);  // Standard damping
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

    // Two-pole lowpass pre-filter: ~8kHz cutoff to remove artifacts
    const float lpCoeff1 = 0.45f;
    const float lpCoeff2 = 0.35f;

    for(size_t i = 0; i < size; ++i)
    {
        float inL = in[0][i];
        float inR = in[1][i];

        // Soft-clip input
        inL = fmaxf(-0.95f, fminf(0.95f, inL));
        inR = fmaxf(-0.95f, fminf(0.95f, inR));

        // Two-stage lowpass to aggressively remove high-frequency artifacts
        preFilterL1_ += (inL - preFilterL1_) * lpCoeff1;
        preFilterL2_ += (preFilterL1_ - preFilterL2_) * lpCoeff2;
        preFilterR1_ += (inR - preFilterR1_) * lpCoeff1;
        preFilterR2_ += (preFilterR1_ - preFilterR2_) * lpCoeff2;

        // Process through reverb with filtered input
        float reverbL = 0.0f, reverbR = 0.0f;
        reverb_.Process(preFilterL2_, preFilterR2_, &reverbL, &reverbR);

        // Denormal protection and hard limit reverb output
        if(fabsf(reverbL) < 1e-10f) reverbL = 0.0f;
        if(fabsf(reverbR) < 1e-10f) reverbR = 0.0f;
        reverbL = fmaxf(-1.0f, fminf(1.0f, reverbL));
        reverbR = fmaxf(-1.0f, fminf(1.0f, reverbR));

        // Mix dry (unfiltered) and wet signals
        out[0][i] = inL * dryMix + reverbL * wetMix;
        out[1][i] = inR * dryMix + reverbR * wetMix;
    }
}
