# TapeScam VST3 - Build Instructions

## Quick Start (Automated Build)

### macOS / Linux

```bash
cd /Volumes/ExtFS/charlesvestal/github/CVCHothouse/TapeScamVST
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release -j8
```

### Windows

```cmd
cd C:\path\to\CVCHothouse\TapeScamVST
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

## What Happens During Build

1. **CMake Configuration**: Downloads JUCE framework (~30MB) via FetchContent
2. **JUCE Setup**: Configures VST3, AU, and Standalone targets
3. **Compilation**: Builds all DSP modules + JUCE wrapper
4. **Installation**: Copies plugin to system folder automatically

## Build Outputs

After successful build, you'll find:

- **VST3**: `TapeScamVST.vst3` (copied to system plugin folder)
- **AU** (macOS): `TapeScamVST.component` (copied to `~/Library/Audio/Plug-Ins/Components/`)
- **Standalone**: `TapeScamVST` (standalone app in build directory)

## Build Targets

Build specific formats:

```bash
# VST3 only
cmake --build . --target TapeScamVST_VST3 --config Release

# AU only (macOS)
cmake --build . --target TapeScamVST_AU --config Release

# Standalone only
cmake --build . --target TapeScamVST_Standalone --config Release
```

## Troubleshooting

### CMake can't find compiler

**macOS**: Install Xcode Command Line Tools
```bash
xcode-select --install
```

**Linux**: Install build essentials
```bash
sudo apt-get install build-essential cmake git
```

### JUCE download fails

If FetchContent can't download JUCE, manually clone it:

```bash
cd /path/to/CVCHothouse
git clone https://github.com/juce-framework/JUCE.git
cd TapeScamVST
```

Then edit `CMakeLists.txt` line 12:
```cmake
# Comment out FetchContent, use local JUCE
# FetchContent_Declare(...)
add_subdirectory(../JUCE JUCE)
```

### Plugin doesn't load in DAW

1. Check plugin was copied to correct location
2. Rescan plugins in DAW
3. macOS: Run AU validation:
   ```bash
   auval -v aufx TpSc CVCh
   ```

### Undefined symbols / linking errors

Make sure all DSP source files from the shared `TapeScam` checkout are included in `CMakeLists.txt` (or pass `-DTAPESCAM_DSP_PATH=/path/to/TapeScam` when configuring):
```cmake
target_sources(TapeScamVST PRIVATE
    ${TAPESCAM_DSP_PATH}/GainStageModule.cpp
    ${TAPESCAM_DSP_PATH}/TapeSatModule.cpp
    # ... etc
)
```

## Development Build (Debug)

For debugging with symbols:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug
```

## Clean Build

To start fresh:

```bash
cd build
rm -rf *
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

## Advanced: Custom JUCE Location

If you already have JUCE installed:

Edit `CMakeLists.txt` and set JUCE path:
```cmake
set(JUCE_DIR "/path/to/JUCE")
add_subdirectory(${JUCE_DIR} JUCE)
```

## Xcode Project Generation (macOS)

To work in Xcode IDE:

```bash
cmake -G Xcode ..
open TapeScamVST.xcodeproj
```

## Visual Studio Project (Windows)

```cmd
cmake -G "Visual Studio 17 2022" -A x64 ..
start TapeScamVST.sln
```

---

**Estimated build time**: 2-5 minutes (first build downloads JUCE)
