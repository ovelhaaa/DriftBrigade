#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

void setParameter(juce::AudioPluginInstance& plugin, const juce::String& name, float value) {
    for (auto* parameter : plugin.getParameters())
        if (parameter->getName(64) == name) {
            parameter->setValueNotifyingHost(value);
            return;
        }
    throw std::runtime_error("host parameter missing: " + name.toStdString());
}

std::vector<float> render(juce::AudioPluginInstance& plugin, int channels, bool bypass) {
    juce::MidiBuffer midi;
    std::vector<float> result;
    std::size_t sample = 0;
    for (int i = 0; i < 36; ++i) {
        const int length = std::array<int, 6>{16, 32, 64, 127, 17, 512}[i % 6];
        juce::AudioBuffer<float> audio(channels, length);
        for (int ch = 0; ch < channels; ++ch)
            for (int n = 0; n < length; ++n)
                audio.setSample(ch, n, .25f * std::sin(.035f * static_cast<float>(sample + n + 11 * ch)));
        juce::AudioBuffer<float> dry;
        if (bypass) dry.makeCopyOf(audio);
        if (bypass) plugin.processBlockBypassed(audio, midi);
        else plugin.processBlock(audio, midi);
        for (int ch = 0; ch < channels; ++ch)
            for (int n = 0; n < length; ++n) {
                const auto x = audio.getSample(ch, n);
                require(std::isfinite(x), "nonfinite VST3 output");
                if (bypass) require(x == dry.getSample(ch, n), "host bypass changed dry signal");
                result.push_back(x);
            }
        sample += static_cast<std::size_t>(length);
    }
    return result;
}
} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    try {
        require(argc == 2, "usage: drift_vst3_host_tests path-to-bundle");
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> types;
        const auto path = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
        format.findAllTypesForFile(types, path.getFullPathName());
        require(types.size() == 1, "expected one VST3 audio effect");
        require(types[0]->version == "0.2.0", "expected VST3 bundle version 0.2.0");
        require(types[0]->manufacturerName == "Drift Audio", "expected VST3 manufacturer Drift Audio");
        juce::String error;
        auto plugin = format.createInstanceFromDescription(*types[0], 44100, 512, error);
        if (!plugin) throw std::runtime_error(error.toStdString());
        // JUCE's hosted normalized Center value of 1 gives 30 ms in both builds.
        setParameter(*plugin, "Center", 1);
        setParameter(*plugin, "Mix", .6f);
        juce::MemoryBlock saved;
        for (double rate : {44100., 48000., 88200., 96000., 44100.}) {
            for (int channels : {1, 2}) {
                juce::AudioProcessor::BusesLayout layout;
                const auto set = juce::AudioChannelSet::canonicalChannelSet(channels);
                layout.inputBuses.add(set);
                layout.outputBuses.add(set);
                require(plugin->setBusesLayout(layout), "VST3 host routing rejected");
                plugin->setNonRealtime(false);
                plugin->prepareToPlay(rate, 512);
                const auto normal = render(*plugin, channels, false);
                plugin->getStateInformation(saved);
                require(saved.getSize() > 0, "VST3 state is empty");
                plugin->releaseResources();

                setParameter(*plugin, "Mix", 0);
                plugin->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
                plugin->setNonRealtime(true);
                plugin->prepareToPlay(rate, 1024);
                const auto offline = render(*plugin, channels, false);
                require(normal == offline, "VST3 restored state or offline render differs");
                if (plugin->getBypassParameter()) {
                    const auto dry = render(*plugin, channels, true);
                    require(dry != normal, "processed VST3 output equals bypass");
                }
                plugin->releaseResources();
            }
        }
        std::cout << "PASS: loaded " << plugin->getName() << " in JUCE VST3 host\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
