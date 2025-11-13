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

    hissLevelOffsetDb_ = 0.0f;
    noiseColorBias_ = 0.0f;
    dropoutDepthScale_ = 1.0f;
    dropoutDurationMinSec_ = 0.01f;
    dropoutDurationMaxSec_ = 0.15f;
}

void HissDropModule::SetAmount(float amount)
{
    targetAmount_ = Clamp(amount, 0.0f, 1.0f);
}

void HissDropModule::SetHissMultiplier(float multiplier)
{
    // Ensure multiplier is positive and within reasonable range
    hissMultiplier_ = Clamp(multiplier, 0.1f, 3.0f);
}

void HissDropModule::SetDropoutRateMultiplier(float multiplier)
{
    // Ensure multiplier is positive and within reasonable range
    dropoutRateMultiplier_ = Clamp(multiplier, 0.1f, 3.0f);
}

void HissDropModule::SetHissLevelOffsetDb(float offsetDb)
{
    hissLevelOffsetDb_ = offsetDb;
}

void HissDropModule::SetNoiseColorBias(float bias)
{
    noiseColorBias_ = bias;
}

void HissDropModule::SetDropoutDepthScale(float scale)
{
    dropoutDepthScale_ = Clamp(scale, 0.0f, 2.0f);
}

void HissDropModule::SetDropoutDurationRange(float minSeconds, float maxSeconds)
{
    dropoutDurationMinSec_ = std::max(0.001f, minSeconds);
    dropoutDurationMaxSec_ = std::max(dropoutDurationMinSec_, maxSeconds);
}

void HissDropModule::SetDropoutBias(float bias)
{
    dropoutBias_ = Clamp(bias, 0.0f, 0.95f);
}

void HissDropModule::SetMinimumDropoutAmount(float minAmount)
{
    minDropoutAmount_ = Clamp(minAmount, 0.0f, 1.0f);
}

void HissDropModule::UpdateControls()
{
    const float noiseAmt = Clamp(targetAmount_, 0.0f, 1.0f);
    const float biasAdjusted = std::max(std::max(targetAmount_, minDropoutAmount_), dropoutBias_);
    const float target = Clamp(biasAdjusted, 0.0f, 1.0f);
    const float delta = target - smoothedAmount_;
    smoothedAmount_ += delta * kAmountSmooth;
    if(std::fabs(delta) < 1e-5f)
    {
        smoothedAmount_ = target;
    }

    if(noiseAmt <= kBypassThreshold)
    {
        hissLevelDb_ = kMinHissDb;
        hissLevelLin_ = 0.0f;
        noiseColorFactor_ = 0.0f;
    }
    else
    {
        const float baseHissDb = kMinHissDb + noiseAmt * ((kMaxHissDb - kMinHissDb) + hissLevelOffsetDb_);
        hissLevelDb_  = baseHissDb + 20.0f * log10f(hissMultiplier_);
        hissLevelLin_ = DbToLin(hissLevelDb_);
        noiseColorFactor_ = Clamp(noiseAmt + noiseColorBias_, 0.0f, 1.0f);
    }

    const float amt = smoothedAmount_;
    dropoutRate_  = amt * dropoutRateMultiplier_ * 6.0f;
    dropoutDepth_ = Clamp(amt * dropoutDepthScale_, 0.0f, 0.98f);
    const float durationSec = dropoutDurationMinSec_ + amt * (dropoutDurationMaxSec_ - dropoutDurationMinSec_);
    dropoutDurationSamples_ = durationSec * sampleRate_;
    if(dropoutDurationSamples_ < 1.0f)
        dropoutDurationSamples_ = 1.0f;

    if(dropoutRate_ > 0.0001f)
    {
        avgDropSamples_ = sampleRate_ / dropoutRate_;
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
                    const float limitedDepth = Clamp(dropoutDepth_, 0.0f, 0.98f);
                    desiredGain = std::max(1.0f - limitedDepth, kMinDropoutGain);
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

        dropGain += (desiredGain - dropGain) * (kDropoutSlew * 0.5f);

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
    float jitter = 0.5f + 1.5f * NextRand();
    float interval = avgDropSamples_ * jitter;
    const float minInterval = dropoutDurationSamples_ + sampleRate_ * 0.05f;
    const float maxInterval = avgDropSamples_ * 4.0f;
    if(interval < minInterval)
        interval = minInterval;
    if(interval > maxInterval)
        interval = maxInterval;
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
