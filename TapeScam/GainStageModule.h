#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include "daisysp.h"

class GainStageModule
{
  public:
    struct Params
    {
        float trimGainDb;
        float channelGainDb;
        float masterVolDb;
        float bassGainDb;
        float trebleGainDb;
        float character;
        int   inputType;
        int   clippingType;
        int   toneMode;
        int   debugMode;
        bool  bypass;
        bool  boostEngage;
    };

    void Init(float sampleRate);
    void SetPendingParams(const Params& params);
    void UpdateControls();
    void Process(const float* const* in, float** out, size_t size);

  private:
    struct BiquadFilter
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void Reset()
        {
            z1 = 0.0f;
            z2 = 0.0f;
        }

        float Process(float in)
        {
            float out = b0 * in + z1;
            z1 = b1 * in - a1 * out + z2;
            z2 = b2 * in - a2 * out;
            return out;
        }

        void SetLowShelf(float sampleRate, float freq, float gainDb, float shelfSlope = 1.0f);
        void SetHighShelf(float sampleRate, float freq, float gainDb, float shelfSlope = 1.0f);
    };

    struct OnePoleLowpass
    {
        float a = 1.0f;
        float b = 0.0f;
        float z = 0.0f;

        void SetCutoff(float sampleRate, float cutoffHz)
        {
            const float x = expf(-2.0f * kPi * cutoffHz / sampleRate);
            a = 1.0f - x;
            b = x;
        }

        float Process(float in)
        {
            z = a * in + b * z;
            return z;
        }
    };

    float dBToLin(float dB) const { return powf(10.0f, dB / 20.0f); }
    float ApplyClipping(float inSample) const;
    float SafeLimit(float v) const;
    void  ComputeFilterCoeffs();
    float InputCompensationGain(int inputType) const;
    float ToneModeFactor(int toneMode) const;
    static inline float Clamp(float v, float lo, float hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    float sampleRate_ = 48000.0f;

    Params params_{};
    Params pendingParams_{};
    bool   paramsDirty_ = false;

    float trimGainLin_ = 1.0f;
    float channelGainLin_ = 1.0f;
    float masterVolLin_ = 1.0f;
    float inputCompGain_ = 1.0f;
    float characterDriveScale_ = 1.0f;
    float boostFactor_ = 1.8f;
    float softClipBase_ = 1.2f;
    float hardClipBaseThreshold_ = 0.9f;

    static constexpr float kPi = 3.14159265358979323846f;

    float bassModeOffsetDb_ = 0.0f;
    float trebleModeOffsetDb_ = 0.0f;

    bool  loFiEnabled_ = false;
    OnePoleLowpass loFiLowpass_[2];

    float lastBassSetting_ = std::numeric_limits<float>::quiet_NaN();
    float lastTrebleSetting_ = std::numeric_limits<float>::quiet_NaN();
    int   lastToneMode_ = -1;
    bool  lastLoFiState_ = false;

    BiquadFilter bassShelf_[2];
    BiquadFilter trebleShelf_[2];
};
