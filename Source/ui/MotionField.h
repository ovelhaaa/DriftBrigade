#pragma once
#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include "params/Parameters.h"

namespace drift::ui {

class MotionField final : public juce::Component, private juce::Timer {
public:
    static constexpr std::size_t kHistoryPoints = 96;
    static constexpr std::size_t kNumBands = 4;

    explicit MotionField(juce::AudioProcessorValueTreeState& state,
                         std::function<void()> onParametersChanged = {});
    ~MotionField() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    void setAnimationEnabled(bool enabled);
    bool isAnimationEnabled() const noexcept { return animationEnabled; }

    // Synchronous evaluation for deterministic visual snapshots and tests
    void stepSimulation(double deltaSeconds = 1.0 / 30.0) noexcept;

    const EngineParameters& currentParameters() const noexcept { return snapshot; }
    float trajectorySample(std::size_t band, std::size_t channel, std::size_t pointIndex) const noexcept {
        return trajectories[channel][band][pointIndex];
    }

private:
    void timerCallback() override;
    void recomputeTrajectories() noexcept;

    static double evaluateWander(std::uint32_t seed, double phaseCycles, double chaos) noexcept;

    ParameterReader reader;
    EngineParameters snapshot;
    std::function<void()> parameterChangeCallback;

    double phaseCycles = 0.0;
    bool animationEnabled = true;

    // Preallocated trajectory coordinates in normalized [-1, +1] canvas space: [channel L/R][band 0..3][point 0..95]
    std::array<std::array<std::array<float, kHistoryPoints>, kNumBands>, 2> trajectories {};
    // Preallocated ghost echo trail for feedback visualization
    std::array<std::array<std::array<float, kHistoryPoints>, kNumBands>, 2> feedbackTrails {};

    juce::TextButton freezeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotionField)
};

} // namespace drift::ui
