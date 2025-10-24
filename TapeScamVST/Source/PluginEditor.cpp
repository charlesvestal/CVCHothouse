#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TapeScamAudioProcessorEditor::TapeScamAudioProcessorEditor (TapeScamAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), audioProcessor (p), valueTreeState(vts)
{
    // Set up knobs
    setupKnob(inputKnob, inputLabel, "INPUT");
    setupKnob(driveKnob, driveLabel, "DRIVE");
    setupKnob(saturationKnob, saturationLabel, "SATURATION");
    setupKnob(wowFlutterKnob, wowFlutterLabel, "WOW/FLUTTER");
    setupKnob(noiseKnob, noiseLabel, "NOISE");
    setupKnob(toneKnob, toneLabel, "TONE");
    setupKnob(levelKnob, levelLabel, "LEVEL");

    // Set up toggles
    setupToggle(tapeAgeToggle, tapeAgeLabel, "TAPE AGE",
               juce::StringArray{"New", "Used", "Worn"});
    setupToggle(tapeSpeedToggle, tapeSpeedLabel, "TAPE SPEED",
               juce::StringArray{"High", "Standard", "Lo-Fi"});
    setupToggle(compressionToggle, compressionLabel, "COMPRESSION",
               juce::StringArray{"Off", "Light", "Heavy"});

    // Set up bypass button
    addAndMakeVisible(bypassButton);
    bypassButton.setButtonText("BYPASS");
    bypassButton.setClickingTogglesState(true);

    // Create parameter attachments
    inputAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "input", inputKnob));
    driveAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "drive", driveKnob));
    saturationAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "saturation", saturationKnob));
    wowFlutterAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "wowFlutter", wowFlutterKnob));
    noiseAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "noise", noiseKnob));
    toneAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "tone", toneKnob));
    levelAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        valueTreeState, "level", levelKnob));

    tapeAgeAttachment.reset(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(
        valueTreeState, "tapeAge", tapeAgeToggle));
    tapeSpeedAttachment.reset(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(
        valueTreeState, "tapeSpeed", tapeSpeedToggle));
    compressionAttachment.reset(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(
        valueTreeState, "compression", compressionToggle));

    bypassAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(
        valueTreeState, "bypass", bypassButton));

    // Set plugin window size - larger for iOS touch targets
    #if JUCE_IOS
    setSize (800, 1000);  // iOS: Taller layout for touch
    #else
    setSize (600, 400);   // Desktop: Compact layout
    #endif
}

TapeScamAudioProcessorEditor::~TapeScamAudioProcessorEditor()
{
}

//==============================================================================
void TapeScamAudioProcessorEditor::setupKnob(juce::Slider& knob, juce::Label& label,
                                              const juce::String& labelText)
{
    addAndMakeVisible(knob);
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    knob.setColour(juce::Slider::thumbColourId, juce::Colours::darkred);
    knob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::darkred);

    addAndMakeVisible(label);
    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.attachToComponent(&knob, false);
    label.setFont(juce::Font(12.0f, juce::Font::bold));
}

void TapeScamAudioProcessorEditor::setupToggle(juce::ComboBox& toggle, juce::Label& label,
                                                const juce::String& labelText,
                                                const juce::StringArray& items)
{
    addAndMakeVisible(toggle);
    for (int i = 0; i < items.size(); ++i)
        toggle.addItem(items[i], i + 1);
    toggle.setSelectedId(1); // Default to first item

    addAndMakeVisible(label);
    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.attachToComponent(&toggle, true);
    label.setFont(juce::Font(12.0f, juce::Font::bold));
}

//==============================================================================
void TapeScamAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Pedal-style background
    g.fillAll(juce::Colour(0xff1a1a1a));

    // Title area (top strip)
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(0, 0, getWidth(), 50);

    // Title text
    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::Font(24.0f, juce::Font::bold));
    g.drawText("TAPESCAM", 0, 10, getWidth(), 30, juce::Justification::centred);

    // Subtitle
    g.setFont(juce::Font(10.0f));
    g.drawText("Lo-Fi Tape Emulation", 0, 35, getWidth(), 15, juce::Justification::centred);

    // Divider lines for sections
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawLine(10, 190, getWidth() - 10, 190, 2.0f);
}

void TapeScamAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    #if JUCE_IOS
    // iOS Layout - vertical, larger touch targets

    // Title area
    area.removeFromTop(80);

    // Knobs in grid layout (2 rows for better touch)
    const int padding = 20;
    const int knobSize = 140;  // Large enough for touch

    // Row 1: Input, Drive, Saturation, Wow/Flutter
    auto row1 = area.removeFromTop(knobSize + 60);
    int knobSpacing = (getWidth() - padding * 2) / 4;
    inputKnob.setBounds(padding + knobSpacing * 0, row1.getY(), knobSize, knobSize);
    driveKnob.setBounds(padding + knobSpacing * 1, row1.getY(), knobSize, knobSize);
    saturationKnob.setBounds(padding + knobSpacing * 2, row1.getY(), knobSize, knobSize);
    wowFlutterKnob.setBounds(padding + knobSpacing * 3, row1.getY(), knobSize, knobSize);

    // Row 2: Noise, Tone, Level
    auto row2 = area.removeFromTop(knobSize + 60);
    knobSpacing = (getWidth() - padding * 2) / 3;
    noiseKnob.setBounds(padding + knobSpacing * 0, row2.getY(), knobSize, knobSize);
    toneKnob.setBounds(padding + knobSpacing * 1, row2.getY(), knobSize, knobSize);
    levelKnob.setBounds(padding + knobSpacing * 2, row2.getY(), knobSize, knobSize);

    // Toggles section - larger touch areas
    area.removeFromTop(30);
    const int toggleHeight = 60;
    const int labelWidth = 150;

    tapeAgeToggle.setBounds(labelWidth, area.getY(), getWidth() - labelWidth - 20, toggleHeight);
    area.removeFromTop(toggleHeight + 10);

    tapeSpeedToggle.setBounds(labelWidth, area.getY(), getWidth() - labelWidth - 20, toggleHeight);
    area.removeFromTop(toggleHeight + 10);

    compressionToggle.setBounds(labelWidth, area.getY(), getWidth() - labelWidth - 20, toggleHeight);
    area.removeFromTop(toggleHeight + 10);

    // Bypass button at bottom - large touch target
    auto buttonArea = area.removeFromBottom(100);
    bypassButton.setBounds(getWidth() / 2 - 100, buttonArea.getY() + 20, 200, 60);

    #else
    // Desktop Layout - compact horizontal

    // Title area
    area.removeFromTop(60);

    // Top row: 7 knobs
    auto knobArea = area.removeFromTop(130);
    const int knobWidth = getWidth() / 7;

    inputKnob.setBounds(knobWidth * 0, knobArea.getY() + 20, knobWidth - 10, 100);
    driveKnob.setBounds(knobWidth * 1, knobArea.getY() + 20, knobWidth - 10, 100);
    saturationKnob.setBounds(knobWidth * 2, knobArea.getY() + 20, knobWidth - 10, 100);
    wowFlutterKnob.setBounds(knobWidth * 3, knobArea.getY() + 20, knobWidth - 10, 100);
    noiseKnob.setBounds(knobWidth * 4, knobArea.getY() + 20, knobWidth - 10, 100);
    toneKnob.setBounds(knobWidth * 5, knobArea.getY() + 20, knobWidth - 10, 100);
    levelKnob.setBounds(knobWidth * 6, knobArea.getY() + 20, knobWidth - 10, 100);

    // Divider space
    area.removeFromTop(20);

    // Bottom section: Toggles
    auto toggleArea = area.removeFromTop(120);
    const int toggleHeight = 30;
    const int toggleSpacing = 15;
    const int labelWidth = 120;

    tapeAgeToggle.setBounds(labelWidth + 30, toggleArea.getY() + toggleSpacing,
                           getWidth() - labelWidth - 60, toggleHeight);
    tapeSpeedToggle.setBounds(labelWidth + 30, toggleArea.getY() + toggleSpacing + 40,
                             getWidth() - labelWidth - 60, toggleHeight);
    compressionToggle.setBounds(labelWidth + 30, toggleArea.getY() + toggleSpacing + 80,
                               getWidth() - labelWidth - 60, toggleHeight);

    // Bypass button at bottom
    auto buttonArea = area.removeFromBottom(50);
    bypassButton.setBounds(getWidth() / 2 - 60, buttonArea.getY() + 10, 120, 30);

    #endif
}
