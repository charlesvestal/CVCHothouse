#pragma once

#include <foleys_gui_magic/foleys_gui_magic.h>

namespace tape
{

class MagicThreeWaySwitch : public foleys::GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (MagicThreeWaySwitch)

    MagicThreeWaySwitch (foleys::MagicGUIBuilder& builder, const juce::ValueTree& node);

    void update() override;
    std::vector<foleys::SettableProperty> getSettableProperties() const override;
    juce::String getControlledParameterID (juce::Point<int>) override;
    juce::Component* getWrappedComponent() override;

private:
    class SwitchComponent : public juce::Component
    {
    public:
        SwitchComponent();

        void paint (juce::Graphics& g) override;
        void resized() override;

        void setImages (juce::Image track, juce::Image handle);
        void setPadding (float padding);
        void setHandleGeometry (float width, float height, float offsetY);
        juce::Slider& getSlider() { return slider; }

    private:
        class HiddenSlider : public juce::Slider
        {
        public:
            HiddenSlider();
            void paint (juce::Graphics&) override {}
        };

        HiddenSlider slider;
        juce::Image trackImage;
        juce::Image handleImage;
        float horizontalPadding = 12.0f;
        float handleWidthOverride = 0.0f;
        float handleHeightOverride = 0.0f;
        float handleOffsetY = 0.0f;
    };

    std::unique_ptr<SwitchComponent> switchComponent;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;

    static const juce::Identifier pTrackImage;
    static const juce::Identifier pHandleImage;
    static const juce::Identifier pPadding;
    static const juce::Identifier pHandleWidth;
    static const juce::Identifier pHandleHeight;
    static const juce::Identifier pHandleOffsetY;
};

} // namespace tape
