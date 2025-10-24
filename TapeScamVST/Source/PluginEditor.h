#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
class TapeScamAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    TapeScamAudioProcessorEditor (TapeScamAudioProcessor&, juce::AudioProcessorValueTreeState&);
    ~TapeScamAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    TapeScamAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& valueTreeState;

    // Knob components (7 rotary sliders)
    juce::Slider inputKnob;
    juce::Slider driveKnob;
    juce::Slider saturationKnob;
    juce::Slider wowFlutterKnob;
    juce::Slider noiseKnob;
    juce::Slider toneKnob;
    juce::Slider levelKnob;

    // Labels for knobs
    juce::Label inputLabel;
    juce::Label driveLabel;
    juce::Label saturationLabel;
    juce::Label wowFlutterLabel;
    juce::Label noiseLabel;
    juce::Label toneLabel;
    juce::Label levelLabel;

    // Toggle switches (3 combo boxes)
    juce::ComboBox tapeAgeToggle;
    juce::ComboBox tapeSpeedToggle;
    juce::ComboBox compressionToggle;

    // Labels for toggles
    juce::Label tapeAgeLabel;
    juce::Label tapeSpeedLabel;
    juce::Label compressionLabel;

    // Bypass button
    juce::TextButton bypassButton;

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wowFlutterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tapeAgeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tapeSpeedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> compressionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // Helper methods
    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& labelText);
    void setupToggle(juce::ComboBox& toggle, juce::Label& label, const juce::String& labelText,
                    const juce::StringArray& items);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeScamAudioProcessorEditor)
};
