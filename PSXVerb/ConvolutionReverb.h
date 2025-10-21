#pragma once

#include <cstddef>
#include <cstring>
#include <cmath>
#include "daisysp.h"

/**
 * Non-uniform Partitioned Convolution Reverb using Overlap-Save
 *
 * CRITICAL: Uses static SDRAM allocation (no heap/new) for embedded safety
 * All IR spectra buffers allocated at compile time in SDRAM
 */

namespace {

// Simple power-of-two FFT (Cooley-Tukey radix-2)
inline void FFT_Radix2(float* real, float* imag, size_t n, bool inverse)
{
    // Bit-reversal permutation
    size_t j = 0;
    for (size_t i = 1; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;

        if (i < j)
        {
            float tr = real[i]; real[i] = real[j]; real[j] = tr;
            float ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
        }
    }

    // FFT computation
    float sign = inverse ? 1.0f : -1.0f;
    for (size_t len = 2; len <= n; len <<= 1)
    {
        float angle = sign * 2.0f * M_PI / static_cast<float>(len);
        float wlen_r = cosf(angle);
        float wlen_i = sinf(angle);

        for (size_t i = 0; i < n; i += len)
        {
            float w_r = 1.0f;
            float w_i = 0.0f;

            for (size_t j = 0; j < len / 2; ++j)
            {
                size_t u_idx = i + j;
                size_t v_idx = i + j + len / 2;

                float u_r = real[u_idx];
                float u_i = imag[u_idx];
                float v_r = real[v_idx];
                float v_i = imag[v_idx];

                float prod_r = w_r * v_r - w_i * v_i;
                float prod_i = w_r * v_i + w_i * v_r;

                real[u_idx] = u_r + prod_r;
                imag[u_idx] = u_i + prod_i;
                real[v_idx] = u_r - prod_r;
                imag[v_idx] = u_i - prod_i;

                float next_w_r = w_r * wlen_r - w_i * wlen_i;
                float next_w_i = w_r * wlen_i + w_i * wlen_r;
                w_r = next_w_r;
                w_i = next_w_i;
            }
        }
    }

    // Normalize for inverse FFT
    if (inverse)
    {
        float scale = 1.0f / static_cast<float>(n);
        for (size_t i = 0; i < n; ++i)
        {
            real[i] *= scale;
            imag[i] *= scale;
        }
    }
}

} // namespace

struct ConvolutionConfig
{
    size_t block_size = 16;
    size_t head_fft = 256;
    size_t tail_fft = 2048;
    float head_ms = 128.0f;
    int tail_parts_per_block = 1;
};

// Static SDRAM buffers for IR spectra (defined in ConvolutionReverb.cpp)
extern float g_head_spectra[32][512];     // [partition][real256 + imag256]
extern float g_tail_spectra[192][4096];   // [partition][real2048 + imag2048]

class ConvolutionReverb
{
  public:
    ConvolutionReverb() = default;

    void Init(const float* ir, size_t ir_len, const ConvolutionConfig& cfg);
    void Reset();
    void ProcessBlock(const float* in, float* out, size_t block_size);
    void SetMix(float mix) { mix_ = (mix < 0.0f) ? 0.0f : ((mix > 1.0f) ? 1.0f : mix); }
    float GetMix() const { return mix_; }

  private:
    void PrecomputePartitions(const float* ir, size_t ir_len);

    ConvolutionConfig cfg_;

    static constexpr size_t kMaxHeadPartitions = 32;
    static constexpr size_t kMaxTailPartitions = 192;

    size_t num_head_partitions_ = 0;
    size_t num_tail_partitions_ = 0;

    // Ring buffers (power-of-two)
    static constexpr size_t kRingSize = 4096;
    float input_ring_head_[kRingSize];
    float input_ring_tail_[kRingSize];
    size_t write_pos_head_ = 0;
    size_t write_pos_tail_ = 0;

    size_t tail_index_ = 0;

    // Working buffers
    float work_time_[2048];
    float work_freq_real_[2048];
    float work_freq_imag_[2048];

    float y_accum_[32];

    float mix_ = 1.0f;
    bool initialized_ = false;
};

inline void ConvolutionReverb::Init(const float* ir, size_t ir_len, const ConvolutionConfig& cfg)
{
    cfg_ = cfg;

    float sr = 48000.0f;
    size_t head_len_samples = static_cast<size_t>(cfg_.head_ms * 0.001f * sr);

    num_head_partitions_ = (head_len_samples + cfg_.head_fft - 1) / cfg_.head_fft;
    if (num_head_partitions_ > kMaxHeadPartitions)
        num_head_partitions_ = kMaxHeadPartitions;

    size_t tail_start = num_head_partitions_ * cfg_.head_fft;
    if (tail_start < ir_len)
    {
        size_t tail_len = ir_len - tail_start;
        num_tail_partitions_ = (tail_len + cfg_.tail_fft - 1) / cfg_.tail_fft;
        if (num_tail_partitions_ > kMaxTailPartitions)
            num_tail_partitions_ = kMaxTailPartitions;
    }
    else
    {
        num_tail_partitions_ = 0;
    }

    PrecomputePartitions(ir, ir_len);
    Reset();

    initialized_ = true;
}

inline void ConvolutionReverb::PrecomputePartitions(const float* ir, size_t ir_len)
{
    // Precompute head partitions (use static SDRAM buffers)
    for (size_t p = 0; p < num_head_partitions_; ++p)
    {
        size_t offset = p * cfg_.head_fft;
        size_t len = cfg_.head_fft;
        if (offset + len > ir_len)
            len = ir_len - offset;

        float* spec_real = g_head_spectra[p];
        float* spec_imag = g_head_spectra[p] + cfg_.head_fft;

        // Zero-pad and copy
        std::memset(spec_real, 0, cfg_.head_fft * sizeof(float));
        std::memset(spec_imag, 0, cfg_.head_fft * sizeof(float));

        for (size_t i = 0; i < len && offset + i < ir_len; ++i)
            spec_real[i] = ir[offset + i];

        // FFT
        FFT_Radix2(spec_real, spec_imag, cfg_.head_fft, false);
    }

    // Precompute tail partitions
    size_t tail_start = num_head_partitions_ * cfg_.head_fft;
    for (size_t p = 0; p < num_tail_partitions_; ++p)
    {
        size_t offset = tail_start + p * cfg_.tail_fft;
        size_t len = cfg_.tail_fft;
        if (offset + len > ir_len)
            len = ir_len - offset;

        float* spec_real = g_tail_spectra[p];
        float* spec_imag = g_tail_spectra[p] + cfg_.tail_fft;

        std::memset(spec_real, 0, cfg_.tail_fft * sizeof(float));
        std::memset(spec_imag, 0, cfg_.tail_fft * sizeof(float));

        for (size_t i = 0; i < len && offset + i < ir_len; ++i)
            spec_real[i] = ir[offset + i];

        // FFT
        FFT_Radix2(spec_real, spec_imag, cfg_.tail_fft, false);
    }
}

inline void ConvolutionReverb::Reset()
{
    std::memset(input_ring_head_, 0, sizeof(input_ring_head_));
    std::memset(input_ring_tail_, 0, sizeof(input_ring_tail_));
    write_pos_head_ = 0;
    write_pos_tail_ = 0;
    tail_index_ = 0;
    std::memset(y_accum_, 0, sizeof(y_accum_));
}

inline void ConvolutionReverb::ProcessBlock(const float* in, float* out, size_t block_size)
{
    if (!initialized_ || block_size > 32)
    {
        std::memcpy(out, in, block_size * sizeof(float));
        return;
    }

    // Clear accumulator
    std::memset(y_accum_, 0, block_size * sizeof(float));

    // Process HEAD tier
    for (size_t p = 0; p < num_head_partitions_; ++p)
    {
        const size_t N = cfg_.head_fft;
        const size_t B = block_size;

        // Build segment
        for (size_t i = 0; i < N; ++i)
        {
            size_t idx = (write_pos_head_ + i) & (kRingSize - 1);
            work_time_[i] = input_ring_head_[idx];
        }

        // FFT
        std::memset(work_freq_imag_, 0, N * sizeof(float));
        std::memcpy(work_freq_real_, work_time_, N * sizeof(float));
        FFT_Radix2(work_freq_real_, work_freq_imag_, N, false);

        // Complex multiply
        const float* H_real = g_head_spectra[p];
        const float* H_imag = g_head_spectra[p] + N;

        for (size_t i = 0; i < N; ++i)
        {
            float x_r = work_freq_real_[i];
            float x_i = work_freq_imag_[i];
            float h_r = H_real[i];
            float h_i = H_imag[i];

            work_freq_real_[i] = x_r * h_r - x_i * h_i;
            work_freq_imag_[i] = x_r * h_i + x_i * h_r;
        }

        // IFFT
        FFT_Radix2(work_freq_real_, work_freq_imag_, N, true);

        // OLS: accumulate last B samples
        for (size_t i = 0; i < B; ++i)
            y_accum_[i] += work_freq_real_[N - B + i];
    }

    // Process TAIL tier
    for (int k = 0; k < cfg_.tail_parts_per_block && num_tail_partitions_ > 0; ++k)
    {
        size_t p = tail_index_;
        tail_index_ = (tail_index_ + 1) % num_tail_partitions_;

        const size_t N = cfg_.tail_fft;
        const size_t B = block_size;

        // Build segment
        for (size_t i = 0; i < N; ++i)
        {
            size_t idx = (write_pos_tail_ + i) & (kRingSize - 1);
            work_time_[i] = input_ring_tail_[idx];
        }

        // FFT
        std::memset(work_freq_imag_, 0, N * sizeof(float));
        std::memcpy(work_freq_real_, work_time_, N * sizeof(float));
        FFT_Radix2(work_freq_real_, work_freq_imag_, N, false);

        // Complex multiply
        const float* H_real = g_tail_spectra[p];
        const float* H_imag = g_tail_spectra[p] + N;

        for (size_t i = 0; i < N; ++i)
        {
            float x_r = work_freq_real_[i];
            float x_i = work_freq_imag_[i];
            float h_r = H_real[i];
            float h_i = H_imag[i];

            work_freq_real_[i] = x_r * h_r - x_i * h_i;
            work_freq_imag_[i] = x_r * h_i + x_i * h_r;
        }

        // IFFT
        FFT_Radix2(work_freq_real_, work_freq_imag_, N, true);

        // OLS: accumulate last B samples
        for (size_t i = 0; i < B; ++i)
            y_accum_[i] += work_freq_real_[N - B + i];
    }

    // Update ring buffers
    for (size_t i = 0; i < block_size; ++i)
    {
        input_ring_head_[write_pos_head_ & (kRingSize - 1)] = in[i];
        input_ring_tail_[write_pos_tail_ & (kRingSize - 1)] = in[i];
        write_pos_head_++;
        write_pos_tail_++;
    }

    // Output with mix
    for (size_t i = 0; i < block_size; ++i)
    {
        float dry = (1.0f - mix_) * in[i];
        float wet = mix_ * y_accum_[i] * 0.5f;
        out[i] = dry + wet;
    }
}
