#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/PsxReverb.h"
#include "../DSP/PsxPreset.h"

//==============================================================================
class PSXVerbAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    PSXVerbAudioProcessor();
    ~PSXVerbAudioProcessor() override;

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
        PARAM_PRESET = 0,
        PARAM_INPUT_GAIN,
        PARAM_OUTPUT_GAIN,
        PARAM_DECAY,
        PARAM_REVERB_LEVEL,
        PARAM_MIX,
        PARAM_BYPASS,
        NUM_PARAMETERS
    };

    // Get parameter values
    int getPreset() const { return static_cast<int>(*presetParam); }
    float getInputGain() const { return *inputGainParam; }
    float getOutputGain() const { return *outputGainParam; }
    float getDecay() const { return *decayParam; }
    float getReverbLevel() const { return *reverbLevelParam; }
    float getMix() const { return *mixParam; }
    bool getBypassed() const { return *bypassParam > 0.5f; }

private:
    //==============================================================================
    // Audio Parameter Tree
    juce::AudioProcessorValueTreeState parameters;

    // Parameter pointers (for fast access)
    std::atomic<float>* presetParam = nullptr;
    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* reverbLevelParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    // DSP
    PsxReverb reverb;

    // State
    float currentSampleRate = 48000.0f;
    int currentPresetIndex = 0;

    // Processing buffers
    std::vector<float> tempBufferL;
    std::vector<float> tempBufferR;
    std::vector<float> wetBufferL;
    std::vector<float> wetBufferR;

    // Mix smoothing
    float smoothedMix = 0.5f;
    float smoothedOutputGain = 1.0f;

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void updateReverbFromParameters();
    void initReverbPreset(int presetIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PSXVerbAudioProcessor)
};
