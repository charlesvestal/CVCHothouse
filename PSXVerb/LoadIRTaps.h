// Hardcoded FIR taps for all 4 IRs - 1500 taps each
// This file is auto-generated and quite large (~24KB of data)
// Compiled into FLASH but loaded into SRAM at startup

#pragma once
#include "PrecomputedIRProfiles.h"
#include <cstring>

// Forward declare - implementation in separate file to avoid compilation slowdown
void LoadAllIRTaps();
