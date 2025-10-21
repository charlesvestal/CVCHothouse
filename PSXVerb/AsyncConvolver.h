#pragma once

#include <cstddef>
#include <cstring>
#include <atomic>
#include <complex>
#include "MinimalFFT.h"
#include "daisy_seed.h"

using namespace daisy;

/**
 * Async Convolution Engine
 *
 * ARCHITECTURE:
 * - Audio callback: lightweight I/O only (pushes input, pops output)
 * - Main loop: heavy convolution processing
 * - Communication via lock-free ring buffers
 *
 * This separates real-time I/O from non-real-time processing,
 * preventing the "spiky computation cost" problem in partitioned convolution.
 */

// SDRAM storage for large buffers (2 channels)
// Each channel needs: ir[4096] + delay[4096] + in[16384] + out[16384] ~= 165KB
// Total for stereo: ~330KB in SDRAM
DSY_SDRAM_BSS float g_async_ir_L[4096];
DSY_SDRAM_BSS float g_async_ir_R[4096];
DSY_SDRAM_BSS float g_async_delay_L[4096];
DSY_SDRAM_BSS float g_async_delay_R[4096];
DSY_SDRAM_BSS float g_async_input_L[16384];
DSY_SDRAM_BSS float g_async_input_R[16384];
DSY_SDRAM_BSS float g_async_output_L[16384];
DSY_SDRAM_BSS float g_async_output_R[16384];

class AsyncConvolver {
public:
    static constexpr size_t PARTITION_SIZE = 4096;   // Process 4096 samples at a time
    static constexpr size_t FFT_SIZE = 8192;         // Zero-padded FFT size
    static constexpr size_t MAX_IR_LEN = 4096;       // Limit to 4096 taps (~85ms @ 48kHz)
    static constexpr size_t BUFFER_SIZE = 16384;     // Ring buffer size (must be power of 2)

    AsyncConvolver() = default;

    void Init(const float* ir, size_t ir_len,
              float* ir_buf, float* delay_buf,
              float* input_buf, float* output_buf) {
        ir_len_ = (ir_len > MAX_IR_LEN) ? MAX_IR_LEN : ir_len;

        // Use external SDRAM buffers
        ir_ = ir_buf;
        delay_line_ = delay_buf;
        input_buffer_ = input_buf;
        output_buffer_ = output_buf;

        // For simplicity: use time-domain convolution
        // (FFT version can be added later if needed)
        std::memcpy(ir_, ir, ir_len_ * sizeof(float));
        std::memset(delay_line_, 0, MAX_IR_LEN * sizeof(float));
        std::memset(input_buffer_, 0, BUFFER_SIZE * sizeof(float));
        std::memset(output_buffer_, 0, BUFFER_SIZE * sizeof(float));

        delay_pos_ = 0;
        input_read_ = 0;
        input_write_ = 0;
        output_read_ = 0;
        output_write_ = 0;

        initialized_ = true;
    }

    void Reset() {
        std::memset(delay_line_, 0, MAX_IR_LEN * sizeof(float));
        std::memset(input_buffer_, 0, BUFFER_SIZE * sizeof(float));
        std::memset(output_buffer_, 0, BUFFER_SIZE * sizeof(float));
        delay_pos_ = 0;
        input_read_ = 0;
        input_write_ = 0;
        output_read_ = 0;
        output_write_ = 0;
    }

    // Called from AUDIO CALLBACK (real-time thread)
    // Pushes N input samples, pops N output samples
    void ProcessBlockIO(const float* in, float* out, size_t N) {
        if (!initialized_) {
            std::memcpy(out, in, N * sizeof(float));
            return;
        }

        // Push input samples to ring buffer
        for (size_t i = 0; i < N; ++i) {
            input_buffer_[input_write_.load() & (BUFFER_SIZE - 1)] = in[i];
            input_write_.fetch_add(1);
        }

        // Pop output samples from ring buffer
        for (size_t i = 0; i < N; ++i) {
            size_t available = output_write_.load() - output_read_.load();
            if (available > 0) {
                out[i] = output_buffer_[output_read_.load() & (BUFFER_SIZE - 1)];
                output_read_.fetch_add(1);
            } else {
                // Underrun - output zero
                out[i] = 0.0f;
            }
        }
    }

    // Called from MAIN LOOP (non-real-time thread)
    // Processes as many samples as available
    void ProcessAsync() {
        if (!initialized_) return;

        // Check how many samples available to process
        size_t available = input_write_.load() - input_read_.load();

        // For now: SEVERELY limit IR length to prevent hang
        // TODO: Implement proper FFT-based async convolution
        size_t safe_ir_len = (ir_len_ > 1024) ? 1024 : ir_len_;

        // Process samples one at a time (time-domain convolution - SLOW!)
        while (available > 0) {
            // Read one input sample
            float x = input_buffer_[input_read_.load() & (BUFFER_SIZE - 1)];
            input_read_.fetch_add(1);
            available--;

            // Insert into delay line
            delay_line_[delay_pos_] = x;

            // Convolve (time-domain FIR) - LIMITED TO 1024 TAPS
            float y = 0.0f;
            for (size_t k = 0; k < safe_ir_len; ++k) {
                size_t idx = (delay_pos_ + MAX_IR_LEN - k) % MAX_IR_LEN;
                y += delay_line_[idx] * ir_[k];
            }

            delay_pos_ = (delay_pos_ + 1) % MAX_IR_LEN;

            // Check output buffer has space
            size_t output_used = output_write_.load() - output_read_.load();
            if (output_used < BUFFER_SIZE - 1) {
                output_buffer_[output_write_.load() & (BUFFER_SIZE - 1)] = y * 0.5f;  // Scale
                output_write_.fetch_add(1);
            } else {
                // Output buffer full - skip this sample (should not happen if main loop runs fast)
                break;
            }
        }
    }

    size_t GetInputAvailable() const {
        return input_write_.load() - input_read_.load();
    }

    size_t GetOutputAvailable() const {
        return output_write_.load() - output_read_.load();
    }

private:
    bool initialized_ = false;
    size_t ir_len_ = 0;

    // Pointers to external SDRAM storage
    float* ir_ = nullptr;
    float* delay_line_ = nullptr;
    float* input_buffer_ = nullptr;
    float* output_buffer_ = nullptr;

    size_t delay_pos_ = 0;

    // Lock-free indices (atomic)
    std::atomic<size_t> input_read_{0};
    std::atomic<size_t> input_write_{0};
    std::atomic<size_t> output_read_{0};
    std::atomic<size_t> output_write_{0};
};
