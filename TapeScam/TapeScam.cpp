#include "GainStageModule.h"
#include "TapeSatModule.h"
#include <algorithm>
#include <cmath>
#include "daisysp.h"
#include "hothouse.h"

using namespace daisy;
using namespace daisysp;
using clevelandmusicco::Hothouse;

inline float ClampLevel(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

Hothouse         hw;
GainStageModule  gainStage;
TapeSatModule    tapeSat;
Led              ledBypass;
Led              ledBoost;

static bool  bypassEnabled = false;
static float sampleRateHz  = 48000.0f;
static float globalLevelTarget = 0.707f;
static float globalLevelCurrent = 0.707f;
static constexpr float kLevelSmooth = 0.01f;

GainStageModule::Params BuildParams()
{
    GainStageModule::Params params{};

    const float drive = hw.GetKnobValue(Hothouse::KNOB_1);
    params.driveNorm     = drive;
    params.trimGainDb    = 0.0f;
    params.channelGainDb = 0.0f;
    params.masterVolDb   = 0.0f;
    params.bassGainDb    = 0.0f;
    params.trebleGainDb  = 0.0f;
    params.character     = drive;

    params.inputType    = 0;
    params.clippingType = 0;
    params.toneMode     = 0;
    params.debugMode    = 0;

    params.bypass       = bypassEnabled;
    params.boostEngage  = false;

    return params;
}

void HandleFootswitches()
{
    if(hw.switches[Hothouse::FOOTSWITCH_1].RisingEdge())
    {
        bypassEnabled = !bypassEnabled;
    }
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();
    HandleFootswitches();

    auto params = BuildParams();
    gainStage.SetPendingParams(params);
    gainStage.UpdateControls();
    gainStage.Process(in, out, size);

    tapeSat.SetDrive(hw.GetKnobValue(Hothouse::KNOB_2));
    tapeSat.UpdateControls();

    if(!bypassEnabled)
    {
        tapeSat.Process(out, size);
    }

    const float levelKnob = hw.GetKnobValue(Hothouse::KNOB_6);
    const float levelDb   = -12.0f + levelKnob * 18.0f;
    globalLevelTarget = powf(10.0f, levelDb / 20.0f);
    globalLevelCurrent += (globalLevelTarget - globalLevelCurrent) * kLevelSmooth;

    for(size_t i = 0; i < size; ++i)
    {
        out[0][i] = ClampLevel(out[0][i] * globalLevelCurrent, -1.0f, 1.0f);
        out[1][i] = ClampLevel(out[1][i] * globalLevelCurrent, -1.0f, 1.0f);
    }
}

int main()
{
    hw.Init();
    hw.SetAudioBlockSize(4);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    sampleRateHz = hw.AudioSampleRate();

    ledBypass.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    ledBoost.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    gainStage.Init(sampleRateHz);
    tapeSat.Init(sampleRateHz);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while(true)
    {
        ledBypass.Set(bypassEnabled ? 0.0f : 1.0f);
        ledBypass.Update();

        ledBoost.Set(0.0f);
        ledBoost.Update();

        hw.CheckResetToBootloader();
        hw.DelayMs(10);
    }

    return 0;
}
