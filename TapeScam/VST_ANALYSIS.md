# TAPESCAM VST COMPLETE ANALYSIS
## Hardware Rebuild from VST Reference

---

## 1. COMPLETE SIGNAL CHAIN ORDER

The signal chain processes audio in this exact order (both VST and hardware):

1. **Input Level Scaling** (fixed 0.7x)
   - Applies input gain to prevent clipping before drive stage
   - VST: `inputLevel = 0.7f`
   - Hardware: `inputLevel = 0.7f`

2. **Gain Stage with Dual-Path Crossfading**
   - Two parallel gain stage modules for glitch-free clipping type transitions
   - Continuously blends between clipping types based on COLOR knob
   - Includes:
     - Pre-EQ (bass/treble shelves)
     - Drive mapping (trim & channel gains)
     - Oversampling (4x) for anti-aliasing
     - Clipping (three types: soft, gentle, hard)
     - Post-EQ
   - Headroom adjustments for tape age/speed

3. **Tape Saturation**
   - Soft-knee compression curves
   - Asymmetric saturation (even-order harmonics)
   - Split-band processing (low/high at ~400Hz)
   - Compression envelope followers
   - HF rolloff (frequency-dependent on tape speed)

4. **Wow & Flutter**
   - Pitch modulation via variable delay line
   - Two independent LFO systems (wow + flutter)
   - Rate-dependent burst effects
   - Stereo linking/blending

5. **Hiss & Dropouts (Noise)**
   - Pink noise generation (filtered white noise)
   - Tone-shaping via noise color factor
   - Stereo blending

6. **Dropout Events**
   - Random envelope-based amplitude reduction
   - Configurable rate/depth/duration per tape age
   - Cluster probability for multiple events

7. **Tone Shaping**
   - Bass/treble shelves based on TONE knob
   - Two biquad filters per channel

8. **Lo-Fi Compressor (AGC)**
   - Upward compression (boosts quiet signals)
   - Three modes: OFF, LITE, HEAVY
   - Makes hiss "breathe" with signal level

9. **Stereo Width (M/S Processing)**
   - Widen toggle: 1.0x (mono) or 1.35x (wide)
   - Mid/side decomposition and recomposition

10. **Post-Chain Level**
    - Output level knob (-12 to +6 dB)
    - Exponential smoothing (0.01 alpha)
    - Final soft limiting via tanh

11. **True Bypass**
    - If enabled, all processing is skipped
    - Direct input-to-output copy

---

## 2. ALL PARAMETER MAPPINGS

### KNOBS (0.0 - 1.0 normalized ranges)

| Knob | ID | Name | Min | Max | Default | Mapping |
|------|-----|------|-----|-----|---------|---------|
| 1 | PARAM_INPUT | INPUT | 0.0 | 1.0 | 0.7 | Direct scale to output |
| 2 | PARAM_DRIVE | DRIVE | 0.0 | 1.0 | 0.0 | Squared for trim, linear for channel gain |
| 3 | PARAM_SATURATION | COLOR | 0.0 | 1.0 | 0.0 | Clipping type crossfade + tape saturation |
| 4 | PARAM_WOW_FLUTTER | WOW/FLUT | 0.0 | 1.0 | 0.0 | Wow/flutter depth control |
| 5 | PARAM_NOISE | NOISE | 0.0 | 1.0 | 0.0 | Pink noise level |
| 6 | PARAM_TONE | TONE | 0.0 | 1.0 | 0.5 | Bass/treble shelving |
| 7 | PARAM_LEVEL | OUTPUT | 0.0 | 1.0 | 1.0 | Post-chain level (-12 to +6 dB) |

### TOGGLES (Multi-position)

| Toggle | ID | Name | Mode 0 (UP) | Mode 1 (MID) | Mode 2 (DOWN) |
|--------|-----|------|-------------|--------------|----------------|
| 1 | PARAM_TAPE_AGE | AGE | NEW | USED | WORN |
| 2 | PARAM_TAPE_SPEED | TAPE | HIGH | STD | LOW |
| 3 | PARAM_COMPRESSION | COMP | OFF | LITE | HEAVY |

### FOOTSWITCHES (Boolean toggles)

| Footswitch | Default | Function |
|------------|---------|----------|
| 1 | false | Bypass (LED 1 shows state) |
| 2 | true | Widen (LED 2 shows state) |

### HIDDEN PARAMETERS

| Parameter | Type | Default | Function |
|-----------|------|---------|----------|
| PARAM_BYPASS | Bool | false | Bypass control |
| PARAM_WIDEN | Bool | true | Stereo width enable |
| PARAM_OUTPUT_ROUTING | Bool | false | Output into drive stage (unused in current version) |

---

## 3. DRIVE & GAIN STAGE MAPPINGS

### Input Level Gain
```
inputLevel = 0.7f (fixed, matches VST)
```

### Drive Parameter to Gain Values
Drive parameter (0-1) is mapped as follows:

```cpp
const float driveNorm = clamp(drive, 0.0f, 1.0f);
const float driveShaped = driveNorm * driveNorm;  // Quadratic scaling

// Trim Gain (using squared value for smoother curves)
trimGainDb = clamp(jmap(driveShaped, 0.0f, 1.0f, -2.0f, 32.0f), -40.0f, 40.0f);
// Maps: 0.0 -> -2.0 dB, 1.0 -> 32.0 dB

// Channel Gain (using linear value)
channelGainDb = clamp(jmap(driveNorm, 0.0f, 1.0f, -2.0f, 24.0f), -40.0f, 40.0f);
// Maps: 0.0 -> -2.0 dB, 1.0 -> 24.0 dB

// Convert dB to linear gain
gainLin = pow(10.0f, gainDb / 20.0f);
```

### Tone Parameter to EQ
```cpp
const float toneNorm = clamp(tone, 0.0f, 1.0f);
const float toneTilt = toneNorm * 2.0f - 1.0f;  // Range: -1.0 to +1.0
const float toneShape = toneTilt * (1.0f + 0.9f * abs(toneTilt));  // Curved response

// Bass and Treble shelves (±12 dB each)
const float bassRangeDb = 12.0f;
const float trebleRangeDb = 12.0f;

bassGainDb = clamp(-toneShape * bassRangeDb, -12.0f, 12.0f);
trebleGainDb = clamp(toneShape * trebleRangeDb, -12.0f, 12.0f);

// Tone knob:
// 0.0 (dark):   bass +12 dB, treble -12 dB
// 0.5 (neutral): bass  0 dB, treble   0 dB
// 1.0 (bright):  bass -12 dB, treble +12 dB
```

### Output Level Knob
```cpp
const float postLevelDb = jmap(levelKnob, 0.0f, 1.0f, -12.0f, 6.0f);
globalLevel = pow(10.0f, postLevelDb / 20.0f);
// Smoothed with alpha = 0.01 during processing
```

---

## 4. DUAL GAIN STAGE CROSSFADING

The COLOR knob controls continuous crossfading between three clipping types:

### Clipping Type Mapping
```cpp
if (colorNorm < 0.5f)
{
    // Lower half: Type 0 (soft) → Type 2 (gentle)
    blendTypeA = 0;
    blendTypeB = 2;
    blendAmount = colorNorm * 2.0f;  // Map 0.0-0.5 to 0.0-1.0
}
else
{
    // Upper half: Type 2 (gentle) → Type 1 (hard)
    blendTypeA = 2;
    blendTypeB = 1;
    blendAmount = (colorNorm - 0.5f) * 2.0f;  // Map 0.5-1.0 to 0.0-1.0
}

currentClippingType = blendTypeA;
targetClippingType = blendTypeB;
clippingCrossfadeProgress = 1.0f - blendAmount;

// Crossfade formula:
output = gainStage[currentType] * currentWeight + 
         gainStageCrossfade[targetType] * targetWeight;
```

### Clipping Types
- **Type 0 (Soft)**: Smooth saturation, gentlest clipping
- **Type 1 (Hard)**: Hard clipping, brightest transients
- **Type 2 (Gentle)**: Middle ground, warm character

### Boost Amount Ramping
```cpp
// Continuous ramp from 0.70-0.95 (no discrete jumps)
const float boostRampStart = 0.70f;
const float boostRampEnd = 0.95f;
boostAmount = clamp((colorNorm - boostRampStart) / (boostRampEnd - boostRampStart), 0.0f, 1.0f);
// Below 0.70: boostAmount = 0.0 (no boost)
// 0.70-0.95: linear ramp to 1.0
// Above 0.95: boostAmount = 1.0 (full boost)
```

### Color Parameter Smoothing
**CRITICAL**: Must smooth color parameter to prevent aliasing when modulating clipping threshold!

```cpp
const float kColorSmoothAlpha = 0.08f;  // 1/12.5 roughly
smoothedColorParam += (colorKnob - smoothedColorParam) * kColorSmoothAlpha;

// Then use smoothedColorParam everywhere (not raw colorKnob)
```

---

## 5. TAPE AGE PARAMETER EFFECTS

The TAPE AGE toggle (positions 0=NEW, 1=USED, 2=WORN) affects multiple parameters:

### Age Normalization
```cpp
float ageNorm = (tapeAge / 2.0f);  // 0.0, 0.5, 1.0
auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
```

### Headroom Adjustment
```cpp
const float kAgeHeadroomRangeDb = 1.5f;
ageHeadroomAdj_dB = (ageNorm - 0.5f) * kAgeHeadroomRangeDb;
// NEW:  -0.75 dB (more headroom)
// USED:  0.0 dB
// WORN: +0.75 dB (less headroom)
```

### Saturation Multiplier
```cpp
ageSaturationMul = lerp(1.0f, 1.4f, ageNorm);
// NEW:  1.0x (clean)
// USED: 1.2x
// WORN: 1.4x (more saturation)
```

### Wow/Flutter Depth Scaling
```cpp
ageWowDepthScale = lerp(1.0f, 2.6f, ageNorm);
// NEW:  1.0x
// USED: 1.8x
// WORN: 2.6x (more wobble)

ageFlutterDepthScale = lerp(1.0f, 0.8f, ageNorm);
// NEW:  1.0x
// USED: 0.9x
// WORN: 0.8x (less flutter)
```

### HF Rolloff Cutoff
```cpp
ageCutoffHz = lerp(19000.0f, 7000.0f, ageNorm);
// NEW:  19000 Hz (bright)
// USED: 13000 Hz
// WORN:  7000 Hz (dark, worn sound)
```

### Dropout Characteristics
```cpp
// NEW: No dropouts
// USED:
//   rateHz: 0.08 Hz
//   depthMinDb: -2.5 dB, depthMaxDb: -0.8 dB
//   durMinMs: 40 ms, durMaxMs: 120 ms
//   clusterProb: 0.10, monoLink: 0.9, minRestSec: 1.5
//
// WORN:
//   rateHz: 0.18 Hz
//   depthMinDb: -6.0 dB, depthMaxDb: -2.5 dB
//   durMinMs: 60 ms, durMaxMs: 220 ms
//   clusterProb: 0.15, monoLink: 0.7, minRestSec: 0.8
```

---

## 6. TAPE SPEED EFFECTS

The TAPE SPEED toggle (positions 0=HIGH, 1=STD, 2=LOW/LO-FI) affects:

### Headroom Adjustment
```cpp
// HIGH:       +2.0 dB (most headroom, clean)
// STANDARD:  -0.5 dB
// LOFI:      -2.0 dB (least headroom, gritty)
```

### Saturation Multiplier
```cpp
// HIGH:       0.9x (less saturation, clean)
// STANDARD:   1.0x
// LOFI:       1.25x (more saturation, dirty)
```

### HF Rolloff Cutoff Frequency
```cpp
// HIGH:       18000 Hz (extended highs)
// STANDARD:   12000 Hz
// LOFI:        8000 Hz (aggressive rolloff)
```

### Wow/Flutter Characteristics
```cpp
// HIGH (clean):
//   wowAmountBias: +0.08 (adds 0.08 to knob value)
//   wowRateMin/Max: 0.36-0.42 Hz
//   flutterRateMin/Max: 3.4-5.6 Hz
//   speedWowDepthScale: 0.85x
//   speedFlutterDepthScale: 0.85x (reduced modulation)
//   speedDriftAmount: 0.15x

// STANDARD (balanced):
//   wowAmountBias: 0.0
//   wowRateMin/Max: 0.28-0.34 Hz
//   flutterRateMin/Max: 2.2-4.6 Hz
//   speedWowDepthScale: 1.3x
//   speedFlutterDepthScale: 1.0x
//   speedDriftAmount: 0.5x

// LOFI (degraded):
//   wowAmountBias: -0.12 (reduces wow at low knob)
//   wowRateMin/Max: 0.22-0.30 Hz
//   flutterRateMin/Max: 1.6-3.6 Hz
//   speedWowDepthScale: 1.8x
//   speedFlutterDepthScale: 1.1x (more modulation)
//   speedDriftAmount: 1.0x
```

### Final Cutoff (Combined Age & Speed)
```cpp
finalCutoff = min(ageCutoffHz, hfRolloffCutoff);
// Takes the more restrictive of the two
```

---

## 7. COMPRESSION MODES

The COMPRESSION toggle has three modes:

### Mode 0: OFF
- No AGC processing
- Compressor disabled

### Mode 1: LITE (Light AGC)
- Gentle boost to quiet signals
- Hiss "breathes" subtly with signal
- Lower envelope following thresholds

### Mode 2: HEAVY (Heavy AGC)
- Aggressive boost to quiet signals
- Pronounced pumping, hiss pulses loudly
- Higher envelope following thresholds

Each mode uses preset attack/release coefficients and threshold values in the LoFiCompressor.

---

## 8. INITIAL PARAMETER VALUES & DEFAULTS

### On Plugin Startup (prepareToPlay)
```cpp
// Parameters use these defaults (from createParameterLayout):
INPUT:          0.7f   // Slight input reduction for headroom
DRIVE:          0.0f   // Clean, no distortion
COLOR:          0.0f   // Type 0 clipping (soft)
WOW/FLUTTER:    0.0f   // No modulation
NOISE:          0.0f   // No hiss
TONE:           0.5f   // Neutral tone
OUTPUT:         1.0f   // Unity level
TAPE_AGE:       0      // NEW tape
TAPE_SPEED:     2      // Standard (maps to internal 1)
COMPRESSION:    0      // OFF
BYPASS:         false
WIDEN:          true

// Internal state initialization:
colorSmoothingInitialized = false
smoothedColorParam = will be initialized to first colorKnob value
globalLevelSmooth = 1.0f
bypassEnabled = false
widenEnabled = true
currentClippingType = 0
targetClippingType = 0
clippingCrossfadeProgress = 1.0f
```

### Module Initialization
All modules (gainStage, tapeSat, tapeWobble, tapeNoise, tapeTone, lofiComp, dropoutModule) are initialized with:
- Current sample rate (48kHz hardware, variable VST)
- Default filter state (reset)
- Pending parameters applied

The VST runs an extra 200 UpdateControls() calls after prepareToPlay to "warm up" the modules and ensure smooth parameter transitions from the start.

---

## 9. BYPASS HANDLING

### True Bypass Logic
```cpp
if (getBypassed())
{
    // Direct signal pass-through
    for (size_t i = 0; i < numSamples; ++i)
    {
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
    return;  // Skip all processing
}
```

**Important**: 
- Bypass is checked BEFORE any processing
- No crossfading or gain compensation
- Completely dry signal
- LED feedback (if available) shows bypass state

---

## 10. PARAMETER SMOOTHING & RAMPING

### Color Parameter Smoothing (Critical!)
```cpp
const float kColorSmoothAlpha = 0.08f;
smoothedColorParam += (colorKnob - smoothedColorParam) * kColorSmoothAlpha;
// Time constant: ~12.5 samples at 48kHz ≈ 260 µs
```

**Why**: Prevents aliasing artifacts when COLOR knob modulates the clipping threshold

### Gain Smoothing
```cpp
const float kLevelSmooth = 0.01f;
globalLevelSmooth += (globalLevel - globalLevelSmooth) * kLevelSmooth;
// Time constant: ~100 samples at 48kHz ≈ 2.08 ms
```

**Why**: Matches hardware AGC feel, prevents amplitude discontinuities

### Drive Parameter Smoothing (in TapeSatModule)
```cpp
const float driveSmoothCoeff = 0.3f;  // Plugin (0.005f hardware)
smoothedDrive += (targetDrive - smoothedDrive) * driveSmoothCoeff;
```

### Wow/Flutter Amount Smoothing (in WowFlutterModule)
```cpp
const float kAmountSmooth = 0.3f;  // Plugin (0.005f hardware)
smoothedAmount += (targetAmount - smoothedAmount) * kAmountSmooth;
```

### Noise Amount Smoothing (in HissDropModule)
```cpp
const float kAmountSmooth = 0.5f;  // Plugin
smoothedAmount += (targetAmount - smoothedAmount) * kAmountSmooth;
```

---

## 11. FOOTSWITCHES & TOGGLES BEHAVIOR

### Footswitch 1 (Bypass)
```cpp
if (hw.switches[FOOTSWITCH_1].RisingEdge())
{
    bypassEnabled = !bypassEnabled;
}
// LED 1 feedback: ON if bypass disabled, OFF if enabled
ledBypass.Set(bypassEnabled ? 0.0f : 1.0f);
```

### Footswitch 2 (Widen)
```cpp
if (hw.switches[FOOTSWITCH_2].RisingEdge())
{
    widenEnabled = !widenEnabled;
}
// LED 2 feedback: ON if widen enabled, OFF if disabled
ledBoost.Set(widenEnabled ? 1.0f : 0.0f);
```

### Toggle Switch 1 (Tape Age)
- Updated only on value change (reduces overhead)
- Recalculates age-dependent parameters
- Affects: headroom, saturation, wow/flutter depths, HF cutoff, dropouts

### Toggle Switch 2 (Tape Speed)
- Updated only on value change
- Recalculates speed-dependent parameters
- Affects: headroom, saturation, wow/flutter rates/depths, HF rolloff

### Toggle Switch 3 (Compression)
- Simply updates lofiComp.SetMode()
- Modes: 0=OFF, 1=LITE, 2=HEAVY

---

## 12. STEREO LINKING & WIDTH

### Widen Mode Effects
```cpp
const float width = widenEnabled ? 1.35f : 1.0f;

if (std::abs(width - 1.0f) > 1.0e-3f)
{
    for (size_t i = 0; i < numSamples; ++i)
    {
        const float mid = 0.5f * (out[0][i] + out[1][i]);
        float side = 0.5f * (out[0][i] - out[1][i]);
        side *= width;
        out[0][i] = mid + side;
        out[1][i] = mid - side;
    }
}
```

**Width = 1.0**: Mono-compatible (equal L/R)
**Width = 1.35**: Stereo widened (+35% side channel)

### Stereo Blending in Noise
```cpp
const float stereoBlend = widenEnabled ? 1.0f : 0.0f;
tapeNoise.SetStereoBlend(stereoBlend);
// 0.0 = fully linked (mono), 1.0 = independent (stereo)
```

### Stereo Blending in Wow/Flutter
```cpp
const float stereoBlendForWidth(float widthValue)
{
    if (widthValue <= kWidthUnity)         // 1.0
        return 0.0f;
    if (widthValue >= kWidthDefault)       // 1.35
        return 1.0f;
    const float denom = kWidthDefault - kWidthUnity;
    return (widthValue - kWidthUnity) / denom;
}
```

---

## 13. FINAL SOFT LIMITER

### Post-Chain Safety Limiting
```cpp
// After all processing and level scaling:
out[0][i] = std::tanh(out[0][i] * 0.95f);
out[1][i] = std::tanh(out[1][i] * 0.95f);

// tanh curve:
// ±1.0 input → ±0.76 output (slight compression)
// ±2.0 input → ±0.96 output (more aggressive)
// Provides soft knee to prevent digital clipping
```

The 0.95f pre-gain ensures the tanh curve operates in a controlled region.

---

## 14. PARAMETER UPDATE FLOW (PER AUDIO BLOCK)

```
AudioCallback/processBlock()
  └─ updateModulesFromParameters()  // Read all knob/toggle values
     ├─ Apply color smoothing (CRITICAL!)
     ├─ Calculate clipping crossfade parameters
     ├─ Build GainStageModule::Params
     │  ├─ Gain mappings (trim, channel, bass, treble, master)
     │  ├─ Boost amount ramp
     │  └─ Clipping type crossfade setup
     ├─ Update TapeSatModule
     │  ├─ SetDrive (smoothed color param)
     │  ├─ SetDriveMultiplier (age * speed)
     │  └─ SetHFRolloffCutoff (min of age/speed cutoffs)
     ├─ Update HissDropModule
     │  ├─ SetAmount (noise knob)
     │  └─ SetStereoBlend (widen toggle)
     ├─ Update ToneModule
     │  └─ SetAmount (tone knob → bass/treble gains)
     ├─ Update LoFiCompressor
     │  └─ SetMode (compression toggle)
     ├─ Update WowFlutterModule
     │  ├─ SetAmount (wow knob + speed bias)
     │  ├─ SetStereoBlend (widen toggle)
     │  └─ SetModeParameters (depth scales, rates)
     ├─ Update DropoutModule
     │  └─ SetParams (age-dependent dropout characteristics)
     └─ Update global level (post-chain level knob)

  └─ Check bypass → return if true

  └─ Apply input level (0.7x)

  └─ Call UpdateControls on all modules (apply pending param changes)

  └─ Process signal through 11-stage chain:
     1. Gain stage (dual with crossfade)
     2. Tape saturation
     3. Wow & flutter
     4. Hiss & noise
     5. Dropouts
     6. Tone shaping
     7. Compressor
     8. Stereo width
     9. Post-chain level
     10. Soft limiting
     └─ Output
```

---

## 15. KEY IMPLEMENTATION DETAILS FOR HARDWARE

### Oversampling
The Gain Stage uses 4x oversampling to prevent aliasing from clipping type crossfading. This is critical for smooth COLOR knob operation.

### Filter Coefficients
All shelf and highpass/lowpass filters use standard biquad equations. Coefficients must be recomputed whenever:
- Sample rate changes (prepareToPlay only in hardware)
- Bass/treble gain changes
- Tone mode changes

### State Management
All modules maintain internal state that persists between blocks. Be careful with:
- Filter delay line state (z1, z2 registers)
- Envelope followers (compression state)
- Delay buffers (wow/flutter)
- Phase accumulators (LFOs)

Reset state only on:
- prepareToPlay
- SetParams called (for DropoutModule)
- Explicit Init() call

### Memory Allocation
- Wow/Flutter: 2048-sample delay buffers per channel (~42ms at 48kHz)
- Crossfade: One temporary buffer (size of audio block)
- Noise: Pink state per channel
- All allocated during prepareToPlay

### Block Size Handling
- VST: Variable block size (queried from host)
- Hardware: Fixed 4-sample blocks (very tight loops!)
- Must handle both cases for reusability

---

## 16. CRITICAL GOTCHAS

1. **Color Smoothing**: Must smooth COLOR parameter BEFORE using it for any other calculations. This is essential to prevent aliasing when modulating clipping threshold.

2. **Tape Speed Mapping**: UI shows "LOW/STD/HIGH" but internal enum is "HIGH/STD/LOFI". Must remap: `internalSpeed = 2 - uiSpeed`

3. **Dual Gain Stage**: BOTH modules must always process, even when weights are 0 or 1. This keeps the "warm" module ready for smooth transitions.

4. **Headroom Combination**: Age and speed headroom adjustments are ADDED together:
   ```cpp
   combinedHeadroom = ageHeadroomAdj_dB + speedHeadroomAdj_dB;
   ```

5. **Saturation Combination**: Age and speed saturation multipliers are MULTIPLIED:
   ```cpp
   finalMultiplier = ageSaturationMul * speedSaturationMul;
   ```

6. **HF Rolloff Selection**: Take the MORE RESTRICTIVE cutoff:
   ```cpp
   finalCutoff = min(ageCutoffHz, hfRolloffCutoff);
   ```

7. **Wow Bias Application**: Applied AFTER dropout calculations:
   ```cpp
   wowControl = clamp(wowFlutter + wowAmountBias, 0.0f, 1.0f);
   ```

8. **Output Level Routing**: Currently unused but reserved. When implemented, will allow OUTPUT knob to feed into the gain stage instead of post-chain.

9. **Boost Amount Ramp**: ONLY active from 0.70-0.95 COLOR range. Below 0.70 is always 0.0, above 0.95 is always 1.0.

10. **Dropout Mono Linking**: monoLink = 1.0 means both channels are affected equally. Lower values allow independent channel dropouts.

