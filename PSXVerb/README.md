# PSXVerb - Authentic PlayStation SPU Reverb


https://github.com/user-attachments/assets/dd6fbded-8949-4f11-ae72-cafdf5b99798


A faithful recreation of the PlayStation 1 SPU reverb algorithm for the Daisy Seed / Hothouse platform, based on official PSX-SPX documentation and reference implementations.

## Features

- **Authentic PSX SPU algorithm** - Implements the exact signal flow from the PlayStation 1 hardware
- **6 classic presets** - Room, Studio Small/Medium/Large, Hall, Space Echo
- **Halfband resampling** - 48kHz ↔ 24kHz using 39-tap FIR filters (scaled from PSX's 22.05kHz)
- **16-bit saturating work area** - Faithful SPU RAM emulation with circular buffer
- **Sub-1ms dry path** - Dry signal passes through untouched
- **Real-time parameter control** - 6 knobs for comprehensive sound shaping
- **Efficient implementation** - 61.58% FLASH, 3.08% SRAM

## Hardware

Built for **Hothouse** pedal platform (Daisy Seed based):
- STM32H750 (128KB FLASH, 512KB SRAM)
- 48kHz audio I/O, 16-sample blocks
- 6 knobs, 1 footswitch, 2 LEDs

## Controls

### Knobs
- **K1**: Preset Selection (Room → Studio Small → Studio Medium → Studio Large → Hall → Space Echo)
  - *LED1 flashes N times to indicate preset number (1-6 flashes)*
- **K2**: Input Gain (0x to 2x - **50% = unity/authentic***, 100% = hot input)
- **K3**: Overall Output Gain (0x to 2x - **50% = unity**, only active when reverb enabled)
- **K4**: Decay Time (0.5x to 1.0x - **100% = authentic PSX**, 0% = shorter)
- **K5**: Reverb Level (0x to 4x - **50% = unity/2x**, 100% = hot)
- **K6**: Dry/Wet Mix (0% = dry, 100% = wet)

### Toggle Switches
- **Switch 1**: *(Unused)*
- **Switch 2**: Buffer Clear Mode
  - **Up/Middle**: Normal (tails continue when bypassed)
  - **Down**: Clear buffer when entering bypass (prevents old reverb tail)
- **Switch 3**: *(Unused)*

### Footswitch & LEDs
- **Footswitch 1**: Bypass toggle (hold 1 second to enter DFU mode)
- **LED1**: Active indicator (ON when reverb enabled, OFF when bypassed, flashes N times on preset change)
- **LED2**: Mix level indicator (brightness tracks K6: OFF at 0%, BRIGHT at 100%)

## Technical Details

### Algorithm Structure

The PSX reverb uses this exact signal flow:

```
Input (vLIN/vRIN)
    ↓
Same-Side Reflection (L→L, R→R with IIR damping)
    +
Different-Side Reflection (L→R, R→L with IIR damping)
    ↓
Comb Filter (4-tap weighted sum)
    ↓
All-Pass Filter 1 (diffusion)
    ↓
All-Pass Filter 2 (diffusion)
    ↓
Output (vLOUT/vROUT)
```

### Key Implementation Details

1. **Processing Rate**:
   - PSX hardware runs at 22.05kHz internally
   - We run at 24kHz (48kHz with 2:1 decimation/interpolation)
   - All delay times scaled proportionally

2. **Memory Model**:
   - Single circular buffer (power-of-2 size)
   - Addresses relative to advancing buffer base
   - All writes saturate to int16 range [-32768, 32767]

3. **Channel Processing**:
   - Both L and R processed every sample tick (not alternating!)
   - Cross-channel reflections for stereo width
   - Separate resampler states for each channel

4. **Resampling**:
   - 39-tap linear-phase halfband FIR
   - Processes ALL taps (including zeros) to prevent aliasing
   - Separate decimators and interpolators for L/R

## Building

### Prerequisites
- ARM GCC toolchain (`arm-none-eabi-gcc`)
- libDaisy and DaisySP libraries
- dfu-util (for flashing)

### Compile
```bash
make
```

### Flash
1. Hold **BOOT** button on Daisy Seed
2. Press **RESET** button
3. Release both buttons
4. Run:
```bash
make program-dfu
```

## Memory Usage

- **FLASH**: 83,936 bytes (64.04% of 128KB)
- **SRAM**: 17,012 bytes (3.24% of 512KB)
- **Work buffer**: Varies by preset (largest is Space Echo at ~62KB)
- **Audio block size**: 4 samples (83μs latency at 48kHz)

## Presets

| Preset | SPU Memory | Character |
|--------|------------|-----------|
| Room | 9.9KB | Small, intimate space |
| Studio Small | 8.0KB | Tight studio reverb |
| Studio Medium | 18.1KB | Medium studio with depth |
| Studio Large | 28.5KB | Large studio hall |
| Hall | 44.5KB | Concert hall ambience |
| Space Echo | 63.2KB | Long, spacious echo |

## References

### Primary Specifications
- [PSX-SPX: SPU Reverb Examples](https://psx-spx.consoledev.net/soundprocessingunitspu/#spu-reverb-examples) 
- [PSX-SPX: SPU Reverb Formula](https://problemkaputt.de/psx-spx.htm#spureverbformula) 
- [jsgroth PSX SPU Blog Series](https://jsgroth.dev/blog/posts/ps1-spu-part-3/) - 


## Files

- `PSXVerb_SPU.cpp` - Main firmware, Daisy integration, controls, audio callback
- `PsxReverb.h` - Core reverb DSP engine with authentic PSX algorithm
- `PsxPreset.h` - 6 PSX reverb preset definitions (from PSX-SPX docs)
- `WorkArea.h` - SPU RAM emulation (circular buffer with int16 saturation)
- `Halfband39.h` - 39-tap linear-phase halfband FIR for 48kHz ↔ 24kHz resampling
- `hothouse.h` - Hothouse hardware abstraction layer
- `Makefile` - Build configuration for ARM GCC toolchain

## Known Issues & Limitations

- Preset changes have 500ms debounce to prevent parameter glitches
- Decay range limited to 0.5x-1.0x to prevent feedback (authentic PSX = 100%)
- No MIDI control (knobs only)
- No preset save/recall (factory presets only)

## License

This implementation is based on publicly documented PSX SPU behavior. See individual source files for license details.


## Inspiration
Inspired by Shirobon's lovely [PS1 Impulse Responses](https://shirobon.bandcamp.com/album/ps1-reverb-impulse-responses)
