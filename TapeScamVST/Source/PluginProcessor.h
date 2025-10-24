#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/GainStageModule.h"
#include "../DSP/TapeSatModule.h"
#include "../DSP/WowFlutterModule.h"
#include "../DSP/HissDropModule.h"
#include "../DSP/ToneModule.h"
#include "../DSP/LoFiCompressor.h"

//==============================================================================
class TapeScamAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    TapeScamAudioProcessor();
    ~TapeScamAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter IDs
    enum ParameterID
    {
        PARAM_INPUT = 0,
        PARAM_DRIVE,
        PARAM_SATURATION,
        PARAM_WOW_FLUTTER,
        PARAM_NOISE,
        PARAM_TONE,
        PARAM_LEVEL,
        PARAM_TAPE_AGE,
        PARAM_TAPE_SPEED,
        PARAM_COMPRESSION,
        PARAM_BYPASS,
        NUM_PARAMETERS
    };

    // Get parameter values
    float getInput() const { return *inputParam; }
    float getDrive() const { return *driveParam; }
    float getSaturation() const { return *saturationParam; }
    float getWowFlutter() const { return *wowFlutterParam; }
    float getNoise() const { return *noiseParam; }
    float getTone() const { return *toneParam; }
    float getLevel() const { return *levelParam; }
    int getTapeAge() const { return static_cast<int>(*tapeAgeParam); }
    int getTapeSpeed() const { return static_cast<int>(*tapeSpeedParam); }
    int getCompression() const { return static_cast<int>(*compressionParam); }
    bool getBypassed() const { return *bypassParam > 0.5f; }

private:
    //==============================================================================
    // Audio Parameter Tree
    juce::AudioProcessorValueTreeState parameters;

    // Parameter pointers (for fast access)
    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* saturationParam = nullptr;
    std::atomic<float>* wowFlutterParam = nullptr;
    std::atomic<float>* noiseParam = nullptr;
    std::atomic<float>* toneParam = nullptr;
    std::atomic<float>* levelParam = nullptr;
    std::atomic<float>* tapeAgeParam = nullptr;
    std::atomic<float>* tapeSpeedParam = nullptr;
    std::atomic<float>* compressionParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    // DSP Modules (same as firmware)
    GainStageModule gainStage;
    TapeSatModule tapeSat;
    WowFlutterModule tapeWobble;
    HissDropModule tapeNoise;
    ToneModule tapeTone;
    LoFiCompressor lofiComp;

    // State
    float currentSampleRate = 48000.0f;
    float globalLevel = 1.0f;
    float globalLevelSmooth = 1.0f;

    // Processing buffers
    std::vector<float*> tempBuffers;
    std::vector<float> tempBufferStorage;

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void updateModulesFromParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeScamAudioProcessor)
};
