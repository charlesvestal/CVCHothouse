#include "daisysp.h"
#include "hothouse.h"
#include <algorithm>
#include <cmath>

using namespace daisy;
using namespace daisysp;
using clevelandmusicco::Hothouse;

/**
 * PS1-Style Reverb using DaisySP ReverbSc
 *
 * ARCHITECTURE:
 * - Uses ReverbSc (Schroeder/Chowning algorithmic reverb)
 * - Much more CPU-efficient than convolution
 * - Tuned to sound like PS1 game reverb
 *
 * BLOCK SIZE: 128 samples (2.67ms @ 48kHz)
 * FOOTSWITCH: Bypass toggle with tail
 * KNOB 6: Wet/Dry Mix
 */

namespace {

inline float Clamp(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace

// Global hardware and state
Hothouse hw;
ReverbSc verb;

Led led1;
Led led2;

// State
bool bypass_ = true;
bool tail_active_ = false;
float sample_rate_ = 48000.0f;

// Block size
constexpr size_t kBlockSize = 128;

// Parameter smoothing
struct ParamSmoother
{
    float value = 0.0f;
    float coeff = 0.02f;

    ParamSmoother() = default;
    explicit ParamSmoother(float c) : coeff(c) {}

    void Init(float v) { value = v; }
    float Process(float target)
    {
        value += coeff * (target - value);
        return value;
    }
};

ParamSmoother smMix(0.05f);
ParamSmoother smFeedback(0.01f);
ParamSmoother smLpf(0.01f);

// Audio callback counter
static volatile uint32_t audio_callback_count = 0;

// Audio callback
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    audio_callback_count++;

    for (size_t i = 0; i < size; ++i)
    {
        float dry_l = in[0][i];
        float dry_r = in[1][i];
        float wet_l, wet_r;

        if (!bypass_)
        {
            // Active reverb
            verb.Process(dry_l, dry_r, &wet_l, &wet_r);
            float mix = smMix.value;
            out[0][i] = dry_l * (1.0f - mix) + wet_l * mix;
            out[1][i] = dry_r * (1.0f - mix) + wet_r * mix;
        }
        else if (tail_active_)
        {
            // Tail mode - process zeros to let reverb decay
            verb.Process(0.0f, 0.0f, &wet_l, &wet_r);
            out[0][i] = dry_l + wet_l;
            out[1][i] = dry_r + wet_r;

            // Check if tail has ended
            if (fabsf(wet_l) < 0.0001f && fabsf(wet_r) < 0.0001f)
            {
                tail_active_ = false;
            }
        }
        else
        {
            // Bypassed
            out[0][i] = dry_l;
            out[1][i] = dry_r;
        }
    }
}

int main()
{
    // Hardware initialization
    hw.Init();
    hw.SetAudioBlockSize(kBlockSize);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    sample_rate_ = hw.AudioSampleRate();

    // Initialize USB logging
    hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);
    System::Delay(250);
    hw.seed.StartLog(false);

    // Blink LED1 while waiting for USB
    led1.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    for (int i = 0; i < 50; ++i) {
        led1.Set((i % 2) ? 1.0f : 0.0f);
        led1.Update();
        System::Delay(100);
    }

    hw.seed.PrintLine("=== PSXVerb ReverbSc ===");
    hw.seed.PrintLine("Initializing ReverbSc...");

    // Initialize ReverbSc
    verb.Init(sample_rate_);

    // PS1-style settings (warm, dense, moderate decay)
    verb.SetFeedback(0.85f);   // Medium-long decay
    verb.SetLpFreq(8000.0f);   // Warm tone (roll off highs)

    hw.seed.PrintLine("ReverbSc initialized");

    // LED initialization
    led1.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    led2.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    // Initialize parameter smoothers
    smMix.Init(0.5f);
    smFeedback.Init(0.85f);
    smLpf.Init(8000.0f);

    hw.seed.PrintLine("Starting audio...");

    // Start audio
    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    hw.seed.PrintLine("Audio started - ready!");
    hw.seed.PrintLine("Press footswitch to toggle bypass");

    // Main loop
    uint32_t loop_count = 0;
    uint32_t last_audio_count = 0;
    uint32_t last_log_time = System::GetNow();

    while (true)
    {
        loop_count++;

        uint32_t now = System::GetNow();

        // Process controls
        hw.ProcessAllControls();

        // Handle footswitch (bypass toggle with tail)
        if (hw.switches[Hothouse::FOOTSWITCH_1].RisingEdge())
        {
            bypass_ = !bypass_;
            if (bypass_)
            {
                tail_active_ = true;  // Enable tail when bypassing
                smMix.Init(0.0f);      // Fade out
            }
        }

        // Handle knob changes (rate limited)
        static uint32_t last_knob_check = 0;
        if (now - last_knob_check >= 10)
        {
            // Knob 1: Feedback (decay time)
            float feedback_target = hw.GetKnobValue(Hothouse::KNOB_1) * 0.35f + 0.6f;  // 0.6-0.95
            float feedback = smFeedback.Process(feedback_target);
            verb.SetFeedback(feedback);

            // Knob 2: Low-pass filter (tone)
            float lpf_target = hw.GetKnobValue(Hothouse::KNOB_2) * 15000.0f + 2000.0f;  // 2K-17K Hz
            float lpf = smLpf.Process(lpf_target);
            verb.SetLpFreq(lpf);

            // Knob 6: Wet/Dry Mix
            float mix_target = bypass_ ? 0.0f : hw.GetKnobValue(Hothouse::KNOB_6);
            smMix.Process(mix_target);

            last_knob_check = now;
        }

        // Log every second
        if (now - last_log_time >= 1000) {
            char buf[128];
            uint32_t current_audio_count = audio_callback_count;
            snprintf(buf, sizeof(buf), "Loop: %lu | Audio: %lu (Δ%lu) | Bypass: %s | Tail: %s",
                     loop_count, current_audio_count, current_audio_count - last_audio_count,
                     bypass_ ? "ON" : "OFF",
                     tail_active_ ? "ACTIVE" : "OFF");
            hw.seed.PrintLine(buf);
            last_audio_count = current_audio_count;
            last_log_time = now;
        }

        // LED 1: Bypass indicator (on = active, off = bypassed)
        led1.Set(bypass_ ? 0.0f : 1.0f);
        led1.Update();

        // LED 2: Mix level
        led2.Set(smMix.value);
        led2.Update();

        hw.CheckResetToBootloader();
    }

    return 0;
}
