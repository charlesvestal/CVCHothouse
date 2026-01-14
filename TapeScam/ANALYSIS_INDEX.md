# TapeScam VST Implementation Analysis Index

**Generated**: November 24, 2025  
**Analysis Source**: 
- `/Users/charlesvestal/github-local/TapeScamVST/Source/PluginProcessor.h/cpp`
- `/Users/charlesvestal/github-local/CVCHothouse/TapeScam/TapeScam.cpp`
- `/Users/charlesvestal/github-local/CVCHothouse/TapeScam/shared_dsp/*.h/cpp`

---

## Documentation Files

### 1. VST_ANALYSIS.md (Main Reference - 696 lines)
**Use this for**: Complete technical understanding and rebuilding from scratch

**Contents**:
- Section 1: Complete signal chain order (11 stages)
- Section 2: All parameter mappings (knobs, toggles, footswitches)
- Section 3: Drive & gain stage mappings with formulas
- Section 4: Dual gain stage crossfading mechanics (most complex part)
- Section 5: Tape age parameter effects (NEW/USED/WORN modes)
- Section 6: Tape speed parameter effects (HIGH/STD/LOFI modes)
- Section 7: Compression modes documentation
- Section 8: Initial parameter values and defaults
- Section 9: Bypass handling
- Section 10: Parameter smoothing & ramping (5 different coefficients)
- Section 11: Footswitches & toggles behavior
- Section 12: Stereo linking & width (M/S processing)
- Section 13: Final soft limiter implementation
- Section 14: Complete parameter update flow
- Section 15: Key implementation details for hardware
- Section 16: 16 critical gotchas to avoid

**Key Takeaways**:
- Complete parameter update pipeline documented
- All formulas provided with example calculations
- Smoothing coefficients for each parameter explained
- Dual gain stage crossfading is the most critical component

---

### 2. QUICK_REFERENCE.md (Fast Lookup - 125 lines)
**Use this for**: Quick lookup while coding, reference tables

**Contains**:
- Signal chain at a glance (11 stages listed)
- Parameter mappings summary table
- Critical calculation formulas (drive, tone, color)
- Age effects lookup table (3 modes)
- Speed effects lookup table (3 modes)
- Dropout patterns per age
- Smoothing coefficients reference
- Stereo width formula
- Final limiter formula
- Boost amount ramp ranges
- Quick gotchas list (10 items)

**Best For**: Having open in one terminal window while coding

---

### 3. FILE_LOCATIONS.txt (Reference Guide)
**Use this for**: Understanding file structure and recommended reading order

**Contains**:
- File locations for all source files
- Descriptions of each module's responsibility
- Key parameter IDs
- Recommended reading order (6 steps, ~2 hours total)
- Build information and toolchain references

---

## How to Use This Analysis

### For Understanding the Full Implementation
1. Read **QUICK_REFERENCE.md** (5 minutes)
2. Read **VST_ANALYSIS.md** Sections 1-5 (15 minutes)
3. Read **PluginProcessor.cpp** (25 minutes)
4. Read **TapeScam.cpp** Hardware (25 minutes)
5. Read **VST_ANALYSIS.md** Sections 6-16 (30 minutes)
6. Study module headers (20 minutes)

### For Rebuilding TapeScam.cpp from Scratch
1. Keep **QUICK_REFERENCE.md** open in one window
2. Reference **VST_ANALYSIS.md** for each section you're implementing
3. Compare with existing implementation in **TapeScam.cpp**
4. Check **FILE_LOCATIONS.txt** for file organization

### For Porting to Another Platform
1. Start with **QUICK_REFERENCE.md** for overall understanding
2. Use **VST_ANALYSIS.md** Section 1 for signal chain order
3. Use **VST_ANALYSIS.md** Sections 2-7 for parameter calculations
4. Study GainStageModule.h/cpp for the most complex module
5. Use shared_dsp modules as templates (platform-independent)

---

## Critical Implementation Points

### Must-Know Details
1. **COLOR Smoothing** (Section 10 of VST_ANALYSIS.md)
   - Alpha = 0.08 (NOT 0.08f in some code - check your version)
   - Applied BEFORE any COLOR knob usage
   - This prevents aliasing artifacts

2. **Dual Gain Stage** (Section 4 of VST_ANALYSIS.md)
   - Two modules run in PARALLEL
   - Both maintain full state at all times
   - Outputs are continuously blended

3. **Parameter Combination Rules**
   - Headroom: ADD age + speed adjustments
   - Saturation: MULTIPLY age * speed multipliers
   - HF Cutoff: MIN(age_cutoff, speed_cutoff)

4. **Toggle Speed Mapping**
   - UI shows: LOW, STD, HIGH (positions 0, 1, 2)
   - Internal uses: HIGH, STD, LOFI (positions 0, 1, 2)
   - Remap: internal = 2 - UI_position

### Common Mistakes to Avoid
- Not smoothing COLOR before using it
- Only processing one gain stage (both must run)
- Adding saturation multipliers instead of multiplying them
- Using raw COLOR knob instead of smoothedColor
- Forgetting to apply wow bias

---

## Parameter Quick Reference

### Knob Ranges (0.0-1.0)
| Name | Default | Formula | Notes |
|------|---------|---------|-------|
| INPUT | 0.7 | Direct scale | Fixed 0.7x level |
| DRIVE | 0.0 | Squared for trim | -2 to 32 dB trim |
| COLOR | 0.0 | Crossfade types | MUST smooth (0.08) |
| WOW | 0.0 | Depth + bias | Speed-dependent bias |
| NOISE | 0.0 | Pink noise level | -60 to -6 dB |
| TONE | 0.5 | Curved bass/treble | ±12 dB each |
| OUTPUT | 1.0 | Post-chain level | -12 to +6 dB |

### Toggle Settings
| Toggle | Positions | Default | Effects |
|--------|-----------|---------|---------|
| AGE | NEW/USED/WORN | NEW | Saturation, WF, dropouts |
| SPEED | HIGH/STD/LOFI | STD | Headroom, rates, bias |
| COMP | OFF/LITE/HEAVY | OFF | AGC mode |

### Footswitches
| Switch | Default | Function |
|--------|---------|----------|
| BYPASS | OFF | True bypass |
| WIDEN | ON | Stereo width (1.0x or 1.35x) |

---

## Smoothing Coefficients Summary

All applied independently, cumulative in effect:

```
kColorSmooth = 0.08              *** CRITICAL - prevents aliasing ***
kLevelSmooth = 0.01              (Post-chain level)
kDriveSmooth = 0.3               (TapeSat drive, plugin)
kWowSmooth = 0.3                 (WowFlutter amount, plugin)
kNoiseSmooth = 0.5               (HissDropModule amount)
```

### Time Constants at 48 kHz
- COLOR (0.08): ~12.5 samples = 260 microseconds
- LEVEL (0.01): ~100 samples = 2.08 milliseconds
- DRIVE (0.3): ~3.3 samples = 69 microseconds
- WOW (0.3): ~3.3 samples = 69 microseconds
- NOISE (0.5): ~2 samples = 42 microseconds

---

## Signal Chain Diagram

```
Input (0.7x)
    ↓
Dual Gain Stage [continuously crossfaded on COLOR]
    ├─ PreHP/LP Filters
    ├─ Drive Gain Mapping
    ├─ 4x Oversampling
    ├─ 3-Type Clipping (soft/gentle/hard)
    └─ PostLP Filter
    ↓
Tape Saturation
    ├─ Soft-knee compression
    ├─ Split-band processing (low/high at 400Hz)
    ├─ Asymmetric saturation
    └─ HF Rolloff (age + speed dependent)
    ↓
Wow & Flutter
    ├─ Dual LFOs (wow + flutter)
    ├─ 2048-sample delay line
    ├─ Burst effects
    └─ Stereo blending
    ↓
Hiss & Noise
    ├─ Pink noise generation
    └─ Stereo blending
    ↓
Dropouts
    └─ Envelope-based amplitude reduction (age dependent)
    ↓
Tone Shaping
    ├─ Bass shelf (±12 dB)
    └─ Treble shelf (±12 dB)
    ↓
Lo-Fi Compressor
    └─ AGC: OFF/LITE/HEAVY
    ↓
Stereo Width (M/S)
    └─ 1.0x or 1.35x
    ↓
Post-Chain Level (-12 to +6 dB)
    ├─ Exponential smoothing (0.01)
    └─ Soft limiter (tanh)
    ↓
Output
```

---

## Module Responsibilities

### GainStageModule
- Most complex module
- Dual-path configuration for crossfading
- 4x oversampling for anti-aliasing
- Three clipping types continuously blended
- Pre/post EQ with biquad filters

### TapeSatModule
- Soft-knee compression curves
- Asymmetric saturation for even-order harmonics
- Split-band processing
- Compression envelope followers
- HF rolloff (frequency-dependent on tape speed)

### WowFlutterModule
- Pitch modulation via 2048-sample delay line
- Two independent LFO systems
- Burst effects for characterful modulation
- Stereo linking with blending

### HissDropModule
- Pink noise generation (filtered white noise)
- Tone shaping via noise color factor
- Stereo blending (mono-linked or independent)

### DropoutModule
- Random envelope-based amplitude reduction
- Rate/depth/duration configurable per tape age
- Cluster probability and mono-linking

### ToneModule
- Bass/treble shelf filtering
- Two biquad filters per channel

### LoFiCompressor
- Upward compression (boosts quiet signals)
- Three modes: OFF, LITE, HEAVY
- Makes hiss "breathe" with signal level

---

## Testing Checklist for Rebuild

- [ ] Input level applied (0.7x)
- [ ] COLOR parameter smoothing active (0.08 alpha)
- [ ] Both gain stages processing in parallel
- [ ] Clipping type crossfade working smoothly
- [ ] TAPE AGE affecting headroom, saturation, wow depth, cutoff, dropouts
- [ ] TAPE SPEED affecting headroom, saturation, WF rates, bias, cutoff
- [ ] COMPRESSION modes OFF/LITE/HEAVY working
- [ ] WIDEN toggle changing width from 1.0x to 1.35x
- [ ] BYPASS enabling true bypass
- [ ] All smoothing coefficients applied
- [ ] Output soft limiter (tanh) working
- [ ] Final level scaling with 0.01 alpha smoothing

---

## References

**VST Plugin Code**:
- `/Users/charlesvestal/github-local/TapeScamVST/Source/PluginProcessor.cpp`
- Look for: `updateModulesFromParameters()` and `processBlock()`

**Hardware Code**:
- `/Users/charlesvestal/github-local/CVCHothouse/TapeScam/TapeScam.cpp`
- Look for: `UpdateAllParameters()` and `AudioCallback()`

**Shared DSP**:
- `/Users/charlesvestal/github-local/CVCHothouse/TapeScam/shared_dsp/`
- Start with: `GainStageModule.h` for the most complex component

---

**Last Updated**: November 24, 2025  
**Analysis Status**: COMPLETE  
**Implementation Status**: Hardware (tested), VST (complete with GUI)
