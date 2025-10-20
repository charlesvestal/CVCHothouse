# TapeScam

Tape emulation effects processor for Daisy Seed / Cleveland Music Co. Hothouse platform.

Authentic tape degradation with musical character - designed for lo-fi vibes, vintage warmth, and cassette deck nostalgia.

## Features

### Tape Emulation Chain
- **Gain Stage** - Input conditioning with character
- **Tape Saturation** (Knob 2) - Smooth harmonic warmth
- **Wow & Flutter** (Knob 3) - Musical pitch wobble and tape speed variation
- **Tape Hiss & Dropouts** (Knob 4) - Noise and occasional signal dropouts
- **Tone Shaping** (Knob 5) - Frequency response adjustment
- **Lo-Fi Compressor** (Toggle 3) - Upward AGC compression (makes hiss breathe)
- **Global Level** (Knob 6) - Output level control

## Controls

### Knobs
1. **Drive** - Input gain staging
2. **Saturation** - Tape saturation intensity
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
  - Less saturation (0.5×)
- **Middle (Standard)**: Normal tape speed
  - Normal HF rolloff (14kHz)
  - Baseline headroom (-1dB)
  - Normal saturation
- **Down (Lo-Fi)**: Degraded/slow speed
  - Dark, telephone-like HF (8kHz rolloff)
  - Heavily reduced headroom (-6dB)
  - Heavy saturation (2.0×)

#### Toggle 3 - Lo-Fi Compression
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

## Sound Character

**Saturation:** Smooth, warm tape compression using soft saturation curves. Adds harmonic richness without harsh digital artifacts. Works beautifully with both clean and overdriven signals.

**Wow & Flutter:** Musical pitch modulation that breathes life into static sources. Slow wow provides gentle warble, while faster flutter adds tape machine character. Subtle stereo width keeps things interesting without phase issues.

**Hiss & Dropouts:** Authentic tape noise that ranges from subtle warmth to lo-fi grit. Occasional dropouts add unpredictability. Combined with the upward compressor, the hiss becomes a dynamic musical element.

**Tone Control:** Shapes the frequency response from dark and muffled (cassette deck) to bright and open. Works in tandem with the tape speed setting for creative filtering.

**Lo-Fi Compressor:** The secret sauce! Makes quiet passages louder, causing the background hiss to swell up dramatically between notes or during decay. Creates that classic cassette deck "breathing" effect where the noise floor pumps with the music.

## Tips & Techniques

- **Subtle Tape Warmth**: New tape + Standard speed + light saturation
- **Cassette Deck Vibes**: Used tape + Lo-Fi speed + heavy compression
- **Warped VHS**: Worn tape + heavy wow/flutter + lots of dropouts
- **Dreamscape**: Moderate saturation + heavy wow/flutter + minimal hiss
- **Lo-Fi Hip-Hop**: Used/Worn tape + Lo-Fi speed + heavy compression + generous hiss

The toggle switches provide broad sonic territories, while the knobs let you dial in the perfect amount of degradation for your material.

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
