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
int current_preset_ = 0;  // Start with Room
float sample_rate_ = 48000.0f;

// Block size
constexpr size_t kBlockSize = 16;

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

    // Mix dry + wet
    float mix = smMix.value;
    for (size_t i = 0; i < size; ++i) {
        out[0][i] = dry_L[i] * (1.0f - mix) + wet_L[i] * mix;
        out[1][i] = dry_R[i] * (1.0f - mix) + wet_R[i] * mix;
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

    // Initialize mix smoother
    smMix.Init(0.5f);

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

        // Knob 4: Decay time (0.5x to 3.0x feedback, 50% = 1.0x)
        float decay = hw.GetKnobValue(Hothouse::KNOB_4);
        reverb.SetDecayTime(decay);

        // Knob 6: Dry/Wet Mix
        float mix_target = hw.GetKnobValue(Hothouse::KNOB_6);
        smMix.Process(mix_target);

        // Change preset if knob moved significantly
        uint32_t now = System::GetNow();
        if (preset_idx != current_preset_ && (now - last_preset_change) > 500) {
            InitReverb(preset_idx);
            last_preset_change = now;

            // Blink LED2 to indicate preset change
            led2.Set(1.0f);
            led2.Update();
            System::Delay(50);
            led2.Set(0.0f);
            led2.Update();
        }

        // Footswitch: Bypass toggle
        if (hw.switches[Hothouse::FOOTSWITCH_1].RisingEdge()) {
            bypass_ = !bypass_;
        }

        // LED1: Bypass indicator (off when bypassed, on when active)
        led1.Set(bypass_ ? 0.0f : 1.0f);

        // LED2: Mix level indicator
        led2.Set(bypass_ ? 0.0f : smMix.value);

        // Update LED outputs
        led1.Update();
        led2.Update();
    }
}
