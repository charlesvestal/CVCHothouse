# PSXVerb - Authentic PlayStation SPU Reverb

A faithful recreation of the PlayStation 1 SPU reverb algorithm for the Daisy Seed / Hothouse platform, based on official PSX-SPX documentation and reference implementations.

## Features

- **Authentic PSX SPU algorithm** - Implements the exact signal flow from the PlayStation 1 hardware
- **6 classic presets** - Room, Studio Small/Medium/Large, Hall, Space Echo
- **Halfband resampling** - 48kHz ↔ 24kHz using 39-tap FIR filters (scaled from PSX's 22.05kHz)
- **16-bit saturating work area** - Faithful SPU RAM emulation with circular buffer
- **Zero-latency dry path** - Dry signal passes through untouched
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
- **K2-K3**: *(Unused - set to 12 o'clock for neutral)*
- **K4**: Decay Time (0.5x to 3x - reverb tail length, **50% = authentic PSX**)
- **K5**: *(Unused - set to 12 o'clock for neutral)*
- **K6**: Dry/Wet Mix (0% = dry, 100% = wet)

### Footswitch & LEDs
- **Footswitch 1**: Bypass toggle
- **LED1**: Active indicator (on when processing)
- **LED2**: Mix level indicator

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

- **FLASH**: 80,536 bytes (61.44% of 128KB)
- **SRAM**: 16,132 bytes (3.08% of 512KB)
- **Work buffer**: Varies by preset (largest is Space Echo at ~62KB)

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

- [PSX-SPX: SPU Reverb Examples](https://psx-spx.consoledev.net/soundprocessingunitspu/#spu-reverb-examples)
- [PSX-SPX: SPU Reverb Formula](https://problemkaputt.de/psx-spx.htm#spureverbformula)
- [Reference Implementation: lv2-psx-reverb](https://github.com/ipatix/lv2-psx-reverb)
- [jsgroth PSX SPU Blog](https://www.jsgroth.dev/posts/ps1-spu-part2/)

## Files

- `PSXVerb_SPU.cpp` - Main firmware, Daisy integration, controls
- `PsxReverb.h` - Core reverb DSP engine
- `PsxPreset.h` - 6 PSX reverb preset definitions
- `WorkArea.h` - SPU RAM emulation (circular buffer with saturation)
- `Halfband39.h` - 39-tap halfband FIR for resampling
- `hothouse.h` - Hothouse hardware abstraction

## Known Issues & Limitations

- Preset changes have 500ms debounce to prevent parameter glitches
- Extreme decay settings (>2x) may cause buildup on some presets
- No MIDI control (knobs only)

## License

This implementation is based on publicly documented PSX SPU behavior. See individual source files for license details.

## Credits

Implementation by Claude Code for Cleveland Music Co. Hothouse platform.

Based on reverse-engineering work by:
- Martin Korth (PSX-SPX documentation)
- jsgroth (blog series on PSX SPU internals)
- ipatix (lv2-psx-reverb reference implementation)
