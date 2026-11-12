#include "ui/QuickStartSelector.h"
#include "ui/DriftLookAndFeel.h"
#include <cmath>

namespace drift::ui {

QuickStartSelector::QuickStartSelector(juce::AudioProcessorValueTreeState& state)
    : apvts(state) {
    for (std::size_t i = 0; i < quickStartPresets.size(); ++i) {
        buttons[i].setButtonText(quickStartPresets[i].name);
        buttons[i].setTooltip(quickStartPresets[i].description);
        buttons[i].setClickingTogglesState(false);
        buttons[i].onClick = [this, i] { applyPreset(i); };
        addAndMakeVisible(buttons[i]);
    }
    syncHighlightWithParameters();
}

void QuickStartSelector::applyPresetToState(juce::AudioProcessorValueTreeState& state, std::size_t index) {
    if (index >= quickStartPresets.size()) return;
    const auto& preset = quickStartPresets[index];
    for (std::size_t p = 0; p < Count; ++p) {
        if (auto* param = state.getParameter(parameterSpecs[p].id)) {
            const float normalized = param->convertTo0to1(preset.values[p]);
            param->beginChangeGesture();
            param->setValueNotifyingHost(normalized);
            param->endChangeGesture();
        }
    }
}

void QuickStartSelector::applyPreset(std::size_t index) {
    applyPresetToState(apvts, index);
    syncHighlightWithParameters();
}

void QuickStartSelector::syncHighlightWithParameters() {
    int matched = -1;
    for (std::size_t i = 0; i < quickStartPresets.size(); ++i) {
        bool allMatch = true;
        for (std::size_t p = 0; p < Count; ++p) {
            const auto* param = apvts.getParameter(parameterSpecs[p].id);
            if (!param) {
                allMatch = false;
                break;
            }
            const float targetNorm = param->convertTo0to1(quickStartPresets[i].values[p]);
            const float currentNorm = param->getValue();
            if (std::abs(currentNorm - targetNorm) > 0.008f) {
                allMatch = false;
                break;
            }
        }
        if (allMatch) {
            matched = static_cast<int>(i);
            break;
        }
    }

    if (matched != currentActiveIndex) {
        currentActiveIndex = matched;
        for (std::size_t i = 0; i < buttons.size(); ++i) {
            buttons[i].setToggleState(static_cast<int>(i) == currentActiveIndex, juce::dontSendNotification);
            buttons[i].repaint();
        }
        repaint();
    }
}

void QuickStartSelector::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    const int labelW = std::clamp(static_cast<int>(bounds.getWidth() * 0.30f), 82, 96);

    // Small orange dot + QUICK START label on off-white header
    g.setColour(Palette::accentOrange);
    g.fillEllipse(2.0f, bounds.getCentreY() - 2.5f, 5.0f, 5.0f);

    g.setFont(DriftLookAndFeel::makeTechnicalFont(9.5f, true));
    g.setColour(Palette::textOnChassisSecondary);
    g.drawFittedText("QUICK START", 10, 0, labelW - 13, static_cast<int>(bounds.getHeight()),
                     juce::Justification::centredLeft, 1, 0.88f);

    // Recessed dark graphite housing around the 3 instrument selector keys
    const juce::Rectangle<float> housing(static_cast<float>(labelW), 0.5f,
                                         bounds.getWidth() - static_cast<float>(labelW) - 0.5f,
                                         bounds.getHeight() - 1.0f);
    g.setColour(Palette::panelBg);
    g.fillRoundedRectangle(housing, 4.5f);
    g.setColour(Palette::panelBorder);
    g.drawRoundedRectangle(housing, 4.5f, 1.0f);
}

void QuickStartSelector::resized() {
    auto area = getLocalBounds();
    const int labelW = std::clamp(static_cast<int>(area.getWidth() * 0.30f), 82, 96);
    area.removeFromLeft(labelW);
    area = area.reduced(3, 3);

    const int gap = 3;
    const int btnW = (area.getWidth() - gap * 2) / 3;
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        buttons[i].setBounds(area.removeFromLeft(i == 2 ? area.getWidth() : btnW));
        if (i < 2) area.removeFromLeft(gap);
    }
}

} // namespace drift::ui
