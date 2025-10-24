# Debugging TapeScam VST Parameters

## Issue
Parameters not responding except drive, level, tape speed, and compression.

## Diagnostic Steps

### 1. Check Parameter Values in DAW
Open plugin in your DAW and verify:
- Saturation knob at 0.5 (50%)
- Wow/Flutter knob at 0.3 (30%)
- Noise knob at 0.3 (30%)
- Tone knob at 0.5 (50%)

### 2. Expected Behavior

**Saturation at 0.5:**
- Should apply tanh() saturation with drive = 1.0 + (0.5 * 1.0) * 2.5 = 2.25x
- Should be VERY audible on drums or vocals

**Wow/Flutter at 0.3:**
- Wow rate: 0.25 + 0.3 * 0.25 = 0.325 Hz (slow wobble)
- Flutter rate: 2.0 + 0.3 * 3.0 = 2.9 Hz (faster wobble)
- Should create obvious pitch warbling

**Noise at 0.3:**
- Hiss level: -60 + 0.3 * 54 = -43.8 dB
- Should be audible pink noise

**Tone at 0.5:**
- Bass gain: 0.5 * 12 - 6 = 0 dB (flat)
- Treble gain: 0.5 * 12 - 6 = 0 dB (flat)
- At 0.0: -6dB bass/treble (dark)
- At 1.0: +6dB bass/treble (bright)

### 3. Manual Testing

Try these extreme settings to verify modules work:

**Test 1: Maximum Saturation**
- Saturation: 1.0
- Drive: 0.7
- Play a sine wave → should hear heavy distortion

**Test 2: Maximum Wow/Flutter**
- Wow/Flutter: 1.0
- Play sustained note → should hear obvious pitch wobble

**Test 3: Maximum Noise**
- Noise: 1.0
- No input signal → should hear loud pink noise

**Test 4: Tone Sweep**
- Tone: 0.0 → very muffled/dark
- Tone: 1.0 → very bright

### 4. Code Check Points

Check if modules are actually processing:

**TapeSatModule::Process()** (Line 270)
```cpp
if(smoothedDrive_ < kDriveBypass)  // kDriveBypass = 0.01
    return;  // Early exit if drive too low
```

**Question**: Is `smoothedDrive_` actually being set correctly?

**WowFlutterModule** needs buffer allocation - check if `Init()` was called properly.

**HissDropModule** - check if `SetAmount()` is being called.

**ToneModule** - check if filter coefficients are being calculated.

## Possible Causes

1. **UpdateControls() timing**: Modules might clear dirty flag before processing
2. **Parameter smoothing**: Values might be smoothing from 0→target slowly
3. **Early returns**: Modules might be bypassing processing due to threshold checks
4. **Buffer initialization**: Some modules need proper Init() with sample rate

## Quick Fix Test

Try adding this to `prepareToPlay()` in PluginProcessor.cpp:

```cpp
// Force initial parameter update
tapeSat.SetDrive(0.8f);
tapeWobble.SetAmount(0.5f);
tapeNoise.SetAmount(0.5f);
tapeTone.SetAmount(0.5f);

// Force update cycles
for(int i = 0; i < 100; i++) {
    tapeSat.UpdateControls();
    tapeWobble.UpdateControls();
    tapeNoise.UpdateControls();
    tapeTone.UpdateControls();
}
```

This forces parameter smoothing to complete during initialization.

## Next Step

Reload the plugin in your DAW after rebuilding and test each parameter individually with extreme values (0.0 and 1.0).
