#include "Parameters.h"
namespace drift {
juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for (const auto& s : parameterSpecs) {
        juce::NormalisableRange<float> range(s.minimum, s.maximum);
        if (s.logarithmic) {
            range = juce::NormalisableRange<float>(s.minimum, s.maximum,
                [](float lo, float hi, float t) { return lo*std::pow(hi/lo, t); },
                [](float lo, float hi, float x) { return std::log(x/lo)/std::log(hi/lo); });
        }
        auto attributes = juce::AudioParameterFloatAttributes().withLabel(s.unit)
            .withStringFromValueFunction([s](float value, int) {
                return juce::String(s.logarithmic ? value : value*100.0f, s.logarithmic ? 2 : 0);
            })
            .withValueFromStringFunction([s](const juce::String& text) { return text.getFloatValue()/(s.logarithmic ? 1.0f : 100.0f); });
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{s.id, 1}, s.name, range, s.initial, attributes));
    }
    return result;
}
ParameterReader::ParameterReader(juce::AudioProcessorValueTreeState& state) {
    for (std::size_t i=0; i<Count; ++i) pointers[i] = state.getRawParameterValue(parameterSpecs[i].id);
}
EngineParameters ParameterReader::read() const noexcept {
    EngineParameters p;
    for (std::size_t i=0; i<Count; ++i) p.values[i] = pointers[i]->load(std::memory_order_relaxed);
    return p;
}
}
