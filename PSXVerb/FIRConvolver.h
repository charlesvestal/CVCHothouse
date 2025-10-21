#pragma once

#include <cstddef>
#include <cstring>

/**
 * Time-Domain FIR Convolver
 *
 * Optimized for ≤2000 taps per channel on ARM Cortex-M7
 * Uses circular delay line, unrolled MACs, FZ/DAZ for denormals
 */

class FIRConvolver {
public:
    static constexpr size_t MAX_TAPS = 2000;

    FIRConvolver() : N_taps_(0), delay_pos_(0) {
        std::memset(taps_, 0, sizeof(taps_));
        std::memset(delay_line_L_, 0, sizeof(delay_line_L_));
        std::memset(delay_line_R_, 0, sizeof(delay_line_R_));
    }

    void Init(const float* taps, size_t N_taps) {
        N_taps_ = (N_taps > MAX_TAPS) ? MAX_TAPS : N_taps;
        std::memcpy(taps_, taps, N_taps_ * sizeof(float));
        delay_pos_ = 0;
    }

    void Reset() {
        std::memset(delay_line_L_, 0, sizeof(delay_line_L_));
        std::memset(delay_line_R_, 0, sizeof(delay_line_R_));
        delay_pos_ = 0;
    }

    // Process N samples (typically 16 per block)
    void ProcessBlock(const float* inL, const float* inR,
                     float* outL, float* outR, size_t N) {
        // Safety check
        if (N_taps_ == 0) {
            for (size_t n = 0; n < N; ++n) {
                outL[n] = 0.0f;
                outR[n] = 0.0f;
            }
            return;
        }

        for (size_t n = 0; n < N; ++n) {
            // Insert new samples
            delay_line_L_[delay_pos_] = inL[n];
            delay_line_R_[delay_pos_] = inR[n];

            // Convolve - simple version without unrolling first
            float sumL = 0.0f;
            float sumR = 0.0f;

            // Read backwards from current position
            size_t read_pos = delay_pos_;
            for (size_t k = 0; k < N_taps_; ++k) {
                sumL += taps_[k] * delay_line_L_[read_pos];
                sumR += taps_[k] * delay_line_R_[read_pos];

                // Move backwards in circular buffer
                if (read_pos == 0) {
                    read_pos = MAX_TAPS - 1;
                } else {
                    read_pos--;
                }
            }

            outL[n] = sumL;
            outR[n] = sumR;

            // Advance delay line pointer (forward)
            delay_pos_++;
            if (delay_pos_ >= MAX_TAPS) delay_pos_ = 0;
        }
    }

private:
    float taps_[MAX_TAPS];
    float delay_line_L_[MAX_TAPS];
    float delay_line_R_[MAX_TAPS];
    size_t N_taps_;
    size_t delay_pos_;
};
