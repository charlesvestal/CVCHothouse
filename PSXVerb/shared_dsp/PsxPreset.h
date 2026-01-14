// PsxPreset.h - PSX SPU Reverb preset definitions
// Based on psx-spx documentation and jsgroth blog
#pragma once

#include <cstdint>

struct PsxPreset {
    // All-pass filter parameters
    uint16_t dAPF1, dAPF2;           // APF displacement offsets
    int16_t vIIR;                    // Reflection volume 1 (IIR feedback)
    int16_t vCOMB1, vCOMB2, vCOMB3, vCOMB4;  // Comb filter volumes
    int16_t vWALL;                   // Reflection volume 2 (wall reflection)
    int16_t vAPF1, vAPF2;            // APF volumes

    // Same-side reflection addresses and offsets
    uint16_t mLSAME, mRSAME;         // Memory addresses for same-side reflections
    uint16_t dLSAME, dRSAME;         // Displacement offsets for same-side

    // Different-side reflection addresses and offsets (cross-channel)
    uint16_t mLDIFF, mRDIFF;         // Memory addresses for different-side
    uint16_t dLDIFF, dRDIFF;         // Displacement offsets for different-side

    // Comb filter addresses
    uint16_t mLCOMB1, mRCOMB1;
    uint16_t mLCOMB2, mRCOMB2;
    uint16_t mLCOMB3, mRCOMB3;
    uint16_t mLCOMB4, mRCOMB4;

    // All-pass filter addresses
    uint16_t mLAPF1, mRAPF1;
    uint16_t mLAPF2, mRAPF2;

    // Input volumes
    int16_t vLIN, vRIN;

    // Output volumes
    int16_t vLOUT, vROUT;

    // Work area size (in bytes, must be power of 2)
    uint32_t work_size;

    const char* name;
};

// Scale delay values from 22.05kHz to target rate
inline uint16_t ScaleDelay(uint16_t psx_delay, float target_rate_hz = 24000.0f) {
    constexpr float kPsxRate = 22050.0f;
    return static_cast<uint16_t>((psx_delay * target_rate_hz / kPsxRate) + 0.5f);
}

// Convert SPU signed 16-bit coefficient to float [-1.0, 1.0)
inline float CoeffToFloat(int16_t coeff) {
    return static_cast<float>(coeff) / 32768.0f;
}

// PSX SPU Reverb Presets (from psx-spx documentation)
// All values in hexadecimal as specified in SPU registers

namespace PsxPresets {

constexpr PsxPreset kRoom = {
    .dAPF1 = 0x007D, .dAPF2 = 0x005B,
    .vIIR = 0x6D80, .vCOMB1 = 0x54B8, .vCOMB2 = static_cast<int16_t>(0xBED0),
    .vCOMB3 = 0x0000, .vCOMB4 = 0x0000,
    .vWALL = static_cast<int16_t>(0xBA80),
    .vAPF1 = 0x5800, .vAPF2 = 0x5300,
    .mLSAME = 0x04D6, .mRSAME = 0x0333,
    .dLSAME = 0x0334, .dRSAME = 0x01B5,
    .mLDIFF = 0x0000, .mRDIFF = 0x0000,
    .dLDIFF = 0x0000, .dRDIFF = 0x0000,
    .mLCOMB1 = 0x03F0, .mRCOMB1 = 0x0227,
    .mLCOMB2 = 0x0374, .mRCOMB2 = 0x01EF,
    .mLCOMB3 = 0x0000, .mRCOMB3 = 0x0000,
    .mLCOMB4 = 0x0000, .mRCOMB4 = 0x0000,
    .mLAPF1 = 0x01B4, .mRAPF1 = 0x0136,
    .mLAPF2 = 0x00B8, .mRAPF2 = 0x005C,
    .vLIN = static_cast<int16_t>(0x8000), .vRIN = static_cast<int16_t>(0x8000),
    .vLOUT = static_cast<int16_t>(0x8000), .vROUT = static_cast<int16_t>(0x8000),
    .work_size = 0x26C0,
    .name = "Room"
};

constexpr PsxPreset kStudioSmall = {
    .dAPF1 = 0x0033, .dAPF2 = 0x0025,
    .vIIR = 0x70F0, .vCOMB1 = 0x4FA8, .vCOMB2 = static_cast<int16_t>(0xBCE0),
    .vCOMB3 = 0x4410, .vCOMB4 = static_cast<int16_t>(0xC0F0),
    .vWALL = static_cast<int16_t>(0x9C00),
    .vAPF1 = 0x5280, .vAPF2 = 0x4EC0,
    .mLSAME = 0x03E4, .mRSAME = 0x031B,
    .dLSAME = 0x031C, .dRSAME = 0x025D,
    .mLDIFF = 0x025C, .mRDIFF = 0x018E,
    .dLDIFF = 0x018F, .dRDIFF = 0x00B5,
    .mLCOMB1 = 0x03A4, .mRCOMB1 = 0x02AF,
    .mLCOMB2 = 0x0372, .mRCOMB2 = 0x0266,
    .mLCOMB3 = 0x022F, .mRCOMB3 = 0x0135,
    .mLCOMB4 = 0x01D2, .mRCOMB4 = 0x00B7,
    .mLAPF1 = 0x00B4, .mRAPF1 = 0x0080,
    .mLAPF2 = 0x004C, .mRAPF2 = 0x0026,
    .vLIN = static_cast<int16_t>(0x8000), .vRIN = static_cast<int16_t>(0x8000),
    .vLOUT = static_cast<int16_t>(0x8000), .vROUT = static_cast<int16_t>(0x8000),
    .work_size = 0x1F40,
    .name = "Studio Small"
};

constexpr PsxPreset kStudioMedium = {
    .dAPF1 = 0x00B1, .dAPF2 = 0x007F,
    .vIIR = 0x70F0, .vCOMB1 = 0x4FA8, .vCOMB2 = static_cast<int16_t>(0xBCE0),
    .vCOMB3 = 0x4510, .vCOMB4 = static_cast<int16_t>(0xBEF0),
    .vWALL = static_cast<int16_t>(0xB4C0),
    .vAPF1 = 0x5280, .vAPF2 = 0x4EC0,
    .mLSAME = 0x0904, .mRSAME = 0x076B,
    .dLSAME = 0x076C, .dRSAME = 0x05ED,
    .mLDIFF = 0x05EC, .mRDIFF = 0x042E,
    .dLDIFF = 0x042F, .dRDIFF = 0x0265,
    .mLCOMB1 = 0x0824, .mRCOMB1 = 0x065F,
    .mLCOMB2 = 0x07A2, .mRCOMB2 = 0x0616,
    .mLCOMB3 = 0x050F, .mRCOMB3 = 0x0305,
    .mLCOMB4 = 0x0462, .mRCOMB4 = 0x02B7,
    .mLAPF1 = 0x0264, .mRAPF1 = 0x01B2,
    .mLAPF2 = 0x0100, .mRAPF2 = 0x0080,
    .vLIN = static_cast<int16_t>(0x8000), .vRIN = static_cast<int16_t>(0x8000),
    .vLOUT = static_cast<int16_t>(0x8000), .vROUT = static_cast<int16_t>(0x8000),
    .work_size = 0x4840,
    .name = "Studio Medium"
};

constexpr PsxPreset kStudioLarge = {
    .dAPF1 = 0x00E3, .dAPF2 = 0x00A9,
    .vIIR = 0x6F60, .vCOMB1 = 0x4FA8, .vCOMB2 = static_cast<int16_t>(0xBCE0),
    .vCOMB3 = 0x4510, .vCOMB4 = static_cast<int16_t>(0xBEF0),
    .vWALL = static_cast<int16_t>(0xA680),
    .vAPF1 = 0x5680, .vAPF2 = 0x52C0,
    .mLSAME = 0x0DFB, .mRSAME = 0x0B58,
    .dLSAME = 0x0B59, .dRSAME = 0x08DA,
    .mLDIFF = 0x08D9, .mRDIFF = 0x05E9,
    .dLDIFF = 0x05EA, .dRDIFF = 0x031D,
    .mLCOMB1 = 0x0D09, .mRCOMB1 = 0x0A3C,
    .mLCOMB2 = 0x0BD9, .mRCOMB2 = 0x0973,
    .mLCOMB3 = 0x07EC, .mRCOMB3 = 0x04B0,
    .mLCOMB4 = 0x06EF, .mRCOMB4 = 0x03D2,
    .mLAPF1 = 0x031C, .mRAPF1 = 0x0238,
    .mLAPF2 = 0x0154, .mRAPF2 = 0x00AA,
    .vLIN = static_cast<int16_t>(0x8000), .vRIN = static_cast<int16_t>(0x8000),
    .vLOUT = static_cast<int16_t>(0x8000), .vROUT = static_cast<int16_t>(0x8000),
    .work_size = 0x6FE0,
    .name = "Studio Large"
};

constexpr PsxPreset kHall = {
    .dAPF1 = 0x01A5, .dAPF2 = 0x0139,
    .vIIR = 0x6000, .vCOMB1 = 0x5000, .vCOMB2 = 0x4C00,
    .vCOMB3 = static_cast<int16_t>(0xB800), .vCOMB4 = static_cast<int16_t>(0xBC00),
    .vWALL = static_cast<int16_t>(0xC000),
    .vAPF1 = 0x6000, .vAPF2 = 0x5C00,
    .mLSAME = 0x15BA, .mRSAME = 0x11BB,
    .dLSAME = 0x11C0, .dRSAME = 0x0DC3,
    .mLDIFF = 0x0DC0, .mRDIFF = 0x09C1,
    .dLDIFF = 0x09C2, .dRDIFF = 0x05C1,
    .mLCOMB1 = 0x14C2, .mRCOMB1 = 0x10BD,
    .mLCOMB2 = 0x11BC, .mRCOMB2 = 0x0DC1,
    .mLCOMB3 = 0x0BC4, .mRCOMB3 = 0x07C1,
    .mLCOMB4 = 0x0A00, .mRCOMB4 = 0x06CD,
    .mLAPF1 = 0x05C0, .mRAPF1 = 0x041A,
    .mLAPF2 = 0x0274, .mRAPF2 = 0x013A,
    .vLIN = static_cast<int16_t>(0x8000), .vRIN = static_cast<int16_t>(0x8000),
    .vLOUT = static_cast<int16_t>(0x8000), .vROUT = static_cast<int16_t>(0x8000),
    .work_size = 0xADE0,
    .name = "Hall"
};

constexpr PsxPreset kSpaceEcho = {
    .dAPF1 = 0x033D, .dAPF2 = 0x0231,
    .vIIR = 0x7E00, .vCOMB1 = 0x5000, .vCOMB2 = static_cast<int16_t>(0xB400),
    .vCOMB3 = static_cast<int16_t>(0xB000), .vCOMB4 = 0x4C00,
    .vWALL = static_cast<int16_t>(0xB000),
    .vAPF1 = 0x6000, .vAPF2 = 0x5400,
    .mLSAME = 0x1ED6, .mRSAME = 0x1A31,
    .dLSAME = 0x1A32, .dRSAME = 0x15EF,
    .mLDIFF = 0x15EE, .mRDIFF = 0x1055,
    .dLDIFF = 0x1056, .dRDIFF = 0x0AE1,
    .mLCOMB1 = 0x1D14, .mRCOMB1 = 0x183B,
    .mLCOMB2 = 0x1BC2, .mRCOMB2 = 0x16B2,
    .mLCOMB3 = 0x1334, .mRCOMB3 = 0x0F2D,
    .mLCOMB4 = 0x11F6, .mRCOMB4 = 0x0C5D,
    .mLAPF1 = 0x0AE0, .mRAPF1 = 0x07A2,
    .mLAPF2 = 0x0464, .mRAPF2 = 0x0232,
    .vLIN = static_cast<int16_t>(0x8000), .vRIN = static_cast<int16_t>(0x8000),
    .vLOUT = static_cast<int16_t>(0x8000), .vROUT = static_cast<int16_t>(0x8000),
    .work_size = 0xF6C0,
    .name = "Space Echo"
};

// Array of all presets for easy iteration
constexpr const PsxPreset* kAllPresets[] = {
    &kRoom,
    &kStudioSmall,
    &kStudioMedium,
    &kStudioLarge,
    &kHall,
    &kSpaceEcho
};

constexpr int kNumPresets = 6;

} // namespace PsxPresets
