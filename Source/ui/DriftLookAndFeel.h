#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace drift::ui {

struct Palette {
    // Mandatory core palette — M3.2.1 Precision Instrument (Off-White + Graphite + Orange)
    static inline const juce::Colour chassisBg             {0xfff0ede6}; // #F0EDE6 warm off-white chassis
    static inline const juce::Colour chassisSurface        {0xffe6e2d8}; // recessed neutral detail on chassis
    static inline const juce::Colour chassisBorder         {0xffd4cfc3}; // subtle divider on off-white
    static inline const juce::Colour chassisBorderCrisp    {0xffbcb6a8}; // firmer structural line on off-white

    static inline const juce::Colour panelBg               {0xff292b2d}; // #292B2D neutral dark graphite panels
    static inline const juce::Colour panelElevated         {0xff333639}; // tactile switch / elevated control surface
    static inline const juce::Colour panelRecessed         {0xff212325}; // subtle header/bay recess on graphite
    static inline const juce::Colour panelBorder           {0xff3b3e42}; // subtle border inside dark panels
    static inline const juce::Colour panelBorderCrisp      {0xff4d5156}; // defined neutral border on dark panels

    static inline const juce::Colour displayBg             {0xff17191b}; // #17191B almost-black display interior
    static inline const juce::Colour gridLine              {0xff222528}; // subtle neutral graticule line
    static inline const juce::Colour gridAxis              {0xff303438}; // neutral reference axis line

    // Single chromatic accent family: Orange (#F36B2B)
    static inline const juce::Colour accentOrange          {0xfff36b2b}; // #F36B2B primary chromatic accent
    static inline const juce::Colour accentOrangeBright    {0xfff6824b}; // hover / active highlight shade of #F36B2B
    static inline const juce::Colour accentOrangeSoft      {0xffd85b20}; // deeper shade of #F36B2B
    static inline const juce::Colour accentOrangeDim       {0xff7a3414}; // muted dark shade of #F36B2B for selection

    // Knob hardware surfaces (strictly neutral dark grays)
    static inline const juce::Colour knobBodyOuter         {0xff1d1f21};
    static inline const juce::Colour knobBodyInner         {0xff232628};
    static inline const juce::Colour knobCapTop            {0xff303336};
    static inline const juce::Colour knobRing              {0xff404448};
    static inline const juce::Colour knobTrack             {0xff17191b};

    // Typography on off-white chassis (#F0EDE6)
    static inline const juce::Colour textOnChassis         {0xff292b2d}; // #292B2D primary text on off-white
    static inline const juce::Colour textOnChassisSecondary{0xff595753}; // neutral secondary text on off-white
    static inline const juce::Colour textOnChassisMuted    {0xff78756e}; // subtle technical caption on off-white

    // Typography on dark graphite (#292B2D) & display (#17191B)
    static inline const juce::Colour textOnPanel           {0xfff3f0e9}; // #F3F0E9 primary text on graphite
    static inline const juce::Colour textOnPanelSecondary  {0xffbab6ad}; // neutral secondary text on graphite
    static inline const juce::Colour textOnPanelMuted      {0xff85827a}; // neutral muted text on graphite
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
