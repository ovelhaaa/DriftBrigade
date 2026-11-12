#pragma once
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "params/ParameterSpecs.h"

namespace drift::ui {

struct QuickStartPreset {
    const char* name;
    const char* description;
    // Order matches drift::Param: Motion, Depth, Center, Chaos, Coherence, Dynamics, Feedback, Mix, Width
    std::array<float, Count> values;
};

inline constexpr std::array<QuickStartPreset, 3> quickStartPresets {{
    {
        "SUBTLE",
        "SUBTLE — Movimento discreto, preservando a fonte com profundidade estereo natural.",
        {0.45f, 0.35f, 6.50f, 0.30f, 0.70f, 0.20f, 0.08f, 0.35f, 0.45f}
    },
    {
        "WIDE",
        "WIDE — Chorus aberto e expressivo, explorando separacao de bandas e largura estereo.",
        {0.85f, 0.62f, 11.50f, 0.45f, 0.30f, 0.25f, 0.18f, 0.50f, 0.90f}
    },
    {
        "DRIFT",
        "DRIFT — Movimento organico profundo e envolvente para pads, texturas e ambiencias.",
        {0.28f, 0.78f, 18.00f, 0.82f, 0.20f, 0.45f, 0.38f, 0.58f, 0.80f}
    }
}};

class QuickStartSelector final : public juce::Component {
public:
    explicit QuickStartSelector(juce::AudioProcessorValueTreeState& state);
    ~QuickStartSelector() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void applyPreset(std::size_t index);
    void syncHighlightWithParameters();
    int activePresetIndex() const noexcept { return currentActiveIndex; }

    static void applyPresetToState(juce::AudioProcessorValueTreeState& state, std::size_t index);

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::array<juce::TextButton, 3> buttons;
    int currentActiveIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuickStartSelector)
};

} // namespace drift::ui
