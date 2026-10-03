#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterSpecs.h"
namespace drift {
static_assert(std::atomic<float>::is_always_lock_free, "Audio parameter reads must be lock-free");
juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout();
class ParameterReader {
public:
    explicit ParameterReader(juce::AudioProcessorValueTreeState& state);
    EngineParameters read() const noexcept;
private: std::array<std::atomic<float>*, Count> pointers {};
};
}
