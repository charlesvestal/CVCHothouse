// WorkArea.h - SPU "work area" RAM emulation
// Circular int16_t buffer with saturating writes (faithful to PSX SPU)
// PSX uses ABSOLUTE addresses in a circular buffer, not relative delays
#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>

class WorkArea {
public:
    WorkArea() : buf_(nullptr), size_mask_(0), base_(0) {}

    // Initialize with power-of-2 size buffer (must be static/SRAM allocated)
    void Init(int16_t* buffer, uint32_t size_pow2) {
        buf_ = buffer;
        size_mask_ = size_pow2 - 1;  // For fast modulo via AND
        base_ = 0;
        std::memset(buf_, 0, size_pow2 * sizeof(int16_t));
    }

    // Read from ABSOLUTE address (offset from base, wraps circularly)
    // PSX formula: ram[addr] where addr is absolute position in circular buffer
    inline float ReadAbsolute(uint32_t address) const {
        uint32_t idx = (base_ + address) & size_mask_;
        return static_cast<float>(buf_[idx]) * kInt16ToFloat;
    }

    // Read at relative offset from current base (for IIR: base - 1)
    inline float ReadRelative(int32_t offset) const {
        uint32_t idx = (base_ + offset) & size_mask_;
        return static_cast<float>(buf_[idx]) * kInt16ToFloat;
    }

    // Write to ABSOLUTE address with 16-bit saturation
    // PSX formula: ram[addr] = value
    inline void WriteAbsolute(uint32_t address, float value) {
        // Saturate to int16 range [-32768, 32767]
        int32_t val_int = static_cast<int32_t>(value * kFloatToInt16);
        val_int = std::max<int32_t>(-32768, std::min<int32_t>(32767, val_int));
        uint32_t idx = (base_ + address) & size_mask_;
        buf_[idx] = static_cast<int16_t>(val_int);
    }

    // Write at relative offset from current base with 16-bit saturation
    inline void WriteRelative(int32_t offset, float value) {
        int32_t val_int = static_cast<int32_t>(value * kFloatToInt16);
        val_int = std::max<int32_t>(-32768, std::min<int32_t>(32767, val_int));
        uint32_t idx = (base_ + offset) & size_mask_;
        buf_[idx] = static_cast<int16_t>(val_int);
    }

    // Advance base pointer (called once per tick)
    inline void Advance(uint32_t n = 1) {
        base_ = (base_ + n) & size_mask_;
    }

    // Get current base position (for debugging)
    inline uint32_t Base() const { return base_; }

    // Get buffer size
    inline uint32_t Size() const { return size_mask_ + 1; }

private:
    static constexpr float kInt16ToFloat = 1.0f / 32768.0f;
    static constexpr float kFloatToInt16 = 32768.0f;

    int16_t* buf_;
    uint32_t size_mask_;  // size - 1, for fast wrap with &
    uint32_t base_;       // Current base position (advances each tick)
};
