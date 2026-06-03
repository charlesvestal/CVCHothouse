#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>   // std::size_t (not transitively guaranteed via <cmath>)
#include <limits>

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
        float driveNorm;
        int   inputType;
        int   clippingType;
        int   toneMode;
        int   debugMode;
        bool  bypass;
        float boostAmount;  // Changed from bool boostEngage to float for smooth ramping (0.0-1.0)
    };

    void Init(float sampleRate);
    void SetPendingParams(const Params& params);

    // Adjust headroom for tape age/condition (0dB = no change, negative = more headroom)
    void AdjustHeadroom(float headroomDb);

    // Full-range mode drops the tape pre-emphasis high-pass to a near-subsonic
    // cutoff so bass fundamentals pass through (e.g. for bass guitar / full mixes).
    // Default (false) keeps the original tape voicing.
    void SetFullRange(bool fullRange);

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
        void SetLowpass(float sampleRate, float freq, float q = 0.7071f);
        void SetHighpass(float sampleRate, float freq, float q = 0.7071f);
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
    float ApplyOpAmpStage1(float x) const;
    float ApplyOpAmpStage2(float x, int channel) const;
    float ApplyClippingCore(float x, int clippingType, float softness, float hardThreshold) const;
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
    float stage1Softness_ = 1.0f;
    float stage1HardThreshold_ = 1.2f;
    float stage2Softness_ = 1.2f;
    float stage2HardThreshold_ = 0.9f;
    float stage2Asymmetry_ = 0.05f;
    float boostFactor_ = 1.8f;
    float driveEnv_[2] = {0.0f, 0.0f};

    struct Oversampler4x
    {
        static constexpr int kFactor = 4;

        void Init(float sampleRate, float preCut, float postCut)
        {
            sampleRate4x_ = sampleRate * 4.0f;
            Configure(preCut, preB0_, preB1_, preB2_, preA1_, preA2_);
            Configure(postCut, postB0_, postB1_, postB2_, postA1_, postA2_);
            Reset();
        }

        void Configure(float cutoff, float& b0, float& b1, float& b2, float& a1, float& a2)
        {
            const float w0 = 2.0f * kPi * cutoff / sampleRate4x_;
            const float cosw0 = cosf(w0);
            const float sinw0 = sinf(w0);
            const float alpha = sinw0 / (2.0f * 0.7071f);
            const float B0 = (1.0f - cosw0) * 0.5f;
            const float B1 = 1.0f - cosw0;
            const float B2 = (1.0f - cosw0) * 0.5f;
            const float A0 = 1.0f + alpha;
            const float A1 = -2.0f * cosw0;
            const float A2 = 1.0f - alpha;
            b0 = B0 / A0;
            b1 = B1 / A0;
            b2 = B2 / A0;
            a1 = A1 / A0;
            a2 = A2 / A0;
        }

        void Reset()
        {
            prevInput_ = 0.0f;
            preZ1_ = preZ2_ = 0.0f;
            postZ1_ = postZ2_ = 0.0f;
        }

        std::array<float, kFactor> Upsample(float input)
        {
            std::array<float, kFactor> up{};
            const float step = (input - prevInput_) / static_cast<float>(kFactor);
            float value = prevInput_ + step;
            for(int i = 0; i < kFactor; ++i)
            {
                up[i] = value;
                value += step;
            }
            prevInput_ = input;
            return up;
        }

        float PreFilter(float x)
        {
            const float y = preB0_ * x + preZ1_;
            preZ1_ = preB1_ * x - preA1_ * y + preZ2_;
            preZ2_ = preB2_ * x - preA2_ * y;
            return y;
        }

        float PostFilter(float x)
        {
            const float y = postB0_ * x + postZ1_;
            postZ1_ = postB1_ * x - postA1_ * y + postZ2_;
            postZ2_ = postB2_ * x - postA2_ * y;
            return y;
        }

      private:
        float sampleRate4x_ = 192000.0f;
        float prevInput_ = 0.0f;
        float preB0_ = 1.0f, preB1_ = 0.0f, preB2_ = 0.0f;
        float preA1_ = 0.0f, preA2_ = 0.0f;
        float postB0_ = 1.0f, postB1_ = 0.0f, postB2_ = 0.0f;
        float postA1_ = 0.0f, postA2_ = 0.0f;
        float preZ1_ = 0.0f, preZ2_ = 0.0f;
        float postZ1_ = 0.0f, postZ2_ = 0.0f;
    };

    Oversampler4x oversamplerL_;
    Oversampler4x oversamplerR_;

    // Headroom adjustment for tape age/condition
    float headroomAdjustmentDb_ = 0.0f;
    float softClipBase_ = 1.2f;
    float hardClipBaseThreshold_ = 0.9f;

    static constexpr float kPi = 3.14159265358979323846f;

    bool  fullRange_ = false;   // true => bass-preserving high-pass (see SetFullRange)
    float bassModeOffsetDb_ = 0.0f;
    float trebleModeOffsetDb_ = 0.0f;
    float preHpCutHz_ = 0.0f;
    float preLpCutHz_ = 0.0f;
    float postLpCutHz_ = 0.0f;
    float loFiCutHz_ = 0.0f;

    bool  loFiEnabled_ = false;
    OnePoleLowpass loFiLowpass_[2];
    BiquadFilter mkPreHP_[2];
    BiquadFilter mkPreLP_[2];
    BiquadFilter mkPostLP_[2];

    float lastBassSetting_ = std::numeric_limits<float>::quiet_NaN();
    float lastTrebleSetting_ = std::numeric_limits<float>::quiet_NaN();
    int   lastToneMode_ = -1;
    bool  lastLoFiState_ = false;
    float lastPreHpCut_ = 0.0f;
    float lastPreLpCut_ = 0.0f;
    float lastPostLpCut_ = 0.0f;

    BiquadFilter bassShelf_[2];
    BiquadFilter trebleShelf_[2];
};
