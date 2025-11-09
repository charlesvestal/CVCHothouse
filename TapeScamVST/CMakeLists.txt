cmake_minimum_required(VERSION 3.15)

project(TapeScamVST VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE subdirectory (adjust path to where you have JUCE installed)
# Option 1: If JUCE is in a sibling directory
# add_subdirectory(../../JUCE JUCE)
#
# Option 2: Download JUCE via FetchContent (recommended)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(
    FoleySGuiMagic
    GIT_REPOSITORY https://github.com/ffAudio/foleys_gui_magic.git
    GIT_TAG main
)
FetchContent_MakeAvailable(FoleySGuiMagic)

# Suppress deprecation warnings for macOS 15 SDK
if(APPLE)
    add_compile_options(-Wno-deprecated-declarations)
endif()

# VST3 SDK path (optional, JUCE can download it)
# juce_set_vst3_sdk_path(...)

# Create plugin target
juce_add_plugin(TapeScamVST
    COMPANY_NAME "CVC Hothouse"
    BUNDLE_ID com.vestal.TapeScam
    PLUGIN_MANUFACTURER_CODE CVCh
    PLUGIN_CODE TpSc
    FORMATS VST3 AU AUv3 Standalone
    PRODUCT_NAME "TapeScam"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    # iOS/AUv3 settings
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown
)

# Source files
target_sources(TapeScamVST
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ui/GuiFactories.cpp
        Source/ui/MagicKnobWithOverlay.cpp
        Source/ui/MagicThreeWaySwitch.cpp
        DSP/GainStageModule.cpp
        DSP/TapeSatModule.cpp
        DSP/WowFlutterModule.cpp
        DSP/HissDropModule.cpp
        DSP/ToneModule.cpp
        DSP/LoFiCompressor.cpp
)

file(GLOB TAPESCAM_GUI_IMAGES CONFIGURE_DEPENDS Resources/Images/*)

juce_add_binary_data(TapeScamGUIAssets
    SOURCES
        Resources/Magic/TapeScam.magic
        ${TAPESCAM_GUI_IMAGES}
)

# Compile definitions
target_compile_definitions(TapeScamVST
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        FOLEYS_ENABLE_BINARY_DATA=1
)

# Link JUCE modules
target_link_libraries(TapeScamVST
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        foleys_gui_magic
        TapeScamGUIAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Include directories
target_include_directories(TapeScamVST
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/DSP
)
