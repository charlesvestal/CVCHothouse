#include "ToneModule.h"

#include <cmath>

namespace
{
constexpr float kTwoPi = 6.283185307179586476925286766559f;
}

void ToneModule::Init(float sampleRate, size_t numChannels)
{
    sampleRate_   = sampleRate;
    numChannels_  = numChannels > 0 ? numChannels : 2;
    targetAmount_ = 0.5f;
    smoothedAmount_ = 0.5f;
    bassGainDb_   = 0.0f;
    trebleGainDb_ = 0.0f;

    lowShelf_.assign(numChannels_, {});
    highShelf_.assign(numChannels_, {});
    UpdateCoefficients();
}

void ToneModule::SetAmount(float amount)
{
    targetAmount_ = Clamp(amount, 0.0f, 1.0f);
}

void ToneModule::UpdateControls()
{
    float delta = targetAmount_ - smoothedAmount_;
    smoothedAmount_ += delta * kSmooth;
    if(std::fabs(targetAmount_ - smoothedAmount_) < 1e-5f)
    {
        smoothedAmount_ = targetAmount_;
    }

    float amt = smoothedAmount_;
    bassGainDb_   = amt * 12.0f - 6.0f;
    trebleGainDb_ = amt * 12.0f - 6.0f;
    UpdateCoefficients();
}

void ToneModule::UpdateCoefficients()
{
    for(size_t ch = 0; ch < numChannels_; ++ch)
    {
        ComputeShelf(lowShelf_[ch], bassGainDb_, bassCutoff_, false);
        ComputeShelf(highShelf_[ch], trebleGainDb_, trebleCutoff_, true);
    }
}

void ToneModule::ComputeShelf(BiquadState& state, float gainDb, float cutoffHz, bool highShelf)
{
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float w0 = kTwoPi * cutoffHz / sampleRate_;
    const float cosw0 = std::cos(w0);
    const float sinw0 = std::sin(w0);
    const float sqrtA = std::sqrt(A);
    const float alpha = sinw0 / 2.0f * std::sqrt(2.0f);

    float b0, b1, b2, a0, a1, a2;
    if(!highShelf)
    {
        b0 =    A * ((A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
        b1 =  2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0);
        b2 =    A * ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
        a0 =         (A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
        a1 =  -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0);
        a2 =         (A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;
    }
    else
    {
        b0 =    A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
        b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0);
        b2 =    A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
        a0 =         (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
        a1 =   2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0);
        a2 =         (A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;
    }

    const float invA0 = 1.0f / a0;
    state.b0 = b0 * invA0;
    state.b1 = b1 * invA0;
    state.b2 = b2 * invA0;
    state.a1 = a1 * invA0;
    state.a2 = a2 * invA0;

    if(std::fabs(gainDb) < 1e-4f)
    {
        state.z1 = 0.0f;
        state.z2 = 0.0f;
    }
}

void ToneModule::Process(float** in, float** out, size_t size)
{
    if(numChannels_ == 0 || size == 0)
        return;
    float** src = in ? in : out;
    for(size_t i = 0; i < size; ++i)
    {
        for(size_t ch = 0; ch < numChannels_; ++ch)
        {
            float* inBuf  = src ? src[ch] : nullptr;
            float* outBuf = out ? out[ch] : nullptr;
            float x = inBuf ? inBuf[i] : outBuf[i];

            BiquadState& low = lowShelf_[ch];
            float y = x * low.b0 + low.z1;
            low.z1 = x * low.b1 + low.z2 - low.a1 * y;
            low.z2 = x * low.b2 - low.a2 * y;

            BiquadState& high = highShelf_[ch];
            float y2 = y * high.b0 + high.z1;
            high.z1 = y * high.b1 + high.z2 - high.a1 * y2;
            high.z2 = y * high.b2 - high.a2 * y2;

            if(outBuf)
                outBuf[i] = y2;
        }
    }
}
