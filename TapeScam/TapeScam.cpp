#include "GainStageModule.h"
#include "TapeSatModule.h"
#include "WowFlutterModule.h"
#include "HissDropModule.h"
#include "ToneModule.h"
#include "LoFiCompressor.h"
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

// Tape Speed/Format modes for Toggle Switch 2
enum TapeSpeedMode
{
    SPEED_HIGH     = 0,  // Corresponds to TOGGLESWITCH_UP
    SPEED_STANDARD = 1,  // Corresponds to TOGGLESWITCH_MIDDLE
    SPEED_LOFI     = 2   // Corresponds to TOGGLESWITCH_DOWN
};

// Lo-Fi Compression modes for Toggle Switch 3
enum CompressionMode
{
    COMP_OFF   = 0,  // Corresponds to TOGGLESWITCH_UP
    COMP_LIGHT = 1,  // Corresponds to TOGGLESWITCH_MIDDLE (gentle AGC, hiss breathes)
    COMP_HEAVY = 2   // Corresponds to TOGGLESWITCH_DOWN (aggressive pumping, lots of hiss)
};

Hothouse         hw;
GainStageModule  gainStage;
TapeSatModule    tapeSat;
WowFlutterModule tapeWobble;
HissDropModule   tapeNoise;
ToneModule       tapeTone;
LoFiCompressor   lofiComp;
Led              ledBypass;
Led              ledBoost;

static bool  bypassEnabled = false;
static float sampleRateHz  = 48000.0f;
static float globalLevelTarget = 0.707f;
static float globalLevelCurrent = 0.707f;
static constexpr float kLevelSmooth = 0.01f;

// Toggle state tracking
static TapeAgeMode currentAgeMode = AGE_USED;              // Default to Used tape
static TapeSpeedMode currentSpeedMode = SPEED_STANDARD;    // Default to Standard speed
static CompressionMode currentCompMode = COMP_OFF;         // Default to Compression Off

// Separate parameter adjustments from each toggle (for proper accumulation/combination)
static float ageHeadroomAdj_dB = -1.0f;   // Age mode headroom adjustment
static float speedHeadroomAdj_dB = -1.0f; // Speed mode headroom adjustment
static float ageSaturationMul = 1.0f;     // Age mode saturation multiplier
static float speedSaturationMul = 1.0f;   // Speed mode saturation multiplier

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
        float hissMul, dropoutRateMul, wowDepthMul;

        switch(ageMode)
        {
            case AGE_NEW:
                hissMul            = 0.5f;   // Less hiss
                dropoutRateMul     = 0.5f;   // Fewer dropouts
                wowDepthMul        = 0.8f;   // Less wow/flutter
                ageHeadroomAdj_dB  = 0.0f;   // Full headroom
                ageSaturationMul   = 1.0f;   // Normal saturation
                break;

            case AGE_USED:
                hissMul            = 1.0f;   // Normal hiss
                dropoutRateMul     = 1.0f;   // Normal dropout rate
                wowDepthMul        = 1.0f;   // Normal wow/flutter
                ageHeadroomAdj_dB  = -1.0f;  // Slightly reduced headroom
                ageSaturationMul   = 1.0f;   // Normal saturation
                break;

            case AGE_WORN:
                hissMul            = 1.5f;   // More hiss
                dropoutRateMul     = 1.5f;   // More dropouts
                wowDepthMul        = 1.3f;   // More wow/flutter
                ageHeadroomAdj_dB  = -2.5f;  // Reduced headroom
                ageSaturationMul   = 1.2f;   // Increased saturation
                break;

            default:
                // Should never reach here due to switch above, but safety first
                hissMul            = 1.0f;
                dropoutRateMul     = 1.0f;
                wowDepthMul        = 1.0f;
                ageHeadroomAdj_dB  = 0.0f;
                ageSaturationMul   = 1.0f;
                break;
        }

        // Apply age-specific multipliers to modules
        tapeNoise.SetHissMultiplier(hissMul);
        tapeNoise.SetDropoutRateMultiplier(dropoutRateMul);
        tapeWobble.SetDepthMultiplier(wowDepthMul);

        // Combined headroom and saturation will be applied after both toggles are read
        gainStage.AdjustHeadroom(ageHeadroomAdj_dB + speedHeadroomAdj_dB);
        tapeSat.SetDriveMultiplier(ageSaturationMul * speedSaturationMul);
    }
}

void HandleTapeSpeedToggle()
{
    // Read Toggle Switch 2 position
    auto togglePos = hw.GetToggleswitchPosition(Hothouse::TOGGLESWITCH_2);

    // Map toggle position to TapeSpeedMode
    TapeSpeedMode speedMode = SPEED_STANDARD;  // Safe default
    switch(togglePos)
    {
        case Hothouse::TOGGLESWITCH_UP:
            speedMode = SPEED_HIGH;
            break;
        case Hothouse::TOGGLESWITCH_MIDDLE:
            speedMode = SPEED_STANDARD;
            break;
        case Hothouse::TOGGLESWITCH_DOWN:
            speedMode = SPEED_LOFI;
            break;
        case Hothouse::TOGGLESWITCH_UNKNOWN:
        default:
            speedMode = SPEED_STANDARD;  // Fallback to safe default
            break;
    }

    // Only update parameters if mode changed (reduces overhead)
    if(speedMode != currentSpeedMode)
    {
        currentSpeedMode = speedMode;

        // Define parameters for each tape speed mode
        float hfRollOffCutoff;

        switch(speedMode)
        {
            case SPEED_HIGH:
                hfRollOffCutoff       = 20000.0f;  // Extended HF response (bright!)
                speedHeadroomAdj_dB   = 3.0f;      // Boosted headroom (loud, clean)
                speedSaturationMul    = 0.5f;      // Much less saturation (pristine)
                break;

            case SPEED_STANDARD:
                hfRollOffCutoff       = 14000.0f;  // Normal HF rolloff
                speedHeadroomAdj_dB   = -1.0f;     // Slightly reduced headroom
                speedSaturationMul    = 1.0f;      // Normal saturation (baseline)
                break;

            case SPEED_LOFI:
                hfRollOffCutoff       = 8000.0f;   // Very dark, telephone-like HF
                speedHeadroomAdj_dB   = -6.0f;     // Heavily reduced headroom (crushed)
                speedSaturationMul    = 2.0f;      // Heavy saturation (distorted)
                break;

            default:
                // Should never reach here due to switch above, but safety first
                hfRollOffCutoff       = 14000.0f;
                speedHeadroomAdj_dB   = -1.0f;
                speedSaturationMul    = 1.0f;
                break;
        }

        // Apply speed-specific parameters to modules
        tapeSat.SetHFRolloffCutoff(hfRollOffCutoff);

        // Combined headroom and saturation will be applied after both toggles are read
        gainStage.AdjustHeadroom(ageHeadroomAdj_dB + speedHeadroomAdj_dB);
        tapeSat.SetDriveMultiplier(ageSaturationMul * speedSaturationMul);
    }
}

void HandleCompressionToggle()
{
    // Read Toggle Switch 3 position
    auto togglePos = hw.GetToggleswitchPosition(Hothouse::TOGGLESWITCH_3);

    // Map toggle position to CompressionMode
    CompressionMode compMode = COMP_OFF;  // Safe default
    switch(togglePos)
    {
        case Hothouse::TOGGLESWITCH_UP:
            compMode = COMP_OFF;
            break;
        case Hothouse::TOGGLESWITCH_MIDDLE:
            compMode = COMP_LIGHT;
            break;
        case Hothouse::TOGGLESWITCH_DOWN:
            compMode = COMP_HEAVY;
            break;
        case Hothouse::TOGGLESWITCH_UNKNOWN:
        default:
            compMode = COMP_OFF;
            break;
    }

    // Only update parameters if mode changed
    if(compMode != currentCompMode)
    {
        currentCompMode = compMode;
        lofiComp.SetMode(static_cast<int>(compMode));
    }
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();
    HandleFootswitches();
    HandleTapeAgeToggle();
    HandleTapeSpeedToggle();
    HandleCompressionToggle();

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

    // Update global level control
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
        tapeSat.Process(out, size);
        tapeWobble.Process(out, out, size);
        tapeNoise.Process(out, out, size);
        tapeTone.Process(out, out, size);

        // Lo-Fi compression BEFORE level control (makes hiss breathe dramatically!)
        lofiComp.Process(out[0], out[1], size);

        // Apply global level (trim) after compression
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

    // Initialize USB logging (non-blocking)
    hw.seed.StartLog(false);

    sampleRateHz = hw.AudioSampleRate();

    ledBypass.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    ledBoost.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    gainStage.Init(sampleRateHz);
    tapeSat.Init(sampleRateHz);
    tapeWobble.Init(sampleRateHz);
    tapeWobble.SetAmount(0.0f);
    tapeNoise.Init(sampleRateHz, 2);
    tapeTone.Init(sampleRateHz, 2);
    lofiComp.Init(sampleRateHz);

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
