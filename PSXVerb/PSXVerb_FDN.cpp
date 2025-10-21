#include "PsxFDNReverb.h"
#include "FDNProfiles.h"
#include "hothouse.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;
using clevelandmusicco::Hothouse;

/**
 * PSXVerb - Pure FDN Algorithmic Reverb
 *
 * Architecture:
 * - 8 early reflection taps (extracted from IR analysis)
 * - 8 parallel comb filters with HF damping
 * - 4 series allpass filters for diffusion
 * - No convolution - pure algorithmic like PSX SPU
 *
 * All parameters extracted from PS1 IR measurements.
 */

// Hardware
Hothouse hw;
Led led1;
Led led2;

// DSP engine - allocate in SDRAM
__attribute__((section(".sdram_bss"))) PsxFDNReverb reverb;

// State
bool bypass_ = true;
bool reverb_ready_ = false;
int current_ir_index_ = 0;
float sample_rate_ = 48000.0f;

// Block size
constexpr size_t kBlockSize = 16;

// Parameter smoothing
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

// Audio callback counter
static volatile uint32_t audio_callback_count = 0;

// Audio callback
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    audio_callback_count++;

    // Always copy dry (zero latency)
    float dry_L[kBlockSize];
    float dry_R[kBlockSize];

    for (size_t i = 0; i < size; ++i) {
        dry_L[i] = in[0][i];
        dry_R[i] = in[1][i];
    }

    if (!reverb_ready_ || bypass_) {
        // Bypass - just output dry
        for (size_t i = 0; i < size; ++i) {
            out[0][i] = dry_L[i];
            out[1][i] = dry_R[i];
        }
        return;
    }

    // Process FDN reverb
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

void InitReverb(int ir_index) {
    char buf[64];

    reverb_ready_ = false;
    current_ir_index_ = ir_index;

    snprintf(buf, sizeof(buf), "Loading IR %d...", ir_index);
    hw.seed.PrintLine(buf);

    // Configure FDN based on IR index
    using namespace FDNProfiles;

    const float* early_delays;
    const float* early_gains;
    const float* comb_delays;
    const float* comb_gains;
    float lpf_alpha;

    switch (ir_index) {
        case 0: // Studio
            early_delays = kStudio_EarlyDelays;
            early_gains = kStudio_EarlyGains;
            comb_delays = kStudio_CombDelays;
            comb_gains = kStudio_CombGains;
            lpf_alpha = kStudio_LPFAlpha;
            hw.seed.PrintLine("  IR 0: Studio");
            break;

        case 1: // Church
            early_delays = kChurch_EarlyDelays;
            early_gains = kChurch_EarlyGains;
            comb_delays = kChurch_CombDelays;
            comb_gains = kChurch_CombGains;
            lpf_alpha = kChurch_LPFAlpha;
            hw.seed.PrintLine("  IR 1: Church");
            break;

        case 2: // Dome
            early_delays = kDome_EarlyDelays;
            early_gains = kDome_EarlyGains;
            comb_delays = kDome_CombDelays;
            comb_gains = kDome_CombGains;
            lpf_alpha = kDome_LPFAlpha;
            hw.seed.PrintLine("  IR 2: Dome");
            break;

        case 3: // Hall
        default:
            early_delays = kHall_EarlyDelays;
            early_gains = kHall_EarlyGains;
            comb_delays = kHall_CombDelays;
            comb_gains = kHall_CombGains;
            lpf_alpha = kHall_LPFAlpha;
            hw.seed.PrintLine("  IR 3: Hall");
            break;
    }

    reverb.Configure(
        early_delays, early_gains, kNumEarlyTaps,
        comb_delays, comb_gains, kNumCombs,
        lpf_alpha,
        kAllpassDelays, kAllpassCoeffs, kNumAllpass
    );

    hw.seed.PrintLine("  FDN configured:");
    snprintf(buf, sizeof(buf), "    %d early taps", (int)kNumEarlyTaps);
    hw.seed.PrintLine(buf);
    snprintf(buf, sizeof(buf), "    %d combs, %d allpass", (int)kNumCombs, (int)kNumAllpass);
    hw.seed.PrintLine(buf);

    reverb_ready_ = true;
    hw.seed.PrintLine("Ready!");
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(kBlockSize);

    sample_rate_ = hw.seed.AudioSampleRate();

    led1.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    led2.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    smMix.Init(0.5f);

    // Print startup banner
    hw.seed.PrintLine("==================================");
    hw.seed.PrintLine("PSXVerb - Pure FDN Reverb");
    hw.seed.PrintLine("==================================");
    char buf[64];
    snprintf(buf, sizeof(buf), "Sample rate: %.0f Hz", sample_rate_);
    hw.seed.PrintLine(buf);
    snprintf(buf, sizeof(buf), "Block size: %d samples", kBlockSize);
    hw.seed.PrintLine(buf);

    // Initialize reverb engine
    reverb.Init(sample_rate_);

    System::Delay(500);

    // Load initial IR (Dome - IR 2)
    InitReverb(2);

    // Start audio
    hw.seed.StartAudio(AudioCallback);

    // Main loop
    uint32_t last_button_check = 0;
    uint32_t last_callback = audio_callback_count;

    while (true) {
        uint32_t now = System::GetNow();

        // Check button every 50ms (debounce)
        if (now - last_button_check > 50) {
            last_button_check = now;

            bool button_pressed = hw.switches[Hothouse::FOOTSWITCH_1].RisingEdge();

            // Toggle bypass on button press
            if (button_pressed) {
                bypass_ = !bypass_;
                led1.Set(bypass_ ? 0.0f : 1.0f);

                if (bypass_) {
                    hw.seed.PrintLine("BYPASS ON");
                } else {
                    hw.seed.PrintLine("BYPASS OFF");
                }
            }
        }

        // Check for IR change via KNOB_1 and update mix (every 100ms)
        static uint32_t last_knob_check = 0;
        if (now - last_knob_check > 100) {
            last_knob_check = now;

            float knob = hw.GetKnobValue(Hothouse::KNOB_1);
            int new_ir = static_cast<int>(knob * 4.0f);
            if (new_ir > 3) new_ir = 3;

            if (new_ir != current_ir_index_) {
                InitReverb(new_ir);
            }

            // Update mix
            float mix_target = bypass_ ? 0.0f : hw.GetKnobValue(Hothouse::KNOB_6);
            smMix.Process(mix_target);
        }

        // Audio watchdog
        if (audio_callback_count == last_callback) {
            led2.Set(1.0f);  // Red = audio stalled
        } else {
            led2.Set(0.0f);  // Off = audio running
            last_callback = audio_callback_count;
        }

        led1.Update();
        led2.Update();

        System::Delay(1);
    }
}
