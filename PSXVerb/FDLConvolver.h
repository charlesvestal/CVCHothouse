#pragma once

#include <cstddef>
#include <cstring>
#include <complex>
#include "MinimalFFT.h"
#include "daisy_seed.h"

using namespace daisy;

// Global SDRAM storage for FDL convolvers (can't use DSY_SDRAM_BSS in template class members)
// Head engine (L=256, N=512): MAX_PARTITIONS=128
DSY_SDRAM_BSS std::complex<float> g_head_H_L[128][257];   // [partition][freq_bins]
DSY_SDRAM_BSS std::complex<float> g_head_H_R[128][257];
DSY_SDRAM_BSS std::complex<float> g_head_X_hist_L[128][257];
DSY_SDRAM_BSS std::complex<float> g_head_X_hist_R[128][257];

// Tail engine (L=2048, N=4096): MAX_PARTITIONS=128
DSY_SDRAM_BSS std::complex<float> g_tail_H_L[128][2049];
DSY_SDRAM_BSS std::complex<float> g_tail_H_R[128][2049];
DSY_SDRAM_BSS std::complex<float> g_tail_X_hist_L[128][2049];
DSY_SDRAM_BSS std::complex<float> g_tail_X_hist_R[128][2049];

/**
 * Frequency-Domain Line (FDL) Convolver with Overlap-Add
 *
 * Template parameter L = hop size (partition size)
 * FFT size N = 2*L (always)
 *
 * OLA INVARIANTS:
 * - Zero-pad input block L to N (append L zeros)
 * - FFT forward (no scaling)
 * - Complex multiply with precomputed H[q]
 * - IFFT (scale by 1/N)
 * - Discard first L samples, output last L samples
 * - Accumulate across all partitions
 */
template<size_t L>
class FDLConvolver {
public:
    static constexpr size_t N = 2 * L;
    static constexpr size_t MAX_PARTITIONS = 128;

    // CPU OPTIMIZATION: Limit partitions processed per callback
    // For L=2048: process max 16 partitions per call (rotating window)
    // For L=4096: process max 8 partitions per call
    static constexpr size_t MAX_PARTITIONS_PER_CALL = (L == 2048) ? 16 : ((L == 4096) ? 8 : MAX_PARTITIONS);

    FDLConvolver() = default;

    // Initialize with external SDRAM storage
    void Init(const float* ir, size_t ir_len_samples,
              std::complex<float> (*H_ext)[N/2+1],
              std::complex<float> (*X_hist_ext)[N/2+1],
              DaisySeed* seed = nullptr,
              size_t crossfade_len = 0) {
        if (seed) {
            char buf[64];
            snprintf(buf, sizeof(buf), "      FDL: Start (L=%u, N=%u)", (unsigned int)L, (unsigned int)N);
            seed->PrintLine(buf);
        }

        H_ = H_ext;
        X_hist_ = X_hist_ext;
        // Calculate number of partitions
        Q_ = (ir_len_samples + L - 1) / L;
        if (Q_ > MAX_PARTITIONS) Q_ = MAX_PARTITIONS;

        ir_len_ = ir_len_samples;

        if (seed) {
            char buf[64];
            snprintf(buf, sizeof(buf), "      FDL: Computing %u partitions", (unsigned int)Q_);
            seed->PrintLine(buf);
        }

        // Precompute H[q] for each partition
        for (size_t q = 0; q < Q_; ++q) {
            if (seed && q % 10 == 0) {
                char buf[64];
                snprintf(buf, sizeof(buf), "      FDL: Partition %u/%u", (unsigned int)q, (unsigned int)Q_);
                seed->PrintLine(buf);
            }

            // Extract partition from IR
            float partition[N];
            std::memset(partition, 0, N * sizeof(float));

            size_t offset = q * L;
            size_t len = (offset + L <= ir_len_samples) ? L : (ir_len_samples - offset);

            // Copy samples with optional crossfade
            for (size_t i = 0; i < len; ++i) {
                float sample = ir[offset + i];

                // Apply fade-in for tail (first crossfade_len samples)
                if (crossfade_len > 0 && (offset + i) < crossfade_len) {
                    float t = static_cast<float>(offset + i) / crossfade_len;
                    float fade = 0.5f * (1.0f - cosf(3.14159265359f * t));  // Raised cosine (0 -> 1)
                    sample *= fade;
                }

                partition[i] = sample;
            }

            // FFT -> H[q]
            if constexpr (N == 512) {
                MinimalFFT::fft_forward_real<512>(partition, H_[q]);
            } else if constexpr (N == 4096) {
                MinimalFFT::fft_forward_real<4096>(partition, H_[q]);
            } else if constexpr (N == 8192) {
                MinimalFFT::fft_forward_real<8192>(partition, H_[q]);
            }
        }

        if (seed) seed->PrintLine("      FDL: Calling Reset()");

        Reset();
        initialized_ = true;

        if (seed) seed->PrintLine("      FDL: Init complete");
    }

    void Reset() {
        // Clear X_hist (can't use sizeof on pointer, must calculate manually)
        for (size_t q = 0; q < MAX_PARTITIONS; ++q) {
            for (size_t k = 0; k <= N / 2; ++k) {
                X_hist_[q][k] = std::complex<float>(0.0f, 0.0f);
            }
        }
        block_idx_ = 0;
    }

    void ProcessBlock(const float* in_L, float* out_L) {
        if (!initialized_) {
            std::memcpy(out_L, in_L, L * sizeof(float));
            return;
        }

        // Zero-pad input to N
        float x_padded[N];
        std::memcpy(x_padded, in_L, L * sizeof(float));
        std::memset(x_padded + L, 0, L * sizeof(float));

        // FFT forward
        std::complex<float> X_current[N / 2 + 1];
        if constexpr (N == 512) {
            MinimalFFT::fft_forward_real<512>(x_padded, X_current);
        } else if constexpr (N == 4096) {
            MinimalFFT::fft_forward_real<4096>(x_padded, X_current);
        } else if constexpr (N == 8192) {
            MinimalFFT::fft_forward_real<8192>(x_padded, X_current);
        }

        // Store in X_hist circular buffer
        size_t hist_idx = block_idx_ & (MAX_PARTITIONS - 1);
        std::memcpy(X_hist_[hist_idx], X_current, (N / 2 + 1) * sizeof(std::complex<float>));

        // Frequency-domain accumulation: Y = Σ H[q] * X_hist[(b - q) mod Q]
        std::complex<float> Y[N / 2 + 1];
        for (size_t k = 0; k <= N / 2; ++k) {
            Y[k] = std::complex<float>(0.0f, 0.0f);
        }

        // Process ALL partitions (tail limited to manageable length)
        for (size_t q = 0; q < Q_; ++q) {
            // X_hist index: (b - q) mod MAX_PARTITIONS
            size_t x_idx = (block_idx_ - q) & (MAX_PARTITIONS - 1);

            // Complex multiply-accumulate
            for (size_t k = 0; k <= N / 2; ++k) {
                Y[k] += H_[q][k] * X_hist_[x_idx][k];
            }
        }

        // IFFT (already scaled by 1/N inside)
        float y_time[N];
        if constexpr (N == 512) {
            MinimalFFT::fft_inverse_real<512>(Y, y_time);
        } else if constexpr (N == 4096) {
            MinimalFFT::fft_inverse_real<4096>(Y, y_time);
        } else if constexpr (N == 8192) {
            MinimalFFT::fft_inverse_real<8192>(Y, y_time);
        }

        // OLA: Keep LAST L samples (discard first L)
        // The "overlap-add" happens automatically from the circular convolution
        std::memcpy(out_L, y_time + L, L * sizeof(float));

        // Increment block index
        block_idx_++;
    }

private:
    size_t Q_ = 0;                                          // Number of partitions
    size_t ir_len_ = 0;                                     // IR length in samples
    size_t block_idx_ = 0;                                  // Current block index
    bool initialized_ = false;

    // Pointers to external SDRAM storage
    std::complex<float> (*H_)[N / 2 + 1] = nullptr;
    std::complex<float> (*X_hist_)[N / 2 + 1] = nullptr;
};
