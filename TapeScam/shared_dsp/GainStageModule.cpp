#include "GainStageModule.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kBassFreqHz   = 120.0f;
constexpr float kTrebleFreqHz = 6000.0f;
constexpr float kLoFiCutHz    = 10000.0f;
constexpr float kMinThreshold = 0.2f;
constexpr float kMkPreHighpassHz = 160.0f;
constexpr float kMkPreLowpassHz  = 5200.0f;
constexpr float kMkPostLowpassHz = 11000.0f;
constexpr float kDriveEnvCoeff   = 0.02f;
}

void GainStageModule::BiquadFilter::SetLowShelf(float sampleRate, float freq, float gainDb, float shelfSlope)
{
    const float A     = powf(10.0f, gainDb / 40.0f);
    const float w0    = 2.0f * kPi * freq / sampleRate;
    const float cosw0 = cosf(w0);
    const float sinw0 = sinf(w0);
    const float alpha = sinw0 / 2.0f * sqrtf((A + 1.0f / A) * (1.0f / shelfSlope - 1.0f) + 2.0f);
    const float beta  = 2.0f * sqrtf(A) * alpha;

    float b0 =    A * ((A + 1.0f) - (A - 1.0f) * cosw0 + beta);
    float b1 =  2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0);
    float b2 =    A * ((A + 1.0f) - (A - 1.0f) * cosw0 - beta);
    float a0 =        (A + 1.0f) + (A - 1.0f) * cosw0 + beta;
    float a1 =   -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0);
    float a2 =        (A + 1.0f) + (A - 1.0f) * cosw0 - beta;

    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;

    this->b0 = b0;
    this->b1 = b1;
    this->b2 = b2;
    this->a1 = a1;
    this->a2 = a2;
}

void GainStageModule::BiquadFilter::SetHighShelf(float sampleRate, float freq, float gainDb, float shelfSlope)
{
    const float A     = powf(10.0f, gainDb / 40.0f);
    const float w0    = 2.0f * kPi * freq / sampleRate;
    const float cosw0 = cosf(w0);
    const float sinw0 = sinf(w0);
    const float alpha = sinw0 / 2.0f * sqrtf((A + 1.0f / A) * (1.0f / shelfSlope - 1.0f) + 2.0f);
    const float beta  = 2.0f * sqrtf(A) * alpha;

    float b0 =    A * ((A + 1.0f) + (A - 1.0f) * cosw0 + beta);
    float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0);
    float b2 =    A * ((A + 1.0f) + (A - 1.0f) * cosw0 - beta);
    float a0 =        (A + 1.0f) - (A - 1.0f) * cosw0 + beta;
    float a1 =    2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0);
    float a2 =        (A + 1.0f) - (A - 1.0f) * cosw0 - beta;

    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;

    this->b0 = b0;
    this->b1 = b1;
    this->b2 = b2;
    this->a1 = a1;
    this->a2 = a2;
}

void GainStageModule::BiquadFilter::SetLowpass(float sampleRate, float freq, float q)
{
    const float w0 = 2.0f * kPi * freq / sampleRate;
    const float cosw0 = cosf(w0);
    const float sinw0 = sinf(w0);
    const float alpha = sinw0 / (2.0f * q);

    float b0 = (1.0f - cosw0) * 0.5f;
    float b1 = 1.0f - cosw0;
    float b2 = (1.0f - cosw0) * 0.5f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosw0;
    float a2 = 1.0f - alpha;

    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;

    this->b0 = b0;
    this->b1 = b1;
    this->b2 = b2;
    this->a1 = a1;
    this->a2 = a2;
}

void GainStageModule::BiquadFilter::SetHighpass(float sampleRate, float freq, float q)
{
    const float w0 = 2.0f * kPi * freq / sampleRate;
    const float cosw0 = cosf(w0);
    const float sinw0 = sinf(w0);
    const float alpha = sinw0 / (2.0f * q);

    float b0 =  (1.0f + cosw0) * 0.5f;
    float b1 = -(1.0f + cosw0);
    float b2 =  (1.0f + cosw0) * 0.5f;
    float a0 =   1.0f + alpha;
    float a1 =  -2.0f * cosw0;
    float a2 =   1.0f - alpha;

    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;

    this->b0 = b0;
    this->b1 = b1;
    this->b2 = b2;
    this->a1 = a1;
    this->a2 = a2;
}

void GainStageModule::Init(float sampleRate)
{
    sampleRate_ = sampleRate;

    params_.trimGainDb    = 6.0f;
    params_.channelGainDb = 0.0f;
    params_.masterVolDb   = 0.0f;
    params_.bassGainDb    = 0.0f;
    params_.trebleGainDb  = 0.0f;
    params_.character     = 0.5f;
    params_.inputType     = 0;
    params_.clippingType  = 0;
    params_.toneMode      = 0;
    params_.debugMode     = 0;
    params_.bypass        = false;
    params_.boostEngage   = false;

    pendingParams_ = params_;
    paramsDirty_   = true;

    trimGainLin_    = dBToLin(params_.trimGainDb);
    channelGainLin_ = dBToLin(params_.channelGainDb);
    masterVolLin_   = dBToLin(params_.masterVolDb);
    inputCompGain_  = InputCompensationGain(params_.inputType);
    characterDriveScale_ = 1.0f;

    for(auto& f : bassShelf_)
    {
        f.Reset();
        f.SetLowShelf(sampleRate_, kBassFreqHz, 0.0f);
    }
    for(auto& f : trebleShelf_)
    {
        f.Reset();
        f.SetHighShelf(sampleRate_, kTrebleFreqHz, 0.0f);
    }
    for(auto& lp : loFiLowpass_)
    {
        lp.z = 0.0f;  // Explicit state reset
        lp.SetCutoff(sampleRate_, kLoFiCutHz);
    }

    for(int ch = 0; ch < 2; ++ch)
    {
        mkPreHP_[ch].Reset();
        mkPreHP_[ch].SetHighpass(sampleRate_, kMkPreHighpassHz, 0.7071f);
        mkPreLP_[ch].Reset();
        mkPreLP_[ch].SetLowpass(sampleRate_, kMkPreLowpassHz, 0.7071f);
        mkPostLP_[ch].Reset();
        mkPostLP_[ch].SetLowpass(sampleRate_, kMkPostLowpassHz, 0.7071f);
        driveEnv_[ch] = 0.0f;
    }

    ComputeFilterCoeffs();
}

void GainStageModule::SetPendingParams(const Params& params)
{
    pendingParams_ = params;
    paramsDirty_   = true;
}

void GainStageModule::AdjustHeadroom(float headroomDb)
{
    // Clamp to reasonable range: -12dB to +6dB (allows accumulation from multiple toggles)
    headroomAdjustmentDb_ = Clamp(headroomDb, -12.0f, 6.0f);
    paramsDirty_ = true;
}

float GainStageModule::ToneModeFactor(int toneMode) const
{
    switch(toneMode)
    {
        case 1: return 0.7f;
        case 2: return 1.3f;
        default: return 1.0f;
    }
}

float GainStageModule::InputCompensationGain(int inputType) const
{
    switch(inputType)
    {
        case 1: return 0.55f; // Line
        case 2: return 0.18f; // Mic / Low-Z
        default: return 1.0f;
    }
}

void GainStageModule::ComputeFilterCoeffs()
{
    const float toneFactor = ToneModeFactor(params_.toneMode);

    const float bassGain = (params_.bassGainDb + bassModeOffsetDb_) * toneFactor;
    const float trebleGain = (params_.trebleGainDb + trebleModeOffsetDb_) * toneFactor;

    for(auto& f : bassShelf_)
    {
        f.SetLowShelf(sampleRate_, kBassFreqHz, bassGain);
    }
    for(auto& f : trebleShelf_)
    {
        f.SetHighShelf(sampleRate_, kTrebleFreqHz, trebleGain);
    }

    if(loFiEnabled_)
    {
        for(auto& lp : loFiLowpass_)
        {
            lp.SetCutoff(sampleRate_, kLoFiCutHz);
        }
    }

    lastBassSetting_ = bassGain;
    lastTrebleSetting_ = trebleGain;
    lastToneMode_ = params_.toneMode;
    lastLoFiState_ = loFiEnabled_;
}

void GainStageModule::UpdateControls()
{
    if(!paramsDirty_)
    {
        return;
    }

    params_ = pendingParams_;
    paramsDirty_ = false;

    float drive = params_.driveNorm;
    if(drive < 0.0f) drive = 0.0f;
    if(drive > 1.0f) drive = 1.0f;
    params_.driveNorm    = drive;
    params_.trimGainDb   = drive * 20.0f;
    params_.channelGainDb = drive * 20.0f;
    params_.character    = drive;
    params_.masterVolDb  = 0.0f;
    params_.bassGainDb   = 0.0f;
    params_.trebleGainDb = 0.0f;
    params_.inputType    = 0;
    params_.clippingType = 0;
    params_.toneMode     = 0;

    trimGainLin_    = dBToLin(params_.trimGainDb);
    channelGainLin_ = dBToLin(params_.channelGainDb);
    // Apply headroom adjustment to master volume
    masterVolLin_   = dBToLin(headroomAdjustmentDb_);
    inputCompGain_  = 1.0f;

    bassModeOffsetDb_   = 0.0f;
    trebleModeOffsetDb_ = 0.0f;
    characterDriveScale_ = 1.0f;
    loFiEnabled_ = false;

    lastBassSetting_ = std::numeric_limits<float>::quiet_NaN();
    lastTrebleSetting_ = std::numeric_limits<float>::quiet_NaN();
    lastToneMode_ = -1;
    lastLoFiState_ = true;

    ComputeFilterCoeffs();
}

float GainStageModule::ApplyClipping(float inSample) const
{
    const float boost = params_.boostEngage ? boostFactor_ : 1.0f;
    const float gainFactor = 1.0f + params_.character * characterDriveScale_ * boost;
    float v = inSample * gainFactor;

    switch(params_.clippingType)
    {
        case 0:
        {
            const float softFactor = softClipBase_ + params_.character * characterDriveScale_;
            v = tanhf(v * softFactor);
            break;
        }
        case 1:
        {
            const float threshold = std::max(kMinThreshold, hardClipBaseThreshold_ - 0.4f * params_.character * characterDriveScale_);
            if(v > threshold)
            {
                v = threshold;
            }
            else if(v < -threshold)
            {
                v = -threshold;
            }
            break;
        }
        case 2:
        default:
        {
            const float sat = params_.character * characterDriveScale_;
            if(sat > 0.001f)
            {
                const float gentle = 1.0f + 0.3f * sat;
                v = tanhf(v * gentle);
            }
            v = Clamp(v, -1.2f, 1.2f);
            break;
        }
    }

    const float asym = 0.02f + 0.08f * params_.character;
    const float even = v * v;
    v += asym * even * (v >= 0.0f ? 1.0f : -1.0f);

    return v;
}

float GainStageModule::SafeLimit(float v) const
{
    constexpr float limit = 0.98f;
    if(params_.clippingType == 2)
    {
        const float sat = params_.character * characterDriveScale_;
        if(sat > 0.001f)
        {
            const float gentle = 1.0f + 0.2f * sat;
            v = tanhf(v * gentle);
        }
    }
    return Clamp(v, -limit, limit);
}

void GainStageModule::Process(const float* const* in, float** out, size_t size)
{
    UpdateControls();

    if(params_.bypass)
    {
        for(size_t i = 0; i < size; ++i)
        {
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
        return;
    }

    const float inComp = inputCompGain_;
    const bool skipClip = params_.debugMode >= 1;
    const bool skipTone = params_.debugMode >= 2;
    const bool rawTap   = params_.debugMode >= 3;

    for(size_t i = 0; i < size; ++i)
    {
        float preL = in[0][i] * inComp;
        float preR = in[1][i] * inComp;

        preL *= trimGainLin_;
        preR *= trimGainLin_;

        preL *= channelGainLin_;
        preR *= channelGainLin_;

        const float rawPreL = preL;
        const float rawPreR = preR;

        driveEnv_[0] += (fabsf(preL) - driveEnv_[0]) * kDriveEnvCoeff;
        driveEnv_[1] += (fabsf(preR) - driveEnv_[1]) * kDriveEnvCoeff;

        if(!rawTap)
        {
            const float dynL = 1.0f + (0.12f * params_.driveNorm) * driveEnv_[0];
            const float dynR = 1.0f + (0.12f * params_.driveNorm) * driveEnv_[1];

            float shapedPreL = mkPreHP_[0].Process(preL * dynL);
            shapedPreL = mkPreLP_[0].Process(shapedPreL);
            preL = 0.35f * shapedPreL + 0.65f * preL;

            float shapedPreR = mkPreHP_[1].Process(preR * dynR);
            shapedPreR = mkPreLP_[1].Process(shapedPreR);
            preR = 0.35f * shapedPreR + 0.65f * preR;
        }

        float clipL = skipClip ? preL : ApplyClipping(preL);
        float clipR = skipClip ? preR : ApplyClipping(preR);

        if(!rawTap)
        {
            clipL = mkPostLP_[0].Process(clipL);
            clipR = mkPostLP_[1].Process(clipR);
        }

        float toneL = clipL;
        float toneR = clipR;
        if(!skipTone)
        {
            toneL = bassShelf_[0].Process(toneL);
            toneL = trebleShelf_[0].Process(toneL);

            toneR = bassShelf_[1].Process(toneR);
            toneR = trebleShelf_[1].Process(toneR);

            if(loFiEnabled_)
            {
                toneL = loFiLowpass_[0].Process(toneL);
                toneR = loFiLowpass_[1].Process(toneR);
            }
        }

        float selectedL = rawTap ? rawPreL : toneL;
        float selectedR = rawTap ? rawPreR : toneR;

        selectedL *= masterVolLin_;
        selectedR *= masterVolLin_;

        if(rawTap)
        {
            selectedL = Clamp(selectedL, -1.1f, 1.1f);
            selectedR = Clamp(selectedR, -1.1f, 1.1f);
        }
        else
        {
            selectedL = SafeLimit(selectedL);
            selectedR = SafeLimit(selectedR);
        }

        // Denormal protection: flush very small values to zero
        // This prevents CPU slowdown and potential L/R asymmetry from denormal handling
        constexpr float kDenormalThreshold = 1.0e-15f;
        if(fabsf(selectedL) < kDenormalThreshold) selectedL = 0.0f;
        if(fabsf(selectedR) < kDenormalThreshold) selectedR = 0.0f;

        out[0][i] = selectedL;
        out[1][i] = selectedR;
    }
}
