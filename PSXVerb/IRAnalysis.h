#pragma once

#include <cstddef>
#include <cmath>

/**
 * IR Analysis - Objective Parameter Extraction
 *
 * Analyzes impulse response to extract:
 * - Early reflection FIR taps (≤40ms)
 * - Banded RT60 for tail fitting
 * - Comb delays from autocorrelation
 * - HF damping characteristics
 * - Allpass diffusion parameters
 *
 * All decisions driven by measurements, zero manual tuning.
 */

struct IRProfile {
    // Early reflections (FIR)
    float taps[2000];           // Max 2000 taps (~42ms @ 48kHz)
    size_t N_taps;              // Actual tap count

    // Tail parameters (algorithmic reverb)
    float rt60_band[3];         // RT60 in seconds: [low, mid, high]
    float hf_damp;              // HF damping LPF cutoff (Hz)
    float comb_delays_ms[6];    // Comb filter delays (ms)
    float comb_gains[6];        // Comb feedback gains
    size_t N_combs;             // Number of combs
    float lpf_alpha;            // 1-pole LPF coefficient for damping
    float allpass_delays_ms[3]; // Allpass delays (ms)
    float allpass_coeffs[3];    // Allpass coefficients
    size_t N_allpass;           // Number of allpass stages
    float predelay_ms;          // Predelay before tail (ms)

    // Metadata
    float T_head_ms;            // Early/late split point
    float peak_level;           // Normalized peak
};

class IRAnalysis {
public:
    /**
     * Analyze IR and extract all parameters
     *
     * @param ir Input impulse response (mono, float)
     * @param N Length of IR in samples
     * @param fs Sample rate (Hz)
     * @param profile Output parameter structure
     * @return true if successful
     */
    static bool AnalyzeIR(const float* ir, size_t N, float fs, IRProfile* profile);

private:
    // Helper functions
    static void TrimLeadingSilence(const float* ir, size_t N, size_t* start_idx);
    static float MeasureNoiseFloor(const float* ir, size_t N);
    static void ComputeEDC(const float* ir, size_t N, float* edc);
    static float FindT_head(const float* edc, size_t N, float fs);
    static void ExtractFIRTaps(const float* ir, size_t N_taps, float* taps);
    static void MeasureBandedRT60(const float* ir, size_t start_idx, size_t N, float fs, float rt60[3]);
    static float EstimateHFDamping(const float rt60[3], float fs);
    static void FindCombDelays(const float* ir, size_t start_idx, size_t N, float fs,
                               float delays_ms[], float* gains, size_t* N_combs);
    static void FitAllpassParams(const float* ir, size_t start_idx, size_t N,
                                float delays_ms[], float coeffs[], size_t* N_allpass);

    // DSP utilities
    static void BandpassFilter(const float* in, size_t N, float fs,
                              float f_low, float f_high, float* out);
    static void Autocorrelation(const float* x, size_t N, size_t max_lag, float* acf);
    static void ApplyWindow(float* data, size_t N, const char* window_type);
};
