#include "IRAnalysis.h"
#include <cstring>
#include <cmath>
#include <algorithm>

// Static buffer for EDC computation (reused across all IRs)
// Large enough for 400K samples (8.3 sec @ 48kHz)
// IMPORTANT: Place in SDRAM to avoid overflowing SRAM
float s_edc_buffer[400000] __attribute__((section(".sdram_bss")));

// Simplified implementation focusing on what will actually run on embedded
// Full statistical analysis would require more CPU than available at init

bool IRAnalysis::AnalyzeIR(const float* ir, size_t N, float fs, IRProfile* profile) {
    if (!ir || !profile || N < 1000) return false;

    // BYPASS trim for now - just start from 0
    // This is a debugging step to see if TrimLeadingSilence is the problem
    size_t start_idx = 0;
    // TrimLeadingSilence(ir, N, &start_idx);

    // Check if we trimmed too much
    // if (start_idx >= N - 1000) {
    //     // IR is all silence or too short after trimming - use from beginning
    //     start_idx = 0;
    // }

    // 2. Find peak and normalize
    float peak = 0.0f;
    for (size_t i = start_idx; i < N; ++i) {
        peak = std::max(peak, fabsf(ir[i]));
    }
    profile->peak_level = peak;

    // If peak is zero, IR is silent - bail out
    if (peak < 0.000001f) {
        return false;
    }

    // 3. Compute EDC (Energy Decay Curve) - use static buffer
    size_t edc_len = N - start_idx;
    if (edc_len > 400000) edc_len = 400000;  // Clamp to buffer size
    ComputeEDC(ir + start_idx, edc_len, s_edc_buffer);

    // 4. Find T_head (early/late split) - use much smaller FIR for testing
    // Start with 10ms (480 taps) to verify it works, then scale up
    profile->T_head_ms = 10.0f;  // Reduced for CPU budget
    profile->N_taps = static_cast<size_t>(profile->T_head_ms * fs / 1000.0f);

    // Clamp to limits
    if (profile->N_taps > 512) profile->N_taps = 512;  // Max 512 for testing
    if (profile->N_taps < 256) profile->N_taps = 256;

    // 5. Extract FIR taps with Hann window
    ExtractFIRTaps(ir + start_idx, profile->N_taps, profile->taps);

    // Debug: Check if taps are non-zero
    float tap_sum = 0.0f;
    for (size_t i = 0; i < profile->N_taps; ++i) {
        tap_sum += fabsf(profile->taps[i]);
    }
    // If tap_sum is zero, something is very wrong
    if (tap_sum < 0.0001f) {
        return false;  // Taps are all zero
    }

    // 6. Measure banded RT60 on tail portion
    size_t tail_start = start_idx + profile->N_taps;
    if (tail_start < N) {
        MeasureBandedRT60(ir, tail_start, N, fs, profile->rt60_band);
    } else {
        // Fallback if IR too short
        profile->rt60_band[0] = 2.0f;
        profile->rt60_band[1] = 1.5f;
        profile->rt60_band[2] = 1.0f;
    }

    // 7. Estimate HF damping from RT60 slope
    profile->hf_damp = EstimateHFDamping(profile->rt60_band, fs);

    // 8. Find comb delays from tail autocorrelation
    if (tail_start < N) {
        FindCombDelays(ir, tail_start, N, fs,
                      profile->comb_delays_ms,
                      profile->comb_gains,
                      &profile->N_combs);
    } else {
        // Fallback comb structure
        profile->N_combs = 4;
        profile->comb_delays_ms[0] = 37.0f;
        profile->comb_delays_ms[1] = 41.0f;
        profile->comb_delays_ms[2] = 43.0f;
        profile->comb_delays_ms[3] = 47.0f;

        // Compute gains from mid-band RT60
        for (size_t i = 0; i < profile->N_combs; ++i) {
            float L_samples = profile->comb_delays_ms[i] * fs / 1000.0f;
            profile->comb_gains[i] = powf(10.0f, -3.0f * L_samples / (profile->rt60_band[1] * fs));
        }
    }

    // 9. Compute LPF alpha for HF damping
    // Simple exponential mapping: α = 1 - e^(-2π fc/fs)
    float fc = profile->hf_damp;
    profile->lpf_alpha = 1.0f - expf(-2.0f * 3.14159265f * fc / fs);

    // 10. Fit allpass parameters (simplified)
    profile->N_allpass = 3;
    profile->allpass_delays_ms[0] = 7.0f;
    profile->allpass_delays_ms[1] = 11.0f;
    profile->allpass_delays_ms[2] = 13.0f;

    // Coefficients based on normalized decay spread
    float decay_spread = (profile->rt60_band[0] - profile->rt60_band[2]) / profile->rt60_band[1];
    float a_base = 0.6f + 0.2f * decay_spread;  // More spread → more diffusion
    for (size_t i = 0; i < profile->N_allpass; ++i) {
        profile->allpass_coeffs[i] = std::min(0.85f, a_base + 0.05f * i);
    }

    // 11. Predelay = T_head
    profile->predelay_ms = profile->T_head_ms;

    return true;
}

void IRAnalysis::TrimLeadingSilence(const float* ir, size_t N, size_t* start_idx) {
    const float threshold = 0.001f;  // -60 dBFS
    const size_t window = 480;  // 10ms @ 48kHz

    float rms = 0.0f;
    for (size_t i = 0; i < N - window; ++i) {
        // Compute RMS over window
        rms = 0.0f;
        for (size_t j = 0; j < window; ++j) {
            rms += ir[i + j] * ir[i + j];
        }
        rms = sqrtf(rms / window);

        if (rms > threshold) {
            *start_idx = i;
            return;
        }
    }
    *start_idx = 0;
}

float IRAnalysis::MeasureNoiseFloor(const float* ir, size_t N) {
    // Measure RMS of last 10%
    size_t start = (N * 9) / 10;
    float sum = 0.0f;
    for (size_t i = start; i < N; ++i) {
        sum += ir[i] * ir[i];
    }
    return sqrtf(sum / (N - start));
}

void IRAnalysis::ComputeEDC(const float* ir, size_t N, float* edc) {
    // Schroeder integration: EDC(t) = Σ_{n=t}^{end} x[n]^2
    edc[N - 1] = ir[N - 1] * ir[N - 1];
    for (int i = N - 2; i >= 0; --i) {
        edc[i] = edc[i + 1] + ir[i] * ir[i];
    }
}

float IRAnalysis::FindT_head(const float* edc, size_t N, float fs) {
    // Simplified: return fixed 40ms
    // Full implementation would do Lundeby late-time line fit
    return 40.0f;
}

void IRAnalysis::ExtractFIRTaps(const float* ir, size_t N_taps, float* taps) {
    // Copy IR with Hann window
    for (size_t i = 0; i < N_taps; ++i) {
        float w = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * i / (N_taps - 1)));
        taps[i] = ir[i] * w;
    }

    // Remove DC
    float mean = 0.0f;
    for (size_t i = 0; i < N_taps; ++i) {
        mean += taps[i];
    }
    mean /= N_taps;
    for (size_t i = 0; i < N_taps; ++i) {
        taps[i] -= mean;
    }

    // Scale to prevent clipping (find peak, scale to -3dBFS)
    float peak = 0.0f;
    for (size_t i = 0; i < N_taps; ++i) {
        peak = std::max(peak, fabsf(taps[i]));
    }
    if (peak > 0.0f) {
        float scale = 0.707f / peak;  // -3dBFS
        for (size_t i = 0; i < N_taps; ++i) {
            taps[i] *= scale;
        }
    }
}

void IRAnalysis::MeasureBandedRT60(const float* ir, size_t start_idx, size_t N, float fs,
                                   float rt60[3]) {
    // Simplified: use energy decay of whole tail
    // Full implementation would bandpass filter first

    size_t tail_len = N - start_idx;
    if (tail_len < 1000) {
        rt60[0] = rt60[1] = rt60[2] = 1.0f;
        return;
    }

    // Compute EDC on tail - use static buffer
    if (tail_len > 400000) tail_len = 400000;  // Clamp to buffer size
    ComputeEDC(ir + start_idx, tail_len, s_edc_buffer);

    // Find where EDC drops to -60dB from start
    float edc_start = s_edc_buffer[0];
    float edc_60db = edc_start * 0.001f;  // -60dB = 10^(-60/10)

    size_t t_60 = tail_len - 1;
    for (size_t i = 0; i < tail_len; ++i) {
        if (s_edc_buffer[i] < edc_60db) {
            t_60 = i;
            break;
        }
    }

    float measured_rt60 = t_60 / fs;

    // Simulate band-dependent decay (low decays slower, high faster)
    rt60[0] = measured_rt60 * 1.2f;  // Low: +20%
    rt60[1] = measured_rt60;         // Mid: reference
    rt60[2] = measured_rt60 * 0.7f;  // High: -30%
}

float IRAnalysis::EstimateHFDamping(const float rt60[3], float fs) {
    // HF damping cutoff based on high/mid RT60 ratio
    // More HF decay → lower cutoff
    float ratio = rt60[2] / rt60[1];

    // Map ratio [0.5..1.0] → cutoff [3000..10000] Hz
    float fc = 3000.0f + 7000.0f * (ratio - 0.5f) / 0.5f;
    return std::max(3000.0f, std::min(10000.0f, fc));
}

void IRAnalysis::FindCombDelays(const float* ir, size_t start_idx, size_t N, float fs,
                                float delays_ms[], float* gains, size_t* N_combs) {
    // Simplified: use fixed delay structure based on typical room modes
    // Full implementation would compute autocorrelation

    *N_combs = 4;

    // Classic Schroeder delays (primes to avoid common factors)
    delays_ms[0] = 37.0f + (start_idx % 5);  // 37-41ms
    delays_ms[1] = 41.0f + (start_idx % 5);
    delays_ms[2] = 43.0f + (start_idx % 5);
    delays_ms[3] = 47.0f + (start_idx % 5);

    // Gains computed in main AnalyzeIR function
}

void IRAnalysis::FitAllpassParams(const float* ir, size_t start_idx, size_t N,
                                  float delays_ms[], float coeffs[], size_t* N_allpass) {
    // Implemented in main AnalyzeIR function (simplified)
}

void IRAnalysis::BandpassFilter(const float* in, size_t N, float fs,
                                float f_low, float f_high, float* out) {
    // Simple 2-pole butterworth approximation
    // Not implemented for now - using simplified RT60 estimation
    std::memcpy(out, in, N * sizeof(float));
}

void IRAnalysis::Autocorrelation(const float* x, size_t N, size_t max_lag, float* acf) {
    for (size_t lag = 0; lag < max_lag && lag < N; ++lag) {
        float sum = 0.0f;
        for (size_t i = 0; i < N - lag; ++i) {
            sum += x[i] * x[i + lag];
        }
        acf[lag] = sum / (N - lag);
    }
}

void IRAnalysis::ApplyWindow(float* data, size_t N, const char* window_type) {
    for (size_t i = 0; i < N; ++i) {
        float w = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * i / (N - 1)));  // Hann
        data[i] *= w;
    }
}
