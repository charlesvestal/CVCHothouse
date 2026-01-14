// Halfband39.h - 39-tap linear-phase halfband FIR for 48kHz <-> 24kHz
// Used for PSX reverb internal processing at half sample rate
#pragma once

#include <cstdint>
#include <cstring>

class Halfband39 {
public:
    Halfband39() { Init(); }

    void Init() {
        std::memset(state_, 0, sizeof(state_));
        pos_ = 0;
    }

    // Decimate: 48kHz -> 24kHz (call every 2 input samples, returns 1 output)
    float Decimate(float x0, float x1) {
        // Push two new samples
        state_[pos_] = x0;
        pos_ = (pos_ + 1) & 0x3F;  // wrap at 64
        state_[pos_] = x1;
        pos_ = (pos_ + 1) & 0x3F;

        // Convolve with ALL 39 taps (CRITICAL: must process all taps to prevent aliasing)
        float sum = 0.0f;
        int idx = pos_;

        // Process every tap, including zeros (can't skip for decimation!)
        for (int i = 0; i < 39; ++i) {
            idx = (idx - 1) & 0x3F;
            sum += kCoeffs[i] * state_[idx];
        }

        return sum;
    }

    // Interpolate: 24kHz -> 48kHz (call once per 24kHz tick, produces 2 samples)
    void Interpolate(float in_tick, float* out_s0, float* out_s1) {
        // Push upsampled input (zero-stuff: [in, 0])
        state_[pos_] = in_tick;
        pos_ = (pos_ + 1) & 0x3F;

        // Polyphase decomposition: phase 0 and phase 1
        // Phase 0: even coefficients (produces sample 0)
        float sum0 = 0.0f;
        int idx = pos_;
        for (int i = 0; i < 20; ++i) {  // 20 even taps
            idx = (idx - 1) & 0x3F;
            sum0 += kCoeffsPhase0[i] * state_[idx];
        }

        // Phase 1: odd coefficients (produces sample 1)
        state_[pos_] = 0.0f;  // zero-stuffed sample
        pos_ = (pos_ + 1) & 0x3F;

        float sum1 = 0.0f;
        idx = pos_;
        for (int i = 0; i < 19; ++i) {  // 19 odd taps
            idx = (idx - 1) & 0x3F;
            sum1 += kCoeffsPhase1[i] * state_[idx];
        }

        *out_s0 = sum0 * 2.0f;  // Compensate for zero-stuffing
        *out_s1 = sum1 * 2.0f;
    }

private:
    static constexpr int kTaps = 39;
    static constexpr int kStateSize = 64;  // Power of 2, > 39 for safety

    // Halfband FIR coefficients (39 taps, Fs=48kHz, Fc=12kHz)
    // Linear phase, halfband: every other tap is zero except center
    static constexpr float kCoeffs[39] = {
        -0.000275135f,  // 0
         0.0f,          // 1
        -0.001467466f,  // 2
         0.0f,          // 3
        -0.004356503f,  // 4
         0.0f,          // 5
        -0.009765625f,  // 6
         0.0f,          // 7
        -0.018493652f,  // 8
         0.0f,          // 9
        -0.031494141f,  // 10
         0.0f,          // 11
        -0.050598145f,  // 12
         0.0f,          // 13
        -0.079833984f,  // 14
         0.0f,          // 15
        -0.130859375f,  // 16
         0.0f,          // 17
        -0.281494141f,  // 18
         0.632812500f,  // 19 CENTER TAP
        -0.281494141f,  // 20
         0.0f,          // 21
        -0.130859375f,  // 22
         0.0f,          // 23
        -0.079833984f,  // 24
         0.0f,          // 25
        -0.050598145f,  // 26
         0.0f,          // 27
        -0.031494141f,  // 28
         0.0f,          // 29
        -0.018493652f,  // 30
         0.0f,          // 31
        -0.009765625f,  // 32
         0.0f,          // 33
        -0.004356503f,  // 34
         0.0f,          // 35
        -0.001467466f,  // 36
         0.0f,          // 37
        -0.000275135f,  // 38
    };

    // Polyphase decomposition for interpolation
    // Phase 0: coeffs at even indices [0, 2, 4, ..., 38]
    static constexpr float kCoeffsPhase0[20] = {
        -0.000275135f, -0.001467466f, -0.004356503f, -0.009765625f,
        -0.018493652f, -0.031494141f, -0.050598145f, -0.079833984f,
        -0.130859375f, -0.281494141f,  0.632812500f, -0.281494141f,
        -0.130859375f, -0.079833984f, -0.050598145f, -0.031494141f,
        -0.018493652f, -0.009765625f, -0.004356503f, -0.001467466f
    };

    // Phase 1: coeffs at odd indices [1, 3, 5, ..., 37] (all zeros for halfband!)
    static constexpr float kCoeffsPhase1[19] = {
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };

    float state_[kStateSize];
    int pos_;
};
