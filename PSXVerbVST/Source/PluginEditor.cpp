#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PSXVerbAudioProcessorEditor::PSXVerbAudioProcessorEditor (PSXVerbAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), audioProcessor (p), valueTreeState(vts)
{
    // Set up preset selector
    addAndMakeVisible(presetSelector);
    presetSelector.addItem("Room", 1);
    presetSelector.addItem("Studio Small", 2);
    presetSelector.addItem("Studio Medium", 3);
    presetSelector.addItem("Studio Large", 4);
    presetSelector.addItem("Hall", 5);
    presetSelector.addItem("Space Echo", 6);
    presetSelector.setSelectedId(1);

    addAndMakeVisible(presetLabel);
    presetLabel.setText("PRESET", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centredLeft);
    presetLabel.attachToComponent(&presetSelector, true);
    presetLabel.setFont(juce::Font(14.0f, juce::Font::bold));

    // Set up knobs
    setupKnob(inputGainKnob, inputGainLabel, "INPUT GAIN");
    setupKnob(outputGainKnob, outputGainLabel, "OUTPUT GAIN");
    setupKnob(decayKnob, decayLabel, "DECAY");
    setupKnob(reverbLevelKnob, reverbLevelLabel, "REVERB LEVEL");
    setupKnob(mixKnob, mixLabel, "MIX");

    // Set up bypass button
    addAndMakeVisible(bypassButton);
    bypassButton.setButtonText("BYPASS");
    bypassButton.setClickingTogglesState(true);

    // Create parameter attachments
    presetAttachment.reset(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(
        valueTreeState, "preset", presetSelector));

    inputGainAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "inputGain", inputGainKnob));
    outputGainAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "outputGain", outputGainKnob));
    decayAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "decay", decayKnob));
    reverbLevelAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "reverbLevel", reverbLevelKnob));
    mixAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "mix", mixKnob));

    bypassAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(
        valueTreeState, "bypass", bypassButton));

    // Set plugin window size - larger for iOS touch targets
    #if JUCE_IOS
    setSize (800, 900);   // iOS: Taller layout for touch
    #else
    setSize (600, 400);   // Desktop: Compact layout
    #endif
}

PSXVerbAudioProcessorEditor::~PSXVerbAudioProcessorEditor()
{
}

//==============================================================================
void PSXVerbAudioProcessorEditor::setupKnob(juce::Slider& knob, juce::Label& label,
                                             const juce::String& labelText)
{
    addAndMakeVisible(knob);
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    knob.setColour(juce::Slider::thumbColourId, juce::Colours::cyan);
    knob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::cyan);

    addAndMakeVisible(label);
    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.attachToComponent(&knob, false);
    label.setFont(juce::Font(12.0f, juce::Font::bold));
}

//==============================================================================
void PSXVerbAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Dark background inspired by PlayStation aesthetic
    g.fillAll(juce::Colour(0xff0a0a1a));

    // Title area (top strip)
    g.setColour(juce::Colour(0xff1a1a2e));
    g.fillRect(0, 0, getWidth(), 50);

    // Title text
    g.setColour(juce::Colours::cyan);
    g.setFont(juce::Font(24.0f, juce::Font::bold));
    g.drawText("PSXVerb", 0, 10, getWidth(), 30, juce::Justification::centred);

    // Subtitle
    g.setFont(juce::Font(10.0f));
    g.drawText("Authentic PlayStation SPU Reverb", 0, 35, getWidth(), 15, juce::Justification::centred);

    // Divider line
    g.setColour(juce::Colour(0xff2a2a3e));
    g.drawLine(10, 160, getWidth() - 10, 160, 2.0f);
}

void PSXVerbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    #if JUCE_IOS
    // iOS Layout - vertical, larger touch targets

    // Title area
    area.removeFromTop(80);

    // Preset selector - large touch area
    auto presetArea = area.removeFromTop(80);
    const int labelWidth = 120;
    presetSelector.setBounds(labelWidth, presetArea.getY() + 10,
                            getWidth() - labelWidth - 20, 60);

    // Knobs in grid layout
    const int padding = 20;
    const int knobSize = 140;

    // Row 1: Input Gain, Output Gain, Decay
    auto row1 = area.removeFromTop(knobSize + 60);
    int knobSpacing = (getWidth() - padding * 2) / 3;
    inputGainKnob.setBounds(padding + knobSpacing * 0, row1.getY(), knobSize, knobSize);
    outputGainKnob.setBounds(padding + knobSpacing * 1, row1.getY(), knobSize, knobSize);
    decayKnob.setBounds(padding + knobSpacing * 2, row1.getY(), knobSize, knobSize);

    // Row 2: Reverb Level, Mix (centered)
    auto row2 = area.removeFromTop(knobSize + 60);
    knobSpacing = (getWidth() - padding * 2) / 2;
    reverbLevelKnob.setBounds(padding + knobSpacing * 0, row2.getY(), knobSize, knobSize);
    mixKnob.setBounds(padding + knobSpacing * 1, row2.getY(), knobSize, knobSize);

    // Bypass button at bottom - large touch target
    auto buttonArea = area.removeFromBottom(100);
    bypassButton.setBounds(getWidth() / 2 - 100, buttonArea.getY() + 20, 200, 60);

    #else
    // Desktop Layout - compact horizontal

    // Title area
    area.removeFromTop(60);

    // Preset selector at top
    auto presetArea = area.removeFromTop(40);
    const int labelWidth = 80;
    presetSelector.setBounds(labelWidth + 30, presetArea.getY() + 5,
                            getWidth() - labelWidth - 60, 30);

    // Knobs row
    auto knobArea = area.removeFromTop(140);
    const int knobWidth = getWidth() / 5;

    inputGainKnob.setBounds(knobWidth * 0, knobArea.getY() + 20, knobWidth - 10, 100);
    outputGainKnob.setBounds(knobWidth * 1, knobArea.getY() + 20, knobWidth - 10, 100);
    decayKnob.setBounds(knobWidth * 2, knobArea.getY() + 20, knobWidth - 10, 100);
    reverbLevelKnob.setBounds(knobWidth * 3, knobArea.getY() + 20, knobWidth - 10, 100);
    mixKnob.setBounds(knobWidth * 4, knobArea.getY() + 20, knobWidth - 10, 100);

    // Bypass button at bottom
    auto buttonArea = area.removeFromBottom(70);
    bypassButton.setBounds(getWidth() / 2 - 60, buttonArea.getY() + 20, 120, 30);

    #endif
}
