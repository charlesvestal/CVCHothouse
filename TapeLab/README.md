# TapeLab Hardware Build

Live tape-machine style processor for the Cleveland Music Co. Hothouse (Daisy Seed). Adds tape saturation, wow/flutter, dynamic tone shaping, stereo reverb routing, and optional hiss/crackle/dropout mojo.

## Feature Highlights

- Tape stage with bias & sag, wow/flutter/dimensional drift.
- Dynamic tone (level-dependent LPF), head-bump EQ, crosstalk, print-through.
- Wet-bus soft-knee compressor. Pink hiss that “breathes” with the signal.
- Optional crackle + dropouts (tape ghosts) driven from hiss amount.
- Stereo reverb with PRE, POST, or DUAL routing and per-mode predelay/damping.
- Footswitch 1: bypass (short tap) / DFU boot (hold ~2 s).
- Footswitch 2: toggles hiss/artifacts globally.

## Controls

### Knobs

| Knob | Function             | Notes                                       |
|------|----------------------|---------------------------------------------|
| 1    | Drive                | Input level into tape stage (0–1)           |
| 2    | Wow                  | Slow modulation depth (squared taper)       |
| 3    | Flutter              | Fast modulation depth (squared taper)       |
| 4    | Tone                 | Brightness target for dynamic LPF           |
| 5    | Hiss amount          | 0–1; disabled if Footswitch 2 is off        |
| 6    | Wet/Dry mix          | 0–1; forced to 0 when bypassed              |

### Toggle Switches

- **Toggle 1**: Tape speed profile (controls wow/flutter rates).
- **Toggle 2**: Reverb routing & profile
  - Up   → PRE placement (~18 ms predelay, 5.5 kHz LPF)
  - Mid  → POST placement (~22 ms predelay, 6.5 kHz LPF)
  - Down → DUAL (half PRE/half POST, ~28 ms predelay, 6 kHz LPF)
- **Toggle 3**: Artifact mode
  - Up   → Off (clean)
  - Mid  → Pink hiss only
  - Down → Hiss + crackles + dropouts

### Footswitches & LEDs

- **FS1**: Toggle bypass; hold ~2 s for DFU reset. LED1 indicates engaged.
- **FS2**: Toggle "Preamp Hot" (extra cassette drive). LED1 flashes briefly when toggled; hiss is now set purely by Knob 5, and LED2 continues to indicate hiss/artifact enable.

## Build & Flash

```bash
cd src/TapeLab
make clean
make            # emits build/tapelab.{elf,bin,hex}
make program-dfu  # Seed must be in DFU (0483:df11)
```

For flashing, hold FS1 while power-cycling to enter DFU. After `make program-dfu`, tap RESET or power-cycle. If LEDs still blink, hold FS1 ~2 s to boot.

## Signal Path Overview

```
Input -> preHP -> PRE reverb send (Toggle 2) -> pre-emphasis -> preLP -> TapeSat
     -> wow/flutter delay -> azimuth skew -> dynamic LPF/HPF -> head bump
     -> crosstalk -> print-through -> DC block -> POST reverb / wet comp
     -> artifacts (hiss/crackle/drop) -> Wet/Dry mix -> Output
```

- PRE reverb is summed before saturation; POST replaces the old slot; DUAL splits mix.
- Reverb predelay buffers (2400 samples) and HP/LP dampers follow Toggle 2.
- SoftKneeComp (-12 dB threshold, 1.6:1, 6 dB knee) runs on the wet path.
- Pink noise via Paul Kellet filter; hiss breathes with EnvFollow-driven gates.
- Crackles: short band-limited blips; Dropouts: gain dips, both scaled by hiss.
- Long buffers (print-through, predelay) live in SDRAM via gPrintBuffer* symbols.

## Default Preset

Boot values:
```
drive=0.68 wow=0.22 flutter=0.18 speed=0.52
tone=0.48 hiss=0.14 reverbMix=0.20 wetDry=0.75 headBump=0.55
```

Suggested live setup: Toggle1=MID, Toggle2=MID (POST), Toggle3=DOWN for full tape ghosting.

## Notes & Safety

- Artifacts rate scales with hiss; set hiss to 0 (or FS2 off) for pristine mode.
- CPU at 48 kHz/48-sample block stays <60% on Seed.
- Bypass keeps dry path untouched; wet/dry mix smoothing prevents zippering.
- DFU reset matches the HardwareTest behavior: hold FS1 ~2 s.

Enjoy the tape lab!
