#pragma once

#include <cstddef>
#include <cstring>
#include <cmath>

/**
 * PSX-Style FDN Reverb - Pure Algorithmic
 *
 * Architecture:
 * - Early reflection taps (8 delays with gains)
 * - 8 parallel comb filters with HF damping
 * - 4 series allpass filters for diffusion
 * - No convolution - matches PSX SPU design
 */

class PsxFDNReverb {
public:
    static constexpr size_t MAX_DELAY = 2400;  // 50ms @ 48kHz (reduced for SRAM)
    static constexpr size_t MAX_EARLY_TAPS = 8;
    static constexpr size_t MAX_COMBS = 8;
    static constexpr size_t MAX_ALLPASS = 4;

    PsxFDNReverb() : fs_(48000.0f) {
        Reset();
    }

    void Init(float fs) {
        fs_ = fs;
        Reset();
    }

    void Configure(
        const float* early_delays_ms,
        const float* early_gains,
        size_t num_early,
        const float* comb_delays_ms,
        const float* comb_gains,
        size_t num_combs,
        float lpf_alpha,
        const float* allpass_delays_ms,
        const float* allpass_coeffs,
        size_t num_allpass
    ) {
        // Early reflections
        num_early_taps_ = (num_early > MAX_EARLY_TAPS) ? MAX_EARLY_TAPS : num_early;
        for (size_t i = 0; i < num_early_taps_; ++i) {
            early_delays_[i] = static_cast<size_t>(early_delays_ms[i] * fs_ / 1000.0f);
            if (early_delays_[i] >= MAX_DELAY) early_delays_[i] = MAX_DELAY - 1;
            if (early_delays_[i] == 0) early_delays_[i] = 1;
            early_gains_[i] = early_gains[i];
        }

        // Comb filters
        num_combs_ = (num_combs > MAX_COMBS) ? MAX_COMBS : num_combs;
        for (size_t i = 0; i < num_combs_; ++i) {
            comb_delays_[i] = static_cast<size_t>(comb_delays_ms[i] * fs_ / 1000.0f);
            if (comb_delays_[i] >= MAX_DELAY) comb_delays_[i] = MAX_DELAY - 1;
            if (comb_delays_[i] == 0) comb_delays_[i] = 1;
            comb_gains_[i] = comb_gains[i];
        }

        lpf_alpha_ = lpf_alpha;

        // Allpass filters
        num_allpass_ = (num_allpass > MAX_ALLPASS) ? MAX_ALLPASS : num_allpass;
        for (size_t i = 0; i < num_allpass_; ++i) {
            allpass_delays_[i] = static_cast<size_t>(allpass_delays_ms[i] * fs_ / 1000.0f);
            if (allpass_delays_[i] >= MAX_DELAY) allpass_delays_[i] = MAX_DELAY - 1;
            if (allpass_delays_[i] == 0) allpass_delays_[i] = 1;
            allpass_coeffs_[i] = allpass_coeffs[i];
        }

        Reset();
    }

    void Reset() {
        std::memset(input_buf_L_, 0, sizeof(input_buf_L_));
        std::memset(input_buf_R_, 0, sizeof(input_buf_R_));
        std::memset(comb_buf_L_, 0, sizeof(comb_buf_L_));
        std::memset(comb_buf_R_, 0, sizeof(comb_buf_R_));
        std::memset(allpass_buf_L_, 0, sizeof(allpass_buf_L_));
        std::memset(allpass_buf_R_, 0, sizeof(allpass_buf_R_));

        for (size_t i = 0; i < MAX_COMBS; ++i) {
            comb_lpf_state_L_[i] = 0.0f;
            comb_lpf_state_R_[i] = 0.0f;
        }

        input_pos_ = 0;
        for (size_t i = 0; i < MAX_COMBS; ++i) comb_pos_[i] = 0;
        for (size_t i = 0; i < MAX_ALLPASS; ++i) allpass_pos_[i] = 0;
    }

    void ProcessBlock(const float* inL, const float* inR,
                     float* outL, float* outR, size_t N) {
        for (size_t n = 0; n < N; ++n) {
            // Write input to delay line
            input_buf_L_[input_pos_] = inL[n];
            input_buf_R_[input_pos_] = inR[n];

            // Early reflections (tap delays)
            float earlyL = 0.0f;
            float earlyR = 0.0f;

            for (size_t t = 0; t < num_early_taps_; ++t) {
                size_t tap_pos = (input_pos_ + MAX_DELAY - early_delays_[t]) % MAX_DELAY;
                earlyL += input_buf_L_[tap_pos] * early_gains_[t];
                earlyR += input_buf_R_[tap_pos] * early_gains_[t];
            }

            input_pos_ = (input_pos_ + 1) % MAX_DELAY;

            // Parallel combs with damping (fed by early reflections)
            float comb_sumL = 0.0f;
            float comb_sumR = 0.0f;

            for (size_t c = 0; c < num_combs_; ++c) {
                size_t delay = comb_delays_[c];
                size_t pos = comb_pos_[c];

                // Read from delay line
                float delayedL = comb_buf_L_[c][pos];
                float delayedR = comb_buf_R_[c][pos];

                // Apply 1-pole LPF to delayed signal (damping)
                comb_lpf_state_L_[c] += lpf_alpha_ * (delayedL - comb_lpf_state_L_[c]);
                comb_lpf_state_R_[c] += lpf_alpha_ * (delayedR - comb_lpf_state_R_[c]);

                // Feedback with gain
                float fbL = comb_lpf_state_L_[c] * comb_gains_[c];
                float fbR = comb_lpf_state_R_[c] * comb_gains_[c];

                // Write early reflections + feedback to delay line
                comb_buf_L_[c][pos] = earlyL + fbL;
                comb_buf_R_[c][pos] = earlyR + fbR;

                // Accumulate output
                comb_sumL += delayedL;
                comb_sumR += delayedR;

                // Advance position
                comb_pos_[c] = (pos + 1) % delay;
            }

            // Normalize comb output
            if (num_combs_ > 0) {
                comb_sumL /= num_combs_;
                comb_sumR /= num_combs_;
            }

            // Series allpass chain (diffusion)
            float diffL = comb_sumL;
            float diffR = comb_sumR;

            for (size_t a = 0; a < num_allpass_; ++a) {
                size_t delay = allpass_delays_[a];
                size_t pos = allpass_pos_[a];
                float coeff = allpass_coeffs_[a];

                float delayedL = allpass_buf_L_[a][pos];
                float delayedR = allpass_buf_R_[a][pos];

                // Allpass: y = -a*x + d + a*y
                float outL_ap = -coeff * diffL + delayedL;
                float outR_ap = -coeff * diffR + delayedR;

                allpass_buf_L_[a][pos] = diffL + coeff * outL_ap;
                allpass_buf_R_[a][pos] = diffR + coeff * outR_ap;

                diffL = outL_ap;
                diffR = outR_ap;

                allpass_pos_[a] = (pos + 1) % delay;
            }

            outL[n] = diffL;
            outR[n] = diffR;
        }
    }

private:
    float fs_;
    size_t num_early_taps_;
    size_t num_combs_;
    size_t num_allpass_;
    float lpf_alpha_;

    // Input buffer for early reflection taps
    float input_buf_L_[MAX_DELAY];
    float input_buf_R_[MAX_DELAY];
    size_t input_pos_;

    // Early reflection taps
    size_t early_delays_[MAX_EARLY_TAPS];
    float early_gains_[MAX_EARLY_TAPS];

    // Comb filters
    float comb_buf_L_[MAX_COMBS][MAX_DELAY];
    float comb_buf_R_[MAX_COMBS][MAX_DELAY];
    size_t comb_delays_[MAX_COMBS];
    float comb_gains_[MAX_COMBS];
    size_t comb_pos_[MAX_COMBS];
    float comb_lpf_state_L_[MAX_COMBS];
    float comb_lpf_state_R_[MAX_COMBS];

    // Allpass filters
    float allpass_buf_L_[MAX_ALLPASS][MAX_DELAY];
    float allpass_buf_R_[MAX_ALLPASS][MAX_DELAY];
    size_t allpass_delays_[MAX_ALLPASS];
    float allpass_coeffs_[MAX_ALLPASS];
    size_t allpass_pos_[MAX_ALLPASS];
};
