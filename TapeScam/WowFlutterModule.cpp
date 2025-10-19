#include "WowFlutterModule.h"

#include <algorithm>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void WowFlutterModule::Init(float sampleRate)
{
    sampleRate_    = sampleRate;
    invSampleRate_ = 1.0f / sampleRate_;
    targetAmount_  = 0.0f;
    smoothedAmount_= 0.0f;

    wowDepth_ = 0.0f;
    wowRate_  = 0.1f;
    flutterDepth_ = 0.0f;
    flutterRate_  = 4.0f;

    phaseWow_     = 0.0f;
    phaseFlutter_ = 0.0f;

    delayBufSize_ = kMaxDelaySamples;
    delayBufL_.reset(new float[delayBufSize_]);
    delayBufR_.reset(new float[delayBufSize_]);
    std::fill(delayBufL_.get(), delayBufL_.get() + delayBufSize_, 0.0f);
    std::fill(delayBufR_.get(), delayBufR_.get() + delayBufSize_, 0.0f);

    writeIndex_ = 0;
    baseDelaySamples_ = 1200.0f;
}

void WowFlutterModule::SetAmount(float amount)
{
    if(amount < 0.0f) amount = 0.0f;
    else if(amount > 1.0f) amount = 1.0f;
    targetAmount_ = amount;
}

void WowFlutterModule::UpdateControls()
{
    smoothedAmount_ += (targetAmount_ - smoothedAmount_) * kAmountSmooth;

    const float amt = smoothedAmount_;
    wowDepth_     = amt * 0.005f;
    wowRate_      = 0.1f + amt * 1.9f;
    flutterDepth_ = amt * 0.0025f;
    flutterRate_  = 4.0f + amt * 16.0f;
}

float WowFlutterModule::InterpolateLinear(const float* buf, size_t size, float index)
{
    int idx0 = static_cast<int>(floorf(index));
    int idx1 = idx0 + 1;
    float frac = index - static_cast<float>(idx0);
    while(idx0 < 0) idx0 += static_cast<int>(size);
    while(idx1 < 0) idx1 += static_cast<int>(size);
    idx0 %= static_cast<int>(size);
    idx1 %= static_cast<int>(size);
    return buf[idx0] + frac * (buf[idx1] - buf[idx0]);
}

void WowFlutterModule::Process(float** in, float** out, size_t size)
{
    const float amount = smoothedAmount_;
    if(amount < 0.0005f)
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
    const float wowDepth     = wowDepth_;
    const float wowRate      = wowRate_;
    const float flutterDepth = flutterDepth_;
    const float flutterRate  = flutterRate_;
    const float invSr        = invSampleRate_;

    for(size_t i = 0; i < size; ++i)
    {
        float xL = in[0][i];
        float xR = in[1][i];

        phaseWow_     += wowRate * invSr;
        phaseFlutter_ += flutterRate * invSr;
        if(phaseWow_ >= 1.0f)
            phaseWow_ -= 1.0f;
        if(phaseFlutter_ >= 1.0f)
            phaseFlutter_ -= 1.0f;

        float modWow  = sinf(2.0f * static_cast<float>(M_PI) * phaseWow_) * wowDepth;
        float modFlutter = sinf(2.0f * static_cast<float>(M_PI) * phaseFlutter_) * flutterDepth;
        float speedMod = 1.0f + modWow + modFlutter;
        speedMod = (speedMod < 0.8f ? 0.8f : (speedMod > 1.2f ? 1.2f : speedMod));

        float delaySamples = baseDelaySamples_ * speedMod;
        float readIndex    = static_cast<float>(writeIndex_) - delaySamples;
        if(readIndex < 0.0f)
            readIndex += static_cast<float>(delayBufSize_);

        float delayedL = InterpolateLinear(delayBufL_.get(), delayBufSize_, readIndex);
        float delayedR = InterpolateLinear(delayBufR_.get(), delayBufSize_, readIndex);

        delayBufL_[writeIndex_] = xL;
        delayBufR_[writeIndex_] = xR;

        writeIndex_++;
        if(writeIndex_ >= delayBufSize_)
            writeIndex_ = 0;

        out[0][i] = delayedL;
        out[1][i] = delayedR;
    }
}

