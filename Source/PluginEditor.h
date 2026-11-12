#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
class DriftEditor final : public juce::AudioProcessorEditor
#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
    , private juce::Timer
#endif
{
public:
    explicit DriftEditor(DriftProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
    void timerCallback() override { repaint(20, 45, getWidth()-40, 24); }
#endif
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::array<juce::Slider, drift::Count> sliders;
    std::array<juce::Label, drift::Count> labels;
    std::array<std::unique_ptr<Attachment>, drift::Count> attachments;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DriftEditor)
};
