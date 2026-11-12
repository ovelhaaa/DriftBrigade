#include "ui/ParameterSection.h"
#include "ui/DriftLookAndFeel.h"

namespace drift::ui {

ParameterSection::ParameterSection(juce::AudioProcessorValueTreeState& state,
                                   juce::String sectionTitle,
                                   juce::String sectionSubtitle,
                                   std::array<Param, 3> params,
                                   int accentGroup)
    : title(std::move(sectionTitle)),
      subtitle(std::move(sectionSubtitle)),
      groupIndex(accentGroup) {
    for (std::size_t i = 0; i < 3; ++i) {
        knobs[i] = std::make_unique<DriftKnob>(state, params[i], groupIndex);
        addAndMakeVisible(*knobs[i]);
    }
}

void ParameterSection::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    constexpr float corner = 5.5f;

    // Main graphite module body (#292B2D) over the off-white chassis
    g.setColour(Palette::panelBg);
    g.fillRoundedRectangle(bounds, corner);

    // Top header band inside the module
    const float headerH = std::clamp(bounds.getHeight() * 0.15f, 23.0f, 30.0f);
    const juce::Rectangle<float> headerBgRect(bounds.getX() + 1.0f, bounds.getY() + 1.0f,
                                              bounds.getWidth() - 2.0f, headerH);
    g.setColour(Palette::panelRecessed);
    g.fillRoundedRectangle(headerBgRect, corner);
    g.fillRect(bounds.getX() + 1.0f, bounds.getY() + headerH - corner, bounds.getWidth() - 2.0f, corner);

    g.setColour(Palette::panelBorder);
    g.drawRoundedRectangle(bounds, corner, 1.0f);

    const juce::Rectangle<float> headerRect(bounds.getX() + 10.0f, bounds.getY(),
                                            bounds.getWidth() - 20.0f, headerH);

    // Minimalist orange index bar
    g.setColour(Palette::accentOrange);
    g.fillRoundedRectangle(bounds.getX() + 10.0f, bounds.getY() + (headerH - 11.0f) * 0.5f, 3.0f, 11.0f, 1.2f);

    g.setFont(DriftLookAndFeel::makeTechnicalFont(std::clamp(headerH * 0.44f, 10.5f, 12.5f), true));
    g.setColour(Palette::textOnPanel);
    g.drawText(title, static_cast<int>(headerRect.getX() + 9.0f), static_cast<int>(headerRect.getY()),
               static_cast<int>(headerRect.getWidth() * 0.52f), static_cast<int>(headerRect.getHeight()),
               juce::Justification::centredLeft, false);

    g.setFont(DriftLookAndFeel::makeTechnicalFont(std::clamp(headerH * 0.35f, 8.8f, 10.2f), false));
    g.setColour(Palette::textOnPanelMuted);
    g.drawText(subtitle, static_cast<int>(headerRect.getX() + headerRect.getWidth() * 0.40f),
               static_cast<int>(headerRect.getY()),
               static_cast<int>(headerRect.getWidth() * 0.60f), static_cast<int>(headerRect.getHeight()),
               juce::Justification::centredRight, false);

    // Subtle divider below section header
    g.setColour(Palette::panelBorder);
    g.drawHorizontalLine(static_cast<int>(bounds.getY() + headerH), bounds.getX() + 1.0f, bounds.getRight() - 1.0f);

    // Discreet vertical dividers between the 3 knobs
    const float contentTop = bounds.getY() + headerH + 8.0f;
    const float contentBottom = bounds.getBottom() - 8.0f;
    const float thirdW = bounds.getWidth() / 3.0f;
    g.setColour(Palette::panelBorder.withAlpha(0.65f));
    g.drawVerticalLine(static_cast<int>(bounds.getX() + thirdW), contentTop, contentBottom);
    g.drawVerticalLine(static_cast<int>(bounds.getX() + thirdW * 2.0f), contentTop, contentBottom);
}

void ParameterSection::resized() {
    auto area = getLocalBounds().reduced(6, 4);
    const int headerH = std::clamp(static_cast<int>(getHeight() * 0.15f), 23, 30);
    area.removeFromTop(headerH + 2);

    const int knobW = area.getWidth() / 3;
    for (std::size_t i = 0; i < 3; ++i) {
        auto cell = area.removeFromLeft(i == 2 ? area.getWidth() : knobW).reduced(2, 2);
        knobs[i]->setBounds(cell);
    }
}

} // namespace drift::ui
