# TapeScam

Tape emulation effects processor for Daisy Seed / Cleveland Music Co. Hothouse platform.

Authentic tape degradation with musical character - designed for lo-fi vibes, vintage warmth, and cassette deck nostalgia.

## Features

### Tape Emulation Chain
- **Gain Stage** - Input conditioning with character
- **Tape Saturation** (Knob 2) - Harmonic saturation with 2x oversampling for alias-free warmth
- **Wow & Flutter** (Knob 3) - Musical pitch wobble and tape speed variation
- **Tape Hiss & Dropouts** (Knob 4) - Noise and occasional signal dropouts
- **Tone Shaping** (Knob 5) - Frequency response adjustment
- **Lo-Fi Compressor** (Toggle 3) - Upward AGC compression (makes hiss breathe)
- **Global Level** (Knob 6) - Output level control

### Signal Flow
```
Input → GainStage → TapeSat (2x OS) → WowFlutter → Hiss/Dropout → Tone → LoFi Comp → Level → Output
```

## Controls

### Knobs
1. **Drive** - Input gain staging
2. **Saturation** - Tape saturation intensity (0-18dB drive range)
3. **Wow/Flutter** - Time modulation amount (slow pitch wobble + fast warble)
4. **Noise** - Tape hiss and dropout intensity
5. **Tone** - Frequency shaping
6. **Level** - Master output level

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
  - Pre-sat filter: 24kHz
- **Middle (Standard)**: Normal tape speed
  - Normal HF rolloff (14kHz)
  - Baseline headroom (-1dB)
  - Normal saturation
  - Pre-sat filter: 20kHz
- **Down (Lo-Fi)**: Degraded/slow speed
  - Dark, telephone-like HF (8kHz rolloff)
  - Heavily reduced headroom (-6dB)
  - Heavy saturation (2.0×)
  - Pre-sat filter: 16kHz

#### Toggle 3 - Lo-Fi Compression (NEW!)
- **Up (Off)**: No compression, clean signal
- **Middle (Light)**: Musical AGC pumping
  - 45% threshold, 8:1 upward ratio
  - 400ms release (moderate breathing)
  - Hiss gently swells during quiet parts
- **Down (Heavy)**: Extreme cassette deck pumping
  - 65% threshold, 18:1 upward ratio
  - 800ms release (dramatic breathing)
  - Hiss ROARS up during quiet passages (lo-fi magic!)

**How it works:** Upward compression boosts quiet signals (including background hiss), making the noise floor "breathe" with the dynamics just like a cheap cassette deck's AGC circuit. The slow release times create that classic pumping effect.

### Footswitch
- **Footswitch 1**: True bypass on/off

## Technical Details

### Memory Usage
- Flash: ~82% (108KB)
- SRAM: ~5% (27KB)

### Tape Saturation - Anti-Aliasing System

**Problem:** `tanh()` nonlinearity generates high-frequency harmonics that fold back as aliasing when processing bright signals.

**Solution:** Multi-stage anti-aliasing approach (CPU-friendly, no compromise on character):

1. **Pre-Saturation Anti-Aliasing Filter**
   - Band-limits input before nonlinear processing
   - Speed-dependent cutoff: LoFi=16kHz, Standard=20kHz, High=24kHz
   - Prevents ultra-HF content from generating foldback harmonics

2. **Drive-Dependent Asymmetry** (improved character)
   - Dead zone below 0.1 drive = zero asymmetry (clean/linear tape region)
   - Above 0.1: quadratic mapping (0 → 0.35 max asymmetry)
   - Mimics real tape bias behavior (linear at low levels, hysteresis at high)

3. **2x Oversampling** (NEW - main aliasing killer!)
   - Saturation stages process at 96kHz effective sample rate
   - Simple upsampling: duplicate samples (hold)
   - Process both samples through `TapeSaturationCurve()`
   - Downsample: average output (acts as anti-aliasing lowpass)
   - Result: Harmonics from `tanh()` have headroom before Nyquist

4. **Dynamic Post-Saturation Filtering**
   - Drive-dependent cutoff: 20kHz @ drive=0 → 12kHz @ drive=1
   - Kills remaining high-order harmonics before output
   - Final safety net against aliasing

**CPU Impact:** Calling saturation function 2x more often (bass + highs), but lightweight enough for Daisy Seed at 48kHz.

**Result:** Clean, warm tape saturation even with bright input signals. No bitcrushed/aliased artifacts.

### Wow/Flutter Implementation - Simplified for Musicality

**Previous version** had complex cross-modulated LFOs that made the effect unpredictable and muddy above 50% knob position.

**Current version** (simplified):
- **Wow**: Pure sine wave, 0.25-0.5 Hz (slow, musical pitch wobble)
- **Flutter**: Pure sine wave, 2-5 Hz (audible tape speed variation)
- **Linear control curve** - knob position directly controls depth (no squaring)
- **Stereo**: 3% L/R offset for subtle width, minimal phasing
- **Always-on design**: Delay buffers run continuously, depths go to zero at 0%

**Technical specs:**
- Buffer size: 2048 samples (~42ms at 48kHz)
- Base delays: 12ms (L), 15ms (R) for stereo width
- Cubic Hermite interpolation for smooth pitch modulation
- Slew-rate limiter (85% smoothing) prevents clicks from abrupt changes
- Max depth: 8ms wow, 2ms flutter

**Result:** Clear, audible effect across full range (0-100%) with predictable behavior.

### Lo-Fi Compressor - Cassette Deck AGC

**Algorithm:** Upward compression (boosts quiet signals instead of reducing loud ones)

**Implementation:**
- Fast attack (2-8ms) to catch transients
- Slow release (250-800ms) creates pumping/breathing
- Envelope follower with attack/release coefficients
- Stereo processing (independent L/R envelopes)

**Light Mode:**
- Threshold: 45% (signals below get boosted)
- Ratio: 8:1 (up to 8x boost for very quiet parts)
- Max boost: 12x limited
- Release: 400ms

**Heavy Mode:**
- Threshold: 65% (most of signal gets boosted!)
- Ratio: 18:1 (extreme boost for quiet parts)
- Max boost: 20x limited
- Release: 800ms (very slow = dramatic breathing)

**Effect:** Background hiss becomes a musical element that swells and ducks with the dynamics, just like a cheap cassette player's AGC circuit desperately trying to maintain level.

### Known Characteristics
- Toggle switches combine effects (Age + Speed both affect headroom and saturation)
- Lo-fi compression placed before level control (so it can really pump before trim)
- Oversampling adds minimal latency (~1 sample = 0.02ms at 48kHz)

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

Tape saturation anti-aliasing techniques based on research into analog tape physics and modern DSP best practices.

Wow/flutter simplified from original cross-modulated LFO system for clarity and musicality.

Lo-fi compressor inspired by vintage cassette deck AGC circuits.
