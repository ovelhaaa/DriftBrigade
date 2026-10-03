#include "PluginProcessor.h"
#include "PluginEditor.h"
DriftProcessor::DriftProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "DriftState", drift::makeParameterLayout()), reader(state) {}
void DriftProcessor::prepareToPlay(double sampleRate, int) {
    engine.setParameters(reader.read()); engine.prepare(sampleRate); setLatencySamples(0);
}
bool DriftProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto input = layout.getMainInputChannelSet(), output = layout.getMainOutputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo())
        && (output == input || (input == juce::AudioChannelSet::mono() && output == juce::AudioChannelSet::stereo()));
}
void DriftProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const auto inputs = getTotalNumInputChannels(), outputs = getTotalNumOutputChannels();
    if (inputs == 1 && outputs == 2) buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
    for (int ch=inputs; ch<buffer.getNumChannels(); ++ch)
        if (!(inputs == 1 && ch == 1 && outputs == 2)) buffer.clear(ch, 0, buffer.getNumSamples());
    engine.setParameters(reader.read());
    engine.process(buffer.getArrayOfWritePointers(), static_cast<std::size_t>(outputs), static_cast<std::size_t>(buffer.getNumSamples()));
}
juce::AudioProcessorEditor* DriftProcessor::createEditor() { return new DriftEditor(*this); }
void DriftProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto saved = state.copyState(); saved.setProperty("schema", 1, nullptr);
    if (auto xml = saved.createXml()) copyXmlToBinary(*xml, dest);
}
void DriftProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) {
            auto restored = juce::ValueTree::fromXml(*xml);
            // Unknown future schema is left intact rather than partially applied.
            if (static_cast<int>(restored.getProperty("schema", 1)) == 1) state.replaceState(restored);
        }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DriftProcessor(); }
