#pragma once

#include <cstddef>
#include <cstring>
#include <cmath>
#include "IRAnalysis.h"

/**
 * PS1-Style Tail Reverb - Fitted to IR
 *
 * Schroeder/Moorer network:
 * - Parallel combs with HF damping (1-pole LPF in feedback)
 * - Series allpass for diffusion
 * - Predelay
 *
 * All parameters set objectively from IRProfile
 */

class PsxTailReverb {
public:
    static constexpr size_t MAX_DELAY = 4800;  // 100ms @ 48kHz
    static constexpr size_t MAX_COMBS = 6;
    static constexpr size_t MAX_ALLPASS = 3;

    PsxTailReverb() : fs_(48000.0f), N_combs_(0), N_allpass_(0) {
        Reset();
    }

    void Init(float fs) {
        fs_ = fs;
        Reset();
    }

    void SetFromProfile(const IRProfile& profile) {
        // Set comb filters
        N_combs_ = profile.N_combs;
        for (size_t i = 0; i < N_combs_; ++i) {
            size_t delay_samples = static_cast<size_t>(profile.comb_delays_ms[i] * fs_ / 1000.0f);
            if (delay_samples >= MAX_DELAY) delay_samples = MAX_DELAY - 1;
            if (delay_samples == 0) delay_samples = 1;  // Prevent division by zero

            comb_delays_[i] = delay_samples;
            comb_gains_[i] = profile.comb_gains[i];
            comb_lpf_state_L_[i] = 0.0f;
            comb_lpf_state_R_[i] = 0.0f;
        }

        lpf_alpha_ = profile.lpf_alpha;

        // Set allpass filters
        N_allpass_ = profile.N_allpass;
        for (size_t i = 0; i < N_allpass_; ++i) {
            size_t delay_samples = static_cast<size_t>(profile.allpass_delays_ms[i] * fs_ / 1000.0f);
            if (delay_samples >= MAX_DELAY) delay_samples = MAX_DELAY - 1;
            if (delay_samples == 0) delay_samples = 1;  // Prevent division by zero

            allpass_delays_[i] = delay_samples;
            allpass_coeffs_[i] = profile.allpass_coeffs[i];
        }

        // Predelay
        predelay_samples_ = static_cast<size_t>(profile.predelay_ms * fs_ / 1000.0f);
        if (predelay_samples_ >= MAX_DELAY) predelay_samples_ = MAX_DELAY - 1;
        if (predelay_samples_ == 0) predelay_samples_ = 1;  // Prevent division by zero
    }

    void Reset() {
        std::memset(predelay_L_, 0, sizeof(predelay_L_));
        std::memset(predelay_R_, 0, sizeof(predelay_R_));
        std::memset(comb_buf_L_, 0, sizeof(comb_buf_L_));
        std::memset(comb_buf_R_, 0, sizeof(comb_buf_R_));
        std::memset(allpass_buf_L_, 0, sizeof(allpass_buf_L_));
        std::memset(allpass_buf_R_, 0, sizeof(allpass_buf_R_));

        for (size_t i = 0; i < MAX_COMBS; ++i) {
            comb_lpf_state_L_[i] = 0.0f;
            comb_lpf_state_R_[i] = 0.0f;
        }

        predelay_pos_ = 0;
        for (size_t i = 0; i < MAX_COMBS; ++i) comb_pos_[i] = 0;
        for (size_t i = 0; i < MAX_ALLPASS; ++i) allpass_pos_[i] = 0;
    }

    void ProcessBlock(const float* inL, const float* inR,
                     float* outL, float* outR, size_t N) {
        for (size_t n = 0; n < N; ++n) {
            // Predelay
            float pdL = predelay_L_[predelay_pos_];
            float pdR = predelay_R_[predelay_pos_];
            predelay_L_[predelay_pos_] = inL[n];
            predelay_R_[predelay_pos_] = inR[n];
            predelay_pos_ = (predelay_pos_ + 1) % predelay_samples_;

            // Parallel combs with damping
            float comb_sumL = 0.0f;
            float comb_sumR = 0.0f;

            for (size_t c = 0; c < N_combs_; ++c) {
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

                // Write input + feedback to delay line
                comb_buf_L_[c][pos] = pdL + fbL;
                comb_buf_R_[c][pos] = pdR + fbR;

                // Accumulate output
                comb_sumL += delayedL;
                comb_sumR += delayedR;

                // Advance position
                comb_pos_[c] = (pos + 1) % delay;
            }

            // Normalize comb output
            if (N_combs_ > 0) {
                comb_sumL /= N_combs_;
                comb_sumR /= N_combs_;
            }

            // Series allpass chain (diffusion)
            float diffL = comb_sumL;
            float diffR = comb_sumR;

            for (size_t a = 0; a < N_allpass_; ++a) {
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
    size_t N_combs_;
    size_t N_allpass_;
    float lpf_alpha_;

    // Predelay
    float predelay_L_[MAX_DELAY];
    float predelay_R_[MAX_DELAY];
    size_t predelay_samples_;
    size_t predelay_pos_;

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
