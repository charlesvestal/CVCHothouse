#pragma once

#include <cstddef>
#include <cstring>
#include <atomic>
#include <complex>
#include "MinimalFFT.h"
#include "daisy_seed.h"

using namespace daisy;

/**
 * Async FFT-based Convolver
 *
 * ARCHITECTURE:
 * - Audio callback: Push input, pop output (ring buffers)
 * - Main loop: When enough samples accumulated, do ONE FFT convolution
 *
 * Block-based processing:
 * - Accumulate 4096 input samples
 * - Do 8192-point FFT convolution in main loop
 * - Output 4096 samples to output buffer
 *
 * This processes ~11 blocks/second, well within CPU budget
 */

// SDRAM storage for FFT convolution (stereo)
// Each channel: H[64 partitions][4097 complex] + X_hist[64][4097] + ring buffers
DSY_SDRAM_BSS std::complex<float> g_fft_H_L[64][4097];
DSY_SDRAM_BSS std::complex<float> g_fft_H_R[64][4097];
DSY_SDRAM_BSS std::complex<float> g_fft_X_hist_L[64][4097];
DSY_SDRAM_BSS std::complex<float> g_fft_X_hist_R[64][4097];
DSY_SDRAM_BSS float g_fft_input_L[16384];
DSY_SDRAM_BSS float g_fft_input_R[16384];
DSY_SDRAM_BSS float g_fft_output_L[16384];
DSY_SDRAM_BSS float g_fft_output_R[16384];

class AsyncFFTConvolver {
public:
    static constexpr size_t L = 4096;                // Partition size
    static constexpr size_t N = 8192;                // FFT size (2*L)
    static constexpr size_t MAX_PARTITIONS = 64;     // Max partitions (~0.55 sec)
    static constexpr size_t BUFFER_SIZE = 16384;     // Ring buffer

    AsyncFFTConvolver() = default;

    void Init(const float* ir, size_t ir_len,
              std::complex<float> (*H_ext)[4097],
              std::complex<float> (*X_hist_ext)[4097],
              float* input_buf,
              float* output_buf,
              DaisySeed* seed = nullptr) {

        H_ = H_ext;
        X_hist_ = X_hist_ext;
        input_buffer_ = input_buf;
        output_buffer_ = output_buf;

        // Calculate partitions
        Q_ = (ir_len + L - 1) / L;
        if (Q_ > MAX_PARTITIONS) Q_ = MAX_PARTITIONS;

        size_t effective_len = Q_ * L;
        if (effective_len > ir_len) effective_len = ir_len;

        if (seed) {
            char buf[64];
            snprintf(buf, sizeof(buf), "AsyncFFT: Precomputing %u partitions", (unsigned int)Q_);
            seed->PrintLine(buf);
        }

        // Precompute H[q] for each partition
        for (size_t q = 0; q < Q_; ++q) {
            float partition[N];
            std::memset(partition, 0, N * sizeof(float));

            size_t offset = q * L;
            size_t len = (offset + L <= ir_len) ? L : (ir_len - offset);

            for (size_t i = 0; i < len; ++i) {
                partition[i] = ir[offset + i];
            }

            // FFT
            MinimalFFT::fft_forward_real<8192>(partition, H_[q]);
        }

        // Clear history and buffers
        for (size_t q = 0; q < MAX_PARTITIONS; ++q) {
            for (size_t k = 0; k <= N/2; ++k) {
                X_hist_[q][k] = std::complex<float>(0.0f, 0.0f);
            }
        }

        std::memset(input_buffer_, 0, BUFFER_SIZE * sizeof(float));
        std::memset(output_buffer_, 0, BUFFER_SIZE * sizeof(float));

        input_read_ = 0;
        input_write_ = 0;
        output_read_ = 0;
        output_write_ = 0;
        block_idx_ = 0;

        initialized_ = true;

        if (seed) {
            char buf[64];
            snprintf(buf, sizeof(buf), "AsyncFFT: Init complete (%u samples = %.2f sec)",
                     (unsigned int)effective_len, (float)effective_len / 48000.0f);
            seed->PrintLine(buf);
        }
    }

    void Reset() {
        for (size_t q = 0; q < MAX_PARTITIONS; ++q) {
            for (size_t k = 0; k <= N/2; ++k) {
                X_hist_[q][k] = std::complex<float>(0.0f, 0.0f);
            }
        }
        std::memset(input_buffer_, 0, BUFFER_SIZE * sizeof(float));
        std::memset(output_buffer_, 0, BUFFER_SIZE * sizeof(float));
        input_read_ = 0;
        input_write_ = 0;
        output_read_ = 0;
        output_write_ = 0;
        block_idx_ = 0;
    }

    // Called from AUDIO CALLBACK - lightweight I/O only
    void ProcessBlockIO(const float* in, float* out, size_t size) {
        if (!initialized_) {
            std::memcpy(out, in, size * sizeof(float));
            return;
        }

        // Push input
        for (size_t i = 0; i < size; ++i) {
            input_buffer_[input_write_.load() & (BUFFER_SIZE - 1)] = in[i];
            input_write_.fetch_add(1);
        }

        // Pop output
        for (size_t i = 0; i < size; ++i) {
            size_t avail = output_write_.load() - output_read_.load();
            if (avail > 0) {
                out[i] = output_buffer_[output_read_.load() & (BUFFER_SIZE - 1)];
                output_read_.fetch_add(1);
            } else {
                out[i] = 0.0f; // Underrun
            }
        }
    }

    // Called from MAIN LOOP - does FFT convolution when L samples ready
    void ProcessAsync() {
        if (!initialized_) return;

        // Check if we have L samples to process (use atomic counters)
        size_t available = input_write_.load() - input_read_.load();
        if (available < L) return;

        // Read L samples from input buffer
        float x_block[L];
        size_t read_pos = input_read_.load();
        for (size_t i = 0; i < L; ++i) {
            x_block[i] = input_buffer_[(read_pos + i) & (BUFFER_SIZE - 1)];
        }
        input_read_.fetch_add(L);

        // Zero-pad to N
        float x_padded[N];
        std::memcpy(x_padded, x_block, L * sizeof(float));
        std::memset(x_padded + L, 0, L * sizeof(float));

        // FFT forward
        std::complex<float> X_current[N/2 + 1];
        MinimalFFT::fft_forward_real<8192>(x_padded, X_current);

        // Store in history
        size_t hist_idx = block_idx_ & (MAX_PARTITIONS - 1);
        std::memcpy(X_hist_[hist_idx], X_current, (N/2 + 1) * sizeof(std::complex<float>));

        // Frequency-domain accumulation
        std::complex<float> Y[N/2 + 1];
        for (size_t k = 0; k <= N/2; ++k) {
            Y[k] = std::complex<float>(0.0f, 0.0f);
        }

        for (size_t q = 0; q < Q_; ++q) {
            size_t x_idx = (block_idx_ - q) & (MAX_PARTITIONS - 1);
            for (size_t k = 0; k <= N/2; ++k) {
                Y[k] += H_[q][k] * X_hist_[x_idx][k];
            }
        }

        // IFFT
        float y_time[N];
        MinimalFFT::fft_inverse_real<8192>(Y, y_time);

        // OLA: output last L samples
        for (size_t i = 0; i < L; ++i) {
            output_buffer_[output_write_.load() & (BUFFER_SIZE - 1)] = y_time[L + i];
            output_write_.fetch_add(1);
        }

        block_idx_++;
    }

    size_t GetInputPending() const {
        return input_write_.load() - input_read_.load();
    }

    size_t GetOutputAvailable() const {
        return output_write_.load() - output_read_.load();
    }

private:
    bool initialized_ = false;
    size_t Q_ = 0;
    size_t block_idx_ = 0;

    std::complex<float> (*H_)[4097] = nullptr;
    std::complex<float> (*X_hist_)[4097] = nullptr;

    float* input_buffer_ = nullptr;
    float* output_buffer_ = nullptr;

    std::atomic<size_t> input_read_{0};
    std::atomic<size_t> input_write_{0};
    std::atomic<size_t> output_read_{0};
    std::atomic<size_t> output_write_{0};
};
