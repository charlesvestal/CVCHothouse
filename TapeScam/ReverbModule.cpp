#include "ReverbModule.h"
#include <cmath>

int ReverbModule::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    mix_ = 0.0f;
    targetMix_ = 0.0f;
    wetGainComp_ = 0.3f;

    reverb_.Init(sampleRate_);
    reverb_.SetDecayTime(0.0f);  // Start with no reverb
    reverb_.SetDamping(0.5f);
    reverb_.SetModulation(0.0f, 0.0f);

    return 0;
}

void ReverbModule::SetMix(float mix)
{
    targetMix_ = mix < 0.0f ? 0.0f : (mix > 1.0f ? 1.0f : mix);
}

void ReverbModule::SetDecayTime(float seconds)
{
    reverb_.SetDecayTime(seconds);

    // Adjust damping based on decay time (longer = less damping for brightness)
    float damping = 0.52f - (seconds / 10.0f) * 0.06f;  // 0.52 to 0.46
    reverb_.SetDamping(damping);
}

void ReverbModule::SetFeedbackLowpassCutoff(float cutoffHz)
{
    reverb_.SetFeedbackLowpassCutoff(cutoffHz);
}

void ReverbModule::SetPostFilterCutoff(float cutoffHz)
{
    reverb_.SetPostFilterCutoff(cutoffHz);
}

void ReverbModule::SetModulation(float depthSec, float rateHz)
{
    reverb_.SetModulation(depthSec, rateHz);
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

        // Process through FDN reverb
        float reverbL, reverbR;
        reverb_.Process(inL, inR, &reverbL, &reverbR);

        // Mix dry and wet signals with gain compensation
        out[0][i] = inL * dryMix + reverbL * wetMix * wetGainComp_;
        out[1][i] = inR * dryMix + reverbR * wetMix * wetGainComp_;
    }
}
