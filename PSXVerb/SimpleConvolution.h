#pragma once

#include <cstddef>
#include <cstring>

/**
 * Simple time-domain convolution (no FFT)
 * Uses first 16384 samples of IR - 341ms @ 48kHz
 *
 * This gives us a short but usable reverb tail
 */

class SimpleConvolution
{
  public:
    static constexpr size_t kMaxIRLength = 16384;  // ~341ms at 48kHz

    SimpleConvolution() = default;

    void Init(const float* ir, size_t ir_length)
    {
        // Copy and truncate IR
        ir_length_ = (ir_length < kMaxIRLength) ? ir_length : kMaxIRLength;
        std::memcpy(ir_, ir, ir_length_ * sizeof(float));

        // Clear history
        std::memset(history_, 0, sizeof(history_));
        history_pos_ = 0;

        initialized_ = true;
    }

    void Reset()
    {
        std::memset(history_, 0, sizeof(history_));
        history_pos_ = 0;
    }

    void ProcessBlock(const float* in, float* out, size_t block_size)
    {
        if (!initialized_)
        {
            std::memcpy(out, in, block_size * sizeof(float));
            return;
        }

        for (size_t n = 0; n < block_size; ++n)
        {
            // Store input in circular buffer
            history_[history_pos_] = in[n];

            // Time-domain convolution: y[n] = sum(x[n-k] * h[k])
            float sum = 0.0f;
            for (size_t k = 0; k < ir_length_; ++k)
            {
                size_t idx = (history_pos_ + kMaxIRLength - k) & (kMaxIRLength - 1);
                sum += history_[idx] * ir_[k];
            }

            out[n] = sum * 0.3f;  // Scale down to prevent clipping

            // Advance circular buffer
            history_pos_ = (history_pos_ + 1) & (kMaxIRLength - 1);
        }
    }

    void SetMix(float mix) { mix_ = mix; }
    float GetMix() const { return mix_; }

  private:
    float ir_[kMaxIRLength];
    float history_[kMaxIRLength];
    size_t ir_length_ = 0;
    size_t history_pos_ = 0;
    float mix_ = 1.0f;
    bool initialized_ = false;
};
