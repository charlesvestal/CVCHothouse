# TAPESCAM QUICK REFERENCE FOR HARDWARE REBUILD

## Signal Chain At A Glance
1. Input (0.7x)
2. Gain Stage (dual path, crossfaded on COLOR)
3. Tape Saturation
4. Wow & Flutter
5. Hiss & Dropouts
6. Tone Shaping
7. Lo-Fi Compressor
8. Stereo Width (M/S)
9. Output Level + Soft Limit

## Parameter Mappings Quick View
```
KNOB_1 (INPUT):  0.7f default, direct scale
KNOB_2 (DRIVE):  0→-2dB, 1→32dB trim (squared), 24dB channel (linear)
KNOB_3 (COLOR):  Clipping type crossfade + tape saturation [SMOOTH WITH 0.08 ALPHA!]
KNOB_4 (WOW):    Depth control + speed bias applied
KNOB_5 (NOISE):  Pink noise level (-60 to -6 dB)
KNOB_6 (TONE):   Bass/treble shelves (±12 dB) from dark (0.0) to bright (1.0)
KNOB_7 (OUTPUT): -12 to +6 dB post-chain

TOGGLE_1 (AGE):   NEW/USED/WORN → affects saturation, wow/flutter, cutoff, dropouts
TOGGLE_2 (SPEED): HIGH/STD/LOFI → affects headroom, saturation, WF rates, cutoff
TOGGLE_3 (COMP):  OFF/LITE/HEAVY → AGC mode

FOOTSW_1 (BYPASS):  Toggle bypass
FOOTSW_2 (WIDEN):   Toggle stereo width (1.0x ↔ 1.35x)
```

## Critical Calculations
```cpp
// Drive to dB (use driveShaped for trim, driveNorm for channel)
trimDb = jmap(driveNorm²,  0, 1,  -2, 32)
chanDb = jmap(driveNorm,   0, 1,  -2, 24)

// Tone curve
toneTilt = tone * 2 - 1          // -1 to +1
toneShape = toneTilt * (1 + 0.9*|toneTilt|)
bassDb = -toneShape * 12
trebleDb = +toneShape * 12

// Color smoothing (DO THIS FIRST!)
smoothedColor += (colorKnob - smoothedColor) * 0.08

// Clipping crossfade
if (smoothedColor < 0.5)
  → blend Type 0 to Type 2 using (smoothedColor * 2.0)
else
  → blend Type 2 to Type 1 using ((smoothedColor - 0.5) * 2.0)

// Output level
postLevelDb = jmap(level, 0, 1, -12, 6)
globalLevel = 10^(postLevelDb / 20)
```

## Age Effects (per setting: NEW=0.0, USED=0.5, WORN=1.0)
```cpp
headroomDb = (ageNorm - 0.5) * 1.5        // ±0.75 dB
satMul = lerp(1.0, 1.4, ageNorm)          // 1.0 → 1.4x
wowScale = lerp(1.0, 2.6, ageNorm)        // 1.0 → 2.6x
flutterScale = lerp(1.0, 0.8, ageNorm)    // 1.0 → 0.8x
cutoffHz = lerp(19000, 7000, ageNorm)     // 19kHz → 7kHz
```

## Speed Effects (HIGH=0, STD=1, LOFI=2)
```cpp
// HEADROOM:     +2.0 dB  |  -0.5 dB   | -2.0 dB
// SATURATION:   0.9x     |  1.0x      | 1.25x
// HF CUTOFF:    18000 Hz | 12000 Hz   | 8000 Hz
// WOW BIAS:     +0.08    |  0.0       | -0.12
// WOW RATES:    0.36-0.42| 0.28-0.34  | 0.22-0.30 Hz
// FLUTTER RATE: 3.4-5.6  | 2.2-4.6    | 1.6-3.6 Hz
// WOW SCALE:    0.85x    | 1.3x       | 1.8x
// FLUTTER SCALE:0.85x    | 1.0x       | 1.1x
// DRIFT AMOUNT: 0.15x    | 0.5x       | 1.0x
```

## Dropout Patterns
```cpp
// NEW:  None
// USED: rate=0.08Hz, depth=-2.5→-0.8dB, dur=40-120ms, cluster=0.10, link=0.9, rest=1.5s
// WORN: rate=0.18Hz, depth=-6.0→-2.5dB, dur=60-220ms, cluster=0.15, link=0.7, rest=0.8s
```

## Smoothing Coefficients
```cpp
kColorSmooth = 0.08       // Critical for aliasing prevention!
kLevelSmooth = 0.01       // Post-chain level
kDriveSmooth = 0.3        // TapeSat drive (0.005 hardware)
kWowSmooth = 0.3          // WowFlutter amount (0.005 hardware)
kNoiseSmooth = 0.5        // Hiss amount
```

## Stereo Width
```cpp
width = widenEnabled ? 1.35f : 1.0f;
M = (L + R) / 2
S = (L - R) / 2
S *= width
L_out = M + S
R_out = M - S
```

## Final Limiter
```cpp
out = tanh(out * 0.95f)    // Soft knee limiting
```

## Boost Amount (ONLY 0.70-0.95)
```cpp
boostAmount = 0.0 if color < 0.70
boostAmount = (color - 0.70) / 0.25 if 0.70 ≤ color ≤ 0.95
boostAmount = 1.0 if color > 0.95
```

## Gotchas to Avoid
- Smooth COLOR param FIRST before using it (0.08 alpha)
- Use BOTH gain stages always (both process, both keep state)
- Combine headroom by ADDING, saturation by MULTIPLYING
- Apply WOW BIAS after clipping range calculation
- Speed remap: internal = 2 - UI_speed
- Dropouts only on AGE_USED and AGE_WORN (not NEW)
- BOTH channels may dropout independently if monoLink < 1.0
