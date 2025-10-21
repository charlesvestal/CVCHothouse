#include "PartitionedReverb.h"  // Back to partitioned convolution
#include "IRData.h"
#include "daisysp.h"
#include "hothouse.h"
#include <algorithm>
#include <cmath>

using namespace daisy;
using namespace daisysp;
using clevelandmusicco::Hothouse;

/**
 * PS1 Impulse Response Reverb - Async Architecture
 *
 * ARCHITECTURE:
 * - Audio callback: Lightweight I/O only (push input, pop output)
 * - Main loop: Heavy convolution processing (async, non-real-time)
 * - Communication via lock-free ring buffers
 *
 * This separates real-time I/O from processing, eliminating the
 * "spiky computation cost" problem in traditional partitioned convolution.
 *
 * BLOCK SIZE: 128 samples (2.67ms @ 48kHz)
 * FOOTSWITCH: Bypass toggle
 * LED 1: Bypass state (off = bypassed, on = active)
 * LED 2: IR selection indicator (brightness = IR index / 3)
 */

namespace {

inline float Clamp(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace

// Global hardware and state
Hothouse hw;
PartitionedReverb reverb_L_;
PartitionedReverb reverb_R_;

Led led1;
Led led2;

// State
bool bypass_ = true;  // START BYPASSED for safe initialization
bool reverb_ready_ = false;  // Flag when IR loading is done
int current_ir_index_ = 0;
float sample_rate_ = 48000.0f;
float mix_ = 1.0f;

// Block size (128 for maximum CPU headroom with 2048-sample tail partitions)
// 128 samples = 2.67ms latency @ 48kHz (acceptable for reverb)
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

// Initialize reverb with selected IR
void InitReverb(int ir_index)
{
    hw.seed.PrintLine("  InitReverb: Starting");

    ir_index = Clamp(static_cast<float>(ir_index), 0.0f, static_cast<float>(IRData::kNumIRs - 1));
    current_ir_index_ = ir_index;

    hw.seed.PrintLine("  InitReverb: Getting IR data");
    const auto& ir = IRData::kAllIRs[ir_index];

    char buf[64];
    snprintf(buf, sizeof(buf), "  IR length: %u samples (~%.2f sec)",
             (unsigned int)ir.length, (float)ir.length / sample_rate_);
    hw.seed.PrintLine(buf);

    hw.seed.PrintLine("  InitReverb: Init LEFT channel");
    reverb_L_.Init(ir.left, ir.length, 0, &hw.seed);

    hw.seed.PrintLine("  InitReverb: Init RIGHT channel");
    reverb_R_.Init(ir.right, ir.length, 1, &hw.seed);

    hw.seed.PrintLine("  InitReverb: Complete");
}

// Map knob value to IR index (0-3)
int MapKnobToIR(float knob_value)
{
    int index = static_cast<int>(knob_value * 3.999f);
    return Clamp(static_cast<float>(index), 0.0f, 3.0f);
}

// Audio callback counter for crash detection
static volatile uint32_t audio_callback_count = 0;
static volatile bool hw_ready_for_controls = false;

// Audio callback - processes reverb
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    audio_callback_count++;

    // Pass through if reverb not ready or bypassed
    if (!reverb_ready_ || bypass_)
    {
        for (size_t i = 0; i < size; ++i) {
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
        return;
    }

    // Reverb processing
    reverb_L_.ProcessBlock128(in[0], out[0]);
    reverb_R_.ProcessBlock128(in[1], out[1]);
}

int main()
{
    // Hardware initialization
    hw.Init();
    hw.SetAudioBlockSize(kBlockSize);  // 16 samples (0.33ms @ 48kHz)
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    sample_rate_ = hw.AudioSampleRate();

    // Initialize USB logging FIRST and WAIT for connection
    hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);
    System::Delay(250);
    hw.seed.StartLog(false);

    // Blink LED1 while waiting for USB serial connection
    led1.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    for (int i = 0; i < 50; ++i) {  // Wait up to 5 seconds
        led1.Set((i % 2) ? 1.0f : 0.0f);
        led1.Update();
        System::Delay(100);
    }

    hw.seed.PrintLine("=== PSXVerb Partitioned Reverb ===");
    hw.seed.PrintLine("Step 1: Hardware initialized");

    // LED initialization
    led1.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    led2.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    hw.seed.PrintLine("Step 2: LEDs initialized");

    // Initialize parameter smoothers
    smMix.Init(0.5f);

    hw.seed.PrintLine("Step 3: Starting audio (bypass mode)");

    // START AUDIO FIRST (in bypass mode, audio callback passes through)
    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    hw.seed.PrintLine("Step 4: Audio started");

    // REMOVED: LED2 blink loop - causes crash due to System::Delay() conflict with audio callback
    // The main loop will handle LED updates safely

    hw.seed.PrintLine("Step 5: Skipping IR init for now");

    // Mark reverb as NOT ready yet (will init in main loop)
    reverb_ready_ = false;
    bypass_ = false;

    hw.seed.PrintLine("Step 6: Entering main loop");

    // Main loop (LED updates, mode indication)
    uint32_t loop_count = 0;
    uint32_t last_audio_count = 0;
    uint32_t last_log_time = System::GetNow();

    hw.seed.PrintLine("Step 8: About to enter while loop");

    // Flag to track if we've initialized IR yet
    bool ir_initialized = false;

    while (true)
    {
        if (loop_count == 0) {
            hw.seed.PrintLine("Step 9a: First loop iteration");
        }

        loop_count++;

        // Initialize IR after main loop has run for a bit
        if (!ir_initialized && loop_count == 1000) {
            hw.seed.PrintLine("Step 9b: NOW initializing IR (main loop stable)");
            InitReverb(0);
            reverb_ready_ = true;
            bypass_ = true;  // START IN BYPASS MODE - use footswitch to enable
            ir_initialized = true;
            hw.seed.PrintLine("Step 9c: IR initialization complete!");
            hw.seed.PrintLine("Step 9d: Started in BYPASS mode (press footswitch to enable reverb)");
        }

        // Get current time for all control processing
        uint32_t now = System::GetNow();

        // Process controls in main loop (NOT in audio callback)
        hw.ProcessAllControls();

        // Handle footswitch (bypass toggle)
        if (hw.switches[Hothouse::FOOTSWITCH_1].RisingEdge())
        {
            bypass_ = !bypass_;
        }

        // Handle knob changes (with rate limiting - only check every 10ms)
        static uint32_t last_knob_check = 0;
        if (now - last_knob_check >= 10)
        {
            // Knob 1: IR Selection (0-3)
            // DISABLED FOR NOW - changing IR while running causes instability
            // int ir_index = MapKnobToIR(hw.GetKnobValue(Hothouse::KNOB_1));
            // if (ir_index != current_ir_index_)
            // {
            //     InitReverb(ir_index);
            // }

            // Knob 6: Wet/Dry Mix (currently not used - always 100% wet)
            // Future: could blend dry signal in audio callback
            mix_ = hw.GetKnobValue(Hothouse::KNOB_6);

            last_knob_check = now;
        }

        // Log every second
        if (now - last_log_time >= 1000) {
            char buf[128];
            uint32_t current_audio_count = audio_callback_count;
            snprintf(buf, sizeof(buf), "Loop: %lu | Audio: %lu (Δ%lu)",
                     loop_count, current_audio_count, current_audio_count - last_audio_count);
            hw.seed.PrintLine(buf);
            last_audio_count = current_audio_count;
            last_log_time = now;
        }

        // LED 1: Bypass indicator (on = active, off = bypassed)
        led1.Set(bypass_ ? 0.0f : 1.0f);
        led1.Update();

        // LED 2: IR selection (brightness = IR index / 3)
        // 0=off, 1=dim, 2=medium, 3=bright
        float led2_level = static_cast<float>(current_ir_index_) / 3.0f;
        led2.Set(led2_level);
        led2.Update();

        hw.CheckResetToBootloader();

        // REMOVED: hw.DelayMs(10) - this was causing crashes
        // Main loop runs as fast as possible; audio callback provides timing
    }

    return 0;
}
