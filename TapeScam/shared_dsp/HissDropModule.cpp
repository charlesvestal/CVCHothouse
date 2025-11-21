#include "HissDropModule.h"

#include <algorithm>
#include <cmath>

void HissDropModule::Init(float sampleRate, size_t numChannels)
{
    sampleRate_   = sampleRate;
    numChannels_  = numChannels > 0 ? numChannels : 2;
    targetAmount_ = 0.0f;
    smoothedAmount_ = 0.0f;

    hissLevelDb_  = kMinHissDb;
    hissLevelLin_ = DbToLin(hissLevelDb_);
    noiseColorFactor_ = 0.0f;

    randState_ ^= static_cast<uint32_t>(sampleRate_);
    pinkStates_.assign(numChannels_, {});
    noiseScratch_.assign(numChannels_, 0.0f);
}

void HissDropModule::SetAmount(float amount)
{
    targetAmount_ = std::clamp(amount, 0.0f, 1.0f);
}

void HissDropModule::SetStereoBlend(float blend)
{
    stereoBlend_ = std::clamp(blend, 0.0f, 1.0f);
}

void HissDropModule::UpdateControls()
{
    const float delta = targetAmount_ - smoothedAmount_;
    smoothedAmount_ += delta * kAmountSmooth;
    if(std::fabs(delta) < 1e-5f)
    {
        smoothedAmount_ = targetAmount_;
    }

    const float amt = smoothedAmount_;
    if(amt <= kBypassThreshold)
    {
        hissLevelLin_ = 0.0f;
        noiseColorFactor_ = 0.0f;
        return;
    }

    hissLevelDb_  = kMinHissDb + amt * (kMaxHissDb - kMinHissDb);
    hissLevelLin_ = DbToLin(hissLevelDb_);
    noiseColorFactor_ = amt;
}

void HissDropModule::Process(float** in, float** out, size_t size)
{
    if(numChannels_ == 0 || size == 0)
        return;

    if(hissLevelLin_ <= 0.0f)
        return;

    float** src = in ? in : out;
    if(noiseScratch_.size() < numChannels_)
        noiseScratch_.resize(numChannels_);

    for(size_t i = 0; i < size; ++i)
    {
        float monoNoise = 0.0f;

        for(size_t ch = 0; ch < numChannels_; ++ch)
        {
            float* inBuf  = src ? src[ch] : nullptr;
            float* outBuf = out ? out[ch] : nullptr;
            const float x = inBuf ? inBuf[i] : (outBuf ? outBuf[i] : 0.0f);

            const float white = NextRandCentered();
            float noiseSample = white;
            if(noiseColorFactor_ >= 0.001f)
            {
                noiseSample = (1.0f - noiseColorFactor_) * white + noiseColorFactor_ * ProcessPink(ch % pinkStates_.size(), white);
            }

            noiseScratch_[ch] = noiseSample;
            monoNoise += noiseSample;
        }

        monoNoise /= static_cast<float>(numChannels_);

        for(size_t ch = 0; ch < numChannels_; ++ch)
        {
            float* inBuf  = src ? src[ch] : nullptr;
            float* outBuf = out ? out[ch] : nullptr;
            const float x = inBuf ? inBuf[i] : (outBuf ? outBuf[i] : 0.0f);

            const float noiseSample = monoNoise + stereoBlend_ * (noiseScratch_[ch] - monoNoise);
            float y = x + noiseSample * hissLevelLin_;
            if(outBuf)
                outBuf[i] = y;
        }
    }
}

float HissDropModule::NextRand()
{
    randState_ = randState_ * 1664525u + 1013904223u;
    return static_cast<float>(randState_) * 2.3283064365386963e-10f;
}

float HissDropModule::NextRandCentered()
{
    return NextRand() * 2.0f - 1.0f;
}

float HissDropModule::ProcessPink(size_t channel, float white)
{
    if(channel >= pinkStates_.size())
        pinkStates_.resize(channel + 1);
    PinkState& s = pinkStates_[channel];
    s.b0 = 0.99886f * s.b0 + white * 0.0555179f;
    s.b1 = 0.99332f * s.b1 + white * 0.0750759f;
    s.b2 = 0.96900f * s.b2 + white * 0.1538520f;
    s.b3 = 0.86650f * s.b3 + white * 0.3104856f;
    s.b4 = 0.55000f * s.b4 + white * 0.5329522f;
    s.b5 = -0.7616f * s.b5 - white * 0.0168980f;
    float pink = s.b0 + s.b1 + s.b2 + s.b3 + s.b4 + s.b5 + s.b6 + white * 0.5362f;
    s.b6 = white * 0.115926f;
    return pink * 0.11f;
}
