// Precomputed IR tail parameters - auto-generated
// Each IR has unique comb delays, RT60, and damping
#pragma once
#include "IRAnalysis.h"

namespace PrecomputedProfiles {
constexpr size_t kNumTaps = 256;

// Global tap storage (loaded at startup)
// 4 IRs × 256 taps × 4 bytes = 4KB in SRAM
extern float g_AllTaps[4 * 256];

constexpr IRProfile kProfiles[4] = {
    // Studio: Short RT60 (0.88s), tight comb spacing
    {
        .taps = {}, .N_taps = 256,
        .rt60_band = {0.8767f, 0.8193f, 0.7221f},
        .hf_damp = 10000.00f,
        .comb_delays_ms = {29.7f, 33.1f, 36.7f, 40.1f, 0.0f, 0.0f},
        .comb_gains = {0.951f, 0.946f, 0.942f, 0.937f, 0.0f, 0.0f},
        .N_combs = 4, .lpf_alpha = 0.729909f,
        .allpass_delays_ms = {5.0f, 9.0f, 11.0f},
        .allpass_coeffs = {0.7000f, 0.7500f, 0.8000f},
        .N_allpass = 3, .predelay_ms = 31.25f, .T_head_ms = 31.25f, .peak_level = 1.0f
    },
    // Church: Medium RT60 (1.10s), moderate comb spacing
    {
        .taps = {}, .N_taps = 256,
        .rt60_band = {1.0959f, 1.0150f, 0.8583f},
        .hf_damp = 7000.00f,
        .comb_delays_ms = {33.3f, 37.3f, 41.1f, 44.7f, 0.0f, 0.0f},
        .comb_gains = {0.961f, 0.957f, 0.953f, 0.949f, 0.0f, 0.0f},
        .N_combs = 4, .lpf_alpha = 0.600003f,
        .allpass_delays_ms = {6.0f, 10.0f, 12.0f},
        .allpass_coeffs = {0.7000f, 0.7500f, 0.8000f},
        .N_allpass = 3, .predelay_ms = 31.25f, .T_head_ms = 31.25f, .peak_level = 1.0f
    },
    // Dome: Long RT60 (2.14s), wide comb spacing
    {
        .taps = {}, .N_taps = 256,
        .rt60_band = {2.1078f, 2.1397f, 2.0502f},
        .hf_damp = 10000.00f,
        .comb_delays_ms = {37.0f, 41.0f, 43.0f, 47.0f, 0.0f, 0.0f},
        .comb_gains = {0.978f, 0.976f, 0.975f, 0.973f, 0.0f, 0.0f},
        .N_combs = 4, .lpf_alpha = 0.729909f,
        .allpass_delays_ms = {7.0f, 11.0f, 13.0f},
        .allpass_coeffs = {0.7000f, 0.7500f, 0.8000f},
        .N_allpass = 3, .predelay_ms = 31.25f, .T_head_ms = 31.25f, .peak_level = 1.0f
    },
    // Hall: Longest RT60 (2.52s), widest comb spacing
    {
        .taps = {}, .N_taps = 256,
        .rt60_band = {2.5203f, 2.3150f, 1.8548f},
        .hf_damp = 7000.00f,
        .comb_delays_ms = {39.7f, 43.3f, 47.1f, 50.3f, 0.0f, 0.0f},
        .comb_gains = {0.982f, 0.980f, 0.979f, 0.977f, 0.0f, 0.0f},
        .N_combs = 4, .lpf_alpha = 0.600003f,
        .allpass_delays_ms = {8.0f, 12.0f, 14.0f},
        .allpass_coeffs = {0.7000f, 0.7500f, 0.8000f},
        .N_allpass = 3, .predelay_ms = 31.25f, .T_head_ms = 31.25f, .peak_level = 1.0f
    }
};
} // namespace PrecomputedProfiles
