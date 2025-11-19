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
constexpr float kOversamplePreCutHz  = 23000.0f;
constexpr float kOversamplePostCutHz = 20000.0f;
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

    params_ = {};
    params_.character   = 0.5f;
    params_.trimGainDb  = 0.0f;
    params_.channelGainDb = 0.0f;
    params_.masterVolDb = 0.0f;
    params_.bassGainDb  = 0.0f;
    params_.trebleGainDb= 0.0f;
    params_.driveNorm   = 0.0f;
    params_.inputType   = 0;
    params_.clippingType= 0;
    params_.toneMode    = 0;
    params_.debugMode   = 0;
    params_.bypass      = false;
    params_.boostAmount = 0.0f;

    pendingParams_ = params_;
    paramsDirty_   = true;

    trimGainLin_    = 1.0f;
    channelGainLin_ = 1.0f;
    masterVolLin_   = 1.0f;
    inputCompGain_  = 1.0f;
    characterDriveScale_ = 1.0f;

    preHpCutHz_  = kMkPreHighpassHz;
    preLpCutHz_  = kMkPreLowpassHz;
    postLpCutHz_ = kMkPostLowpassHz;
    loFiCutHz_   = kLoFiCutHz;

    for(auto& f : bassShelf_)
        f.Reset();
    for(auto& f : trebleShelf_)
        f.Reset();
    for(auto& lp : loFiLowpass_)
    {
        lp.z = 0.0f;
        lp.SetCutoff(sampleRate_, loFiCutHz_);
    }

    for(int ch = 0; ch < 2; ++ch)
    {
        mkPreHP_[ch].Reset();
        mkPreLP_[ch].Reset();
        mkPostLP_[ch].Reset();
        driveEnv_[ch] = 0.0f;
    }

    oversamplerL_.Init(sampleRate_, kOversamplePreCutHz, kOversamplePostCutHz);
    oversamplerR_.Init(sampleRate_, kOversamplePreCutHz, kOversamplePostCutHz);

    UpdateControls();
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

    const bool needShelves = !std::isfinite(lastBassSetting_) ||
                             std::fabs(lastBassSetting_ - bassGain) > 1.0e-3f ||
                             std::fabs(lastTrebleSetting_ - trebleGain) > 1.0e-3f ||
                             lastToneMode_ != params_.toneMode;

    if(needShelves)
    {
        for(auto& f : bassShelf_)
            f.SetLowShelf(sampleRate_, kBassFreqHz, bassGain);
        for(auto& f : trebleShelf_)
            f.SetHighShelf(sampleRate_, kTrebleFreqHz, trebleGain);
        lastBassSetting_ = bassGain;
        lastTrebleSetting_ = trebleGain;
        lastToneMode_ = params_.toneMode;
    }

    const bool needPreFilters = std::fabs(lastPreHpCut_ - preHpCutHz_) > 1.0e-3f ||
                                std::fabs(lastPreLpCut_ - preLpCutHz_) > 1.0e-3f;
    if(needPreFilters)
    {
        for(int ch = 0; ch < 2; ++ch)
        {
            mkPreHP_[ch].SetHighpass(sampleRate_, preHpCutHz_, 0.7071f);
            mkPreLP_[ch].SetLowpass(sampleRate_, preLpCutHz_, 0.7071f);
        }
        lastPreHpCut_ = preHpCutHz_;
        lastPreLpCut_ = preLpCutHz_;
    }

    const bool needPostFilters = std::fabs(lastPostLpCut_ - postLpCutHz_) > 1.0e-3f;
    if(needPostFilters)
    {
        for(int ch = 0; ch < 2; ++ch)
            mkPostLP_[ch].SetLowpass(sampleRate_, postLpCutHz_, 0.7071f);
        lastPostLpCut_ = postLpCutHz_;
    }

    if(loFiEnabled_ != lastLoFiState_)
    {
        for(auto& lp : loFiLowpass_)
            lp.SetCutoff(sampleRate_, loFiEnabled_ ? loFiCutHz_ : 20000.0f);
        lastLoFiState_ = loFiEnabled_;
    }
}

void GainStageModule::UpdateControls()
{
    if(!paramsDirty_)
    {
        return;
    }

    params_ = pendingParams_;
    paramsDirty_ = false;

    params_.trimGainDb    = Clamp(params_.trimGainDb,   -40.0f, 40.0f);
    params_.channelGainDb = Clamp(params_.channelGainDb,-40.0f, 40.0f);
    params_.masterVolDb   = Clamp(params_.masterVolDb,  -24.0f, 12.0f);
    params_.bassGainDb    = Clamp(params_.bassGainDb,   -18.0f, 18.0f);
    params_.trebleGainDb  = Clamp(params_.trebleGainDb, -18.0f, 18.0f);
    params_.driveNorm     = Clamp(params_.driveNorm,     0.0f, 1.0f);
    params_.character     = Clamp(params_.character,     0.0f, 1.0f);
    params_.inputType     = std::clamp(params_.inputType, 0, 2);
    params_.clippingType  = std::clamp(params_.clippingType, 0, 2);
    params_.toneMode      = std::clamp(params_.toneMode, 0, 2);
    params_.debugMode     = std::max(params_.debugMode, 0);

    trimGainLin_    = dBToLin(params_.trimGainDb + headroomAdjustmentDb_);
    channelGainLin_ = dBToLin(params_.channelGainDb + headroomAdjustmentDb_);
    masterVolLin_   = dBToLin(params_.masterVolDb);
    inputCompGain_  = InputCompensationGain(params_.inputType);

    characterDriveScale_ = 0.8f + 1.2f * params_.character;
    stage1Softness_      = 0.8f + 0.4f * (1.0f - params_.character);
    stage1HardThreshold_ = 1.35f - 0.15f * params_.character;
    stage2Softness_      = 1.05f + 1.1f * params_.character;
    stage2HardThreshold_ = 0.95f - 0.35f * params_.character;
    stage2Asymmetry_     = 0.02f + 0.08f * params_.character;

    switch(params_.toneMode)
    {
        case 1: // Standard
            bassModeOffsetDb_   = 0.5f;
            trebleModeOffsetDb_ = -0.5f;
            preHpCutHz_  = kMkPreHighpassHz;
            preLpCutHz_  = kMkPreLowpassHz * 0.9f;
            postLpCutHz_ = 14000.0f;
            loFiCutHz_   = 8500.0f;
            break;
        case 2: // Lo-Fi / Slow
            bassModeOffsetDb_   = 1.5f;
            trebleModeOffsetDb_ = -3.0f;
            preHpCutHz_  = kMkPreHighpassHz * 0.7f;
            preLpCutHz_  = kMkPreLowpassHz * 0.65f;
            postLpCutHz_ = 9000.0f;
            loFiCutHz_   = 6500.0f;
            break;
        case 0:
        default:
            bassModeOffsetDb_   = 0.0f;
            trebleModeOffsetDb_ = 0.0f;
            preHpCutHz_  = kMkPreHighpassHz * 0.9f;
            preLpCutHz_  = kMkPreLowpassHz * 1.1f;
            postLpCutHz_ = 17500.0f;
            loFiCutHz_   = 11000.0f;
            break;
    }

    loFiEnabled_ = (params_.toneMode == 2);

    ComputeFilterCoeffs();
}

float GainStageModule::ApplyClippingCore(float x, int clippingType, float softness, float hardThreshold) const
{
    // Smooth boost interpolation: 0.0 = no boost (1.0x), 1.0 = full boost (boostFactor_)
    const float boost = 1.0f + params_.boostAmount * (boostFactor_ - 1.0f);
    const float drive = 1.0f + params_.driveNorm * characterDriveScale_;
    float v = x * drive * boost;

    switch(clippingType)
    {
        case 1: // hard
            v = Clamp(v, -hardThreshold, hardThreshold);
            break;
        case 2: // gentle
        {
            const float t = tanhf(v * softness * 0.8f);
            v = Clamp(t, -hardThreshold, hardThreshold);
            break;
        }
        case 0:
        default:
            v = tanhf(v * softness);
            break;
    }

    const float asymBase = 0.01f + 0.03f * params_.character;
    v += asymBase * v * v * (v >= 0.0f ? 1.0f : -1.0f);
    return v;
}

float GainStageModule::ApplyOpAmpStage1(float x) const
{
    const int stageType = (params_.clippingType == 1) ? 2 : params_.clippingType;
    return ApplyClippingCore(x, stageType, stage1Softness_, stage1HardThreshold_);
}

float GainStageModule::ApplyOpAmpStage2(float x, int channel) const
{
    float v = ApplyClippingCore(x, params_.clippingType, stage2Softness_, stage2HardThreshold_);
    const float asym = stage2Asymmetry_ * (channel == 0 ? 1.0f : -1.0f);
    v += asym * v * v;
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

    const bool bypassStage2 = params_.debugMode >= 1;
    const bool bypassEq     = params_.debugMode >= 2;
    const bool rawTap       = params_.debugMode >= 3;
    const float inComp      = inputCompGain_;

    auto processStage2 = [this, bypassStage2](float sample, int channel) -> float
    {
        if(bypassStage2)
            return sample;

        auto& os = (channel == 0) ? oversamplerL_ : oversamplerR_;
        auto up = os.Upsample(sample);
        float acc = 0.0f;
        for(float s : up)
        {
            float pre = os.PreFilter(s);
            float clipped = ApplyOpAmpStage2(pre, channel);
            float post = os.PostFilter(clipped);
            acc += post;
        }
        return acc / static_cast<float>(Oversampler4x::kFactor);
    };

    for(size_t i = 0; i < size; ++i)
    {
        float stage0L = in[0][i] * inComp;
        float stage0R = in[1][i] * inComp;

        float stage1InL = stage0L * trimGainLin_;
        float stage1InR = stage0R * trimGainLin_;

        float stage1OutL = ApplyOpAmpStage1(stage1InL);
        float stage1OutR = ApplyOpAmpStage1(stage1InR);

        driveEnv_[0] += (std::fabs(stage1OutL) - driveEnv_[0]) * kDriveEnvCoeff;
        driveEnv_[1] += (std::fabs(stage1OutR) - driveEnv_[1]) * kDriveEnvCoeff;

        float preEqL = mkPreLP_[0].Process(mkPreHP_[0].Process(stage1OutL));
        float preEqR = mkPreLP_[1].Process(mkPreHP_[1].Process(stage1OutR));

        float eqL = preEqL;
        float eqR = preEqR;
        if(!bypassEq)
        {
            eqL = trebleShelf_[0].Process(bassShelf_[0].Process(eqL));
            eqR = trebleShelf_[1].Process(bassShelf_[1].Process(eqR));
        }

        float stage2InL = eqL * channelGainLin_;
        float stage2InR = eqR * channelGainLin_;
        float stage2OutL = processStage2(stage2InL, 0);
        float stage2OutR = processStage2(stage2InR, 1);

        float postL = stage2OutL;
        float postR = stage2OutR;

        if(loFiEnabled_)
        {
            postL = loFiLowpass_[0].Process(postL);
            postR = loFiLowpass_[1].Process(postR);
        }

        postL = mkPostLP_[0].Process(postL);
        postR = mkPostLP_[1].Process(postR);

        float selectedL = rawTap ? stage1OutL : postL;
        float selectedR = rawTap ? stage1OutR : postR;

        selectedL *= masterVolLin_;
        selectedR *= masterVolLin_;

        selectedL = SafeLimit(selectedL);
        selectedR = SafeLimit(selectedR);

        constexpr float kDenormalThreshold = 1.0e-15f;
        if(std::fabs(selectedL) < kDenormalThreshold) selectedL = 0.0f;
        if(std::fabs(selectedR) < kDenormalThreshold) selectedR = 0.0f;

        out[0][i] = selectedL;
        out[1][i] = selectedR;
    }
}
