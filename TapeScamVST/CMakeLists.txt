cmake_minimum_required(VERSION 3.15)

project(TapeScamVST VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET 15.0 CACHE STRING "" FORCE)
    # Ensure libraries can be found during Xcode archiving
    set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_LOCATION NO)
    set(CMAKE_XCODE_ATTRIBUTE_SKIP_INSTALL YES)
    # Tell Xcode to use the built products directory for finding libraries
    set(CMAKE_XCODE_ATTRIBUTE_CONFIGURATION_BUILD_DIR "\$(BUILD_DIR)/\$(CONFIGURATION)\$(EFFECTIVE_PLATFORM_NAME)")
    # Set development team for code signing
    set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "CYVQ7NZ3K6")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
endif()

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
set(FOLEYS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FOLEYS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
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
    ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Images/Icon.png
    ICON_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Images/Icon.png
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "TapeScam needs microphone access to capture and process your audio."
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
    IS_IOS_APP_EXTENSION FALSE
)

set(TAPESCAM_DSP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../TapeScam/shared_dsp" CACHE PATH "Path to the shared TapeScam DSP sources")

set(TAPESCAM_DSP_SOURCES
    "${TAPESCAM_DSP_PATH}/GainStageModule.cpp"
    "${TAPESCAM_DSP_PATH}/TapeSatModule.cpp"
    "${TAPESCAM_DSP_PATH}/WowFlutterModule.cpp"
    "${TAPESCAM_DSP_PATH}/HissDropModule.cpp"
    "${TAPESCAM_DSP_PATH}/ToneModule.cpp"
    "${TAPESCAM_DSP_PATH}/LoFiCompressor.cpp"
)

foreach(dsp_file IN LISTS TAPESCAM_DSP_SOURCES)
    if (NOT EXISTS "${dsp_file}")
        message(FATAL_ERROR "Missing DSP source ${dsp_file}. Set TAPESCAM_DSP_PATH to your TapeScam checkout.")
    endif()
endforeach()

# Source files
target_sources(TapeScamVST
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ui/GuiFactories.cpp
        Source/ui/MagicKnobWithOverlay.cpp
        Source/ui/MagicThreeWaySwitch.cpp
        ${TAPESCAM_DSP_SOURCES}
)

file(GLOB TAPESCAM_GUI_IMAGES CONFIGURE_DEPENDS Resources/Images/*)

juce_add_binary_data(TapeScamGUIAssets
    SOURCES
        Resources/Magic/TapeScam.magic
        ${TAPESCAM_GUI_IMAGES}
)

add_custom_command(TARGET TapeScamVST POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E env CONFIG=$<CONFIG>
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/copy_static.sh"
            "$<TARGET_FILE:TapeScamVST>"
            "${CMAKE_CURRENT_BINARY_DIR}/TapeScamVST_artefacts"
)

add_custom_command(TARGET TapeScamGUIAssets POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E env CONFIG=$<CONFIG>
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/copy_static.sh"
            "$<TARGET_FILE:TapeScamGUIAssets>"
            "${CMAKE_CURRENT_BINARY_DIR}/TapeScamVST_artefacts"
    COMMENT "Copying TapeScamGUIAssets to artefacts directory"
)

# Removed PRE_LINK commands - they were adding incorrect library paths to the linker

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

# For iOS archiving: Don't override CONFIGURATION_BUILD_DIR - let Xcode use default
# Just add library search path to find libraries in BUILT_PRODUCTS_DIR
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(TapeScamVST_AUv3 PROPERTIES
        XCODE_ATTRIBUTE_LIBRARY_SEARCH_PATHS "\$(inherited) \$(BUILT_PRODUCTS_DIR)"
    )
    set_target_properties(TapeScamVST_Standalone PROPERTIES
        XCODE_ATTRIBUTE_LIBRARY_SEARCH_PATHS "\$(inherited) \$(BUILT_PRODUCTS_DIR)"
    )

    # Add post-build script to fix embedded AUv3 during archive
    # This copies the correctly built appex over the symlink placeholder
    add_custom_command(TARGET TapeScamVST_Standalone POST_BUILD
        COMMAND bash -c "if [ -d \"\$(BUILT_PRODUCTS_DIR)/TapeScam.appex\" ]; then rm -rf \"\$(TARGET_BUILD_DIR)/TapeScam.app/PlugIns/TapeScam.appex\" && cp -R \"\$(BUILT_PRODUCTS_DIR)/TapeScam.appex\" \"\$(TARGET_BUILD_DIR)/TapeScam.app/PlugIns/\"; echo 'Fixed embedded AUv3 plugin'; fi"
        COMMENT "Fixing embedded AUv3 plugin for iOS archive"
        VERBATIM
    )

    add_custom_command(TARGET TapeScamVST_AUv3 POST_BUILD
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/set_auv3_icon.sh"
                "$<TARGET_FILE_DIR:TapeScamVST_AUv3>/Info.plist"
        COMMENT "Syncing AUv3 NSExtensionIcon with primary AppIcon"
        VERBATIM
    )
endif()

# Include directories
target_include_directories(TapeScamVST
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${TAPESCAM_DSP_PATH}
)
