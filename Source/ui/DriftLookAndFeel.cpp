#include "ui/DriftLookAndFeel.h"
#include <cmath>

namespace drift::ui {

DriftLookAndFeel::DriftLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, Palette::textPrimary);
    setColour(juce::Slider::textBoxBackgroundColourId, Palette::viewportBg);
    setColour(juce::Slider::textBoxOutlineColourId, Palette::borderSubtle);
    setColour(juce::Slider::textBoxHighlightColourId, Palette::accentCyanDim);
    setColour(juce::TooltipWindow::backgroundColourId, Palette::panelElevated);
    setColour(juce::TooltipWindow::textColourId, Palette::textPrimary);
    setColour(juce::TooltipWindow::outlineColourId, Palette::borderCrisp);
}

juce::Font DriftLookAndFeel::makeTechnicalFont(float height, bool bold) {
    return juce::Font(height, bold ? juce::Font::bold : juce::Font::plain);
}

void DriftLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider) {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height)).reduced(2.0f);
    const float diameter = std::min(bounds.getWidth(), bounds.getHeight());
    if (diameter <= 16.0f) return;

    const auto centre = bounds.getCentre();
    const float outerRadius = diameter * 0.46f;
    const float arcRadius = outerRadius * 0.88f;
    const float bodyRadius = outerRadius * 0.68f;
    const float capRadius = outerRadius * 0.56f;

    const bool useAmber = static_cast<int>(slider.getProperties().getWithDefault("accentGroup", 0)) == 2;
    const juce::Colour activeColour = useAmber ? Palette::accentAmber : Palette::accentCyan;
    const juce::Colour activeBright = useAmber ? Palette::accentAmber.brighter(0.25f) : Palette::accentCyanBright;
    const bool isHovered = slider.isMouseOverOrDragging();
    const bool isDragging = slider.isMouseButtonDown();

    // 1. Precision tick ring (21 ticks along the 270-degree sweep)
    constexpr int numTicks = 21;
    for (int i = 0; i < numTicks; ++i) {
        const float norm = static_cast<float>(i) / static_cast<float>(numTicks - 1);
        const float angle = rotaryStartAngle + norm * (rotaryEndAngle - rotaryStartAngle);
        const bool isMajor = (i == 0 || i == 5 || i == 10 || i == 15 || i == 20);
        const float innerTick = outerRadius * (isMajor ? 0.94f : 0.965f);
        const float outerTick = outerRadius * (isMajor ? 1.04f : 1.015f);
        const float sinA = std::sin(angle);
        const float cosA = -std::cos(angle);
        const bool passed = norm <= sliderPosProportional + 0.001f;
        g.setColour(passed ? activeColour.withAlpha(isMajor ? 0.72f : 0.42f)
                           : Palette::textMuted.withAlpha(isMajor ? 0.52f : 0.26f));
        g.drawLine(centre.x + sinA * innerTick, centre.y + cosA * innerTick,
                   centre.x + sinA * outerTick, centre.y + cosA * outerTick,
                   isMajor ? 1.4f : 1.0f);
    }

    // 2. Default-value marker tick on outer ring
    if (slider.getProperties().contains("defaultNorm")) {
        const float defNorm = juce::jlimit(0.0f, 1.0f, static_cast<float>(slider.getProperties()["defaultNorm"]));
        const float defAngle = rotaryStartAngle + defNorm * (rotaryEndAngle - rotaryStartAngle);
        const float sinD = std::sin(defAngle);
        const float cosD = -std::cos(defAngle);
        const float markerR = outerRadius * 1.075f;
        g.setColour(Palette::accentAmber.withAlpha(0.85f));
        g.fillEllipse(centre.x + sinD * markerR - 1.8f, centre.y + cosD * markerR - 1.8f, 3.6f, 3.6f);
    }

    // 3. Recessed value track arc
    const float trackThickness = std::max(2.6f, diameter * 0.048f);
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(Palette::knobTrack);
    g.strokePath(backgroundArc, juce::PathStrokeType(trackThickness + 1.2f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    g.setColour(Palette::borderSubtle);
    g.strokePath(backgroundArc, juce::PathStrokeType(trackThickness * 0.75f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

    // 4. Active value arc
    const float valueAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    if (sliderPosProportional > 0.002f) {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                               rotaryStartAngle, valueAngle, true);
        if (isHovered) {
            g.setColour(activeColour.withAlpha(isDragging ? 0.28f : 0.16f));
            g.strokePath(valueArc, juce::PathStrokeType(trackThickness * 2.1f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }
        g.setColour(isHovered ? activeBright : activeColour);
        g.strokePath(valueArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // 5. Machined outer collar & shadow
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(centre.x - bodyRadius, centre.y - bodyRadius + 1.8f, bodyRadius * 2.0f, bodyRadius * 2.0f);

    juce::ColourGradient collarGrad(Palette::knobBodyOuter.brighter(isHovered ? 0.12f : 0.04f),
                                    centre.x, centre.y - bodyRadius,
                                    Palette::knobBodyInner.darker(0.2f),
                                    centre.x, centre.y + bodyRadius, false);
    g.setGradientFill(collarGrad);
    g.fillEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

    g.setColour(isHovered ? Palette::borderCrisp.brighter(0.2f) : Palette::knobRing);
    g.drawEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.2f);

    // 6. Inner cap surface
    juce::ColourGradient capGrad(Palette::panelElevated, centre.x, centre.y - capRadius,
                                 Palette::knobBodyInner, centre.x, centre.y + capRadius, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

    g.setColour(Palette::borderSubtle.brighter(0.1f));
    g.drawEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

    // 7. Radial position indicator line + tip dot
    const float sinV = std::sin(valueAngle);
    const float cosV = -std::cos(valueAngle);
    const float pointerInner = capRadius * 0.24f;
    const float pointerOuter = capRadius * 0.86f;
    g.setColour(isHovered ? Palette::textPrimary : activeBright);
    g.drawLine(centre.x + sinV * pointerInner, centre.y + cosV * pointerInner,
               centre.x + sinV * pointerOuter, centre.y + cosV * pointerOuter,
               std::max(1.8f, diameter * 0.032f));

    const float tipX = centre.x + sinV * (arcRadius);
    const float tipY = centre.y + cosV * (arcRadius);
    g.setColour(Palette::textPrimary);
    g.fillEllipse(tipX - 2.1f, tipY - 2.1f, 4.2f, 4.2f);
}

juce::Label* DriftLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* l = LookAndFeel_V4::createSliderTextBox(slider);
    l->setFont(makeTechnicalFont(12.5f, true));
    l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::textColourId, Palette::textPrimary);
    l->setColour(juce::Label::backgroundColourId, Palette::viewportBg.withAlpha(0.85f));
    l->setColour(juce::Label::outlineColourId, Palette::borderSubtle);
    l->setColour(juce::Label::textWhenEditingColourId, Palette::accentCyanBright);
    l->setColour(juce::Label::backgroundWhenEditingColourId, Palette::viewportBg);
    l->setColour(juce::Label::outlineWhenEditingColourId, Palette::accentCyan);
    l->setMinimumHorizontalScale(0.75f);
    return l;
}

void DriftLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const float corner = 4.0f;
    const bool active = button.getToggleState();

    juce::Colour fill = active ? Palette::accentCyan.withAlpha(0.18f) : Palette::panelElevated;
    if (shouldDrawButtonAsDown)
        fill = active ? Palette::accentCyan.withAlpha(0.28f) : Palette::panelElevated.brighter(0.12f);
    else if (shouldDrawButtonAsHighlighted)
        fill = active ? Palette::accentCyan.withAlpha(0.22f) : Palette::panelElevated.brighter(0.06f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, corner);

    const juce::Colour border = active ? Palette::accentCyan
        : (shouldDrawButtonAsHighlighted ? Palette::borderCrisp.brighter(0.25f) : Palette::borderCrisp);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, corner, active ? 1.3f : 1.0f);
}

void DriftLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                      bool shouldDrawButtonAsHighlighted,
                                      bool) {
    const float fontHeight = std::clamp(button.getHeight() * 0.46f, 10.0f, 12.5f);
    g.setFont(makeTechnicalFont(fontHeight, true));
    const bool active = button.getToggleState();
    g.setColour(active ? Palette::accentCyanBright
                       : (shouldDrawButtonAsHighlighted ? Palette::textPrimary : Palette::textSecondary));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(6, 2),
                     juce::Justification::centred, 1);
}

juce::Rectangle<int> DriftLookAndFeel::getTooltipBounds(const juce::String& tipText,
                                                        juce::Point<int> screenPos,
                                                        juce::Rectangle<int> parentArea) {
    const juce::Font font = makeTechnicalFont(12.5f, false);
    constexpr int maxTooltipWidth = 310;
    juce::AttributedString s;
    s.setJustification(juce::Justification::centredLeft);
    s.append(tipText, font, Palette::textPrimary);

    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths(s, static_cast<float>(maxTooltipWidth));

    const int w = static_cast<int>(tl.getWidth()) + 20;
    const int h = static_cast<int>(tl.getHeight()) + 14;
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 10) : screenPos.x + 16,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 8)  : screenPos.y + 8,
                                w, h).constrainedWithin(parentArea);
}

void DriftLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) {
    const juce::Rectangle<float> bounds(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
    g.setColour(Palette::panelElevated. withAlpha(0.97f));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(Palette::borderCrisp);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    juce::AttributedString s;
    s.setJustification(juce::Justification::centredLeft);
    s.append(text, makeTechnicalFont(12.5f, false), Palette::textPrimary);

    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths(s, static_cast<float>(width - 20));
    tl.draw(g, juce::Rectangle<float>(10.0f, 7.0f, static_cast<float>(width - 20), static_cast<float>(height - 14)));
}

void DriftLookAndFeel::drawCornerResizer(juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging) {
    const juce::Colour c = (isMouseOver || isMouseDragging) ? Palette::accentCyan : Palette::textMuted.withAlpha(0.6f);
    g.setColour(c);
    for (int i = 0; i < 3; ++i) {
        const float offset = 4.0f + static_cast<float>(i) * 4.0f;
        g.drawLine(static_cast<float>(w) - offset, static_cast<float>(h) - 2.0f,
                   static_cast<float>(w) - 2.0f, static_cast<float>(h) - offset, 1.2f);
    }
}

} // namespace drift::ui
