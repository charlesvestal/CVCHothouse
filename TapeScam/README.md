# TapeScam

Tape emulation effects processor for Daisy Seed / Cleveland Music Co. Hothouse platform.

## Features

### Tape Emulation Chain
- **Gain Stage** - Input conditioning with character
- **Tape Saturation** (Knob 2) - Harmonic saturation and compression
- **Wow & Flutter** (Knob 3) - Time-varying pitch modulation with drift and jitter
- **Tape Hiss & Dropouts** (Knob 4) - Noise and occasional signal dropouts
- **Tone Shaping** (Knob 5) - Frequency response adjustment
- **Global Level** (Knob 6) - Output level control
- **Reverb** (Toggle 3) - Space and ambience (after level control)

### Signal Flow
```
Input → GainStage → TapeSat → WowFlutter → Hiss/Dropout → Tone → Level → Reverb → Output
```

## Controls

### Knobs
1. **Drive** - Tape saturation drive amount
2. **Saturation** - Tape saturation intensity
3. **Wow/Flutter** - Time modulation amount (wow, flutter, drift, jitter)
4. **Noise** - Tape hiss and dropout intensity
5. **Tone** - Frequency shaping
6. **Level** - Master output level (controls signal going into reverb)

### Toggle Switches

#### Toggle 1 - Tape Age/Condition
- **Up (New)**: Clean tape
  - Less hiss (0.5×)
  - Fewer dropouts (0.5×)
  - Less wow/flutter (0.8×)
  - Full headroom (0dB)
  - Normal saturation
- **Middle (Used)**: Standard tape (baseline)
  - Normal hiss, dropouts, wow/flutter
  - Slightly reduced headroom (-1dB)
- **Down (Worn)**: Degraded tape
  - More hiss (1.5×)
  - More dropouts (1.5×)
  - More wow/flutter (1.3×)
  - Reduced headroom (-2.5dB)
  - Increased saturation (1.2×)

#### Toggle 2 - Tape Speed/Format
- **Up (High Speed)**: Pristine quality
  - Extended HF response (20kHz rolloff)
  - Boosted headroom (+3dB)
  - Much less saturation (0.5×)
- **Middle (Standard)**: Normal tape speed
  - Normal HF rolloff (14kHz)
  - Baseline headroom (-1dB)
  - Normal saturation
- **Down (Lo-Fi)**: Degraded/slow speed
  - Dark, telephone-like HF (8kHz rolloff)
  - Heavily reduced headroom (-6dB)
  - Heavy saturation (2.0×)

#### Toggle 3 - Reverb/Ambience
- **Up (Off)**: No reverb
- **Middle (Light Room)**: 25% wet, 2 second decay
- **Down (Plate/Hall)**: 40% wet, 5 second decay

### Footswitch
- **Footswitch 1**: True bypass on/off

## Technical Details

### Memory Usage
- Flash: ~83% (108KB)
- SRAM: ~5% (27KB)

### Reverb Algorithm
Custom Freeverb-style reverb using 4 comb filters + 2 allpass filters per channel. Designed for stability with modulated input signals - no artifacts when combined with wow/flutter.

### Wow/Flutter Implementation
- Buffer size: 2048 samples (~42ms at 48kHz)
- Base delays: 12ms (L), 15ms (R) for stereo width
- Cubic Hermite interpolation for smooth pitch modulation
- Discontinuity guards prevent buffer wrap artifacts

**Organic Modulation System:**
- **Wow**: 0.015-0.100 Hz range
  - Mixed waveform: 80% sine + 20% triangle + random noise
  - Random rate variation: ±20% per cycle
  - Random depth variation: ±30% (up to ±20ms max)
  - Creates non-periodic, organic warble

- **Flutter**: 4.5-7.3 Hz base rate
  - Turbulent rate variation: ±1 Hz per cycle
  - Turbulent depth variation: ±50% (up to ±1ms)
  - Flutter bursts: 2× depth increase for 0.1-0.3s every ~3 seconds (simulates capstan irregularities)

- **Drift**: Slow random walk (long-term instability)

**Randomness Tied to Tape Age (Toggle 1):**
- New tape: 20% wow randomness, 10% flutter turbulence
- Used tape: 40% wow randomness, 20% flutter turbulence
- Worn tape: 70% wow randomness, 40% flutter turbulence

**Debug counters:** burstCountL/R, maxReadDeltaSamplesL/R, jumpCountL/R

### Known Characteristics
- Toggle switches combine effects (Age + Speed both affect headroom and saturation)
- Reverb is placed after level control for better gain staging
- Simple reverb topology prevents bit-crushing artifacts that occurred with more complex algorithms

## Building

```bash
make clean
make
make program-dfu  # Flash to Daisy Seed
```

## Dependencies
- libDaisy
- DaisySP (LGPL modules enabled)
- Hothouse hardware abstraction layer

## Credits

Based on Cleveland Music Co. Hothouse platform.

Reverb design inspired by Freeverb (comb + allpass topology).
Wow/flutter modulation techniques from classic tape emulation research.
