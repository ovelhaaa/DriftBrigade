#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace drift::ui {

struct Palette {
    static inline const juce::Colour chassisTop      {0xff0c1016};
    static inline const juce::Colour chassisBottom   {0xff111720};
    static inline const juce::Colour headerBg        {0xff10161f};
    static inline const juce::Colour panelBg         {0xff151c26};
    static inline const juce::Colour panelElevated   {0xff1b2431};
    static inline const juce::Colour viewportBg      {0xff090d12};
    static inline const juce::Colour viewportInner   {0xff0d131b};
    static inline const juce::Colour borderSubtle    {0xff222d3b};
    static inline const juce::Colour borderCrisp     {0xff2e3d4f};
    static inline const juce::Colour gridLine        {0xff16202b};
    static inline const juce::Colour gridAxis        {0xff263648};

    static inline const juce::Colour accentCyan      {0xff38d4c8};
    static inline const juce::Colour accentCyanBright{0xff66efe4};
    static inline const juce::Colour accentCyanDim   {0xff1c6f69};
    static inline const juce::Colour accentAmber     {0xffe5a44c};
    static inline const juce::Colour accentAmberDim  {0xff7d5724};
    static inline const juce::Colour accentMineral   {0xff5cd89d};

    static inline const juce::Colour knobBodyOuter   {0xff1b232e};
    static inline const juce::Colour knobBodyInner   {0xff11171f};
    static inline const juce::Colour knobRing        {0xff2c3949};
    static inline const juce::Colour knobTrack       {0xff0d1218};

    static inline const juce::Colour textPrimary     {0xffebf1f6};
    static inline const juce::Colour textSecondary   {0xff96a7b8};
    static inline const juce::Colour textMuted       {0xff5e7083};
};

class DriftLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    DriftLookAndFeel();
    ~DriftLookAndFeel() override = default;

    static juce::Font makeTechnicalFont(float height, bool bold = false);

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    juce::Label* createSliderTextBox(juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText,
                                          juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;
    void drawCornerResizer(juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging) override;
};

} // namespace drift::ui
