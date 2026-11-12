#include "ui/DriftLookAndFeel.h"
#include <cmath>

namespace drift::ui {

DriftLookAndFeel::DriftLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, Palette::textOnPanel);
    setColour(juce::Slider::textBoxBackgroundColourId, Palette::displayBg);
    setColour(juce::Slider::textBoxOutlineColourId, Palette::panelBorder);
    setColour(juce::Slider::textBoxHighlightColourId, Palette::accentOrangeDim);
    setColour(juce::TooltipWindow::backgroundColourId, Palette::panelBg);
    setColour(juce::TooltipWindow::textColourId, Palette::textOnPanel);
    setColour(juce::TooltipWindow::outlineColourId, Palette::accentOrange);
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
    const float arcRadius = outerRadius * 0.87f;
    const float bodyRadius = outerRadius * 0.68f;
    const float capRadius = outerRadius * 0.55f;

    const juce::Colour activeColour = Palette::accentOrange;
    const juce::Colour activeBright = Palette::accentOrangeBright;
    const bool isHovered = slider.isMouseOverOrDragging();
    const bool isDragging = slider.isMouseButtonDown();

    // 1. Precision scale divisions (21 ticks along the 270-degree sweep)
    constexpr int numTicks = 21;
    for (int i = 0; i < numTicks; ++i) {
        const float norm = static_cast<float>(i) / static_cast<float>(numTicks - 1);
        const float angle = rotaryStartAngle + norm * (rotaryEndAngle - rotaryStartAngle);
        const bool isMajor = (i == 0 || i == 5 || i == 10 || i == 15 || i == 20);
        const float innerTick = outerRadius * (isMajor ? 0.935f : 0.96f);
        const float outerTick = outerRadius * (isMajor ? 1.045f : 1.015f);
        const float sinA = std::sin(angle);
        const float cosA = -std::cos(angle);
        const bool passed = norm <= sliderPosProportional + 0.001f;
        g.setColour(passed ? activeColour.withAlpha(isMajor ? 0.82f : 0.46f)
                           : Palette::textOnPanelMuted.withAlpha(isMajor ? 0.56f : 0.28f));
        g.drawLine(centre.x + sinA * innerTick, centre.y + cosA * innerTick,
                   centre.x + sinA * outerTick, centre.y + cosA * outerTick,
                   isMajor ? 1.45f : 1.0f);
    }

    // 2. Discreet default-value marker on outer scale ring
    if (slider.getProperties().contains("defaultNorm")) {
        const float defNorm = juce::jlimit(0.0f, 1.0f, static_cast<float>(slider.getProperties()["defaultNorm"]));
        const float defAngle = rotaryStartAngle + defNorm * (rotaryEndAngle - rotaryStartAngle);
        const float sinD = std::sin(defAngle);
        const float cosD = -std::cos(defAngle);
        const float markerR = outerRadius * 1.08f;
        g.setColour(Palette::textOnPanelSecondary.withAlpha(0.85f));
        g.fillEllipse(centre.x + sinD * markerR - 1.7f, centre.y + cosD * markerR - 1.7f, 3.4f, 3.4f);
    }

    // 3. Recessed inactive value track arc
    const float trackThickness = std::max(2.6f, diameter * 0.050f);
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(Palette::knobTrack);
    g.strokePath(backgroundArc, juce::PathStrokeType(trackThickness + 1.4f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    g.setColour(Palette::panelBorder);
    g.strokePath(backgroundArc, juce::PathStrokeType(trackThickness * 0.72f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

    // 4. Active orange value arc
    const float valueAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    if (sliderPosProportional > 0.002f) {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                               rotaryStartAngle, valueAngle, true);
        if (isHovered) {
            g.setColour(activeColour.withAlpha(isDragging ? 0.26f : 0.14f));
            g.strokePath(valueArc, juce::PathStrokeType(trackThickness * 2.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }
        g.setColour(isHovered ? activeBright : activeColour);
        g.strokePath(valueArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // 5. Tactile dark graphite outer collar & subtle contact shadow
    g.setColour(Palette::displayBg.withAlpha(0.65f));
    g.fillEllipse(centre.x - bodyRadius, centre.y - bodyRadius + 1.5f, bodyRadius * 2.0f, bodyRadius * 2.0f);

    juce::ColourGradient collarGrad(Palette::knobBodyOuter.brighter(isHovered ? 0.08f : 0.02f),
                                    centre.x, centre.y - bodyRadius,
                                    Palette::displayBg,
                                    centre.x, centre.y + bodyRadius, false);
    g.setGradientFill(collarGrad);
    g.fillEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

    g.setColour(isHovered ? Palette::panelBorderCrisp : Palette::knobRing);
    g.drawEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.15f);

    // 6. Machined inner cap surface
    juce::ColourGradient capGrad(Palette::knobCapTop, centre.x, centre.y - capRadius,
                                 Palette::knobBodyInner, centre.x, centre.y + capRadius, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

    g.setColour(Palette::panelBorder);
    g.drawEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

    // 7. Crisp radial position indicator line + orange arc tip marker
    const float sinV = std::sin(valueAngle);
    const float cosV = -std::cos(valueAngle);
    const float pointerInner = capRadius * 0.22f;
    const float pointerOuter = capRadius * 0.88f;
    g.setColour(Palette::textOnPanel);
    g.drawLine(centre.x + sinV * pointerInner, centre.y + cosV * pointerInner,
               centre.x + sinV * pointerOuter, centre.y + cosV * pointerOuter,
               std::max(1.9f, diameter * 0.034f));

    const float tipX = centre.x + sinV * arcRadius;
    const float tipY = centre.y + cosV * arcRadius;
    g.setColour(activeColour);
    g.fillEllipse(tipX - 2.6f, tipY - 2.6f, 5.2f, 5.2f);
    g.setColour(Palette::textOnPanel);
    g.fillEllipse(tipX - 1.3f, tipY - 1.3f, 2.6f, 2.6f);
}

juce::Label* DriftLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* l = LookAndFeel_V4::createSliderTextBox(slider);
    l->setFont(makeTechnicalFont(12.5f, true));
    l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::textColourId, Palette::textOnPanel);
    l->setColour(juce::Label::backgroundColourId, Palette::displayBg);
    l->setColour(juce::Label::outlineColourId, Palette::panelBorder);
    l->setColour(juce::Label::textWhenEditingColourId, Palette::textOnPanel);
    l->setColour(juce::Label::backgroundWhenEditingColourId, Palette::displayBg);
    l->setColour(juce::Label::outlineWhenEditingColourId, Palette::accentOrange);
    l->setMinimumHorizontalScale(0.75f);
    return l;
}

void DriftLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const float corner = 3.5f;
    const bool active = button.getToggleState();
    const bool isHoldButton = static_cast<bool>(button.getProperties().getWithDefault("isHoldButton", false));

    if (isHoldButton && !active) {
        // HOLD state: dark display surface with crisp orange border
        juce::Colour fill = shouldDrawButtonAsDown ? Palette::panelElevated
            : (shouldDrawButtonAsHighlighted ? Palette::panelRecessed : Palette::displayBg);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, corner);
        g.setColour(Palette::accentOrange);
        g.drawRoundedRectangle(bounds, corner, 1.4f);
        return;
    }

    juce::Colour fill = active ? Palette::accentOrange : Palette::panelElevated;
    if (shouldDrawButtonAsDown)
        fill = active ? Palette::accentOrangeSoft : Palette::panelElevated.brighter(0.10f);
    else if (shouldDrawButtonAsHighlighted)
        fill = active ? Palette::accentOrangeBright : Palette::panelElevated.brighter(0.06f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, corner);

    const juce::Colour border = active ? Palette::accentOrangeBright
        : (shouldDrawButtonAsHighlighted ? Palette::panelBorderCrisp.brighter(0.15f) : Palette::panelBorder);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, corner, 1.0f);

    // Subtle tactile top indicator bar on Quick Start keys
    if (!isHoldButton && bounds.getWidth() > 36.0f) {
        const float ledW = std::min(16.0f, bounds.getWidth() * 0.26f);
        const juce::Rectangle<float> ledRect(bounds.getCentreX() - ledW * 0.5f, bounds.getY() + 2.5f, ledW, 1.6f);
        g.setColour(active ? Palette::displayBg.withAlpha(0.55f)
                           : Palette::textOnPanelMuted.withAlpha(shouldDrawButtonAsHighlighted ? 0.65f : 0.35f));
        g.fillRoundedRectangle(ledRect, 0.8f);
    }
}

void DriftLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                      bool shouldDrawButtonAsHighlighted,
                                      bool) {
    const float fontHeight = std::clamp(button.getHeight() * 0.45f, 10.0f, 12.0f);
    g.setFont(makeTechnicalFont(fontHeight, true));
    const bool active = button.getToggleState();
    const bool isHoldButton = static_cast<bool>(button.getProperties().getWithDefault("isHoldButton", false));

    if (isHoldButton && !active) {
        g.setColour(Palette::accentOrange);
    } else if (active) {
        g.setColour(Palette::displayBg);
    } else {
        g.setColour(shouldDrawButtonAsHighlighted ? Palette::textOnPanel : Palette::textOnPanelSecondary);
    }

    const int topOffset = (!isHoldButton && button.getWidth() > 36) ? 2 : 0;
    auto textBounds = button.getLocalBounds().reduced(5, 1).withTrimmedTop(topOffset);
    g.drawFittedText(button.getButtonText(), textBounds, juce::Justification::centred, 1);
}

juce::Rectangle<int> DriftLookAndFeel::getTooltipBounds(const juce::String& tipText,
                                                        juce::Point<int> screenPos,
                                                        juce::Rectangle<int> parentArea) {
    const juce::Font font = makeTechnicalFont(12.5f, false);
    constexpr int maxTooltipWidth = 310;
    juce::AttributedString s;
    s.setJustification(juce::Justification::centredLeft);
    s.append(tipText, font, Palette::textOnPanel);

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
    g.setColour(Palette::panelBg.withAlpha(0.98f));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(Palette::accentOrange);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    juce::AttributedString s;
    s.setJustification(juce::Justification::centredLeft);
    s.append(text, makeTechnicalFont(12.5f, false), Palette::textOnPanel);

    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths(s, static_cast<float>(width - 20));
    tl.draw(g, juce::Rectangle<float>(10.0f, 7.0f, static_cast<float>(width - 20), static_cast<float>(height - 14)));
}

void DriftLookAndFeel::drawCornerResizer(juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging) {
    const juce::Colour c = (isMouseOver || isMouseDragging)
        ? Palette::accentOrange
        : Palette::textOnChassisMuted.withAlpha(0.75f);
    g.setColour(c);
    for (int i = 0; i < 3; ++i) {
        const float offset = 4.0f + static_cast<float>(i) * 4.0f;
        g.drawLine(static_cast<float>(w) - offset, static_cast<float>(h) - 2.0f,
                   static_cast<float>(w) - 2.0f, static_cast<float>(h) - offset, 1.25f);
    }
}

} // namespace drift::ui
