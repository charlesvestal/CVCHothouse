#include "MagicThreeWaySwitch.h"
#include "ResourceHelpers.h"

namespace tape
{

const juce::Identifier MagicThreeWaySwitch::pTrackImage { "track-image" };
const juce::Identifier MagicThreeWaySwitch::pHandleImage{ "handle-image" };
const juce::Identifier MagicThreeWaySwitch::pPadding    { "track-padding" };
const juce::Identifier MagicThreeWaySwitch::pHandleWidth  { "handle-width" };
const juce::Identifier MagicThreeWaySwitch::pHandleHeight { "handle-height" };
const juce::Identifier MagicThreeWaySwitch::pHandleOffsetY{ "handle-offset-y" };

MagicThreeWaySwitch::MagicThreeWaySwitch (foleys::MagicGUIBuilder& builder, const juce::ValueTree& node)
    : foleys::GuiItem (builder, node)
{
    switchComponent = std::make_unique<SwitchComponent>();
    addAndMakeVisible (switchComponent.get());
}

void MagicThreeWaySwitch::update()
{
    attachment.reset();

    const auto trackVar = getProperty (pTrackImage);
    const auto handleVar = getProperty (pHandleImage);
    const auto paddingVar = getProperty (pPadding);
    const auto handleWidthVar = getProperty (pHandleWidth);
    const auto handleHeightVar = getProperty (pHandleHeight);
    const auto handleOffsetVar = getProperty (pHandleOffsetY);

    switchComponent->setImages (loadBinaryImage (trackVar.isVoid() ? juce::String() : trackVar.toString()),
                                loadBinaryImage (handleVar.isVoid() ? juce::String() : handleVar.toString()));
    switchComponent->setPadding (paddingVar.isVoid() ? 12.0f : static_cast<float> (double (paddingVar)));
    switchComponent->setHandleGeometry (handleWidthVar.isVoid() ? 0.0f : static_cast<float> (double (handleWidthVar)),
                                        handleHeightVar.isVoid() ? 0.0f : static_cast<float> (double (handleHeightVar)),
                                        handleOffsetVar.isVoid() ? 0.0f : static_cast<float> (double (handleOffsetVar)));

    if (auto parameterID = getControlledParameterID ({ }); parameterID.isNotEmpty())
        attachment = getMagicState().createAttachment (parameterID, switchComponent->getSlider());
}

std::vector<foleys::SettableProperty> MagicThreeWaySwitch::getSettableProperties() const
{
    std::vector<foleys::SettableProperty> props;
    props.push_back ({ configNode, foleys::IDs::parameter, foleys::SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() });
    props.push_back ({ configNode, pTrackImage, foleys::SettableProperty::Choice, {}, magicBuilder.createChoicesMenuLambda (getBinaryAssetNames()) });
    props.push_back ({ configNode, pHandleImage, foleys::SettableProperty::Choice, {}, magicBuilder.createChoicesMenuLambda (getBinaryAssetNames()) });
    props.push_back ({ configNode, pPadding, foleys::SettableProperty::Number, 0.0f, {} });
    props.push_back ({ configNode, pHandleWidth, foleys::SettableProperty::Number, 0.0f, {} });
    props.push_back ({ configNode, pHandleHeight, foleys::SettableProperty::Number, 0.0f, {} });
    props.push_back ({ configNode, pHandleOffsetY, foleys::SettableProperty::Number, 0.0f, {} });
    return props;
}

juce::String MagicThreeWaySwitch::getControlledParameterID (juce::Point<int>)
{
    return configNode.getProperty (foleys::IDs::parameter, juce::String()).toString();
}

juce::Component* MagicThreeWaySwitch::getWrappedComponent()
{
    return switchComponent.get();
}

//==============================================================================
MagicThreeWaySwitch::SwitchComponent::HiddenSlider::HiddenSlider()
{
    setSliderStyle (juce::Slider::LinearHorizontal);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRange (0.0, 2.0, 1.0);
    setPopupDisplayEnabled (false, false, nullptr);
}

MagicThreeWaySwitch::SwitchComponent::SwitchComponent()
{
    addAndMakeVisible (slider);
    slider.onValueChange = [this]
    {
        repaint();
    };
}

void MagicThreeWaySwitch::SwitchComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto trackBounds = bounds.reduced (horizontalPadding, 0.0f);

    if (trackImage.isValid())
    {
        g.drawImageWithin (trackImage, static_cast<int> (trackBounds.getX()), static_cast<int> (trackBounds.getY()),
                           static_cast<int> (trackBounds.getWidth()), static_cast<int> (trackBounds.getHeight()),
                           juce::RectanglePlacement::stretchToFit, false);
    }
    else
    {
        g.setColour (juce::Colours::darkgrey.withAlpha (0.9f));
        g.fillRoundedRectangle (trackBounds, trackBounds.getHeight() * 0.25f);
    }

    const float currentValue = static_cast<float> (slider.getValue());
    const float minValue = static_cast<float> (slider.getMinimum());
    const float maxValue = static_cast<float> (slider.getMaximum());
    float handleWidth = handleWidthOverride;
    float handleHeight = handleHeightOverride;
    if (handleImage.isValid())
    {
        if (handleWidth <= 0.0f || handleHeight <= 0.0f)
        {
            handleWidth = static_cast<float> (handleImage.getWidth());
            handleHeight = static_cast<float> (handleImage.getHeight());
        }
    }
    if (handleWidth <= 0.0f)
        handleWidth = bounds.getHeight() * 0.35f;
    if (handleHeight <= 0.0f)
        handleHeight = bounds.getHeight() * 0.35f;

    const float minX = trackBounds.getX();
    const float maxX = trackBounds.getRight() - handleWidth;
    const float handleX = juce::jmap (currentValue, minValue, maxValue, minX, maxX);
    const float handleY = trackBounds.getCentreY() - handleHeight * 0.5f + handleOffsetY;
    const juce::Rectangle<float> handleBounds { handleX, handleY, handleWidth, handleHeight };

    if (handleImage.isValid())
    {
        g.drawImageWithin (handleImage, static_cast<int> (handleBounds.getX()), static_cast<int> (handleBounds.getY()),
                           static_cast<int> (handleBounds.getWidth()), static_cast<int> (handleBounds.getHeight()),
                           juce::RectanglePlacement::stretchToFit, true);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRoundedRectangle (handleBounds, handleBounds.getHeight() * 0.25f);
    }
}

void MagicThreeWaySwitch::SwitchComponent::resized()
{
    slider.setBounds (getLocalBounds());
}

void MagicThreeWaySwitch::SwitchComponent::setImages (juce::Image track, juce::Image handle)
{
    trackImage = track;
    handleImage = handle;
    repaint();
}

void MagicThreeWaySwitch::SwitchComponent::setPadding (float padding)
{
    horizontalPadding = padding;
    repaint();
}

void MagicThreeWaySwitch::SwitchComponent::setHandleGeometry (float width, float height, float offsetY)
{
    handleWidthOverride = width;
    handleHeightOverride = height;
    handleOffsetY = offsetY;
    repaint();
}

} // namespace tape
