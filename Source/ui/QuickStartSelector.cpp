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
    }
}

void QuickStartSelector::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setFont(DriftLookAndFeel::makeTechnicalFont(10.0f, true));
    g.setColour(Palette::textMuted);
    g.drawText("QUICK START", 0, 0, 82, static_cast<int>(bounds.getHeight()),
               juce::Justification::centredLeft, false);
}

void QuickStartSelector::resized() {
    auto area = getLocalBounds();
    const int labelW = std::clamp(static_cast<int>(area.getWidth() * 0.27f), 68, 86);
    area.removeFromLeft(labelW);

    const int gap = 6;
    const int btnW = (area.getWidth() - gap * 2) / 3;
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        buttons[i].setBounds(area.removeFromLeft(i == 2 ? area.getWidth() : btnW));
        if (i < 2) area.removeFromLeft(gap);
    }
}

} // namespace drift::ui
