# PSXVerb Debugging Guide

## Build Parameters

**Final Configuration:**
- **B** (block size): 16 samples
- **N_h** (head FFT): 256 samples
- **N_t** (tail FFT): 2048 samples
- **K** (tail partitions per block): 1
- **Head coverage**: 128ms (24 partitions @ 256 samples each)
- **Tail coverage**: Remainder of IR (~8 seconds, 192 partitions @ 2048 samples each)

**Latency:**
- Worst case: N_t / SR = 2048 / 48000 = **42.7ms**
- Actual: Typically lower due to head processing

**Memory Usage:**
- Flash: 87,912 bytes (67.07% of 128KB)
- SRAM: 148,972 bytes (28.41% of 512KB) - **major improvement from 53%**
- SDRAM: 12,468,680 bytes (18.58% of 64MB)

**Rationale for B=16:**
- Original B=4 was causing CPU overload with FFT-based convolution
- B=16 reduces callback rate 4× while adding only 0.33ms latency
- Eliminates digital whine while providing enough CPU headroom

## CPU Notes

**Head Tier** (processed every block):
- 24 partitions × 256-point FFT = ~6,144 FFT operations per block
- Includes forward FFT + complex multiply + inverse FFT
- Total: ~24 × (256 FFT + 256 multiply + 256 IFFT) per 16-sample block

**Tail Tier** (round-robin, K=1):
- 1 partition × 2048-point FFT per block
- Spreads tail processing over 192 blocks
- Each partition visited every 192 blocks = ~64ms cycle time

**Total Estimate:**
- ~40-50% CPU at 48kHz with B=16
- No audio dropout or underruns observed in testing

## Diagnostic Modes

PSXVerb includes three processing modes selectable via **Toggle Switch 3**:

### Mode 1: Diagnostic Bypass (Toggle 3 UP)
- **Function**: Pass-through (input → output directly)
- **Purpose**: Verify signal path and hardware integrity
- **LED 2**: Slow blink (1Hz, 500ms on/off)
- **Expected**: Clean audio, no processing artifacts

### Mode 2: Convolution Reverb (Toggle 3 MIDDLE) **[DEFAULT]**
- **Function**: Full FFT-based partitioned convolution
- **Purpose**: Production reverb mode
- **LED 2**: Steady brightness = IR index (0=off, 0.33, 0.66, 1.0=full)
- **Expected**: Smooth, long-tail reverb matching PS1 character

### Mode 3: Diagnostic FIR-1024 (Toggle 3 DOWN)
- **Function**: Time-domain 1024-tap FIR filter
- **Purpose**: Verify IR data and test shorter reverb tail
- **LED 2**: Fast blink (5Hz, 100ms on/off)
- **Expected**: Early reflections only, shorter tail (~21ms)

## LED Indicators

### LED 1 (Bypass Indicator)
- **OFF** = Bypassed (dry signal only)
- **ON** = Active (effect engaged)
- **Control**: Footswitch 1 toggles

### LED 2 (Mode + IR Indicator)
| Mode | LED Pattern | Meaning |
|------|-------------|---------|
| Diag Bypass | Slow blink (1Hz) | Signal path test |
| Convolution | Steady brightness | IR index: 0→1.0 |
| Diag FIR-1024 | Fast blink (5Hz) | FIR test mode |

**IR Selection (Convolution mode):**
- Knob 1 at 0%: Studio (LED2 = 0% brightness)
- Knob 1 at 33%: Church (LED2 = 33% brightness)
- Knob 1 at 66%: Dome (LED2 = 66% brightness)
- Knob 1 at 100%: Hall (LED2 = 100% brightness)

## Control Mapping

### Knobs
| Knob | Function | Range | Notes |
|------|----------|-------|-------|
| 1 | IR Selection | Studio/Church/Dome/Hall | 4 discrete positions |
| 2-5 | *Reserved* | - | Future: pre-delay, decay, filters |
| 6 | Wet/Dry Mix | 0-100% | 0=dry, 100=wet |

### Toggle Switches
| Toggle | Function | Positions |
|--------|----------|-----------|
| 1 | *Reserved* | Future: IR length trim |
| 2 | *Reserved* | Future: stereo width |
| 3 | **Mode Select** | UP=Bypass / MID=Convolution / DOWN=FIR-1024 |

### Footswitches
- **Footswitch 1**: Bypass toggle (debounced by Daisy library)
- **Footswitch 2**: *(unused)*

## Troubleshooting

### No Audio Output
1. Check **Toggle 3** - should be MIDDLE for reverb
2. Check **Footswitch 1** - LED1 should be ON
3. Check **Knob 6** - increase wet/dry mix
4. Try **Diag Bypass mode** (Toggle 3 UP) to verify signal path

### Repetitive Tones / Artifacts
1. Switch to **Diag FIR-1024 mode** (Toggle 3 DOWN)
   - If FIR sounds clean → issue in FFT convolution
   - If FIR has artifacts → issue in IR data or signal chain
2. Check for **digital clipping** - reduce input level
3. Verify **block size** in PSXVerb.cpp:107 is `16`

### Footswitch Not Responding
1. Confirm `ProcessAllControls()` is called in audio callback (PSXVerb.cpp:162)
2. Check hardware wiring - footswitch should be normally open
3. Test in **Diag Bypass mode** - bypass should still work

### CPU Overload / Dropouts
1. Reduce **K** (tail_parts_per_block) from 1 to 0 temporarily
2. Reduce **head_ms** from 128 to 64 (fewer head partitions)
3. Check SRAM usage - should be < 50%

### LED2 Not Showing IR Selection
1. Ensure **Toggle 3 = MIDDLE** (Convolution mode)
2. Turn **Knob 1** and observe LED2 brightness change
3. If blinking, you're in diagnostic mode (switch Toggle 3)

## Testing Checklist

### Basic Functionality
- [ ] Footswitch toggles bypass (LED1 reflects state)
- [ ] Diag Bypass mode (Toggle 3 UP) produces clean audio
- [ ] Knob 6 blends dry/wet smoothly

### Convolution Mode
- [ ] Toggle 3 MIDDLE enables reverb
- [ ] Knob 1 switches between 4 IRs (LED2 brightness changes)
- [ ] Long reverb tail (>2 seconds) audible
- [ ] No repetitive tones or artifacts
- [ ] Smooth, natural decay

### Diagnostic FIR Mode
- [ ] Toggle 3 DOWN enables FIR mode
- [ ] LED2 blinks rapidly (5Hz)
- [ ] Short reverb tail (~20ms)
- [ ] Clean early reflections

## Modifying Parameters

### Increase Tail Processing Rate
Edit `PSXVerb.cpp:138`:
```cpp
cfg.tail_parts_per_block = 2;  // Process 2 tail partitions per block
```
**Effect**: Faster tail build-up, higher CPU usage

### Reduce Head Coverage (Lower CPU)
Edit `PSXVerb.cpp:137`:
```cpp
cfg.head_ms = 64.0f;  // Reduce from 128ms to 64ms
```
**Effect**: Fewer head partitions (12 instead of 24), lower CPU, slightly less early reflection detail

### Change Block Size
Edit `PSXVerb.cpp:107`:
```cpp
constexpr size_t kBlockSize = 32;  // Increase from 16
```
**Effect**: Lower CPU usage, higher latency (0.67ms), may reintroduce digital whine

## Known Limitations

1. **IR Length**: Only first ~8 seconds processed (head + 192 tail partitions)
2. **Latency**: ~43ms due to 2048-sample tail FFT
3. **Stereo**: L and R channels processed independently (no crosstalk)
4. **Dynamic Changes**: IR switching causes brief glitch (no crossfade)

## Future Improvements

1. **Soft IR Switching**: 50ms crossfade when changing IRs
2. **Pre-delay**: Knob 2 controls 0-200ms pre-delay
3. **Decay Control**: Knob 3 trims tail partitions for shorter reverb
4. **EQ Controls**: Knobs 4-5 for low/high cut filters
5. **Stereo Width**: Toggle 2 for mono/stereo/wide processing

---

**Last Updated**: 2024-10-20
**Build**: PSXVerb v1.0 (FFT-based partitioned convolution)
**Block Size**: 16 samples
**Sample Rate**: 48kHz
