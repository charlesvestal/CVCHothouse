# Foley's Magic GUI Integration Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the hard-coded JUCE editor with a Foley's GUI Magic-driven interface that uses the provided artwork and exposes all plugin parameters.

**Architecture:** The processor now owns a `foleys::MagicProcessorState` sharing the existing `AudioProcessorValueTreeState`. The editor subclasses `foleys::MagicPluginEditor`, loads a 1680×1220 `.magic` layout from binary data (with dev-time file override), and registers two custom component factories for the dual-layer knobs and horizontal three-way toggles. Binary resources package the background and control PNGs alongside the `.magic` file.

**Tech Stack:** JUCE 8, Foley's GUI Magic, C++17, CMake, juce_add_binary_data, AudioProcessorValueTreeState.

### Task 1: Add Foley's GUI Magic module and binary resources

**Files:**
- Modify: `CMakeLists.txt`
- Create: `Resources/CMakeLists.txt`
- Create: `Resources/Magic/TapeScam.magic`
- Create: `Resources/Images/knob_base.png`, `Resources/Images/knob_overlay.png`, `Resources/Images/switch_track.png`, `Resources/Images/switch_handle.png`, `Resources/Images/panel_background.png` (copy supplied artwork)

**Step 1: Fetch Foley's GUI Magic in CMake**
```cmake
FetchContent_Declare(
    FoleySGuiMagic
    GIT_REPOSITORY https://github.com/ffAudio/foleys_gui_magic.git
    GIT_TAG 1.6.0
)
FetchContent_MakeAvailable(FoleySGuiMagic)
```
**Step 2: Link module**
```cmake
target_link_libraries(TapeScamVST PRIVATE foleys_gui_magic juce::juce_audio_utils juce::juce_dsp)
```
**Step 3: Package assets**
```cmake
juce_add_binary_data(TapeScamAssets SOURCES
    Resources/Magic/TapeScam.magic
    Resources/Images/panel_background.png
    Resources/Images/knob_base.png
    Resources/Images/knob_overlay.png
    Resources/Images/switch_track.png
    Resources/Images/switch_handle.png
)

target_link_libraries(TapeScamVST PRIVATE TapeScamAssets)
```
**Step 4: Configure Resources/CMakeLists** to glob additional artwork if needed, and update the root `CMakeLists` with `add_subdirectory(Resources)` if you prefer to isolate binary data.

**Step 5: Verify**
```bash
cmake -S . -B build && cmake --build build
```
Expect: build succeeds with new module pulled.

### Task 2: Introduce MagicProcessorState bridging AVTS

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

**Step 1: Include Foley's headers**
```cpp
#include <foleys_gui_magic/foleys_gui_magic.h>
```
**Step 2: Add member to processor**
```cpp
foleys::MagicProcessorState magicState { *this, parameters };
```
**Step 3: Expose accessor**
```cpp
foleys::MagicProcessorState& getMagicState() noexcept { return magicState; }
```
**Step 4: In constructor, register default editor settings (optional)**
```cpp
magicState.setGuiValueTree (magicState.readGuiValueTreeFromData (BinaryData::TapeScam_magic, BinaryData::TapeScam_magicSize));
```
**Step 5: Ensure `prepareToPlay`/`releaseResources` call `magicState.prepareToPlay()` / `.releaseResources()` if automation needed (Magic expects this when processors drive modulators).**

### Task 3: Implement custom Magic component factories

**Files:**
- Create: `Source/ui/MagicKnobWithOverlay.h`
- Create: `Source/ui/MagicKnobWithOverlay.cpp`
- Create: `Source/ui/MagicThreeWaySwitch.h`
- Create: `Source/ui/MagicThreeWaySwitch.cpp`

**Step 1: Define `MagicKnobWithOverlay` subclassing `foleys::MagicAttachment` + `juce::Component`**
```cpp
class MagicKnobWithOverlay : public foleys::MagicAttachment, public juce::Component {
public:
    MagicKnobWithOverlay(foleys::MagicBuilder& builder, const juce::ValueTree& node);
    void update() override;
    void paint (juce::Graphics& g) override;
private:
    juce::Image baseImage, overlayImage;
    float minAngle = juce::MathConstants<float>::pi * 1.25f;
    float maxAngle = juce::MathConstants<float>::pi * 2.75f;
};
```
**Step 2: Load images by name from `builder.getManager().getAssets()` and respond to property changes (`base-image`, `overlay-image`, `min-angle`, `max-angle`).**

**Step 3: Register component with builder**
```cpp
builder.registerFactory("MagicKnobWithOverlay", [] (foleys::MagicBuilder& b, const juce::ValueTree& n) {
    return std::make_unique<MagicKnobWithOverlay>(b, n);
});
```
**Step 4: Implement `MagicThreeWaySwitch`** that snaps parameter values 0/1/2 depending on click position, paints supplied `track-image` and `handle-image`, and exposes `labels` property for overlayed text.

**Step 5: Add registration helper** in new `Source/ui/GuiFactories.h/.cpp` to keep `PluginEditor` tidy.

### Task 4: Replace PluginEditor with MagicPluginEditor

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

**Step 1: Change inheritance**
```cpp
class TapeScamAudioProcessorEditor : public foleys::MagicPluginEditor { ... };
```
**Step 2: Remove manual sliders/toggles/bypass button members and attachments.**

**Step 3: In constructor:**
```cpp
TapeScamAudioProcessorEditor::TapeScamAudioProcessorEditor(TapeScamAudioProcessor& p)
    : foleys::MagicPluginEditor (p.getMagicState(), "TapeScam")
{
    auto& builder = getMagicState().getMagicBuilder();
    registerGuiFactories(builder);
    getMagicState().setGuiValueTree (getMagicState().readGuiValueTreeFromProjectFile ("Resources/Magic/TapeScam.magic")); // dev mode fallback
    setResizeLimits (840, 610, 1680, 1220);
}
```
**Step 4: Override `paint` only if you want fallback background; otherwise rely on layout.**

**Step 5: Remove `resized()` logic; Magic handles layout.**

### Task 5: Author the `.magic` layout for 6 knobs + 3 switches

**Files:**
- Modify: `Resources/Magic/TapeScam.magic`

**Step 1: Define root canvas**
```xml
<MagicGUI width="1680" height="1220">
  <View id="root" background="panel_background.png">
    ...
  </View>
</MagicGUI>
```
**Step 2: Add six knobs** (IDs: `drive`, `saturation`, `wowFlutter`, `noise`, `tone`, `level`). Each uses `type="MagicKnobWithOverlay"`, sets `base-image`, `overlay-image`, `parameter-id`, `min-angle="135"`, `max-angle="405"`, `label="DRIVE"`, etc., and positions via `<Bounds x="120" y="140" width="300" height="300"/>` style.

**Step 3: Add three switches** for `tapeAge`, `tapeSpeed`, `compression` with property `positions="New,Used,Worn"`, set `orientation="horizontal"`, `width="400" height="120"`, and place them below knob rows.

**Step 4: Include text labels** either as Magic `<Label>` nodes or embed text into the background art.

**Step 5: Validate with Magic Editor**
```bash
build/TapeScam_artefacts/Debug/Standalone/TapeScam.app/Contents/MacOS/TapeScam --gui-editor
```
Expect: layout hot-reloads when `.magic` changes.

### Task 6: Manual verification & cleanup

**Files:**
- N/A (commands)

**Step 1: Build all formats**
```bash
cmake --build build --config Debug
```
**Step 2: Launch Standalone to spot-check controls**
Verify knobs rotate with overlay intact and switches snap between the three discrete states updating audio.

**Step 3: Document workflow** in `README.md` (optional) explaining how to open the Magic Editor and where assets live.

