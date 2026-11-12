#pragma once
#include "PluginProcessor.h"
#include "ui/DriftLookAndFeel.h"
#include "ui/DriftKnob.h"
#include "ui/MotionField.h"
#include "ui/ParameterSection.h"
#include "ui/QuickStartSelector.h"
#include <juce_gui_basics/juce_gui_basics.h>

class DriftEditor final : public juce::AudioProcessorEditor
#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
    , private juce::Timer
#endif
{
public:
    explicit DriftEditor(DriftProcessor&);
    ~DriftEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    drift::ui::MotionField& motionFieldComponent() noexcept { return motionField; }
    const drift::ui::MotionField& motionFieldComponent() const noexcept { return motionField; }
    drift::ui::QuickStartSelector& quickStartComponent() noexcept { return quickStartSelector; }
    drift::ui::DriftKnob& knobFor(drift::Param param) noexcept;

private:
#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
    void timerCallback() override { repaint(0, 0, getWidth(), 56); }
#endif

    drift::ui::DriftLookAndFeel lookAndFeel;
    drift::ui::QuickStartSelector quickStartSelector;
    drift::ui::MotionField motionField;
    drift::ui::ParameterSection movementSection;
    drift::ui::ParameterSection spaceSection;
    drift::ui::ParameterSection characterSection;
    juce::TooltipWindow tooltipWindow {this, 450};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DriftEditor)
};
