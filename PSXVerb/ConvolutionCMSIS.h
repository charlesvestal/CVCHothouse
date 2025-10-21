#pragma once

#include <cstddef>
#include <cstring>
#include "arm_math.h"
#include "daisy_seed.h"

/**
 * Partitioned Convolution using ARM CMSIS-DSP optimized FFT
 *
 * Strategy: Use 4096-sample partitions, process first 8 partitions only
 * That gives us 32768 samples = 682ms of reverb tail @ 48kHz
 * This is a compromise between quality and CPU budget
 */

struct ConvCfg
{
    size_t block_size = 16;
    size_t partition_size = 4096;
    int max_partitions = 8;  // Limit to 8 partitions for CPU budget
};

// Static SDRAM for partition spectra (pre-computed FFTs)
// 8 partitions × 4096 complex (8192 floats) = 64k floats = 256KB per channel
DSY_SDRAM_BSS float g_partition_spectra_L[8][8192];
DSY_SDRAM_BSS float g_partition_spectra_R[8][8192];

class ConvolutionCMSIS
{
  public:
    ConvolutionCMSIS() = default;

    void Init(const float* ir, size_t ir_len, const ConvCfg& cfg);
    void Reset();
    void ProcessBlock(const float* in, float* out, size_t size);
    void SetMix(float mix) { mix_ = mix; }

  private:
    void PrecomputePartitions(const float* ir, size_t ir_len);

    ConvCfg cfg_;

    static constexpr size_t kMaxPartitions = 8;
    size_t num_partitions_ = 0;

    // CMSIS-DSP FFT instances
    arm_rfft_fast_instance_f32 fft_inst_;
    bool fft_ready_ = false;

    // Input ring buffer (must hold partition_size samples)
    static constexpr size_t kRingSize = 8192;  // Power of 2
    float input_ring_[kRingSize];
    size_t write_pos_ = 0;

    // Working buffers for FFT
    float work_time_[8192];   // Time-domain buffer
    float work_freq_[8192];   // Frequency-domain buffer (complex)

    float y_accum_[32];
    float mix_ = 1.0f;
    bool initialized_ = false;
};

inline void ConvolutionCMSIS::Init(const float* ir, size_t ir_len, const ConvCfg& cfg)
{
    cfg_ = cfg;

    // Initialize CMSIS-DSP FFT
    arm_rfft_fast_init_f32(&fft_inst_, cfg_.partition_size);
    fft_ready_ = true;

    // Calculate partitions needed
    num_partitions_ = (ir_len + cfg_.partition_size - 1) / cfg_.partition_size;
    if (num_partitions_ > kMaxPartitions)
        num_partitions_ = kMaxPartitions;

    PrecomputePartitions(ir, ir_len);
    Reset();
    initialized_ = true;
}

inline void ConvolutionCMSIS::PrecomputePartitions(const float* ir, size_t ir_len)
{
    // For each partition, zero-pad to partition_size, FFT, and store
    for (size_t p = 0; p < num_partitions_; ++p)
    {
        size_t offset = p * cfg_.partition_size;
        size_t len = cfg_.partition_size;
        if (offset + len > ir_len)
            len = ir_len - offset;

        // Zero-pad partition
        std::memset(work_time_, 0, cfg_.partition_size * sizeof(float));
        for (size_t i = 0; i < len && offset + i < ir_len; ++i)
            work_time_[i] = ir[offset + i];

        // FFT (real → complex, output is [r0, r1, i1, r2, i2, ...])
        arm_rfft_fast_f32(&fft_inst_, work_time_, work_freq_, 0);

        // Store spectrum (L channel, assuming this instance is for L)
        // Note: We'll need separate instances for L and R
        std::memcpy(g_partition_spectra_L[p], work_freq_, cfg_.partition_size * 2 * sizeof(float));
    }
}

inline void ConvolutionCMSIS::Reset()
{
    std::memset(input_ring_, 0, sizeof(input_ring_));
    write_pos_ = 0;
    std::memset(y_accum_, 0, sizeof(y_accum_));
}

inline void ConvolutionCMSIS::ProcessBlock(const float* in, float* out, size_t size)
{
    if (!initialized_ || !fft_ready_)
    {
        std::memcpy(out, in, size * sizeof(float));
        return;
    }

    const size_t N = cfg_.partition_size;

    // Add new input to ring buffer
    for (size_t i = 0; i < size; ++i)
    {
        input_ring_[write_pos_ & (kRingSize - 1)] = in[i];
        write_pos_++;
    }

    // Clear accumulator for this block
    std::memset(y_accum_, 0, size * sizeof(float));

    // Process ALL partitions and accumulate (this is the correct way!)
    for (size_t p = 0; p < num_partitions_; ++p)
    {
        // Build input segment from ring buffer
        // For partition p, we need samples from write_pos_ - (p+1)*size to write_pos_ - p*size
        // This implements the delay line for each partition
        size_t delay = p * size;

        // Zero-pad to partition size
        std::memset(work_time_, 0, N * sizeof(float));

        // Copy the last N samples from the delayed position
        for (size_t i = 0; i < N && i < write_pos_; ++i)
        {
            size_t idx = (write_pos_ - N - delay + i) & (kRingSize - 1);
            work_time_[i] = input_ring_[idx];
        }

        // FFT
        arm_rfft_fast_f32(&fft_inst_, work_time_, work_freq_, 0);

        // Complex multiply with partition spectrum
        // CMSIS format: [r0, r1, i1, r2, i2, r3, i3, ...]
        // DC component
        work_freq_[0] *= g_partition_spectra_L[p][0];
        work_freq_[1] *= g_partition_spectra_L[p][1];  // Nyquist

        // Other bins (complex multiply: (a+bi)(c+di) = (ac-bd) + (ad+bc)i)
        for (size_t k = 1; k < N / 2; ++k)
        {
            float xr = work_freq_[2 * k];
            float xi = work_freq_[2 * k + 1];
            float hr = g_partition_spectra_L[p][2 * k];
            float hi = g_partition_spectra_L[p][2 * k + 1];

            work_freq_[2 * k] = xr * hr - xi * hi;
            work_freq_[2 * k + 1] = xr * hi + xi * hr;
        }

        // IFFT
        arm_rfft_fast_f32(&fft_inst_, work_freq_, work_time_, 1);

        // Overlap-Save: discard first (N - size) samples, keep last size samples
        // Accumulate into output
        for (size_t i = 0; i < size; ++i)
        {
            y_accum_[i] += work_time_[N - size + i] / static_cast<float>(N);  // Normalize
        }
    }

    // Output with mix
    for (size_t i = 0; i < size; ++i)
    {
        out[i] = (1.0f - mix_) * in[i] + mix_ * y_accum_[i];
    }
}
