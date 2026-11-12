#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "params/ParameterSpecs.h"

namespace drift::ui {

class DriftKnob final : public juce::Component {
public:
    DriftKnob(juce::AudioProcessorValueTreeState& state, Param param, int accentGroup);
    ~DriftKnob() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    juce::Slider& sliderComponent() noexcept { return slider; }
    const juce::Slider& sliderComponent() const noexcept { return slider; }
    Param parameterIndex() const noexcept { return paramIndex; }

    static juce::String descriptionFor(Param param);

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    Param paramIndex;
    int groupIndex = 0;
    juce::Label titleLabel;
    juce::Slider slider;
    std::unique_ptr<Attachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DriftKnob)
};

} // namespace drift::ui
