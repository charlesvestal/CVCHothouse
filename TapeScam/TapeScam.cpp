#include "GainStageModule.h"
#include "daisysp.h"
#include "hothouse.h"

using namespace daisy;
using namespace daisysp;
using clevelandmusicco::Hothouse;

namespace
{
constexpr float kTrimMinDb     = 0.0f;
constexpr float kTrimMaxDb     = 20.0f;
constexpr float kChannelMinDb  = 0.0f;
constexpr float kChannelMaxDb  = 20.0f;
constexpr float kMasterMinDb   = -12.0f;
constexpr float kMasterMaxDb   = 6.0f;
constexpr float kToneMinDb     = -12.0f;
constexpr float kToneMaxDb     = 12.0f;
}

Hothouse         hw;
GainStageModule  gainStage;
Led              ledBypass;
Led              ledBoost;

static bool bypassEnabled = false;
static bool boostEnabled  = false;

inline float MapKnobToRange(float knob, float min, float max)
{
    return fmap(knob, min, max);
}

GainStageModule::Params BuildParams()
{
    GainStageModule::Params params;

    params.trimGainDb    = MapKnobToRange(hw.GetKnobValue(Hothouse::KNOB_1), kTrimMinDb, kTrimMaxDb);
    params.channelGainDb = MapKnobToRange(hw.GetKnobValue(Hothouse::KNOB_2), kChannelMinDb, kChannelMaxDb);
    params.masterVolDb   = MapKnobToRange(hw.GetKnobValue(Hothouse::KNOB_3), kMasterMinDb, kMasterMaxDb);
    params.bassGainDb    = MapKnobToRange(hw.GetKnobValue(Hothouse::KNOB_4), kToneMinDb, kToneMaxDb);
    params.trebleGainDb  = MapKnobToRange(hw.GetKnobValue(Hothouse::KNOB_5), kToneMinDb, kToneMaxDb);
    params.character     = hw.GetKnobValue(Hothouse::KNOB_6);

    params.inputType    = static_cast<int>(hw.GetToggleswitchPosition(Hothouse::TOGGLESWITCH_1));
    params.clippingType = static_cast<int>(hw.GetToggleswitchPosition(Hothouse::TOGGLESWITCH_2));
    params.toneMode     = static_cast<int>(hw.GetToggleswitchPosition(Hothouse::TOGGLESWITCH_3));

    params.bypass       = bypassEnabled;
    params.boostEngage  = boostEnabled;

    return params;
}

void HandleFootswitches()
{
    if(hw.switches[Hothouse::FOOTSWITCH_1].RisingEdge())
    {
        bypassEnabled = !bypassEnabled;
    }

    if(hw.switches[Hothouse::FOOTSWITCH_2].RisingEdge())
    {
        boostEnabled = !boostEnabled;
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
}

int main()
{
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    ledBypass.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    ledBoost.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    gainStage.Init(hw.AudioSampleRate());

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while(true)
    {
        ledBypass.Set(bypassEnabled ? 0.0f : 1.0f);
        ledBypass.Update();

        ledBoost.Set(boostEnabled ? 1.0f : 0.0f);
        ledBoost.Update();

        hw.CheckResetToBootloader();
        hw.DelayMs(10);
    }

    return 0;
}

