#pragma once

#include <foleys_gui_magic/foleys_gui_magic.h>

namespace tape
{

class MagicKnobWithOverlay : public foleys::GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (MagicKnobWithOverlay)

    MagicKnobWithOverlay (foleys::MagicGUIBuilder& builder, const juce::ValueTree& node);

    void update() override;
    std::vector<foleys::SettableProperty> getSettableProperties() const override;
    juce::String getControlledParameterID (juce::Point<int>) override;
    juce::Component* getWrappedComponent() override;

private:
    class KnobComponent : public juce::Component
    {
    public:
        KnobComponent();

        void paint (juce::Graphics& g) override;
        void resized() override;

        void setImages (juce::Image base, juce::Image overlay);
        void setAngleRange (float minRadians, float maxRadians);

        juce::Slider& getSlider() { return slider; }

    private:
        class TransparentSlider : public juce::Slider
        {
        public:
            TransparentSlider();
            void paint (juce::Graphics&) override {}
        };

        void drawRotatedImage (juce::Graphics& g, const juce::Image& image,
                               juce::Rectangle<float> bounds, float angle) const;

        TransparentSlider slider;
        juce::Image baseImage;
        juce::Image overlayImage;
        float minAngle = juce::degreesToRadians (135.0f);
        float maxAngle = juce::degreesToRadians (405.0f);
    };

    std::unique_ptr<KnobComponent> knobComponent;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;

    static const juce::Identifier pBaseImage;
    static const juce::Identifier pOverlayImage;
    static const juce::Identifier pMinAngle;
    static const juce::Identifier pMaxAngle;
};

} // namespace tape
