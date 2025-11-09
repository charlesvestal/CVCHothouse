#include "MagicKnobWithOverlay.h"
#include "ResourceHelpers.h"

namespace tape
{

const juce::Identifier MagicKnobWithOverlay::pBaseImage   { "base-image" };
const juce::Identifier MagicKnobWithOverlay::pOverlayImage{ "overlay-image" };
const juce::Identifier MagicKnobWithOverlay::pMinAngle    { "min-angle" };
const juce::Identifier MagicKnobWithOverlay::pMaxAngle    { "max-angle" };

MagicKnobWithOverlay::MagicKnobWithOverlay (foleys::MagicGUIBuilder& builder, const juce::ValueTree& node)
    : foleys::GuiItem (builder, node)
{
    knobComponent = std::make_unique<KnobComponent>();
    addAndMakeVisible (knobComponent.get());
}

void MagicKnobWithOverlay::update()
{
    attachment.reset();

    const auto baseVar = getProperty (pBaseImage);
    const auto overlayVar = getProperty (pOverlayImage);

    const auto baseName = baseVar.isVoid() ? juce::String() : baseVar.toString();
    const auto overlayName = overlayVar.isVoid() ? juce::String() : overlayVar.toString();

    knobComponent->setImages (loadBinaryImage (baseName),
                              loadBinaryImage (overlayName));

    const auto minVar = getProperty (pMinAngle);
    const auto maxVar = getProperty (pMaxAngle);

    const auto minDegrees = minVar.isVoid() ? 135.0f : static_cast<float> (double (minVar));
    const auto maxDegrees = maxVar.isVoid() ? 405.0f : static_cast<float> (double (maxVar));
    knobComponent->setAngleRange (juce::degreesToRadians (minDegrees), juce::degreesToRadians (maxDegrees));

    if (auto parameterID = getControlledParameterID ({ }); parameterID.isNotEmpty())
        attachment = getMagicState().createAttachment (parameterID, knobComponent->getSlider());
}

std::vector<foleys::SettableProperty> MagicKnobWithOverlay::getSettableProperties() const
{
    std::vector<foleys::SettableProperty> props;
    props.push_back ({ configNode, foleys::IDs::parameter, foleys::SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() });
    props.push_back ({ configNode, pBaseImage, foleys::SettableProperty::Choice, {}, magicBuilder.createChoicesMenuLambda (getBinaryAssetNames()) });
    props.push_back ({ configNode, pOverlayImage, foleys::SettableProperty::Choice, {}, magicBuilder.createChoicesMenuLambda (getBinaryAssetNames()) });
    props.push_back ({ configNode, pMinAngle, foleys::SettableProperty::Number, 135.0f, {} });
    props.push_back ({ configNode, pMaxAngle, foleys::SettableProperty::Number, 405.0f, {} });
    return props;
}

juce::String MagicKnobWithOverlay::getControlledParameterID (juce::Point<int>)
{
    return configNode.getProperty (foleys::IDs::parameter, juce::String()).toString();
}

juce::Component* MagicKnobWithOverlay::getWrappedComponent()
{
    return knobComponent.get();
}

//==============================================================================
MagicKnobWithOverlay::KnobComponent::TransparentSlider::TransparentSlider()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setPopupDisplayEnabled (false, false, nullptr);
}

MagicKnobWithOverlay::KnobComponent::KnobComponent()
{
    addAndMakeVisible (slider);
    slider.onValueChange = [this]
    {
        repaint();
    };
}

void MagicKnobWithOverlay::KnobComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto proportion = static_cast<float> (slider.valueToProportionOfLength (slider.getValue()));
    const auto angle = juce::jmap (proportion, 0.0f, 1.0f, minAngle, maxAngle);

    if (baseImage.isValid())
        drawRotatedImage (g, baseImage, bounds, angle);

    if (overlayImage.isValid())
        g.drawImageWithin (overlayImage, static_cast<int> (bounds.getX()), static_cast<int> (bounds.getY()),
                           static_cast<int> (bounds.getWidth()), static_cast<int> (bounds.getHeight()),
                           juce::RectanglePlacement::stretchToFit, false);
}

void MagicKnobWithOverlay::KnobComponent::resized()
{
    slider.setBounds (getLocalBounds());
}

void MagicKnobWithOverlay::KnobComponent::setImages (juce::Image base, juce::Image overlay)
{
    baseImage = base;
    overlayImage = overlay;
    repaint();
}

void MagicKnobWithOverlay::KnobComponent::setAngleRange (float minRadians, float maxRadians)
{
    minAngle = minRadians;
    maxAngle = maxRadians;
    repaint();
}

void MagicKnobWithOverlay::KnobComponent::drawRotatedImage (juce::Graphics& g, const juce::Image& image,
                                                            juce::Rectangle<float> bounds, float angle) const
{
    auto source = image.getBounds().toFloat();
    if (source.isEmpty())
        return;

    const auto scale = std::min (bounds.getWidth() / source.getWidth(),
                                 bounds.getHeight() / source.getHeight());

    juce::AffineTransform transform;
    transform = transform.translated (-source.getWidth() * 0.5f, -source.getHeight() * 0.5f);
    transform = transform.scaled (scale);

    const auto centre = bounds.getCentre();
    transform = transform.rotated (angle);
    transform = transform.translated (centre.x, centre.y);

    g.drawImageTransformed (image, transform, false);
}

} // namespace tape
