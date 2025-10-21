#pragma once

#include <cstddef>
#include <cstring>
#include <cmath>
#include "FDLConvolver.h"

/**
 * Non-uniform Partitioned Convolution Reverb
 *
 * Two-tier architecture:
 * - HEAD: L=256, N=512, covers first 2048 samples (~42.7ms) - early reflections
 * - TAIL: L=4096, N=8192, covers 2048+ samples - long decay
 *
 * Crossfade at 2048-sample boundary (256-sample raised-cosine)
 * Hardware callback: 16 samples
 * Internal processing: Head runs at 256-sample hops, Tail at 4096-sample hops
 */

class PartitionedReverb {
public:
    static constexpr size_t L_HEAD = 256;
    static constexpr size_t L_TAIL = 2048;  // REDUCED from 4096 for CPU
    static constexpr size_t CROSSFADE_LEN = 256;
    static constexpr size_t HEAD_IR_LEN = 2048;
    static constexpr size_t MAX_TAIL_LEN = 8192;  // DRASTICALLY REDUCED: ~0.17 seconds tail
    static constexpr size_t FIFO_SIZE = 8192;  // Power of 2

    PartitionedReverb() = default;

    void Init(const float* ir, size_t ir_len, int channel, daisy::DaisySeed* seed = nullptr) {
        if (seed) seed->PrintLine("    PartReverb: Start Init");

        channel_ = channel;
        ir_len_ = ir_len;

        if (seed) seed->PrintLine("    PartReverb: Building head IR");

        // Build head IR (first 2048 samples with crossfade out)
        float head_ir[HEAD_IR_LEN];
        size_t head_len = (ir_len < HEAD_IR_LEN) ? ir_len : HEAD_IR_LEN;

        if (seed) seed->PrintLine("    PartReverb: Copying samples");

        for (size_t i = 0; i < head_len; ++i) {
            head_ir[i] = ir[i];
        }

        if (seed) seed->PrintLine("    PartReverb: Applying crossfade");

        // Apply fade-out in last 256 samples of head
        if (head_len >= CROSSFADE_LEN) {
            for (size_t i = 0; i < CROSSFADE_LEN; ++i) {
                size_t idx = head_len - CROSSFADE_LEN + i;
                float t = static_cast<float>(i) / CROSSFADE_LEN;
                float fade = 0.5f * (1.0f + cosf(M_PI * t));  // Raised cosine (1 -> 0)
                head_ir[idx] *= fade;
            }
        }

        if (seed) seed->PrintLine("    PartReverb: Zero-padding");

        // Zero-pad head to multiple of L_HEAD
        for (size_t i = head_len; i < HEAD_IR_LEN; ++i) {
            head_ir[i] = 0.0f;
        }

        if (seed) seed->PrintLine("    PartReverb: About to call head_.Init()");

        // Initialize head convolver with appropriate channel storage
        if (channel_ == 0) {
            head_.Init(head_ir, HEAD_IR_LEN, g_head_H_L, g_head_X_hist_L, seed);
        } else {
            head_.Init(head_ir, HEAD_IR_LEN, g_head_H_R, g_head_X_hist_R, seed);
        }

        if (seed) seed->PrintLine("    PartReverb: head_.Init() complete");

        // Build tail IR (from 2048+ with crossfade in)
        if (ir_len > HEAD_IR_LEN) {
            if (seed) seed->PrintLine("    PartReverb: Building tail IR (NO COPY approach)");

            size_t tail_len = ir_len - HEAD_IR_LEN;

            // LIMIT TAIL LENGTH to prevent CPU overload
            if (tail_len > MAX_TAIL_LEN) {
                if (seed) {
                    char buf[80];
                    snprintf(buf, sizeof(buf), "    PartReverb: Limiting tail from %u to %u samples (CPU constraint)",
                             (unsigned int)tail_len, (unsigned int)MAX_TAIL_LEN);
                    seed->PrintLine(buf);
                }
                tail_len = MAX_TAIL_LEN;
            } else {
                if (seed) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "    PartReverb: Tail length: %u samples", (unsigned int)tail_len);
                    seed->PrintLine(buf);
                }
            }

            // IMPORTANT: Pass pointer to tail portion of IR directly
            // FDLConvolver will read from it without copying
            const float* tail_ir_ptr = ir + HEAD_IR_LEN;

            if (seed) seed->PrintLine("    PartReverb: About to call tail_.Init()");

            // Initialize tail convolver with appropriate channel storage
            // Pass crossfade info so FDLConvolver can apply it during partition extraction
            if (channel_ == 0) {
                tail_.Init(tail_ir_ptr, tail_len, g_tail_H_L, g_tail_X_hist_L, seed, CROSSFADE_LEN);
            } else {
                tail_.Init(tail_ir_ptr, tail_len, g_tail_H_R, g_tail_X_hist_R, seed, CROSSFADE_LEN);
            }

            if (seed) seed->PrintLine("    PartReverb: tail_.Init() complete");

            tail_enabled_ = true;

            if (seed) seed->PrintLine("    PartReverb: Tail init done");
        } else {
            tail_enabled_ = false;
        }

        if (seed) seed->PrintLine("    PartReverb: Calling PartitionedReverb::Reset()");

        Reset();
        initialized_ = true;

        if (seed) seed->PrintLine("    PartReverb: PartitionedReverb Init complete!");
    }

    void Reset() {
        head_.Reset();
        if (tail_enabled_) {
            tail_.Reset();
        }

        fifo_in_read_ = 0;
        fifo_in_write_ = 0;
        fifo_out_read_ = 0;
        fifo_out_write_ = 0;

        std::memset(fifo_in_, 0, sizeof(fifo_in_));
        std::memset(fifo_out_, 0, sizeof(fifo_out_));

        head_pending_ = 0;
        tail_pending_ = 0;
    }

    // Process N-sample hardware callback (generic)
    void ProcessBlockN(const float* in, float* out, size_t N) {
        if (!initialized_) {
            std::memcpy(out, in, N * sizeof(float));
            return;
        }

        // Push N samples into input FIFO
        for (size_t i = 0; i < N; ++i) {
            fifo_in_[fifo_in_write_ & (FIFO_SIZE - 1)] = in[i];
            fifo_in_write_++;
        }

        head_pending_ += N;
        tail_pending_ += N;

        // Process head engine (L_HEAD = 256)
        while (head_pending_ >= L_HEAD) {
            float in_block[L_HEAD];
            float out_block[L_HEAD];

            // Read L_HEAD samples from input FIFO
            for (size_t i = 0; i < L_HEAD; ++i) {
                size_t read_idx = (fifo_in_read_ + i) & (FIFO_SIZE - 1);
                in_block[i] = fifo_in_[read_idx];
            }

            // Process head
            head_.ProcessBlock(in_block, out_block);

            // Write L_HEAD samples to output FIFO
            for (size_t i = 0; i < L_HEAD; ++i) {
                size_t write_idx = (fifo_out_write_ + i) & (FIFO_SIZE - 1);
                fifo_out_[write_idx] = out_block[i];
            }

            fifo_out_write_ += L_HEAD;
            head_pending_ -= L_HEAD;
        }

        // Process tail engine (L_TAIL = 4096)
        if (tail_enabled_) {
            while (tail_pending_ >= L_TAIL) {
                float in_block[L_TAIL];
                float out_block[L_TAIL];

                // Read L_TAIL samples from input FIFO (aligned with head input)
                size_t tail_read_pos = fifo_in_write_ - tail_pending_;
                for (size_t i = 0; i < L_TAIL; ++i) {
                    size_t read_idx = (tail_read_pos + i) & (FIFO_SIZE - 1);
                    in_block[i] = fifo_in_[read_idx];
                }

                // Process tail
                tail_.ProcessBlock(in_block, out_block);

                // ADD tail output to existing output FIFO (mix with head)
                // Tail output aligns with the input position it read from
                size_t tail_out_pos = fifo_out_write_ - tail_pending_;
                for (size_t i = 0; i < L_TAIL; ++i) {
                    size_t write_idx = (tail_out_pos + i) & (FIFO_SIZE - 1);
                    fifo_out_[write_idx] += out_block[i];
                }

                tail_pending_ -= L_TAIL;
            }
        }

        // Pop N samples from output FIFO
        size_t available = fifo_out_write_ - fifo_out_read_;
        if (available >= N) {
            for (size_t i = 0; i < N; ++i) {
                size_t read_idx = (fifo_out_read_ + i) & (FIFO_SIZE - 1);
                out[i] = fifo_out_[read_idx];
            }
            fifo_out_read_ += N;
            fifo_in_read_ += N;
        } else {
            // Underflow - output zeros until filled
            std::memset(out, 0, N * sizeof(float));
        }
    }

    // Convenience wrappers
    void ProcessBlock16(const float* in, float* out) { ProcessBlockN(in, out, 16); }
    void ProcessBlock48(const float* in, float* out) { ProcessBlockN(in, out, 48); }
    void ProcessBlock128(const float* in, float* out) { ProcessBlockN(in, out, 128); }

    void SetMix(float mix) { mix_ = mix; }
    float GetMix() const { return mix_; }

private:
    FDLConvolver<L_HEAD> head_;
    FDLConvolver<L_TAIL> tail_;

    bool initialized_ = false;
    bool tail_enabled_ = false;
    size_t ir_len_ = 0;
    int channel_ = 0;  // 0 = left, 1 = right
    float mix_ = 1.0f;

    // Input FIFO (shared by head and tail)
    float fifo_in_[FIFO_SIZE];
    size_t fifo_in_read_ = 0;
    size_t fifo_in_write_ = 0;

    // Output FIFO
    float fifo_out_[FIFO_SIZE];
    size_t fifo_out_read_ = 0;
    size_t fifo_out_write_ = 0;

    // Pending sample counts
    size_t head_pending_ = 0;
    size_t tail_pending_ = 0;
};
