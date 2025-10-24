#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PSXVerbAudioProcessor::PSXVerbAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
    parameters(*this, nullptr, juce::Identifier("PSXVerb"), createParameterLayout())
{
    // Attach parameter pointers for fast access
    presetParam = parameters.getRawParameterValue("preset");
    inputGainParam = parameters.getRawParameterValue("inputGain");
    outputGainParam = parameters.getRawParameterValue("outputGain");
    decayParam = parameters.getRawParameterValue("decay");
    reverbLevelParam = parameters.getRawParameterValue("reverbLevel");
    mixParam = parameters.getRawParameterValue("mix");
    bypassParam = parameters.getRawParameterValue("bypass");
}

PSXVerbAudioProcessor::~PSXVerbAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PSXVerbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Preset selector (0-5 for 6 presets)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "preset", "Preset",
        juce::StringArray{"Room", "Studio Small", "Studio Medium",
                         "Studio Large", "Hall", "Space Echo"}, 0));

    // Input Gain (0-1, 0.5 = unity)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "inputGain", "Input Gain", 0.0f, 1.0f, 0.5f));

    // Output Gain (0-1, 0.5 = unity)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "outputGain", "Output Gain", 0.0f, 1.0f, 0.5f));

    // Decay (0-1, 1.0 = authentic PSX)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "decay", "Decay", 0.0f, 1.0f, 1.0f));

    // Reverb Level (0-1, 0.5 = unity)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "reverbLevel", "Reverb Level", 0.0f, 1.0f, 0.5f));

    // Mix (0-1, 0 = dry, 1 = wet)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix", 0.0f, 1.0f, 0.5f));

    // Bypass
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "bypass", "Bypass", false));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String PSXVerbAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PSXVerbAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PSXVerbAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PSXVerbAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PSXVerbAudioProcessor::getTailLengthSeconds() const
{
    return 3.0; // PSX reverb has long decay
}

int PSXVerbAudioProcessor::getNumPrograms()
{
    return 1;
}

int PSXVerbAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PSXVerbAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String PSXVerbAudioProcessor::getProgramName (int index)
{
    return {};
}

void PSXVerbAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void PSXVerbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = static_cast<float>(sampleRate);

    // Initialize reverb with first preset
    initReverbPreset(0);

    // Allocate processing buffers
    tempBufferL.resize(samplesPerBlock);
    tempBufferR.resize(samplesPerBlock);
    wetBufferL.resize(samplesPerBlock);
    wetBufferR.resize(samplesPerBlock);

    // Initialize smoothing
    smoothedMix = getMix();
    smoothedOutputGain = getOutputGain() * 2.0f;
}

void PSXVerbAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PSXVerbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // Only support stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #endif

    return true;
  #endif
}
#endif

void PSXVerbAudioProcessor::initReverbPreset(int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= PsxPresets::kNumPresets)
        presetIndex = 0;

    currentPresetIndex = presetIndex;
    const PsxPreset* preset = PsxPresets::kAllPresets[presetIndex];
    reverb.Init(currentSampleRate, *preset);
}

void PSXVerbAudioProcessor::updateReverbFromParameters()
{
    // Check if preset changed
    int preset = getPreset();
    if (preset != currentPresetIndex)
    {
        initReverbPreset(preset);
    }

    // Update reverb parameters
    reverb.SetInputGain(getInputGain());
    reverb.SetDecayTime(getDecay());
    reverb.SetReverbLevel(getReverbLevel());
}

void PSXVerbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Ensure we have stereo
    if (totalNumInputChannels < 2 || totalNumOutputChannels < 2)
        return;

    const int numSamples = buffer.getNumSamples();

    // Update reverb parameters
    updateReverbFromParameters();

    // Check bypass
    if (getBypassed())
    {
        return;  // Pass audio through unprocessed
    }

    // Get pointers to audio channels
    const float* const inputL = buffer.getReadPointer(0);
    const float* const inputR = buffer.getReadPointer(1);
    float* const outputL = buffer.getWritePointer(0);
    float* const outputR = buffer.getWritePointer(1);

    // Process reverb
    reverb.ProcessBlock(inputL, inputR, wetBufferL.data(), wetBufferR.data(), numSamples);

    // Mix dry + wet with smoothing
    float mix = getMix();
    float outputGain = getOutputGain() * 2.0f;  // 0-2x range
    const float kSmooth = 0.05f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Smooth parameters
        smoothedMix += kSmooth * (mix - smoothedMix);
        smoothedOutputGain += kSmooth * (outputGain - smoothedOutputGain);

        // Mix and apply output gain
        outputL[i] = (inputL[i] * (1.0f - smoothedMix) + wetBufferL[i] * smoothedMix) * smoothedOutputGain;
        outputR[i] = (inputR[i] * (1.0f - smoothedMix) + wetBufferR[i] * smoothedMix) * smoothedOutputGain;
    }
}

//==============================================================================
bool PSXVerbAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PSXVerbAudioProcessor::createEditor()
{
    return new PSXVerbAudioProcessorEditor (*this, parameters);
}

//==============================================================================
void PSXVerbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PSXVerbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PSXVerbAudioProcessor();
}
