#pragma once

#include "AsyncFFTConvolver.h"
#include "daisy_seed.h"

/**
 * Simplified Stereo Reverb - FFT Version
 *
 * Uses async FFT convolution:
 * - Audio callback: just I/O (ProcessBlock)
 * - Main loop: FFT convolution when 4096 samples ready
 *
 * Processes in blocks instead of sample-by-sample for efficiency.
 */

class SimplifiedReverb {
public:
    SimplifiedReverb() = default;

    void Init(const float* ir_left, const float* ir_right, size_t ir_len, daisy::DaisySeed* seed = nullptr) {
        if (seed) {
            char buf[64];
            snprintf(buf, sizeof(buf), "SimplifiedReverb: Init with IR length %u", (unsigned int)ir_len);
            seed->PrintLine(buf);
        }

        // Initialize with external SDRAM buffers
        left_.Init(ir_left, ir_len, g_fft_H_L, g_fft_X_hist_L, g_fft_input_L, g_fft_output_L, seed);
        right_.Init(ir_right, ir_len, g_fft_H_R, g_fft_X_hist_R, g_fft_input_R, g_fft_output_R, seed);

        initialized_ = true;

        if (seed) {
            seed->PrintLine("SimplifiedReverb: Init complete");
        }
    }

    void Reset() {
        left_.Reset();
        right_.Reset();
    }

    // Called from AUDIO CALLBACK
    // Lightweight I/O only - no heavy processing
    void ProcessBlock(const float* in_L, const float* in_R,
                     float* out_L, float* out_R,
                     size_t size) {
        if (!initialized_) {
            std::memcpy(out_L, in_L, size * sizeof(float));
            std::memcpy(out_R, in_R, size * sizeof(float));
            return;
        }

        // Just push input and pop output - very fast
        left_.ProcessBlockIO(in_L, out_L, size);
        right_.ProcessBlockIO(in_R, out_R, size);
    }

    // Called from MAIN LOOP
    // Does the actual convolution work
    void ProcessAsync() {
        if (!initialized_) return;

        left_.ProcessAsync();
        right_.ProcessAsync();
    }

    // Diagnostics
    size_t GetInputAvailableL() const { return left_.GetInputPending(); }
    size_t GetInputAvailableR() const { return right_.GetInputPending(); }
    size_t GetOutputAvailableL() const { return left_.GetOutputAvailable(); }
    size_t GetOutputAvailableR() const { return right_.GetOutputAvailable(); }

private:
    AsyncFFTConvolver left_;
    AsyncFFTConvolver right_;
    bool initialized_ = false;
};
