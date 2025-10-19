#include "HissDropModule.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kMinHissDb = -60.0f;
constexpr float kMaxHissDb = -6.0f;
}

void HissDropModule::Init(float sampleRate, size_t numChannels)
{
    sampleRate_   = sampleRate;
    numChannels_  = numChannels > 0 ? numChannels : 2;
    targetAmount_ = 0.0f;
    smoothedAmount_ = 0.0f;

    hissLevelDb_  = kMinHissDb;
    hissLevelLin_ = DbToLin(hissLevelDb_);
    noiseColorFactor_ = 0.0f;

    dropoutRate_            = 0.0f;
    dropoutDepth_           = 0.0f;
    dropoutDurationSamples_ = 1.0f;
    avgDropSamples_         = std::numeric_limits<float>::infinity();
    samplesUntilNextDrop_   = std::numeric_limits<float>::infinity();
    inDropout_              = false;
    dropoutSamplesRemaining_ = 0.0f;
    dropoutGain_            = 1.0f;
    dropoutTargetGain_      = 1.0f;

    randState_ ^= static_cast<uint32_t>(sampleRate_);

    pinkStates_.assign(numChannels_, {});
}

void HissDropModule::SetAmount(float amount)
{
    targetAmount_ = Clamp(amount, 0.0f, 1.0f);
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
    hissLevelDb_  = kMinHissDb + amt * (kMaxHissDb - kMinHissDb);
    hissLevelLin_ = DbToLin(hissLevelDb_);
    noiseColorFactor_ = amt;

    dropoutRate_  = amt * 5.0f;
    dropoutDepth_ = Clamp(amt, 0.0f, 1.0f);
    dropoutDurationSamples_ = (0.01f + amt * 0.14f) * sampleRate_;
    if(dropoutDurationSamples_ < 1.0f)
        dropoutDurationSamples_ = 1.0f;

    if(dropoutRate_ > 0.0001f)
    {
        avgDropSamples_ = (sampleRate_ * 60.0f) / dropoutRate_;
        if(!inDropout_ && !std::isfinite(samplesUntilNextDrop_))
        {
            samplesUntilNextDrop_ = ComputeNextDropInterval();
        }
    }
    else
    {
        avgDropSamples_ = std::numeric_limits<float>::infinity();
        samplesUntilNextDrop_ = std::numeric_limits<float>::infinity();
        inDropout_ = false;
        dropoutSamplesRemaining_ = 0.0f;
    }

    if(!inDropout_)
    {
        dropoutTargetGain_ = 1.0f;
    }
}

void HissDropModule::Process(float** in, float** out, size_t size)
{
    if(numChannels_ == 0 || size == 0)
        return;

    float** src = in ? in : out;
    const float amt = smoothedAmount_;
    const bool enableNoise = amt > kBypassThreshold && hissLevelLin_ > 0.0f;
    const bool enableDrop  = dropoutDepth_ > 0.0001f && std::isfinite(avgDropSamples_);

    if(!enableNoise && !enableDrop)
    {
        if(out && in && out != in)
        {
            for(size_t ch = 0; ch < numChannels_; ++ch)
            {
                std::copy(src[ch], src[ch] + size, out[ch]);
            }
        }
        return;
    }

    float samplesUntil = samplesUntilNextDrop_;
    bool  dropoutActive = inDropout_;
    float dropRemain    = dropoutSamplesRemaining_;
    float dropGain      = dropoutGain_;
    float desiredGain   = dropoutTargetGain_;

    for(size_t i = 0; i < size; ++i)
    {
        if(enableDrop)
        {
            if(!dropoutActive)
            {
                if(std::isfinite(samplesUntil))
                    samplesUntil -= 1.0f;
                if(samplesUntil <= 0.0f)
                {
                    dropoutActive = true;
                    dropRemain = dropoutDurationSamples_;
                    desiredGain = std::max(1.0f - dropoutDepth_, kMinDropoutGain);
                }
            }
            else
            {
                dropRemain -= 1.0f;
                if(dropRemain <= 0.0f)
                {
                    dropoutActive = false;
                    desiredGain = 1.0f;
                    samplesUntil = ComputeNextDropInterval();
                    dropRemain = 0.0f;
                }
            }
        }
        else
        {
            dropoutActive = false;
            dropRemain = 0.0f;
            desiredGain = 1.0f;
            samplesUntil = std::numeric_limits<float>::infinity();
        }

        dropGain += (desiredGain - dropGain) * kDropoutSlew;

        for(size_t ch = 0; ch < numChannels_; ++ch)
        {
            float* inBuf  = src ? src[ch] : nullptr;
            float* outBuf = out ? out[ch] : nullptr;
            float x = inBuf ? inBuf[i] : outBuf[i];
            float noise = 0.0f;
            if(enableNoise)
            {
                noise = GenerateNoise(ch) * hissLevelLin_;
            }
            float y = x * dropGain + noise;
            if(outBuf)
                outBuf[i] = y;
        }
    }

    inDropout_ = dropoutActive;
    dropoutSamplesRemaining_ = dropRemain;
    dropoutGain_ = dropGain;
    dropoutTargetGain_ = desiredGain;
    samplesUntilNextDrop_ = samplesUntil;
}

float HissDropModule::GenerateNoise(size_t channel)
{
    const float white = NextRandCentered();
    if(noiseColorFactor_ <= 0.0f)
        return white;
    if(noiseColorFactor_ >= 0.999f)
        return ProcessPink(channel % pinkStates_.size(), white);

    float pink = ProcessPink(channel % pinkStates_.size(), white);
    return (1.0f - noiseColorFactor_) * white + noiseColorFactor_ * pink;
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

void HissDropModule::ResetPink()
{
    for(auto& state : pinkStates_)
    {
        state = {};
    }
}

float HissDropModule::ComputeNextDropInterval()
{
    if(dropoutRate_ <= 0.0001f || !std::isfinite(avgDropSamples_))
        return std::numeric_limits<float>::infinity();
    float jitter = 0.8f + 0.4f * NextRand();
    float interval = avgDropSamples_ * jitter;
    float minInterval = dropoutDurationSamples_ + sampleRate_ * 0.01f;
    if(interval < minInterval)
        interval = minInterval;
    return interval;
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

