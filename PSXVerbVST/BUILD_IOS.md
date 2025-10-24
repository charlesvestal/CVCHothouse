# Building PSXVerb for iOS (AUv3)

This guide explains how to build the PSXVerb AUv3 plugin for iOS/iPadOS.

## Prerequisites

- macOS with Xcode installed
- iOS device or simulator (iOS 13.0+)
- Apple Developer account (for device deployment)
- CMake 3.15 or later

## Quick Start

### 1. Generate Xcode Project

```bash
cd PSXVerbVST
mkdir -p build_ios
cd build_ios
cmake .. -G Xcode -DCMAKE_SYSTEM_NAME=iOS
```

### 2. Open in Xcode

```bash
open PSXVerbVST.xcodeproj
```

### 3. Select Target and Device

In Xcode:
1. Select the **PSXVerbVST_AUv3** scheme from the scheme dropdown
2. Choose your iOS device or simulator as the destination
3. If building for a device, configure signing:
   - Select the PSXVerbVST_AUv3 target
   - Go to "Signing & Capabilities"
   - Select your Team
   - Xcode will automatically manage provisioning

### 4. Build and Run

- Press **Cmd+R** to build and run on your device/simulator
- The AUv3 plugin will be installed as a standalone app
- Open GarageBand, AUM, or any AUv3 host to use the plugin

## Plugin Details

- **Bundle ID:** `com.vestal.PSXVerb`
- **Manufacturer Code:** CVch
- **Plugin Code:** Pxvb
- **Supported Orientations:**
  - iPhone: Landscape Left/Right
  - iPad: All orientations

## Using the Plugin

### Standalone App
The plugin builds as a standalone iOS app that can be launched directly. This app hosts the plugin UI and allows you to test it.

### As AUv3 in Host Apps
Once installed, PSXVerb will appear in any AUv3-compatible host:
- **GarageBand** (iOS)
- **AUM** (Audio Mixer)
- **Cubasis**
- **Beatmaker 3**
- **Drambo**
- Any other AUv3-compatible DAW

### Finding the Plugin
1. Open your host app
2. Add an Audio Unit effect
3. Look under "CVC Hothouse" → "PSXVerb"
4. The plugin features:
   - 6 authentic PSX presets (Room, Studio Small/Medium/Large, Hall, Space Echo)
   - 5 control knobs (Input Gain, Output Gain, Decay, Reverb Level, Mix)
   - Preset selector

## Presets

PSXVerb includes 6 authentic PlayStation SPU reverb presets:

1. **Room** - Small room ambience
2. **Studio Small** - Tight studio space
3. **Studio Medium** - Medium-sized studio
4. **Studio Large** - Large studio hall
5. **Hall** - Concert hall reverb
6. **Space Echo** - Sci-fi/delay-like reverb

## Troubleshooting

### Code Signing Issues
If you see signing errors:
1. Open the Xcode project
2. Select PSXVerbVST_AUv3 target
3. Go to Signing & Capabilities
4. Enable "Automatically manage signing"
5. Select your development team

### Plugin Not Appearing in Host
1. Make sure the app was successfully installed (check iOS home screen)
2. Restart the host app
3. Check Settings → General → iPhone Storage to verify the app is installed
4. Some hosts cache the plugin list - force quit and reopen

### Build Errors
If you get CMake errors:
```bash
# Clean and regenerate
rm -rf build_ios
mkdir build_ios
cd build_ios
cmake .. -G Xcode -DCMAKE_SYSTEM_NAME=iOS
```

## Deployment

### TestFlight / App Store
To distribute via TestFlight or App Store:
1. Archive the PSXVerbVST_AUv3 target (Product → Archive)
2. Use Xcode Organizer to upload to App Store Connect
3. The AUv3 plugin will be distributed as part of the container app

### Ad-Hoc Distribution
For internal testing:
1. Archive with Ad-Hoc provisioning
2. Export .ipa file
3. Distribute via TestFlight or install directly via Xcode

## Notes

- **Performance:** The iOS version uses authentic PSX SPU reverb algorithm
- **Sample Rate:** 48kHz I/O with 24kHz internal processing via halfband resampling
- **UI Scaling:** The UI automatically scales to fit iPad and iPhone screens
- **Presets:** Plugin state is saved by the host app
- **Authenticity:** Same DSP code as PlayStation hardware reverb

## Supported iOS Versions

- **Minimum:** iOS 13.0
- **Tested:** iOS 17.x
- **Recommended:** iOS 15.0+ for best performance

## Technical Details

PSXVerb faithfully recreates the PlayStation SPU reverb:
- 2:1 decimation from 48kHz to 24kHz internal rate
- Authentic reflection, comb, and allpass filter structure
- Halfband39 resampling for aliasing-free conversion
- Zero additional latency from reverb processing
