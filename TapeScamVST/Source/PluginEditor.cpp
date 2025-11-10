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

   #if JUCE_IOS
    const bool iosStandalone = isIOSStandaloneHost();
   #else
    constexpr bool iosStandalone = false;
   #endif

    const int initialWidth = iosStandalone ? kDefaultWidth : halfWidth;
    const int initialHeight = iosStandalone ? kDefaultHeight : halfHeight;

    setSize (initialWidth, initialHeight);

   #if JUCE_IOS
   if (iosStandalone)
   {
        setResizeLimits (initialWidth, initialHeight, 8192, 8192);
        setResizable (true, true);
    }
    else
    {
        setResizeLimits (1, 1, kDefaultWidth, kDefaultHeight);
        setResizable (true, true);
        if (auto* constrainer = getConstrainer())
            constrainer->setFixedAspectRatio (static_cast<double> (kDefaultWidth) / static_cast<double> (kDefaultHeight));
    }
   #else
    setResizeLimits (halfWidth, halfHeight, kDefaultWidth, kDefaultHeight);
    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (static_cast<double> (kDefaultWidth) / static_cast<double> (kDefaultHeight));
   #endif

    magicState.setLastEditorSize (initialWidth, initialHeight);
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
#if JUCE_IOS
    if (isIOSStandaloneHost())
    {
        if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        {
            const auto displayArea = display->userArea;
            if (displayArea.getWidth() > 0 && displayArea.getHeight() > 0
                && (getWidth() != displayArea.getWidth() || getHeight() != displayArea.getHeight()))
            {
                setSize (displayArea.getWidth(), displayArea.getHeight());
                return; // resized() will re-run layout with updated bounds
            }
        }
    }
   #endif

    auto& builder = getGUIBuilder();
    if (auto* rootItem = builder.findGuiItemWithId ("root"))
    {
        const auto available = getLocalBounds().toFloat();
        const auto widthRatio = available.getWidth() / static_cast<float> (kDefaultWidth);
        const auto heightRatio = available.getHeight() / static_cast<float> (kDefaultHeight);

        const float scale = std::max (0.0f, std::min (widthRatio, heightRatio));

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

bool TapeScamAudioProcessorEditor::isIOSStandaloneHost() const noexcept
{
#if JUCE_IOS
    return audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
#else
    return false;
#endif
}
