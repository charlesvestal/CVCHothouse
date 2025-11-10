#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kDefaultWidth  = 1204;
constexpr int kDefaultHeight = 1157;
}

TapeScamAudioProcessorEditor::TapeScamAudioProcessorEditor (TapeScamAudioProcessor& p,
                                                            std::unique_ptr<foleys::MagicGUIBuilder> builder)
    : foleys::MagicPluginEditor (p.getMagicState(), std::move (builder)),
      audioProcessor (p)
{
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

   #if JUCE_IOS
    // Allow the standalone container to use the full device bounds so we can letterbox via applyDesignTransform
    setResizeLimits (halfWidth, halfHeight, 8192, 8192);
    setResizable (true, true);
   #else
    setResizeLimits (halfWidth, halfHeight, kDefaultWidth, kDefaultHeight);
    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (static_cast<double> (kDefaultWidth) / static_cast<double> (kDefaultHeight));
   #endif
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
        const auto widthRatio = available.getWidth() / static_cast<float> (kDefaultWidth);
        const auto heightRatio = available.getHeight() / static_cast<float> (kDefaultHeight);

        float scale = std::min (widthRatio, heightRatio);

       #if JUCE_IOS
        if (auto* peer = getPeer())
        {
            const auto peerBounds = peer->getBounds().toFloat();
            const auto peerHeightScale = available.getHeight() > 0.0f
                                             ? peerBounds.getHeight() / available.getHeight()
                                             : 1.0f;

            if (peerHeightScale > 0.0f)
            {
                const float desiredScale = peerBounds.getHeight() / static_cast<float> (kDefaultHeight);
                scale = desiredScale / peerHeightScale;
            }
            else
            {
                scale = juce::jmax (heightRatio, 0.0f);
            }
        }
        else
        {
            scale = juce::jmax (heightRatio, 0.0f);
        }

        scale = juce::jmax (scale, 0.0f);
       #endif

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
