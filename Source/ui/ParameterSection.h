#pragma once
#include <array>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/DriftKnob.h"

namespace drift::ui {

class ParameterSection final : public juce::Component {
public:
    ParameterSection(juce::AudioProcessorValueTreeState& state,
                     juce::String sectionTitle,
                     juce::String sectionSubtitle,
                     std::array<Param, 3> params,
                     int accentGroup);
    ~ParameterSection() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    DriftKnob& knobAt(std::size_t index) noexcept { return *knobs[index]; }
    const DriftKnob& knobAt(std::size_t index) const noexcept { return *knobs[index]; }

private:
    juce::String title;
    juce::String subtitle;
    int groupIndex = 0;
    std::array<std::unique_ptr<DriftKnob>, 3> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterSection)
};

} // namespace drift::ui
