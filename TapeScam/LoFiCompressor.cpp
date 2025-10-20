#include "LoFiCompressor.h"
#include <cmath>

void LoFiCompressor::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    mode_ = 0;
    envelopeL_ = 0.0f;
    envelopeR_ = 0.0f;
    UpdateParameters();
}

void LoFiCompressor::SetMode(int mode)
{
    mode_ = mode < 0 ? 0 : (mode > 2 ? 2 : mode);
    UpdateParameters();
}

void LoFiCompressor::UpdateParameters()
{
    // Calculate envelope follower coefficients
    const float attackMs = mode_ == 2 ? 2.0f : 5.0f;  // Very fast attack
    const float releaseMs = mode_ == 2 ? 800.0f : 400.0f;  // VERY slow release = extreme pumping

    attackCoeff_ = expf(-1.0f / (sampleRate_ * attackMs * 0.001f));
    releaseCoeff_ = expf(-1.0f / (sampleRate_ * releaseMs * 0.001f));

    // Upward compression parameters based on mode
    switch(mode_)
    {
        case 0:  // Off
            threshold_ = 1.0f;  // Above everything, no effect
            ratio_ = 1.0f;
            makeupGain_ = 1.0f;
            break;

        case 1:  // Light - musical AGC pumping
            threshold_ = 0.45f;  // Below 45%, start boosting
            ratio_ = 8.0f;       // 8:1 upward (nice musical boost)
            makeupGain_ = 0.7f;  // Compensate for upward gain
            break;

        case 2:  // Heavy - extreme cassette deck pumping
            threshold_ = 0.65f;  // Below 65%, start boosting
            ratio_ = 18.0f;      // 18:1 upward (strong pumping)
            makeupGain_ = 0.5f;  // Compensate for heavy upward gain
            break;
    }
}

float LoFiCompressor::ProcessGain(float envelope, float inputLevel)
{
    if(mode_ == 0) return 1.0f;  // Bypass

    // Upward compression: boost signals below threshold
    if(envelope < threshold_)
    {
        // Calculate how much boost to apply
        float reduction = threshold_ - envelope;  // How far below threshold
        float boost = reduction * (ratio_ - 1.0f) / ratio_;  // Upward gain

        // Convert to linear gain (add boost in dB-ish space)
        float gainDB = boost * 30.0f;  // Scale to dB-like range (middle ground)
        float gain = powf(10.0f, gainDB / 20.0f);

        // Reasonable max boost for musical pumping
        float maxBoost = mode_ == 2 ? 20.0f : 12.0f;
        gain = gain > maxBoost ? maxBoost : gain;

        return gain * makeupGain_;
    }

    return makeupGain_;  // Above threshold, just makeup gain
}

void LoFiCompressor::Process(float* left, float* right, size_t size)
{
    if(mode_ == 0)
    {
        // Bypass - no processing
        return;
    }

    for(size_t i = 0; i < size; ++i)
    {
        float inL = left[i];
        float inR = right[i];

        // Envelope follower (RMS-ish)
        float absL = fabsf(inL);
        float absR = fabsf(inR);

        // Attack/release envelope
        float envCoeffL = (absL > envelopeL_) ? attackCoeff_ : releaseCoeff_;
        float envCoeffR = (absR > envelopeR_) ? attackCoeff_ : releaseCoeff_;

        envelopeL_ = envCoeffL * envelopeL_ + (1.0f - envCoeffL) * absL;
        envelopeR_ = envCoeffR * envelopeR_ + (1.0f - envCoeffR) * absR;

        // Calculate gain for each channel
        float gainL = ProcessGain(envelopeL_, absL);
        float gainR = ProcessGain(envelopeR_, absR);

        // Apply gain (this will boost quiet parts, making hiss louder)
        left[i] = inL * gainL;
        right[i] = inR * gainR;

        // Soft clip to prevent overs
        left[i] = left[i] > 1.0f ? 1.0f : (left[i] < -1.0f ? -1.0f : left[i]);
        right[i] = right[i] > 1.0f ? 1.0f : (right[i] < -1.0f ? -1.0f : right[i]);
    }
}
