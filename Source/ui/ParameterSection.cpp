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
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    constexpr float corner = 6.0f;

    juce::ColourGradient panelGrad(Palette::panelBg.brighter(0.03f), bounds.getX(), bounds.getY(),
                                   Palette::panelBg.darker(0.08f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(panelGrad);
    g.fillRoundedRectangle(bounds, corner);

    g.setColour(Palette::borderSubtle);
    g.drawRoundedRectangle(bounds, corner, 1.0f);

    // Top header strip inside the bay
    const float headerH = std::clamp(bounds.getHeight() * 0.145f, 22.0f, 30.0f);
    const juce::Rectangle<float> headerRect(bounds.getX() + 10.0f, bounds.getY() + 4.0f,
                                            bounds.getWidth() - 20.0f, headerH - 4.0f);

    const juce::Colour accent = (groupIndex == 2) ? Palette::accentAmber : Palette::accentCyan;
    g.setColour(accent);
    g.fillRoundedRectangle(bounds.getX() + 10.0f, bounds.getY() + 8.0f, 3.0f, headerH - 12.0f, 1.5f);

    g.setFont(DriftLookAndFeel::makeTechnicalFont(std::clamp(headerH * 0.45f, 10.5f, 12.5f), true));
    g.setColour(Palette::textPrimary);
    g.drawText(title, static_cast<int>(headerRect.getX() + 9.0f), static_cast<int>(headerRect.getY()),
               static_cast<int>(headerRect.getWidth() * 0.55f), static_cast<int>(headerRect.getHeight()),
               juce::Justification::centredLeft, false);

    g.setFont(DriftLookAndFeel::makeTechnicalFont(std::clamp(headerH * 0.37f, 9.0f, 10.5f), false));
    g.setColour(Palette::textMuted);
    g.drawText(subtitle, static_cast<int>(headerRect.getX() + headerRect.getWidth() * 0.42f),
               static_cast<int>(headerRect.getY()),
               static_cast<int>(headerRect.getWidth() * 0.58f), static_cast<int>(headerRect.getHeight()),
               juce::Justification::centredRight, false);

    // Subtle divider below section header
    g.setColour(Palette::borderSubtle);
    g.drawHorizontalLine(static_cast<int>(bounds.getY() + headerH), bounds.getX() + 8.0f, bounds.getRight() - 8.0f);

    // Subtle vertical dividers between the 3 knobs
    const float contentTop = bounds.getY() + headerH + 8.0f;
    const float contentBottom = bounds.getBottom() - 8.0f;
    const float thirdW = bounds.getWidth() / 3.0f;
    g.setColour(Palette::borderSubtle.withAlpha(0.55f));
    g.drawVerticalLine(static_cast<int>(bounds.getX() + thirdW), contentTop, contentBottom);
    g.drawVerticalLine(static_cast<int>(bounds.getX() + thirdW * 2.0f), contentTop, contentBottom);
}

void ParameterSection::resized() {
    auto area = getLocalBounds().reduced(6, 4);
    const int headerH = std::clamp(static_cast<int>(getHeight() * 0.145f), 22, 30);
    area.removeFromTop(headerH + 2);

    const int knobW = area.getWidth() / 3;
    for (std::size_t i = 0; i < 3; ++i) {
        auto cell = area.removeFromLeft(i == 2 ? area.getWidth() : knobW).reduced(2, 2);
        knobs[i]->setBounds(cell);
    }
}

} // namespace drift::ui
