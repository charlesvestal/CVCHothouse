#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
class PSXVerbAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    PSXVerbAudioProcessorEditor (PSXVerbAudioProcessor&, juce::AudioProcessorValueTreeState&);
    ~PSXVerbAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PSXVerbAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& valueTreeState;

    // Preset selector
    juce::ComboBox presetSelector;
    juce::Label presetLabel;

    // Knob components (5 rotary sliders)
    juce::Slider inputGainKnob;
    juce::Slider outputGainKnob;
    juce::Slider decayKnob;
    juce::Slider reverbLevelKnob;
    juce::Slider mixKnob;

    // Labels for knobs
    juce::Label inputGainLabel;
    juce::Label outputGainLabel;
    juce::Label decayLabel;
    juce::Label reverbLevelLabel;
    juce::Label mixLabel;

    // Bypass button
    juce::TextButton bypassButton;

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbLevelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // Helper methods
    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& labelText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PSXVerbAudioProcessorEditor)
};
