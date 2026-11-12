#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/DriftEngine.h"
#include "params/Parameters.h"
#include "ProductConfig.h"
class DriftProcessor final : public juce::AudioProcessor {
public:
    DriftProcessor();
    const juce::String getName() const override { return drift::productName; }
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState state;
#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
    drift::BBDPreparationOutcome hostTestOutcome() const noexcept { return outcome.load(std::memory_order_relaxed); }
    bool hostTestParametersAccepted() const noexcept { return accepted.load(std::memory_order_relaxed); }
#endif
private:
#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
    static_assert(std::atomic<drift::BBDPreparationOutcome>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);
    drift::BBDAdmissionRequest hostScene;
    std::atomic<drift::BBDPreparationOutcome> outcome {drift::BBDPreparationOutcome::Unavailable};
    std::atomic<bool> accepted {false};
#endif
    drift::ParameterReader reader;
    drift::DriftEngine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DriftProcessor)
};
