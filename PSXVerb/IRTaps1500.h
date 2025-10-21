// 1500-tap FIR arrays for 4 PSX IRs (31.25ms @ 48kHz)
// Stored in QSPI Flash: 4 × 1500 × 4 bytes = 24KB
#pragma once

#include <cstring>

namespace IRTaps {

// Forward declarations of tap arrays (defined in IRTaps1500.cpp)
extern const float kStudio_Taps[1500];
extern const float kChurch_Taps[1500];
extern const float kDome_Taps[1500];
extern const float kHall_Taps[1500];

} // namespace IRTaps
