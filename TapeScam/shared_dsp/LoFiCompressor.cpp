#include "LoFiCompressor.h"
#include <cmath>
#include <algorithm>

void LoFiCompressor::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    mode_ = 0;
    envelopeL_ = 0.0f;
    envelopeR_ = 0.0f;
    smoothedGainL_ = 1.0f;
    smoothedGainR_ = 1.0f;
    useContinuous_ = false;
    UpdateParameters();
}

void LoFiCompressor::SetMode(int mode)
{
    mode_ = mode < 0 ? 0 : (mode > 2 ? 2 : mode);
    useContinuous_ = false;
    UpdateParameters();
}

void LoFiCompressor::SetContinuousAmount(float amount, float makeupSpeed)
{
    useContinuous_ = true;

    const float amt = std::clamp(amount, 0.0f, 1.0f);
    const float speed = std::clamp(makeupSpeed, 0.0f, 1.0f);

    const float attackMs = 6.0f - amt * 4.0f;          // 6→2 ms
    const float releaseMs = 250.0f + amt * 700.0f;     // 0.25→0.95 s

    attackCoeff_ = expf(-1.0f / (sampleRate_ * attackMs * 0.001f));
    releaseCoeff_ = expf(-1.0f / (sampleRate_ * releaseMs * 0.001f));

    threshold_ = 0.35f + amt * 0.4f;   // 0.35→0.75
    ratio_ = 5.0f + amt * 15.0f;       // 5→20 upward
    makeupGain_ = 0.85f - amt * 0.4f;  // 0.85→0.45

    maxBoost_ = 3.0f + amt * 9.0f;     // 3x→12x boost

    gainAttackCoeff_ = 0.15f + amt * 0.08f;  // faster duck when heavier

    const float swellSeconds = 0.4f + (1.0f - speed) * 2.6f; // 0.4s→3s
    gainReleaseCoeff_ = 1.0f - expf(-1.0f / (sampleRate_ * swellSeconds));
}

void LoFiCompressor::UpdateParameters()
{
    // Calculate envelope follower coefficients
    const float attackMs = mode_ == 2 ? 2.0f : 5.0f;  // Fast attack (ducking)
    const float releaseMs = mode_ == 2 ? 800.0f : 400.0f;  // Back to original timing - start swell sooner

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
            maxBoost_ = 6.0f;
            break;

        case 2:  // Heavy - extreme cassette deck pumping
            threshold_ = 0.65f;  // Below 65%, start boosting
            ratio_ = 18.0f;      // 18:1 upward (strong pumping)
            makeupGain_ = 0.5f;  // Compensate for heavy upward gain
            maxBoost_ = 12.0f;
            break;
    }

    gainAttackCoeff_ = 0.2f;
    gainReleaseCoeff_ = 0.000125f;
}

float LoFiCompressor::ProcessGain(float envelope, float inputLevel)
{
    if(mode_ == 0 && !useContinuous_) return 1.0f;  // Bypass

    // Upward compression: boost signals below threshold
    if(envelope < threshold_)
    {
        // Simple linear boost calculation - much smoother than exponential
        // This creates less intermodulation distortion
        float reduction = threshold_ - envelope;  // How far below threshold (0.0 to threshold_)

        // Linear mapping instead of exponential to reduce artifacts
        // Moderate boost for AGC pumping effect (reduced to prevent clipping)
        float maxBoost = useContinuous_ ? maxBoost_ : (mode_ == 2 ? 12.0f : 6.0f);
        float normalizedReduction = reduction / threshold_;  // 0.0 to 1.0
        float gain = 1.0f + (maxBoost - 1.0f) * normalizedReduction;  // Linear ramp

        return gain * makeupGain_;
    }

    return makeupGain_;  // Above threshold, just makeup gain
}

void LoFiCompressor::Process(float* left, float* right, size_t size)
{
    if(mode_ == 0 && !useContinuous_)
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

        // Calculate target gain for each channel
        float targetGainL = ProcessGain(envelopeL_, absL);
        float targetGainR = ProcessGain(envelopeR_, absR);

        // Smooth the gain changes to prevent rapid modulation artifacts
        const float smoothCoeffL = (targetGainL < smoothedGainL_) ? gainAttackCoeff_ : gainReleaseCoeff_;
        const float smoothCoeffR = (targetGainR < smoothedGainR_) ? gainAttackCoeff_ : gainReleaseCoeff_;

        smoothedGainL_ += (targetGainL - smoothedGainL_) * smoothCoeffL;
        smoothedGainR_ += (targetGainR - smoothedGainR_) * smoothCoeffR;

        // Limit gain to prevent any possibility of clipping
        // Cap maximum gain to reasonable values to prevent distortion
        const float maxAllowedGain = useContinuous_ ? maxBoost_ * 1.25f
                                                    : (mode_ == 2 ? 15.0f : 8.0f);  // Reduced from 20x/10x
        float cappedGainL = smoothedGainL_ > maxAllowedGain ? maxAllowedGain : smoothedGainL_;
        float cappedGainR = smoothedGainR_ > maxAllowedGain ? maxAllowedGain : smoothedGainR_;

        // Additional per-sample safety limiter
        float maxGainL = absL > 0.001f ? 0.9f / absL : cappedGainL;
        float maxGainR = absR > 0.001f ? 0.9f / absR : cappedGainR;

        float finalGainL = cappedGainL < maxGainL ? cappedGainL : maxGainL;
        float finalGainR = cappedGainR < maxGainR ? cappedGainR : maxGainR;

        // Apply gain (this will boost quiet parts, making hiss louder)
        left[i] = inL * finalGainL;
        right[i] = inR * finalGainR;
    }
}
