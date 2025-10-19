#include "GainStageModule.h"
#include "daisysp.h"
#include "hothouse.h"

using namespace daisy;
using namespace daisysp;
using clevelandmusicco::Hothouse;

namespace
{
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
