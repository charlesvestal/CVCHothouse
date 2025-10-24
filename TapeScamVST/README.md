# TapeScam VST3 Plugin

Lo-Fi Tape Emulation - Authentic tape degradation with musical character.

This is a VST3/AU/Standalone audio plugin version of the TapeScam firmware for Daisy Seed. It provides the exact same tape emulation DSP processing in a plugin format for use in your DAW.

## Features

- **Authentic Tape Emulation**: 6-module signal chain replicating tape deck behavior
- **Input Drive & Saturation**: Tape-style soft saturation with frequency-dependent characteristics
- **Wow & Flutter**: Pitch modulation from tape speed variations
- **Hiss & Dropouts**: Pink noise and random signal dropouts
- **Tone Shaping**: Frequency response control from dark to bright
- **Lo-Fi Compressor**: Upward AGC compression creates "breathing" effect
- **Tape Age Modes**: New, Used, Worn (affects hiss, saturation, headroom)
- **Tape Speed Modes**: High (clean), Standard, Lo-Fi (crushed)
- **Compression Modes**: Off, Light (10x boost), Heavy (20x boost)

## Building from Source

### Prerequisites

- CMake 3.15 or higher
- C++17 compatible compiler (Xcode on macOS, Visual Studio on Windows, GCC/Clang on Linux)
- Git

### Build Steps

```bash
cd TapeScamVST
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

The plugin will be automatically copied to your system plugin folder after building.

**macOS**: `~/Library/Audio/Plug-Ins/VST3/` and `~/Library/Audio/Plug-Ins/Components/`
**Windows**: `C:\Program Files\Common Files\VST3\`
**Linux**: `~/.vst3/`

## Parameters

### Knobs (Continuous 0-1)

1. **Drive**: Input gain staging (0.0 = mute, 1.0 = full drive)
2. **Saturation**: Tape saturation intensity
3. **Wow/Flutter**: Time-domain pitch modulation amount
4. **Noise**: Tape hiss and dropout intensity
5. **Tone**: Frequency response shaping (0=dark, 1=bright)
6. **Level**: Master output level

### Tape Age (3-position switch)

- **New**: Clean tape (0.5x hiss, 0.8x wow, full headroom, normal saturation)
- **Used**: Normal wear (1.0x multipliers, -1dB headroom)
- **Worn**: Degraded tape (1.5x hiss, 1.3x wow, -2.5dB headroom, 1.2x saturation)

### Tape Speed (3-position switch)

- **High**: High-speed reel (20kHz HF, +3dB headroom, 0.5x saturation - clean)
- **Standard**: Standard cassette (14kHz HF, -1dB headroom, 1.0x saturation)
- **Lo-Fi**: Slow/budget tape (8kHz HF, -6dB headroom, 2.0x saturation - crushed)

### Compression (3-position switch)

- **Off**: No compression
- **Light**: Gentle AGC pumping (10x boost on silence, musical breathing)
- **Heavy**: Extreme pumping (20x boost, dramatic hiss swell over several seconds)

## Signal Chain

The processing order matches the firmware exactly:

```
Input
  ↓
[Gain Stage] - Drive, clipping, tone shaping
  ↓
[Tape Saturation] - Smooth saturation with anti-aliasing
  ↓
[Wow & Flutter] - Pitch modulation via variable delay
  ↓
[Hiss & Dropouts] - Pink noise + random dropouts
  ↓
[Tone Module] - Frequency response control
  ↓
[Lo-Fi Compressor] - Upward compression (hiss "breathes")
  ↓
[Level Control] - Master output
  ↓
Output
```

## Comparison: Firmware vs. Plugin

| Aspect | Firmware (Daisy Seed) | Plugin (VST3/AU) |
|--------|----------------------|------------------|
| **DSP Code** | Identical | Identical |
| **Parameters** | 6 knobs + 3 toggles | Same (as automation parameters) |
| **Sample Rate** | Fixed 48kHz | Variable (44.1-96kHz+) |
| **Latency** | ~166µs (~8 samples) | Host-dependent buffer size |
| **CPU** | Real-time embedded | Desktop CPU |
| **UI** | Physical knobs/switches | On-screen rotary knobs/combos |
| **Bypass** | Footswitch | Parameter automation |

## Technical Details

### DSP Modules

All DSP modules are shared between the firmware and plugin:

- **GainStageModule**: Input conditioning with biquad EQ
- **TapeSatModule**: Frequency-dependent saturation with SVF filters
- **WowFlutterModule**: Cubic interpolation delay-based pitch modulation
- **HissDropModule**: Paul Kellett pink noise algorithm
- **ToneModule**: Biquad shelving filters
- **LoFiCompressor**: Upward AGC with asymmetric attack/release

### Dependencies

- **JUCE Framework**: Cross-platform audio plugin framework (auto-downloaded via CMake)
- **Standard C++17**: No external DSP libraries required

## Known Limitations

- Reverb module from firmware not included (was not integrated in signal chain)
- Mono input is duplicated to stereo (plugin requires stereo in/out)
- Some hosts may introduce additional latency from buffer size

## Troubleshooting

### Plugin doesn't appear in DAW

- **macOS**: Check `~/Library/Audio/Plug-Ins/VST3/` and run AU validation
- **Windows**: Check `C:\Program Files\Common Files\VST3\`
- Try rescanning plugins in your DAW

### Audio glitches or dropouts

- Increase DAW buffer size (512 or 1024 samples)
- Check CPU usage (wow/flutter can be intensive)

### Parameters not responding

- Check parameter automation in DAW isn't overriding controls
- Try reloading the plugin

## License

This plugin shares the same codebase as the TapeScam firmware. See parent project for license details.

## Credits

- **Original Firmware**: TapeScam for Daisy Seed / Cleveland Music Co. Hothouse
- **Plugin Port**: Adapted for VST3/AU using JUCE framework
- **DSP**: Tape emulation algorithms, pink noise (Paul Kellett)

## Version History

### v1.0.0 (2025)
- Initial release
- Full feature parity with firmware v1.0
- VST3, AU, and Standalone builds
- Parameter automation support
- State save/recall

---

For the hardware firmware version, see: `../TapeScam/`
