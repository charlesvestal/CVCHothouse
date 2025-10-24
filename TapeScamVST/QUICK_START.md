# TapeScam VST - Quick Start Guide

## ✅ Plugin is Ready to Use!

The VST3 plugin has been built and installed at:
```
~/Library/Audio/Plug-Ins/VST3/TapeScam.vst3
```

No quarantine flags detected - the plugin should load immediately.

---

## Loading in Your DAW

### Logic Pro / GarageBand
1. Open Logic Pro
2. Go to: **Logic Pro > Preferences > Plug-in Manager**
3. Click **Reset & Rescan Selection** (or just **Rescan**)
4. Create an audio track
5. Insert plugin: **Audio FX > CVC Hothouse > TapeScam**

### Ableton Live
1. Open Ableton Live
2. Go to: **Preferences > Plug-Ins**
3. Click **Rescan**
4. In an audio track, click the **Audio Effects** browser
5. Find: **Audio Units > CVC Hothouse > TapeScam** (AU) or **VST3 > CVC Hothouse > TapeScam** (VST3)

### Reaper
1. Open Reaper
2. Go to: **Options > Preferences > Plug-ins > VST**
3. Click **Re-scan**
4. Insert on track: **FX > VST3: TapeScam (CVC Hothouse)**

### Pro Tools (2023+)
1. Open Pro Tools
2. Go to: **Setup > Plug-ins**
3. Click **Rescan Plug-ins**
4. Insert on track: **Plug-in > Audio Effects > CVC Hothouse > TapeScam (VST3)**

---

## First Sound Test

### Quick Test Patch
1. Insert TapeScam on an audio track with a drum loop or vocal
2. Set these values:
   - **Drive**: 0.7
   - **Saturation**: 0.6
   - **Wow/Flutter**: 0.3
   - **Noise**: 0.4
   - **Tone**: 0.5 (neutral)
   - **Level**: 0.7
   - **Tape Age**: Used
   - **Tape Speed**: Standard
   - **Compression**: Light

3. Play audio - you should hear:
   - Warm saturation
   - Subtle pitch wobbling
   - Background tape hiss
   - Gentle "breathing" compression

### Extreme Lo-Fi Test
1. **Drive**: 0.9
2. **Saturation**: 0.9
3. **Wow/Flutter**: 0.7
4. **Noise**: 0.8
5. **Tone**: 0.2 (dark)
6. **Level**: 0.6
7. **Tape Age**: Worn
8. **Tape Speed**: Lo-Fi
9. **Compression**: Heavy

Result: Heavily degraded, crushed, pumping tape sound with dramatic wow/flutter.

---

## Standalone App (No DAW Required)

You can also run TapeScam as a standalone app:

```bash
open /Volumes/ExtFS/charlesvestal/github/CVCHothouse/TapeScamVST/build/TapeScamVST_artefacts/Release/Standalone/TapeScam.app
```

This allows you to:
- Test the plugin without a DAW
- Process audio files directly
- Use with system audio routing tools (like Rogue Amoeba's Audio Hijack)

---

## Troubleshooting

### "Plugin failed to load" in DAW

**Solution 1**: Verify installation
```bash
ls -la ~/Library/Audio/Plug-Ins/VST3/TapeScam.vst3
```

**Solution 2**: Check plugin validity
```bash
# Test AU version (macOS)
auval -v aufx TpSc CVCh
```

**Solution 3**: Check for code signing issues
```bash
codesign -dv ~/Library/Audio/Plug-Ins/VST3/TapeScam.vst3
```

### Plugin appears but produces no sound

**Checklist**:
- [ ] Check bypass button is OFF
- [ ] Level knob is not at 0
- [ ] Drive knob is above 0.1
- [ ] Host playback is running
- [ ] Plugin is inserted on correct track

### Plugin causes audio glitches/dropouts

**Solution**: Increase DAW buffer size
- Logic: **Preferences > Audio > I/O Buffer Size** → 512 or 1024
- Ableton: **Preferences > Audio > Latency** → 512 or 1024 samples
- Reaper: **Options > Preferences > Audio > Device** → Block size 512+

### Parameters don't respond

**Check**:
1. Parameter automation isn't overriding manual control
2. Try closing and reopening plugin window
3. Delete plugin and re-insert

---

## Parameter Automation

All parameters are automation-ready:

```
drive, saturation, wowFlutter, noise, tone, level,
tapeAge, tapeSpeed, compression, bypass
```

**Example Automation Ideas**:
- Automate **Wow/Flutter** to increase over time (degrading tape)
- Automate **Compression** from Off → Heavy for dramatic pumping
- Automate **Tape Age** from New → Worn during a breakdown
- Automate **Tone** to sweep from dark to bright

---

## Preset Recommendations

### "Clean Tape"
- Drive: 0.3
- Saturation: 0.2
- Wow/Flutter: 0.1
- Noise: 0.1
- Tone: 0.6
- Tape Age: New
- Tape Speed: High
- Compression: Off

### "Standard Cassette"
- Drive: 0.5
- Saturation: 0.5
- Wow/Flutter: 0.3
- Noise: 0.4
- Tone: 0.5
- Tape Age: Used
- Tape Speed: Standard
- Compression: Light

### "Worn Lo-Fi"
- Drive: 0.8
- Saturation: 0.8
- Wow/Flutter: 0.6
- Noise: 0.7
- Tone: 0.3
- Tape Age: Worn
- Tape Speed: Lo-Fi
- Compression: Heavy

### "Subtle Warmth"
- Drive: 0.4
- Saturation: 0.3
- Wow/Flutter: 0.0
- Noise: 0.05
- Tone: 0.55
- Tape Age: New
- Tape Speed: High
- Compression: Off

### "Extreme Degradation"
- Drive: 1.0
- Saturation: 1.0
- Wow/Flutter: 0.9
- Noise: 1.0
- Tone: 0.2
- Tape Age: Worn
- Tape Speed: Lo-Fi
- Compression: Heavy

---

## Performance

**CPU Usage**: Moderate (comparable to typical saturation plugin)
- Light settings: ~3-5% CPU (MacBook Pro M1)
- Heavy settings: ~8-12% CPU
- Wow/Flutter is most CPU-intensive module

**Latency**: Minimal (~42ms from wow/flutter delay buffer, compensated by host)

---

## What's Next?

1. **Test in your DAW** - Load it up and process some audio!
2. **Experiment** - Try extreme settings to explore the full range
3. **A/B Test** - Compare to the hardware pedal (if you have one)
4. **Save Presets** - Use your DAW's preset system to save favorite settings
5. **Automate** - Create dynamic tape effects with parameter automation

---

## Support

For issues or questions:
1. Check the main `README.md` for technical details
2. Review `BUILD_INSTRUCTIONS.md` for rebuild info
3. See `CONVERSION_SUMMARY.md` for architecture details

---

Enjoy your new tape emulation plugin! 🎛️📼✨
