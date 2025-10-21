// PSX-Style FDN Parameters - Extracted from IR analysis
// Pure algorithmic reverb, no convolution
#pragma once
#include "IRAnalysis.h"

namespace FDNProfiles {

constexpr size_t kNumEarlyTaps = 8;
constexpr size_t kNumCombs = 8;
constexpr size_t kNumAllpass = 4;

// Studio
constexpr float kStudio_EarlyDelays[kNumEarlyTaps] = {
    0.10f, 42.60f, 59.10f, 65.08f, 81.60f, 98.29f, 87.77f, 75.60f
};
constexpr float kStudio_EarlyGains[kNumEarlyTaps] = {
    1.0000f, 0.5403f, 0.4973f, 0.4618f, 0.4091f, 0.3210f, 0.3142f, 0.3062f
};
constexpr float kStudio_CombDelays[kNumCombs] = {
    23.8f, 26.6f, 29.6f, 33.0f, 35.0f, 37.8f, 40.9f, 43.0f
};
constexpr float kStudio_CombGains[kNumCombs] = {
    0.9312f, 0.9232f, 0.9150f, 0.9056f, 0.9004f, 0.8927f, 0.8846f, 0.8791f
};

// Church
constexpr float kChurch_EarlyDelays[kNumEarlyTaps] = {
    0.10f, 64.54f, 74.02f, 40.96f, 23.58f, 31.40f, 95.40f, 88.12f
};
constexpr float kChurch_EarlyGains[kNumEarlyTaps] = {
    1.0000f, 0.3672f, 0.3620f, 0.3416f, 0.3307f, 0.3285f, 0.3271f, 0.3005f
};
constexpr float kChurch_CombDelays[kNumCombs] = {
    23.8f, 26.6f, 29.6f, 33.0f, 35.0f, 37.8f, 40.9f, 43.0f
};
constexpr float kChurch_CombGains[kNumCombs] = {
    0.9312f, 0.9232f, 0.9150f, 0.9056f, 0.9004f, 0.8927f, 0.8846f, 0.8791f
};

// Dome
constexpr float kDome_EarlyDelays[kNumEarlyTaps] = {
    0.10f, 45.85f, 93.02f, 102.32f, 112.56f, 123.81f, 136.19f, 149.81f
};
constexpr float kDome_EarlyGains[kNumEarlyTaps] = {
    1.0000f, 0.4944f, 0.4924f, 0.3940f, 0.3152f, 0.2521f, 0.2017f, 0.1614f
};
constexpr float kDome_CombDelays[kNumCombs] = {
    23.8f, 26.6f, 29.6f, 33.0f, 35.0f, 37.8f, 40.9f, 43.0f
};
constexpr float kDome_CombGains[kNumCombs] = {
    0.9312f, 0.9232f, 0.9150f, 0.9056f, 0.9004f, 0.8927f, 0.8846f, 0.8791f
};

// Hall
constexpr float kHall_EarlyDelays[kNumEarlyTaps] = {
    0.10f, 44.77f, 55.17f, 99.92f, 89.77f, 75.44f, 85.96f, 67.12f
};
constexpr float kHall_EarlyGains[kNumEarlyTaps] = {
    1.0000f, 0.4001f, 0.3514f, 0.3501f, 0.3214f, 0.3099f, 0.2803f, 0.2200f
};
constexpr float kHall_CombDelays[kNumCombs] = {
    23.8f, 26.6f, 29.6f, 33.0f, 35.0f, 37.8f, 40.9f, 43.0f
};
constexpr float kHall_CombGains[kNumCombs] = {
    0.9312f, 0.9232f, 0.9150f, 0.9056f, 0.9004f, 0.8927f, 0.8846f, 0.8791f
};

// Allpass parameters (shared across all presets for diffusion)
constexpr float kAllpassDelays[kNumAllpass] = {5.0f, 11.0f, 17.0f, 23.0f};
constexpr float kAllpassCoeffs[kNumAllpass] = {0.7f, 0.7f, 0.7f, 0.7f};

// HF damping LPF coefficients (based on RT60 analysis)
constexpr float kStudio_LPFAlpha = 0.65f;  // Brighter (less damping)
constexpr float kChurch_LPFAlpha = 0.55f;  // Medium damping
constexpr float kDome_LPFAlpha = 0.50f;    // More damping (darker)
constexpr float kHall_LPFAlpha = 0.52f;    // Medium-dark

} // namespace FDNProfiles