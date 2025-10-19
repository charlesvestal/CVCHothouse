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
static int  debugMode     = 0;
static float sampleRateHz = 48000.0f;
static uint32_t fs2HoldSamples = 0;
static uint32_t fs2HoldThreshold = 0;

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
    params.debugMode    = debugMode;

    params.bypass       = bypassEnabled;
    params.boostEngage  = boostEnabled;

    return params;
}

void CycleDebugMode()
{
    debugMode = (debugMode + 1) % 4;
}

void HandleFootswitches(size_t blockSize)
{
    auto& fs1 = hw.switches[Hothouse::FOOTSWITCH_1];
    auto& fs2 = hw.switches[Hothouse::FOOTSWITCH_2];

    if(fs1.RisingEdge())
    {
        bypassEnabled = !bypassEnabled;
    }

    if(fs2.Pressed())
    {
        fs2HoldSamples += blockSize;
    }

    if(fs2.FallingEdge())
    {
        if(fs2HoldSamples >= fs2HoldThreshold)
        {
            CycleDebugMode();
        }
        else
        {
            boostEnabled = !boostEnabled;
        }
        fs2HoldSamples = 0;
    }
    else if(!fs2.Pressed())
    {
        fs2HoldSamples = 0;
    }
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();
    HandleFootswitches(size);

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

    sampleRateHz = hw.AudioSampleRate();
    fs2HoldThreshold = static_cast<uint32_t>(0.6f * sampleRateHz);

    ledBypass.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    ledBoost.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    gainStage.Init(sampleRateHz);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while(true)
    {
        ledBypass.Set(bypassEnabled ? 0.0f : 1.0f);
        ledBypass.Update();

        float led2Level = boostEnabled ? 1.0f : 0.0f;
        if(debugMode != 0)
        {
            led2Level = 0.25f + 0.2f * static_cast<float>(debugMode);
        }
        if(led2Level > 1.0f)
        {
            led2Level = 1.0f;
        }
        ledBoost.Set(led2Level);
        ledBoost.Update();

        hw.CheckResetToBootloader();
        hw.DelayMs(10);
    }

    return 0;
}
