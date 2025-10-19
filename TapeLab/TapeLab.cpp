
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include "daisysp.h"
#include "hothouse.h"

using namespace daisy;
using namespace daisysp;
using clevelandmusicco::Hothouse;

namespace {
constexpr float kMaxDelaySeconds = 0.050f;
constexpr float kMaxSampleRate = 96000.0f;
constexpr int kMaxDelaySamples = static_cast<int>(kMaxDelaySeconds * kMaxSampleRate);
constexpr float kPrintSeconds = 0.5f;
constexpr size_t kMaxPrintSamples = static_cast<size_t>(kMaxSampleRate * kPrintSeconds) + 1;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float Clamp(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static uint32_t rng = 0x12345678u;
inline float Frand()
{
    rng = 1664525u * rng + 1013904223u;
    return ((rng >> 9) * (1.0f / 8388608.0f)) * 2.0f - 1.0f;
}

inline float EnvFollow(float x, float& env, float atk, float rel)
{
    float target = fabsf(x);
    float coeff = (target > env) ? atk : rel;
    env += coeff * (target - env);
    return env;
}

inline float SoftClipSine(float x)
{
    const float k = 1.253314f;
    float z = Clamp(x * k, -1.570796f, 1.570796f);
    return sinf(z) * (2.0f / 3.1415926f);
}

inline float LogCosh(float v)
{
    float a = fabsf(v);
    return a + log1pf(expf(-2.0f * a)) - 0.69314718f; // -log(2)
}

inline float ADAATanh(float x, float prev, float gain)
{
    float gx = gain * x;
    float gPrev = gain * prev;
    float F = LogCosh(gx) / gain;
    float FPrev = LogCosh(gPrev) / gain;
    float denom = x - prev;
    if (fabsf(denom) < 1e-12f)
    {
        return tanhf(gx);
    }
    return (F - FPrev) / denom;
}

struct PinkNoise
{
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    inline float Process(float w)
    {
        b0 = 0.99886f * b0 + w * 0.0555179f;
        b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f;
        b5 = -0.7616f * b5 - w * 0.0168980f;
        float p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
        b6 = w * 0.115926f;
        return p * 0.11f;
    }
};

inline float Db(float x) { return 20.0f * log10f(x + 1e-12f); }
inline float FromDb(float db) { return powf(10.0f, db / 20.0f); }

struct TapeShapeCoeffs
{
    float wowMul;
    float flutterMul;
    float tiltAdd;
    float deTiltMul;
    float headBumpMul;
    float headBumpFreq;
    float toneOffsetHz;
    float hissMul;
    float reverbMul;
};

constexpr TapeShapeCoeffs kTapeShapes[] = {
    {1.00f, 1.00f, 0.00f, 1.00f, 1.00f, 90.0f,   0.0f, 1.00f, 1.00f}, // Studio
    {1.08f, 1.15f, 0.10f, 1.05f, 1.10f, 82.0f, -520.0f, 1.10f, 1.05f}, // Vintage
    {1.25f, 1.30f, 0.22f, 1.12f, 1.22f, 72.0f, -900.0f, 1.22f, 1.15f}  // Cassette
};

struct SoftKneeComp
{
    float sr = 48000.f, env = 0.f, gain = 1.f;
    float threshDb = -12.f, ratio = 1.6f, kneeDb = 6.f, atkSec = 0.005f, relSec = 0.100f;
    void Init(float sampleRate) { sr = sampleRate; env = 0.f; gain = 1.f; }
    inline float ProcessGain(float sidechain)
    {
        float aA = 1.f - expf(-1.f / (atkSec * sr));
        float aR = 1.f - expf(-1.f / (relSec * sr));
        env += ((sidechain > env) ? aA : aR) * (sidechain - env);
        float envDb = Db(env);
        float over = envDb - threshDb;
        float gDb = 0.f;
        if (over <= -0.5f * kneeDb)
            gDb = 0.f;
        else if (over >= 0.5f * kneeDb)
            gDb = (threshDb + over / ratio) - envDb;
        else
        {
            float t = (over + 0.5f * kneeDb) / kneeDb;
            float compOut = threshDb + over / ratio;
            float softOut = envDb + t * (compOut - envDb);
            gDb = softOut - envDb;
        }
        float target = FromDb(gDb);
        gain += 0.02f * (target - gain);
        return gain;
    }
};

struct TapeStageState
{
    float bias = 0.0f;      // level-driven DC bias memory
    float sagFast = 0.0f;   // quick supply droop
    float sagSlow = 0.0f;   // slow recovery sag
    float mag = 0.0f;       // tape magnetization memory (hysteresis)
    float lp = 0.0f;        // one-pole lowpass state for HF sensing
    float adaaPrev = 0.0f;  // previous input for ADAA shaper
};

inline float TapeStageProcess(float in, TapeStageState& st, float driveCtl)
{
    // --- Bias memory (even harmonics that follow program level) ---
    const float biasAtk = 0.0015f;
    const float biasRel = 0.0002f;
    float target = in * 0.4f;
    float coeff = (fabsf(target) > fabsf(st.bias)) ? biasAtk : biasRel;
    st.bias += coeff * (target - st.bias);

    // --- Dual-time sag behaviour (fast droop + slow supply recovery) ---
    const float sagFastAtk = 0.0025f;
    const float sagFastRel = 0.00035f;
    const float sagSlowAtk = 0.0006f;
    const float sagSlowRel = 0.00008f;
    float level = fabsf(in);
    st.sagFast += ((level > st.sagFast) ? sagFastAtk : sagFastRel) * (level - st.sagFast);
    st.sagSlow += ((level > st.sagSlow) ? sagSlowAtk : sagSlowRel) * (level - st.sagSlow);
    float sagMix = 0.65f * st.sagFast + 0.35f * st.sagSlow;
    float sagScale = 1.8f;
    float sagGain = 1.0f / (1.0f + sagScale * sagMix);

    // --- Hysteresis / magnetization memory (simple leaky integrator) ---
    const float magCoeff = 0.0030f;
    st.mag += magCoeff * (in - st.mag);

    // --- HF sensing to make high frequencies saturate first (cassette vibe) ---
    const float lpCoeff = 0.02f; // ~1 ms LP
    st.lp += lpCoeff * (in - st.lp);
    float hf = in - st.lp;
    float hfSense = tanhf(4.0f * fabsf(hf));
    float hfDrive = 1.0f + (0.6f + 0.8f * driveCtl) * hfSense;

    // --- Assemble pre-clip signal ---
    float headCouple = 0.12f + 0.28f * driveCtl;
    float core = (in + 0.6f * st.bias + headCouple * st.mag);
    float x = core * (0.68f + 2.05f * driveCtl) * hfDrive * sagGain;

    // --- Nonlinear transfer with ADAA tanh (low aliasing) ---
    float asym = 0.020f + 0.085f * driveCtl;
    float adaIn = x + asym;
    float yAda = ADAATanh(adaIn, st.adaaPrev, 1.05f);
    st.adaaPrev = adaIn;
    float yBlend = 0.82f * yAda + 0.14f * SoftClipSine(adaIn) + 0.04f * adaIn;

    // Subtle odd-order enrichment
    return 0.92f * yBlend + 0.08f * (adaIn * adaIn * adaIn);
}

inline float AzimuthAllpass(float x, float a, float& z)
{
    float y = -a * x + z;
    z = x + a * y;
    return y;
}

DSY_SDRAM_BSS float gPrintBufferL[kMaxPrintSamples] = {};
DSY_SDRAM_BSS float gPrintBufferR[kMaxPrintSamples] = {};
} // namespace

struct TapeParams
{
    float drive = 0.6f;
    float wow = 0.35f;
    float flutter = 0.35f;
    float speed = 0.5f;
    float tone = 0.5f;
    float hiss = 0.1f;
    float reverbMix = 0.25f;
    float wetDry = 0.85f;
    float headBump = 0.35f;
};

class TapeLabProcessor
{
  public:
    void Init(float sampleRate, const TapeParams& params);
    void SetParams(const TapeParams& params);
    void Process(const float* inL, const float* inR, float* outL, float* outR, size_t frames);

    enum class RevPlace { PRE, POST, DUAL };
    enum class TapeShape { Studio, Vintage, Cassette };
    void SetReverbPlacement(RevPlace p) { rev_place_ = p; }
    void SetReverbPredelaySamples(int n) { rev_predelay_samps_ = Clamp(static_cast<float>(n), 0.f, 2400.f); }
    void SetReverbDampLP(float hz) { rev_lpf_hz_ = Clamp(hz, 2000.f, 14000.f); }
    void SetArtifactsMode(int m) { artifacts_mode_ = m; }
    void SetTapeShape(TapeShape s) { shape_ = s; }
    TapeShape GetTapeShape() const { return shape_; }

  private:
    void UpdateFilters();
    void UpdatePrintBuffer();
    float ModulatedDelay(daisysp::DelayLine<float, kMaxDelaySamples>& dly,
                         float input,
                         float baseMs,
                         float wowAmt,
                         float wowHz,
                         float fltAmt,
                         float fltHz,
                         float& wowPhase,
                         float& fltPhase,
                         float& drift,
                         float jitter,
                         float& readState);

    float sampleRate_ = 48000.0f;
    TapeParams params_;
    daisysp::DelayLine<float, kMaxDelaySamples> dlyL_;
    daisysp::DelayLine<float, kMaxDelaySamples> dlyR_;
    daisysp::Svf hissBPF_L_;
    daisysp::Svf hissBPF_R_;
    daisysp::Tone tapeLP_L_;
    daisysp::Tone tapeLP_R_;
    daisysp::ATone tapeHP_L_;
    daisysp::ATone tapeHP_R_;
    daisysp::ATone preHP_L_;
    daisysp::ATone preHP_R_;
    daisysp::Tone preAA_L_;
    daisysp::Tone preAA_R_;
    daisysp::ATone scHP_L_;
    daisysp::ATone scHP_R_;
    daisysp::DcBlock dcL_;
    daisysp::DcBlock dcR_;
    daisysp::ReverbSc rvb_;
    daisysp::Svf bumpL_;
    daisysp::Svf bumpR_;
    TapeStageState tstateL_;
    TapeStageState tstateR_;
    RevPlace rev_place_ = RevPlace::POST;
    daisysp::DelayLine<float, 2400> revPreL_;
    daisysp::DelayLine<float, 2400> revPreR_;
    daisysp::ATone revHP_L_;
    daisysp::ATone revHP_R_;
    daisysp::Tone revLP_L_;
    daisysp::Tone revLP_R_;
    float rev_predelay_samps_ = 1000.f;
    float rev_lpf_hz_ = 6000.f;
    float rev_hpf_hz_ = 150.f;
    PinkNoise pnoiseL_, pnoiseR_;
    float hissGateL_ = 1.0f, hissGateR_ = 1.0f;
    daisysp::Tone hissLow_L_;
    daisysp::Tone hissLow_R_;
    SoftKneeComp comp_;
    int artifacts_mode_ = 1;
    uint32_t crackleCtrL_ = 0, crackleCtrR_ = 0;
    float crackleHoldL_ = 0.f, crackleHoldR_ = 0.f;
    float dEnvL_ = 1.f, dEnvR_ = 1.f;
    uint32_t dHoldL_ = 0, dHoldR_ = 0;
    float envL_ = 0.0f;
    float envR_ = 0.0f;
    float azimuth_ = 0.0f;
    float zAzL_ = 0.0f;
    float zAzR_ = 0.0f;
    float* printL_ = gPrintBufferL;
    float* printR_ = gPrintBufferR;
    size_t printLen_ = 1;
    size_t printIdx_ = 0;
    float wowPhaseL_ = 0.0f;
    float wowPhaseR_ = 0.0f;
    float fltPhaseL_ = 0.0f;
    float fltPhaseR_ = 0.0f;
    float driftL_ = 0.0f;
    float driftR_ = 0.0f;
    float fltJitL_ = 0.0f;
    float fltJitR_ = 0.0f;
    float readStateL_ = 0.0f;
    float readStateR_ = 0.0f;
    float reverbDuck_ = 1.0f;
    TapeShape shape_ = TapeShape::Studio;
    float preTiltStateL_ = 0.0f;
    float preTiltStateR_ = 0.0f;
    float postTiltStateL_ = 0.0f;
    float postTiltStateR_ = 0.0f;
    float tiltAlpha_ = 0.0f;
};

void TapeLabProcessor::Init(float sampleRate, const TapeParams& params)
{
    sampleRate_ = sampleRate;
    params_ = params;

    dlyL_.Init();
    dlyR_.Init();

    tapeLP_L_.Init(sampleRate_);
    tapeLP_R_.Init(sampleRate_);
    tapeHP_L_.Init(sampleRate_);
    tapeHP_R_.Init(sampleRate_);
    preHP_L_.Init(sampleRate_);
    preHP_R_.Init(sampleRate_);
    preAA_L_.Init(sampleRate_);
    preAA_R_.Init(sampleRate_);
    float preHighpassHz = 40.0f;
    preHP_L_.SetFreq(preHighpassHz);
    preHP_R_.SetFreq(preHighpassHz);
    float preAAHz = std::min(12000.0f, 10000.0f * (sampleRate_ / 48000.0f));
    preAA_L_.SetFreq(preAAHz);
    preAA_R_.SetFreq(preAAHz);
    scHP_L_.Init(sampleRate_);
    scHP_R_.Init(sampleRate_);
    float scFreq = 1000.0f;
    scHP_L_.SetFreq(scFreq);
    scHP_R_.SetFreq(scFreq);
    tstateL_ = {};
    tstateR_ = {};

    revPreL_.Init();
    revPreR_.Init();
    revHP_L_.Init(sampleRate_);
    revHP_R_.Init(sampleRate_);
    revLP_L_.Init(sampleRate_);
    revLP_R_.Init(sampleRate_);
    revHP_L_.SetFreq(rev_hpf_hz_);
    revHP_R_.SetFreq(rev_hpf_hz_);
    revLP_L_.SetFreq(rev_lpf_hz_);
    revLP_R_.SetFreq(rev_lpf_hz_);

    comp_.Init(sampleRate_);

    hissBPF_L_.Init(sampleRate_);
    hissBPF_R_.Init(sampleRate_);
    hissBPF_L_.SetFreq(7000.0f);
    hissBPF_R_.SetFreq(7000.0f);
    hissBPF_L_.SetRes(0.45f);
    hissBPF_R_.SetRes(0.45f);
    hissBPF_L_.SetDrive(0.35f);
    hissBPF_R_.SetDrive(0.35f);
    hissLow_L_.Init(sampleRate_);
    hissLow_R_.Init(sampleRate_);
    hissLow_L_.SetFreq(1000.0f);
    hissLow_R_.SetFreq(1000.0f);

    rvb_.Init(sampleRate_);
    rvb_.SetLpFreq(9000.0f);
    rvb_.SetFeedback(0.85f);

    dcL_.Init(sampleRate_);
    dcR_.Init(sampleRate_);

    bumpL_.Init(sampleRate_);
    bumpR_.Init(sampleRate_);
    bumpL_.SetFreq(90.0f);
    bumpR_.SetFreq(90.0f);
    bumpL_.SetRes(0.3f);
    bumpR_.SetRes(0.3f);
    bumpL_.SetDrive(1.0f);
    bumpR_.SetDrive(1.0f);

    envL_ = envR_ = 0.0f;
    azimuth_ = 0.0f;
    zAzL_ = zAzR_ = 0.0f;
    printIdx_ = 0;
    std::fill_n(printL_, kMaxPrintSamples, 0.0f);
    std::fill_n(printR_, kMaxPrintSamples, 0.0f);

    wowPhaseL_ = wowPhaseR_ = 0.0f;
    fltPhaseL_ = fltPhaseR_ = 0.0f;
    driftL_ = driftR_ = 0.0f;
    fltJitL_ = fltJitR_ = 0.0f;
    readStateL_ = readStateR_ = 0.0f;
    reverbDuck_ = 1.0f;
    preTiltStateL_ = preTiltStateR_ = 0.0f;
    postTiltStateL_ = postTiltStateR_ = 0.0f;
    tiltAlpha_ = expf(-kTwoPi * 1000.0f / sampleRate_);

    UpdateFilters();
    UpdatePrintBuffer();
}

void TapeLabProcessor::SetParams(const TapeParams& params)
{
    params_ = params;
    UpdateFilters();
    UpdatePrintBuffer();
}

void TapeLabProcessor::UpdateFilters()
{
    float lpfHz = 3000.0f + params_.tone * 14000.0f;
    float hpfHz = 20.0f + params_.headBump * 80.0f;

    float minLp = std::max(10.0f, lpfHz);
    float minHp = std::max(1.0f, hpfHz);

    tapeLP_L_.SetFreq(minLp);
    tapeLP_R_.SetFreq(minLp);
    tapeHP_L_.SetFreq(minHp);
    tapeHP_R_.SetFreq(minHp);

    float preAAHz = Clamp((6000.0f + params_.tone * 10000.0f) * 0.8f, 5000.0f, 14000.0f);
    preAA_L_.SetFreq(preAAHz);
    preAA_R_.SetFreq(preAAHz);

    revHP_L_.SetFreq(rev_hpf_hz_);
    revHP_R_.SetFreq(rev_hpf_hz_);
    revLP_L_.SetFreq(rev_lpf_hz_);
    revLP_R_.SetFreq(rev_lpf_hz_);
}

void TapeLabProcessor::UpdatePrintBuffer()
{
    float seconds = Clamp(0.35f + 0.25f * params_.speed, 0.2f, kPrintSeconds);
    size_t target = static_cast<size_t>(seconds * sampleRate_);
    if (target < 1)
        target = 1;
    if (target > kMaxPrintSamples)
        target = kMaxPrintSamples;
    if (target != printLen_)
    {
        printLen_ = target;
        printIdx_ = 0;
        std::fill(printL_, printL_ + kMaxPrintSamples, 0.0f);
        std::fill(printR_, printR_ + kMaxPrintSamples, 0.0f);
    }
}

float TapeLabProcessor::ModulatedDelay(daisysp::DelayLine<float, kMaxDelaySamples>& dly,
                                       float input,
                                       float baseMs,
                                       float wowAmt,
                                       float wowHz,
                                       float fltAmt,
                                       float fltHz,
                                       float& wowPhase,
                                       float& fltPhase,
                                       float& drift,
                                       float jitter,
                                       float& readState)
{
    dly.Write(input);

    float driftTarget = 0.0006f * Frand();
    drift += 0.0015f * (driftTarget - drift);

    wowPhase += (kTwoPi * wowHz) / sampleRate_;
    fltPhase += (kTwoPi * fltHz) / sampleRate_;
    if (wowPhase > kTwoPi)
        wowPhase -= kTwoPi;
    if (fltPhase > kTwoPi)
        fltPhase -= kTwoPi;

    float wow = sinf(wowPhase);
    float flt = sinf(fltPhase);

    float base = baseMs * 0.001f;
    float fltPlusNoise = flt + 0.35f * jitter;
    float wowDepth = 0.0060f * wowAmt;
    float fltDepth = 0.00045f * sqrtf(fltAmt);
    float dev = wowDepth * wow + fltDepth * fltPlusNoise + drift;
    dev = Clamp(dev, -0.0015f, 0.0015f);

    float readSec = std::max(0.0002f, base + dev);
    float readSamples = readSec * sampleRate_;

    float maxDelta = 0.75f;
    float delta = readSamples - readState;
    if (delta > maxDelta)
        delta = maxDelta;
    else if (delta < -maxDelta)
        delta = -maxDelta;
    readState += delta;

    return dly.ReadHermite(readState);
}

void TapeLabProcessor::Process(const float* inL,
                               const float* inR,
                               float* outL,
                               float* outR,
                               size_t frames)
{
    UpdateFilters();

    const TapeShapeCoeffs& shape = kTapeShapes[static_cast<int>(shape_)];
    const float baseWowHz = (0.25f + 0.6f * params_.speed) * shape.wowMul;
    const float baseFltHz = (4.5f + 7.0f * params_.speed) * shape.flutterMul;
    const float baseMsL = 12.0f;
    const float baseMsR = 14.5f;
    const float driveCtl = Clamp(params_.drive, 0.0f, 1.0f);
    const float drive = 0.5f + 2.0f * params_.drive;
    float reverbMix = Clamp(params_.reverbMix * shape.reverbMul, 0.0f, 1.0f);
    const float wetDry = Clamp(params_.wetDry, 0.0f, 1.0f);
    const float dryMix = 1.0f - wetDry;
    const float hissBase = params_.hiss * 0.04f;
    const float wowHzL = baseWowHz;
    const float wowHzR = baseWowHz * 1.03f;
    const float fltHzL = baseFltHz;
    const float fltHzR = baseFltHz * 0.97f;
    const float toneBaseHz = Clamp(3000.0f + params_.tone * 14000.0f + shape.toneOffsetHz, 2000.0f, 18000.0f);
    const float bumpAmt = (0.08f + 0.18f * params_.headBump) * shape.headBumpMul;
    const float crossTalk = 0.02f;
    const float ptGain = 0.02f;
    const float ptKeep = 0.98f;
    const bool hasPrint = printLen_ > 0;
    const float tiltAmt = Clamp(0.35f + 0.55f * driveCtl + shape.tiltAdd, 0.0f, 1.20f);
    const float deTiltAmt = Clamp((0.32f + 0.48f * driveCtl) * shape.deTiltMul, 0.0f, 1.20f);
    const float tiltFollow = 1.0f - tiltAlpha_;
    const bool artifactsActive = artifacts_mode_ == 2;
    const float hissGain = (artifacts_mode_ >= 1) ? hissBase * shape.hissMul : 0.0f;

    if (!artifactsActive)
    {
        crackleCtrL_ = crackleCtrR_ = 0;
        crackleHoldL_ = crackleHoldR_ = 0.0f;
        dEnvL_ = dEnvR_ = 1.0f;
        dHoldL_ = dHoldR_ = 0;
    }

    bumpL_.SetFreq(shape.headBumpFreq);
    bumpR_.SetFreq(shape.headBumpFreq);

    bumpL_.SetDrive(1.0f + bumpAmt);
    bumpR_.SetDrive(1.0f + bumpAmt);

    comp_.threshDb = -12.0f - 8.0f * driveCtl;
    comp_.ratio = 1.6f + 0.8f * driveCtl;
    comp_.kneeDb = 6.0f + 2.0f * driveCtl;

    for (size_t i = 0; i < frames; ++i)
    {
        float dryL = inL[i];
        float dryR = inR[i];

        float preL = preHP_L_.Process(dryL);
        float preR = preHP_R_.Process(dryR);

        float preMix = 0.f, postMix = 0.f;
        switch (rev_place_)
        {
            case RevPlace::PRE:
                preMix = reverbMix;
                break;
            case RevPlace::POST:
                postMix = reverbMix;
                break;
            case RevPlace::DUAL:
                preMix = 0.5f * reverbMix;
                postMix = 0.5f * reverbMix;
                break;
        }

        if (preMix > 0.f)
        {
            revPreL_.Write(preL);
            revPreR_.Write(preR);
            float prePdL = revPreL_.Read(rev_predelay_samps_);
            float prePdR = revPreR_.Read(rev_predelay_samps_ + 2.f);
            float preInL = revHP_L_.Process(prePdL);
            float preInR = revHP_R_.Process(prePdR);
            float preWetL, preWetR;
            rvb_.Process(preInL, preInR, &preWetL, &preWetR);
            preWetL = revLP_L_.Process(preWetL);
            preWetR = revLP_R_.Process(preWetR);
            preL += preMix * preWetL;
            preR += preMix * preWetR;
        }

        float preLowL = preTiltStateL_ = tiltAlpha_ * preTiltStateL_ + tiltFollow * preL;
        float preLowR = preTiltStateR_ = tiltAlpha_ * preTiltStateR_ + tiltFollow * preR;
        float preHighL = preL - preLowL;
        float preHighR = preR - preLowR;
        preL = preLowL * (1.0f - tiltAmt) + preHighL * (1.0f + tiltAmt);
        preR = preLowR * (1.0f - tiltAmt) + preHighR * (1.0f + tiltAmt);

        preL = preAA_L_.Process(preL);
        preR = preAA_R_.Process(preR);

        preL *= drive;
        preR *= drive;

        float satL = TapeStageProcess(preL, tstateL_, driveCtl);
        float satR = TapeStageProcess(preR, tstateR_, driveCtl);

        float postLowL = postTiltStateL_ = tiltAlpha_ * postTiltStateL_ + tiltFollow * satL;
        float postLowR = postTiltStateR_ = tiltAlpha_ * postTiltStateR_ + tiltFollow * satR;
        float postHighL = satL - postLowL;
        float postHighR = satR - postLowR;
        satL = postLowL * (1.0f + 0.15f * deTiltAmt) + postHighL * (1.0f - deTiltAmt);
        satR = postLowR * (1.0f + 0.15f * deTiltAmt) + postHighR * (1.0f - deTiltAmt);

        float envL = EnvFollow(satL, envL_, 0.005f, 0.0005f);
        float envR = EnvFollow(satR, envR_, 0.005f, 0.0005f);
        {
            float envAvg = 0.5f * (envL + envR);
            float duckTarget = 0.40f + 0.60f * Clamp(1.0f - 2.0f * envAvg, 0.0f, 1.0f);
            reverbDuck_ += 0.01f * (duckTarget - reverbDuck_);
        }

        float flutterAmtL = Clamp(params_.flutter, 0.0f, 1.0f);
        float flutterAmtR = flutterAmtL;

        const float jitA = 0.004f;
        fltJitL_ = (1.f - jitA) * fltJitL_ + jitA * Frand();
        fltJitR_ = (1.f - jitA) * fltJitR_ + jitA * Frand();

        float tapL = ModulatedDelay(dlyL_,
                                    satL,
                                    baseMsL,
                                    params_.wow,
                                    wowHzL,
                                    flutterAmtL,
                                    fltHzL,
                                    wowPhaseL_,
                                    fltPhaseL_,
                                    driftL_,
                                    fltJitL_,
                                    readStateL_);
        float tapR = ModulatedDelay(dlyR_,
                                    satR,
                                    baseMsR,
                                    params_.wow,
                                    wowHzR,
                                    flutterAmtR,
                                    fltHzR,
                                    wowPhaseR_,
                                    fltPhaseR_,
                                    driftR_,
                                    fltJitR_,
                                    readStateR_);

        azimuth_ += 0.0000015f * Frand();
        azimuth_ = Clamp(azimuth_, -0.00002f, 0.00002f);
        float azCoeff = Clamp(azimuth_, -0.12f, 0.12f);
        tapL = AzimuthAllpass(tapL, azCoeff, zAzL_);
        tapR = AzimuthAllpass(tapR, -azCoeff, zAzR_);

        float dynCutL = std::max(2600.0f, toneBaseHz - 5000.0f * envL);
        float dynCutR = std::max(2600.0f, toneBaseHz - 5000.0f * envR);
        tapeLP_L_.SetFreq(dynCutL);
        tapeLP_R_.SetFreq(dynCutR);

        tapL = tapeLP_L_.Process(tapL);
        tapR = tapeLP_R_.Process(tapR);
        tapL = tapeHP_L_.Process(tapL);
        tapR = tapeHP_R_.Process(tapR);

        bumpL_.Process(tapL);
        bumpR_.Process(tapR);
        tapL += bumpAmt * bumpL_.Band();
        tapR += bumpAmt * bumpR_.Band();

        float crossL = tapL;
        float crossR = tapR;
        tapL = crossL + crossTalk * crossR;
        tapR = crossR + crossTalk * crossL;

        if (hasPrint)
        {
            float ptBackL = printL_[printIdx_];
            float ptBackR = printR_[printIdx_];
            printL_[printIdx_] = tapL;
            printR_[printIdx_] = tapR;
            printIdx_ = (printIdx_ + 1) % printLen_;
            tapL = ptKeep * tapL + ptGain * ptBackL;
            tapR = ptKeep * tapR + ptGain * ptBackR;
        }

        tapL = dcL_.Process(tapL);
        tapR = dcR_.Process(tapR);

        float wetL = tapL;
        float wetR = tapR;
        if (postMix > 0.f)
        {
            float postInL = revHP_L_.Process(tapL);
            float postInR = revHP_R_.Process(tapR);
            float postWetL, postWetR;
            rvb_.Process(postInL, postInR, &postWetL, &postWetR);
            postWetL = revLP_L_.Process(postWetL);
            postWetR = revLP_R_.Process(postWetR);
            float postMixEff = postMix * reverbDuck_;
            wetL = (1.0f - postMixEff) * wetL + postMixEff * postWetL;
            wetR = (1.0f - postMixEff) * wetR + postMixEff * postWetR;
        }

        {
            float scL = scHP_L_.Process(wetL);
            float scR = scHP_R_.Process(wetR);
            float sc = 0.5f * (fabsf(wetL) + fabsf(wetR));
            float scHF = 0.5f * (fabsf(scL) + fabsf(scR));
            float side = 0.6f * sc + 1.2f * scHF;
            float g = comp_.ProcessGain(side);
            wetL *= g;
            wetR *= g;
        }

        if (artifactsActive && hissGain > 1e-6f)
        {
            auto do_crackle = [&](float& sample, uint32_t& ctr, float& hold) {
                if (ctr == 0)
                {
                    float rate = params_.hiss * 4.f;
                    float p = rate / sampleRate_;
                    if (Frand() > (1.f - p))
                    {
                        float amp = hissGain * 1.25f;
                        hold = (Frand() * 0.4f + 0.6f) * amp;
                        ctr = 12 + (uint32_t)(Frand() * 24.f);
                    }
                }
                if (ctr > 0)
                {
                    float t = static_cast<float>(ctr);
                    float blip = hold * (t / (t + 6.f));
                    sample += blip;
                    --ctr;
                }
            };
            do_crackle(wetL, crackleCtrL_, crackleHoldL_);
            do_crackle(wetR, crackleCtrR_, crackleHoldR_);

            auto do_dropout = [&](float& env, uint32_t& hold) {
                float rate = params_.hiss * 1.0f;
                float p = rate / sampleRate_;
                if (hold == 0 && (Frand() > (1.f - p)))
                    hold = 20 + (uint32_t)(Frand() * 60.f);
                float target = (hold > 0) ? (0.55f - 0.25f * params_.hiss + 0.20f * Frand()) : 1.f;
                float c = (target < env) ? 0.006f : 0.0025f;
                env += c * (target - env);
                if (hold > 0)
                    --hold;
            };
            do_dropout(dEnvL_, dHoldL_);
            do_dropout(dEnvR_, dHoldR_);
            wetL *= dEnvL_;
            wetR *= dEnvR_;
        }

        if (hissGain > 0.0f)
        {
            float hissTargetL = Clamp(1.0f - 1.5f * envL, 0.0f, 1.0f);
            float hissTargetR = Clamp(1.0f - 1.5f * envR, 0.0f, 1.0f);
            const float hAtk = 0.0004f;
            const float hRel = 0.0030f;
            hissGateL_ += ((hissTargetL > hissGateL_) ? hAtk : hRel) * (hissTargetL - hissGateL_);
            hissGateR_ += ((hissTargetR > hissGateR_) ? hAtk : hRel) * (hissTargetR - hissGateR_);
            float pinkL = pnoiseL_.Process(Frand());
            float pinkR = pnoiseR_.Process(Frand());
            hissBPF_L_.Process(pinkL);
            hissBPF_R_.Process(pinkR);
            float nL = hissBPF_L_.Band();
            float nR = hissBPF_R_.Band();
            float lowL = hissLow_L_.Process(nL);
            float lowR = hissLow_R_.Process(nR);
            float hiL = nL - lowL;
            float hiR = nR - lowR;
            float midLow = 0.5f * (lowL + lowR);
            float sideHiL = hiL - 0.3f * hiR;
            float sideHiR = hiR - 0.3f * hiL;
            float hissL = midLow + sideHiL;
            float hissR = midLow + sideHiR;
            wetL += hissGain * hissGateL_ * hissL;
            wetR += hissGain * hissGateR_ * hissR;
        }

        outL[i] = dryMix * dryL + wetDry * wetL;
        outR[i] = dryMix * dryR + wetDry * wetR;
    }
}

Hothouse hw;
TapeLabProcessor processor;

struct ParamSmoother
{
    float value = 0.0f;
    float coeff = 0.01f;

    ParamSmoother() = default;
    explicit ParamSmoother(float c) : value(0.0f), coeff(c) {}

    void Init(float v) { value = v; }
    float Process(float target)
    {
        value += coeff * (target - value);
        return value;
    }
};

ParamSmoother smDrive(0.01f);
ParamSmoother smWow(0.01f);
ParamSmoother smFlutter(0.01f);
ParamSmoother smSpeed(0.02f);
ParamSmoother smTone(0.01f);
ParamSmoother smHiss(0.02f);
ParamSmoother smReverb(0.03f);
ParamSmoother smMix(0.05f);
ParamSmoother smHead(0.02f);

bool bypass = false;
Led led1;
Led led2;

float ToggleChoice(Hothouse::Toggleswitch sw, float upValue, float midValue, float downValue)
{
    switch (hw.GetToggleswitchPosition(sw))
    {
        case Hothouse::TOGGLESWITCH_UP:
            return upValue;
        case Hothouse::TOGGLESWITCH_DOWN:
            return downValue;
        case Hothouse::TOGGLESWITCH_MIDDLE:
        default:
            return midValue;
    }
}

void InitializeSmoothers(const TapeParams& initial)
{
    smDrive.Init(initial.drive);
    smWow.Init(initial.wow);
    smFlutter.Init(initial.flutter);
    smSpeed.Init(initial.speed);
    smTone.Init(initial.tone);
    smHiss.Init(initial.hiss);
    smReverb.Init(initial.reverbMix);
    smMix.Init(initial.wetDry);
    smHead.Init(initial.headBump);
}

TapeParams ComputeTargetParams()
{
    TapeParams p;
    float drive = hw.GetKnobValue(Hothouse::KNOB_1);
    float wow = hw.GetKnobValue(Hothouse::KNOB_2);
    float flutter = hw.GetKnobValue(Hothouse::KNOB_3);
    float tone = hw.GetKnobValue(Hothouse::KNOB_4);
    float hiss = hw.GetKnobValue(Hothouse::KNOB_5);
    float mix = hw.GetKnobValue(Hothouse::KNOB_6);

    p.drive = Clamp(drive, 0.0f, 1.0f);
    p.wow = Clamp(wow * wow, 0.0f, 1.0f);
    p.flutter = Clamp(flutter * flutter, 0.0f, 1.0f);
    p.tone = Clamp(tone, 0.0f, 1.0f);
    p.hiss = Clamp(hiss, 0.0f, 1.0f);
    p.wetDry = bypass ? 0.0f : Clamp(mix, 0.0f, 1.0f);

    p.speed = Clamp(ToggleChoice(Hothouse::TOGGLESWITCH_1, 0.32f, 0.55f, 0.78f), 0.0f, 1.0f);
    p.reverbMix = Clamp(ToggleChoice(Hothouse::TOGGLESWITCH_2, 0.18f, 0.22f, 0.28f), 0.0f, 1.0f);
    p.headBump = 0.55f;

    return p;
}

TapeParams SmoothParams(const TapeParams& target)
{
    TapeParams p;
    p.drive = Clamp(smDrive.Process(target.drive), 0.0f, 1.0f);
    p.wow = Clamp(smWow.Process(target.wow), 0.0f, 1.0f);
    p.flutter = Clamp(smFlutter.Process(target.flutter), 0.0f, 1.0f);
    p.speed = Clamp(smSpeed.Process(target.speed), 0.0f, 1.0f);
    p.tone = Clamp(smTone.Process(target.tone), 0.0f, 1.0f);
    p.hiss = Clamp(smHiss.Process(target.hiss), 0.0f, 1.0f);
    p.reverbMix = Clamp(smReverb.Process(target.reverbMix), 0.0f, 1.0f);
    float mix = Clamp(smMix.Process(target.wetDry), 0.0f, 1.0f);
    p.wetDry = bypass ? 0.0f : mix;
    p.headBump = Clamp(smHead.Process(target.headBump), 0.0f, 1.0f);
    return p;
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    hw.ProcessAllControls();

    if (hw.switches[Hothouse::FOOTSWITCH_1].RisingEdge())
    {
        bypass = !bypass;
        if (bypass)
        {
            smMix.Init(0.0f);
        }
    }

    if (hw.switches[Hothouse::FOOTSWITCH_2].RisingEdge())
    {
        auto current = processor.GetTapeShape();
        int next = (static_cast<int>(current) + 1) % 3;
        processor.SetTapeShape(static_cast<TapeLabProcessor::TapeShape>(next));
    }

    switch (hw.GetToggleswitchPosition(Hothouse::TOGGLESWITCH_2))
    {
        case Hothouse::TOGGLESWITCH_UP:
            processor.SetReverbPlacement(TapeLabProcessor::RevPlace::PRE);
            processor.SetReverbPredelaySamples(static_cast<int>(0.018f * hw.AudioSampleRate()));
            processor.SetReverbDampLP(5500.f);
            break;
        case Hothouse::TOGGLESWITCH_MIDDLE:
            processor.SetReverbPlacement(TapeLabProcessor::RevPlace::POST);
            processor.SetReverbPredelaySamples(static_cast<int>(0.022f * hw.AudioSampleRate()));
            processor.SetReverbDampLP(6500.f);
            break;
        case Hothouse::TOGGLESWITCH_DOWN:
            processor.SetReverbPlacement(TapeLabProcessor::RevPlace::DUAL);
            processor.SetReverbPredelaySamples(static_cast<int>(0.028f * hw.AudioSampleRate()));
            processor.SetReverbDampLP(6000.f);
            break;
        default:
            break;
    }

    {
        int mode = 1;
        switch (hw.GetToggleswitchPosition(Hothouse::TOGGLESWITCH_3))
        {
            case Hothouse::TOGGLESWITCH_UP:
                mode = 0;
                break;
            case Hothouse::TOGGLESWITCH_MIDDLE:
                mode = 1;
                break;
            case Hothouse::TOGGLESWITCH_DOWN:
                mode = 2;
                break;
            default:
                break;
        }
        processor.SetArtifactsMode(mode);
    }

    TapeParams target = ComputeTargetParams();
    TapeParams smooth = SmoothParams(target);
    processor.SetParams(smooth);
    processor.Process(in[0], in[1], out[0], out[1], size);

    if (bypass)
    {
        for (size_t i = 0; i < size; ++i)
        {
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
    }
}

int main()
{
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    led1.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    led2.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    TapeParams initial;
    initial.drive = 0.68f;
    initial.wow = 0.22f;
    initial.flutter = 0.18f;
    initial.speed = 0.52f;
    initial.tone = 0.48f;
    initial.hiss = 0.14f;
    initial.reverbMix = 0.20f;
    initial.wetDry = 0.75f;
    initial.headBump = 0.55f;

    processor.Init(hw.AudioSampleRate(), initial);
    InitializeSmoothers(initial);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while (true)
    {
        led1.Set(bypass ? 0.0f : 1.0f);
        led1.Update();

        float led2Level = 0.0f;
        switch (processor.GetTapeShape())
        {
            case TapeLabProcessor::TapeShape::Studio:   led2Level = 0.25f; break;
            case TapeLabProcessor::TapeShape::Vintage:  led2Level = 0.55f; break;
            case TapeLabProcessor::TapeShape::Cassette: led2Level = 1.0f;  break;
        }
        led2.Set(led2Level);
        led2.Update();

        hw.CheckResetToBootloader();
        hw.DelayMs(10);
    }

    return 0;
}
