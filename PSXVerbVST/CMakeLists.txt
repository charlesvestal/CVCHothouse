cmake_minimum_required(VERSION 3.24)

project(PSXVerbVST VERSION 1.0.0)

# C++17 for modern features
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Suppress macOS 15 deprecation warnings in JUCE 8.0.4
add_compile_options(-Wno-deprecated-declarations)

# Fetch JUCE framework
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(juce)

# Define plugin
juce_add_plugin(PSXVerbVST
    COMPANY_NAME "CVC Hothouse"
    BUNDLE_ID com.vestal.PSXVerb
    PLUGIN_MANUFACTURER_CODE CVch
    PLUGIN_CODE Pxvb
    FORMATS VST3 AU AUv3 Standalone
    PRODUCT_NAME "PSXVerb"
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Fx" "Reverb"
    AU_MAIN_TYPE kAudioUnitType_Effect
    # iOS/AUv3 settings
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown
)

# Source files
target_sources(PSXVerbVST PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# Include directories
target_include_directories(PSXVerbVST PRIVATE
    Source
    DSP
)

# Link JUCE modules
target_link_libraries(PSXVerbVST PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
)

# Compile definitions
target_compile_definitions(PSXVerbVST PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)
