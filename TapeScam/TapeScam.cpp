#include "GainStageModule.h"
#include "TapeSatModule.h"
#include "WowFlutterModule.h"
#include "HissDropModule.h"
#include "ToneModule.h"
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

// Tape Age/Condition modes for Toggle Switch 1
enum TapeAgeMode
{
    AGE_NEW  = 0,  // Corresponds to TOGGLESWITCH_UP
    AGE_USED = 1,  // Corresponds to TOGGLESWITCH_MIDDLE
    AGE_WORN = 2   // Corresponds to TOGGLESWITCH_DOWN
};

Hothouse         hw;
GainStageModule  gainStage;
TapeSatModule    tapeSat;
WowFlutterModule tapeWobble;
HissDropModule   tapeNoise;
ToneModule      tapeTone;
Led              ledBypass;
Led              ledBoost;

static bool  bypassEnabled = false;
static float sampleRateHz  = 48000.0f;
static float globalLevelTarget = 0.707f;
static float globalLevelCurrent = 0.707f;
static constexpr float kLevelSmooth = 0.01f;
static TapeAgeMode currentAgeMode = AGE_USED;  // Default to Used tape

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

void HandleTapeAgeToggle()
{
    // Read Toggle Switch 1 position
    auto togglePos = hw.GetToggleswitchPosition(Hothouse::TOGGLESWITCH_1);

    // Map toggle position to TapeAgeMode
    TapeAgeMode ageMode = AGE_USED;  // Safe default
    switch(togglePos)
    {
        case Hothouse::TOGGLESWITCH_UP:
            ageMode = AGE_NEW;
            break;
        case Hothouse::TOGGLESWITCH_MIDDLE:
            ageMode = AGE_USED;
            break;
        case Hothouse::TOGGLESWITCH_DOWN:
            ageMode = AGE_WORN;
            break;
        case Hothouse::TOGGLESWITCH_UNKNOWN:
        default:
            ageMode = AGE_USED;  // Fallback to safe default
            break;
    }

    // Only update multipliers if mode changed (reduces overhead)
    if(ageMode != currentAgeMode)
    {
        currentAgeMode = ageMode;

        // Define multipliers for each tape age condition
        float hissMul, dropoutRateMul, wowDepthMul, gainHeadroomAdj_dB, satDriveMul;

        switch(ageMode)
        {
            case AGE_NEW:
                hissMul           = 0.5f;   // Less hiss
                dropoutRateMul    = 0.5f;   // Fewer dropouts
                wowDepthMul       = 0.8f;   // Less wow/flutter
                gainHeadroomAdj_dB= 0.0f;   // Full headroom
                satDriveMul       = 1.0f;   // Normal saturation
                break;

            case AGE_USED:
                hissMul           = 1.0f;   // Normal hiss
                dropoutRateMul    = 1.0f;   // Normal dropout rate
                wowDepthMul       = 1.0f;   // Normal wow/flutter
                gainHeadroomAdj_dB= -1.0f;  // Slightly reduced headroom
                satDriveMul       = 1.0f;   // Normal saturation
                break;

            case AGE_WORN:
                hissMul           = 1.5f;   // More hiss
                dropoutRateMul    = 1.5f;   // More dropouts
                wowDepthMul       = 1.3f;   // More wow/flutter
                gainHeadroomAdj_dB= -2.5f;  // Reduced headroom
                satDriveMul       = 1.2f;   // Increased saturation
                break;

            default:
                // Should never reach here due to switch above, but safety first
                hissMul           = 1.0f;
                dropoutRateMul    = 1.0f;
                wowDepthMul       = 1.0f;
                gainHeadroomAdj_dB= 0.0f;
                satDriveMul       = 1.0f;
                break;
        }

        // Apply multipliers to all modules
        tapeNoise.SetHissMultiplier(hissMul);
        tapeNoise.SetDropoutRateMultiplier(dropoutRateMul);
        tapeWobble.SetDepthMultiplier(wowDepthMul);
        gainStage.AdjustHeadroom(gainHeadroomAdj_dB);
        tapeSat.SetDriveMultiplier(satDriveMul);
    }
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();
    HandleFootswitches();
    HandleTapeAgeToggle();

    auto params = BuildParams();
    gainStage.SetPendingParams(params);
    gainStage.UpdateControls();
    gainStage.Process(in, out, size);

    tapeSat.SetDrive(hw.GetKnobValue(Hothouse::KNOB_2));
    tapeSat.UpdateControls();

    tapeWobble.SetAmount(hw.GetKnobValue(Hothouse::KNOB_3));
    tapeWobble.UpdateControls();

    tapeNoise.SetAmount(hw.GetKnobValue(Hothouse::KNOB_4));
    tapeNoise.UpdateControls();

    tapeTone.SetAmount(hw.GetKnobValue(Hothouse::KNOB_5));
    tapeTone.UpdateControls();

    if(!bypassEnabled)
    {
        tapeSat.Process(out, size);
        tapeWobble.Process(out, out, size);
        tapeNoise.Process(out, out, size);
        tapeTone.Process(out, out, size);
    }

    const float levelKnob = hw.GetKnobValue(Hothouse::KNOB_6);
    if(levelKnob <= 0.0005f)
    {
        globalLevelTarget = 0.0f;
    }
    else
    {
        const float minLevelDb = -60.0f;
        const float levelDb   = minLevelDb + levelKnob * (-minLevelDb);
        globalLevelTarget = powf(10.0f, levelDb / 20.0f);
    }

    globalLevelCurrent += (globalLevelTarget - globalLevelCurrent) * kLevelSmooth;

    if(!bypassEnabled)
    {
        for(size_t i = 0; i < size; ++i)
        {
            out[0][i] = ClampLevel(out[0][i] * globalLevelCurrent, -1.0f, 1.0f);
            out[1][i] = ClampLevel(out[1][i] * globalLevelCurrent, -1.0f, 1.0f);
        }
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
    tapeWobble.Init(sampleRateHz);
    tapeWobble.SetAmount(0.0f);
    tapeNoise.Init(sampleRateHz, 2);
    tapeTone.Init(sampleRateHz, 2);

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
