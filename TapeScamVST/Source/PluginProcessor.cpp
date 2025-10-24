#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TapeScamAudioProcessor::TapeScamAudioProcessor()
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
    parameters(*this, nullptr, juce::Identifier("TapeScam"), createParameterLayout())
{
    // Attach parameter pointers for fast access
    inputParam = parameters.getRawParameterValue("input");
    driveParam = parameters.getRawParameterValue("drive");
    saturationParam = parameters.getRawParameterValue("saturation");
    wowFlutterParam = parameters.getRawParameterValue("wowFlutter");
    noiseParam = parameters.getRawParameterValue("noise");
    toneParam = parameters.getRawParameterValue("tone");
    levelParam = parameters.getRawParameterValue("level");
    tapeAgeParam = parameters.getRawParameterValue("tapeAge");
    tapeSpeedParam = parameters.getRawParameterValue("tapeSpeed");
    compressionParam = parameters.getRawParameterValue("compression");
    bypassParam = parameters.getRawParameterValue("bypass");
}

TapeScamAudioProcessor::~TapeScamAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TapeScamAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Knob 0: Input Level (0-1) - default 0.7 (slight reduction for headroom)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "input", "Input", 0.0f, 1.0f, 0.7f));

    // Knob 1: Drive (0-1) - default 0 (clean)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "drive", "Drive", 0.0f, 1.0f, 0.0f));

    // Knob 2: Saturation (0-1) - default 0 (no saturation)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "saturation", "Saturation", 0.0f, 1.0f, 0.0f));

    // Knob 3: Wow & Flutter (0-1) - default 0 (no wobble)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "wowFlutter", "Wow & Flutter", 0.0f, 1.0f, 0.0f));

    // Knob 4: Noise & Dropouts (0-1) - default 0 (no noise)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "noise", "Noise & Dropouts", 0.0f, 1.0f, 0.0f));

    // Knob 5: Tone (0-1, 0=dark, 1=bright) - default 0.5 (neutral)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "tone", "Tone", 0.0f, 1.0f, 0.5f));

    // Knob 6: Level (0-1) - default 1.0 (unity gain)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "level", "Level", 0.0f, 1.0f, 1.0f));

    // Toggle 1: Tape Age (0=New, 1=Used, 2=Worn)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "tapeAge", "Tape Age",
        juce::StringArray{"New", "Used", "Worn"}, 1));

    // Toggle 2: Tape Speed (0=High, 1=Standard, 2=Lo-Fi)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "tapeSpeed", "Tape Speed",
        juce::StringArray{"High", "Standard", "Lo-Fi"}, 1));

    // Toggle 3: Compression (0=Off, 1=Light, 2=Heavy)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "compression", "Compression",
        juce::StringArray{"Off", "Light", "Heavy"}, 0));

    // Bypass switch
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "bypass", "Bypass", false));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String TapeScamAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TapeScamAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TapeScamAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TapeScamAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TapeScamAudioProcessor::getTailLengthSeconds() const
{
    return 0.05; // Small tail for wow/flutter delay
}

int TapeScamAudioProcessor::getNumPrograms()
{
    return 1;
}

int TapeScamAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TapeScamAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String TapeScamAudioProcessor::getProgramName (int index)
{
    return {};
}

void TapeScamAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void TapeScamAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = static_cast<float>(sampleRate);

    // Initialize all DSP modules with sample rate (matching firmware)
    gainStage.Init(currentSampleRate);
    tapeSat.Init(currentSampleRate);
    tapeWobble.Init(currentSampleRate);
    tapeNoise.Init(currentSampleRate, 2); // 2 channels
    tapeTone.Init(currentSampleRate, 2);
    lofiComp.Init(currentSampleRate);

    // Allocate temporary processing buffers
    tempBufferStorage.resize(samplesPerBlock * 2);
    tempBuffers.resize(2);
    tempBuffers[0] = &tempBufferStorage[0];
    tempBuffers[1] = &tempBufferStorage[samplesPerBlock];

    // Update modules with initial parameter values
    updateModulesFromParameters();

    // Force parameter smoothing to complete immediately on initialization
    // This ensures modules are ready to process from the first block
    for (int i = 0; i < 200; i++)
    {
        tapeSat.UpdateControls();
        tapeWobble.UpdateControls();
        tapeNoise.UpdateControls();
        tapeTone.UpdateControls();
    }
}

void TapeScamAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TapeScamAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void TapeScamAudioProcessor::updateModulesFromParameters()
{
    // Read all parameters
    float drive = getDrive();
    float saturation = getSaturation();
    float wowFlutter = getWowFlutter();
    float noise = getNoise();
    float tone = getTone();
    float level = getLevel();
    int tapeAge = getTapeAge();
    int tapeSpeed = getTapeSpeed();
    int compression = getCompression();

    // Update Gain Stage
    GainStageModule::Params gainParams{};
    gainParams.driveNorm = drive;
    gainParams.bypass = getBypassed();
    gainStage.SetPendingParams(gainParams);

    // Update Tape Saturation
    tapeSat.SetDrive(saturation);

    // Update Wow & Flutter
    tapeWobble.SetAmount(wowFlutter);

    // Update Noise & Dropouts
    tapeNoise.SetAmount(noise);

    // Update Tone
    tapeTone.SetAmount(tone);

    // Update Lo-Fi Compressor
    lofiComp.SetMode(compression);

    // Update global level
    globalLevel = level;

    // Apply Tape Age mode effects (same as firmware)
    float hissMul = 1.0f;
    float dropoutRateMul = 1.0f;
    float wowDepthMul = 1.0f;
    float ageHeadroomAdj_dB = 0.0f;
    float ageSaturationMul = 1.0f;

    switch(tapeAge)
    {
        case 0: // New
            hissMul = 0.5f;
            dropoutRateMul = 0.5f;
            wowDepthMul = 0.8f;
            ageHeadroomAdj_dB = 0.0f;
            ageSaturationMul = 1.0f;
            break;
        case 1: // Used
            hissMul = 1.0f;
            dropoutRateMul = 1.0f;
            wowDepthMul = 1.0f;
            ageHeadroomAdj_dB = -1.0f;
            ageSaturationMul = 1.0f;
            break;
        case 2: // Worn
            hissMul = 1.5f;
            dropoutRateMul = 1.5f;
            wowDepthMul = 1.3f;
            ageHeadroomAdj_dB = -2.5f;
            ageSaturationMul = 1.2f;
            break;
    }

    // Apply Tape Speed mode effects (same as firmware)
    float speedHeadroomAdj_dB = 0.0f;
    float speedSaturationMul = 1.0f;
    float hfRolloffCutoff = 14000.0f;

    switch(tapeSpeed)
    {
        case 0: // High
            speedHeadroomAdj_dB = 3.0f;
            speedSaturationMul = 0.5f;
            hfRolloffCutoff = 20000.0f;
            break;
        case 1: // Standard
            speedHeadroomAdj_dB = -1.0f;
            speedSaturationMul = 1.0f;
            hfRolloffCutoff = 14000.0f;
            break;
        case 2: // Lo-Fi
            speedHeadroomAdj_dB = -6.0f;
            speedSaturationMul = 2.0f;
            hfRolloffCutoff = 8000.0f;
            break;
    }

    // Apply combined effects to modules
    tapeNoise.SetHissMultiplier(hissMul);
    tapeNoise.SetDropoutRateMultiplier(dropoutRateMul);
    tapeWobble.SetDepthMultiplier(wowDepthMul);
    gainStage.AdjustHeadroom(ageHeadroomAdj_dB + speedHeadroomAdj_dB);
    tapeSat.SetDriveMultiplier(ageSaturationMul * speedSaturationMul);
    tapeSat.SetHFRolloffCutoff(hfRolloffCutoff);
}

void TapeScamAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const size_t numSamples = buffer.getNumSamples();

    // Update modules from parameters
    updateModulesFromParameters();

    // Check bypass first - if bypassed, just return (true bypass)
    if (getBypassed())
    {
        return;  // Pass audio through unprocessed
    }

    // Get pointers to audio channels
    const float* const* inputChannels = buffer.getArrayOfReadPointers();
    float* const* outputChannels = buffer.getArrayOfWritePointers();

    // Apply input level scaling to prevent clipping (copy input to output with gain)
    float inputLevel = getInput();
    for (size_t i = 0; i < numSamples; ++i)
    {
        outputChannels[0][i] = inputChannels[0][i] * inputLevel;
        outputChannels[1][i] = inputChannels[1][i] * inputLevel;
    }

    // Process signal chain (same order as firmware):
    // Cast away const for module processing (modules write to output buffers)
    float** outPtr = const_cast<float**>(outputChannels);

    // Call UpdateControls on all modules to apply parameter changes
    tapeSat.UpdateControls();
    tapeWobble.UpdateControls();
    tapeNoise.UpdateControls();
    tapeTone.UpdateControls();

    // 1. Gain Stage (input conditioning + drive + tone) - process in-place since we already copied with input gain
    gainStage.Process(const_cast<const float**>(outPtr), outPtr, numSamples);

    // 2. Tape Saturation
    tapeSat.Process(outPtr, numSamples);

    // 3. Wow & Flutter (pitch modulation via delay)
    tapeWobble.Process(outPtr, outPtr, numSamples);

    // 4. Hiss & Dropouts
    tapeNoise.Process(outPtr, outPtr, numSamples);

    // 5. Tone shaping
    tapeTone.Process(outPtr, outPtr, numSamples);

    // 6. Lo-Fi Compressor (AGC pumping)
    lofiComp.Process(outPtr[0], outPtr[1], numSamples);

    // 7. Apply global level with smoothing (matching firmware)
    const float kLevelSmooth = 0.01f;
    for (size_t i = 0; i < numSamples; ++i)
    {
        globalLevelSmooth += (globalLevel - globalLevelSmooth) * kLevelSmooth;

        outputChannels[0][i] *= globalLevelSmooth;
        outputChannels[1][i] *= globalLevelSmooth;

        // Soft clipping (final safety limiter)
        outputChannels[0][i] = std::tanh(outputChannels[0][i] * 0.95f);
        outputChannels[1][i] = std::tanh(outputChannels[1][i] * 0.95f);
    }
}

//==============================================================================
bool TapeScamAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* TapeScamAudioProcessor::createEditor()
{
    return new TapeScamAudioProcessorEditor (*this, parameters);
}

//==============================================================================
void TapeScamAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void TapeScamAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new TapeScamAudioProcessor();
}
