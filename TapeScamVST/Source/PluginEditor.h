#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <foleys_gui_magic/foleys_gui_magic.h>
#include <memory>

#include "PluginProcessor.h"
#include "ui/GuiFactories.h"

class TapeScamAudioProcessorEditor  : public foleys::MagicPluginEditor
{
public:
    TapeScamAudioProcessorEditor (TapeScamAudioProcessor&, std::unique_ptr<foleys::MagicGUIBuilder> builder);
    ~TapeScamAudioProcessorEditor() override = default;

    void resized() override;

private:
    TapeScamAudioProcessor& audioProcessor;

    void initialiseGUI();
    void applyDesignTransform();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeScamAudioProcessorEditor)
};
