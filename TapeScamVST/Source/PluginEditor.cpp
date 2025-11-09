#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kDefaultWidth  = 1680;
constexpr int kDefaultHeight = 1220;
}

TapeScamAudioProcessorEditor::TapeScamAudioProcessorEditor (TapeScamAudioProcessor& p)
    : foleys::MagicPluginEditor (p.getMagicState()),
      audioProcessor (p)
{
    auto& builder = getGUIBuilder();
    builder.registerJUCEFactories();
    builder.registerJUCELookAndFeels();
    tape::registerTapeScamFactories (builder);

    initialiseGUI();
}

void TapeScamAudioProcessorEditor::initialiseGUI()
{
    auto& magicState = audioProcessor.getMagicState();

   #if JUCE_DEBUG
    const juce::File editorFile (__FILE__);
    const auto resourcesDir = editorFile.getParentDirectory().getParentDirectory()
                                .getParentDirectory().getChildFile ("Resources/Magic");
    const auto layoutFile = resourcesDir.getChildFile ("TapeScam.magic");

    if (layoutFile.existsAsFile())
        magicState.setGuiValueTree (layoutFile);
   #endif

    constexpr int halfWidth  = kDefaultWidth / 2;
    constexpr int halfHeight = kDefaultHeight / 2;

    setSize (halfWidth, halfHeight);
    setResizeLimits (halfWidth, halfHeight, kDefaultWidth, kDefaultHeight);
    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (static_cast<double> (kDefaultWidth) / static_cast<double> (kDefaultHeight));
    magicState.setLastEditorSize (halfWidth, halfHeight);
    setConfigTree (magicState.getGuiTree());
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<TapeScamAudioProcessorEditor>(this)]
    {
        if (auto* self = safe.getComponent())
            self->applyDesignTransform();
    });
}

void TapeScamAudioProcessorEditor::resized()
{
    foleys::MagicPluginEditor::resized();
    applyDesignTransform();
}

void TapeScamAudioProcessorEditor::applyDesignTransform()
{
    auto& builder = getGUIBuilder();
    if (auto* rootItem = builder.findGuiItemWithId ("root"))
    {
        const auto available = getLocalBounds().toFloat();
        const float scale = std::min (available.getWidth() / static_cast<float> (kDefaultWidth),
                                      available.getHeight() / static_cast<float> (kDefaultHeight));

        const float scaledWidth = static_cast<float> (kDefaultWidth) * scale;
        const float scaledHeight = static_cast<float> (kDefaultHeight) * scale;
        const float offsetX = available.getX() + (available.getWidth() - scaledWidth) * 0.5f;
        const float offsetY = available.getY() + (available.getHeight() - scaledHeight) * 0.5f;

        const juce::Rectangle<int> designBounds { 0, 0, kDefaultWidth, kDefaultHeight };
        builder.updateLayout (designBounds);

        rootItem->setBounds (designBounds);
        rootItem->setTransform (juce::AffineTransform::scale (scale)
                                                      .translated (offsetX, offsetY));
    }
}
