// PSXVerb_SPU.cpp - Authentic PSX SPU Reverb for Daisy Seed
// Zero-latency dry path, 24kHz internal processing with halfband resampling

#include "PsxReverb.h"
#include "PsxPreset.h"
#include "hothouse.h"
#include "daisysp.h"
#include <cstring>

using namespace daisy;
using namespace daisysp;
using clevelandmusicco::Hothouse;

// Hardware
Hothouse hw;
Led led1;
Led led2;

// DSP
PsxReverb reverb;

// State
bool bypass_ = true;
bool prev_bypass_ = true;  // Track bypass state changes
bool clear_on_bypass_ = false;  // Switch 2: Clear buffer when bypassing
int current_preset_ = 0;  // Start with Room
float sample_rate_ = 48000.0f;

// Block size (reduced to 4 to minimize digital artifacts/whine)
constexpr size_t kBlockSize = 4;

// Mix parameter with smoothing
struct ParamSmoother {
    float value = 0.0f;
    float coeff = 0.05f;

    void Init(float v) { value = v; }
    float Process(float target) {
        value += coeff * (target - value);
        return value;
    }
};

ParamSmoother smMix;
ParamSmoother smGain;

// Audio callback - ZERO LATENCY DRY PATH
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    // Always copy dry immediately (zero latency)
    float dry_L[kBlockSize];
    float dry_R[kBlockSize];

    for (size_t i = 0; i < size; ++i) {
        dry_L[i] = in[0][i];
        dry_R[i] = in[1][i];
    }

    if (bypass_) {
        // Pure bypass - just output dry
        for (size_t i = 0; i < size; ++i) {
            out[0][i] = dry_L[i];
            out[1][i] = dry_R[i];
        }
        return;
    }

    // Process reverb
    float wet_L[kBlockSize];
    float wet_R[kBlockSize];

    reverb.ProcessBlock(in[0], in[1], wet_L, wet_R, size);

    // Mix dry + wet, then apply overall gain
    float mix = smMix.value;
    float gain = smGain.value;
    for (size_t i = 0; i < size; ++i) {
        out[0][i] = (dry_L[i] * (1.0f - mix) + wet_L[i] * mix) * gain;
        out[1][i] = (dry_R[i] * (1.0f - mix) + wet_R[i] * mix) * gain;
    }
}

void InitReverb(int preset_index) {
    if (preset_index < 0 || preset_index >= PsxPresets::kNumPresets) {
        preset_index = 0;
    }
    current_preset_ = preset_index;

    const PsxPreset* preset = PsxPresets::kAllPresets[preset_index];
    reverb.Init(sample_rate_, *preset);
}

int main()
{
    // Initialize hardware
    hw.Init();
    hw.SetAudioBlockSize(kBlockSize);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    sample_rate_ = hw.AudioSampleRate();

    // Initialize LEDs (active low = true for Hothouse)
    led1.Init(hw.seed.GetPin(Hothouse::LED_1), true);
    led2.Init(hw.seed.GetPin(Hothouse::LED_2), true);

    // Initialize parameter smoothers
    smMix.Init(0.5f);
    smGain.Init(1.0f);  // Default to unity gain

    // Enable flush-to-zero and denormals-are-zero
    #ifdef __ARM_ARCH
    uint32_t fpcr;
    asm volatile("vmrs %0, fpscr" : "=r" (fpcr));
    fpcr |= (1 << 24);  // FZ bit
    fpcr |= (1 << 19);  // DN bit
    asm volatile("vmsr fpscr, %0" : : "r" (fpcr));
    #endif

    // Initialize reverb with first preset
    InitReverb(0);  // Room preset

    // Start ADC and audio
    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    // Main loop - handle controls
    uint32_t last_preset_change = 0;

    while(1) {
        System::Delay(10);

        // Check for DFU bootloader entry (hold FS1 for 1 second)
        hw.CheckResetToBootloader();

        // Read ADC values
        hw.ProcessAnalogControls();
        hw.ProcessDigitalControls();

        // Knob 1: Preset selection (6 presets)
        float preset_knob = hw.GetKnobValue(Hothouse::KNOB_1);
        int preset_idx = static_cast<int>(preset_knob * (PsxPresets::kNumPresets - 0.01f));

        // Knob 2: Input gain (0x to 2x, 50% = 1x unity)
        float input_gain = hw.GetKnobValue(Hothouse::KNOB_2);
        reverb.SetInputGain(input_gain);

        // Knob 3: Overall output gain (0x to 2x, 50% = 1x unity)
        // Only affects output when reverb is enabled, not in bypass
        float gain_target = hw.GetKnobValue(Hothouse::KNOB_3);
        smGain.Process(gain_target * 2.0f);  // 0x to 2x

        // Knob 4: Decay time (0.5x to 1.0x, 100% = authentic)
        float decay = hw.GetKnobValue(Hothouse::KNOB_4);
        reverb.SetDecayTime(decay);

        // Knob 5: Reverb level (0x to 4x, 50% = 2x unity)
        float level = hw.GetKnobValue(Hothouse::KNOB_5);
        reverb.SetReverbLevel(level);

        // Knob 6: Dry/Wet Mix
        float mix_target = hw.GetKnobValue(Hothouse::KNOB_6);
        smMix.Process(mix_target);

        // Change preset if knob moved significantly
        uint32_t now = System::GetNow();
        if (preset_idx != current_preset_ && (now - last_preset_change) > 500) {
            InitReverb(preset_idx);
            last_preset_change = now;

            // Flash LED1 N times to indicate preset number (1-6)
            for (int i = 0; i < preset_idx + 1; i++) {
                led1.Set(0.0f);  // ON (active low)
                led1.Update();
                System::Delay(150);
                led1.Set(1.0f);  // OFF (active low)
                led1.Update();
                System::Delay(150);
            }
        }

        // Read toggle switches
        // Switch 2 down = clear buffer on bypass
        clear_on_bypass_ = (hw.GetToggleswitchPosition(Hothouse::TOGGLESWITCH_2) == Hothouse::TOGGLESWITCH_DOWN);

        // Footswitch: Bypass toggle
        if (hw.switches[Hothouse::FOOTSWITCH_1].RisingEdge()) {
            bypass_ = !bypass_;
        }

        // If entering bypass and clear mode is enabled, reset reverb
        if (bypass_ && !prev_bypass_ && clear_on_bypass_) {
            reverb.Reset();
        }
        prev_bypass_ = bypass_;

        // LED1: Active indicator (on when reverb enabled, off when bypassed)
        // Note: LEDs are active_low, so invert the logic
        led1.Set(bypass_ ? 1.0f : 0.0f);

        // LED2: Mix level indicator (brightness tracks K6)
        // Note: LEDs are active_low, so 0 = bright, 1 = off
        // We want: K6=0% → LED off, K6=100% → LED bright
        led2.Set(1.0f - smMix.value);

        // Update LED outputs
        led1.Update();
        led2.Update();
    }
}
