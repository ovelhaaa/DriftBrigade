#include "ui/DriftKnob.h"
#include "ui/DriftLookAndFeel.h"

namespace drift::ui {

juce::String DriftKnob::descriptionFor(Param param) {
    switch (param) {
        case Motion:
            return "Motion (0.05 - 10.00 Hz): Velocidade base das quatro trajetorias de modulacao.";
        case Depth:
            return "Depth (0 - 100 %): Amplitude do deslocamento temporal guiada pela taxa de Motion.";
        case Center:
            return "Center (0.30 - 30.00 ms): Atraso base das vozes, do flange curto ao chorus espacoso.";
        case Chaos:
            return "Chaos (0 - 100 %): Transicao continua entre movimento senoidal e deriva organica Wander.";
        case Coherence:
            return "Coherence (0 - 100 %): Grau de acoplamento entre o movimento das quatro bandas.";
        case Dynamics:
            return "Dynamics (0 - 100 %): Resposta ao envelope de entrada para expandir movimento e preservar clareza.";
        case Feedback:
            return "Feedback (0 - 65 %): Realimentacao das linhas de atraso para maior densidade e ressonancia.";
        case Mix:
            return "Mix (0 - 100 %): Equilibrio entre o sinal original seco (Dry) e o sinal processado (Wet).";
        case Width:
            return "Width (0 - 100 %): Decorrelacao e abertura estereo entre os canais esquerdo e direito.";
        case Count:
        default:
            return {};
    }
}

DriftKnob::DriftKnob(juce::AudioProcessorValueTreeState& state, Param param, int accentGroup)
    : paramIndex(param), groupIndex(accentGroup) {
    const auto& spec = parameterSpecs[static_cast<std::size_t>(param)];
    const auto tooltipText = descriptionFor(param);

    titleLabel.setText(juce::String(spec.name).toUpperCase(), juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, Palette::textSecondary);
    titleLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(titleLabel);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 20);
    slider.setTextValueSuffix(juce::String(" ") + spec.unit);
    slider.setName(spec.name);
    slider.setTooltip(tooltipText);
    slider.setSliderSnapsToMousePosition(false);
    slider.setVelocityModeParameters(1.0, 1, 0.08, true, juce::ModifierKeys::ctrlAltCommandModifiers);
    slider.getProperties().set("accentGroup", groupIndex);

    if (const auto* rawParam = state.getParameter(spec.id)) {
        slider.getProperties().set("defaultNorm", rawParam->convertTo0to1(spec.initial));
    }

    addAndMakeVisible(slider);
    attachment = std::make_unique<Attachment>(state, spec.id, slider);
    slider.setDoubleClickReturnValue(true, static_cast<double>(spec.initial));
}

void DriftKnob::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f, 1.0f);
    if (slider.isMouseOverOrDragging()) {
        g.setColour(Palette::panelElevated.withAlpha(0.32f));
        g.fillRoundedRectangle(bounds, 5.0f);
    }
}

void DriftKnob::resized() {
    auto area = getLocalBounds().reduced(2);
    const int labelH = std::clamp( static_cast<int>(area.getHeight() * 0.14f), 15, 22);
    const int textBoxH = std::clamp(static_cast<int>(area.getHeight() * 0.14f), 17, 22);
    const int textBoxW = std::clamp(area.getWidth() - 8, 54, 88);

    titleLabel.setFont(DriftLookAndFeel::makeTechnicalFont(std::clamp(labelH * 0.64f, 10.0f, 12.5f), true));
    titleLabel.setBounds(area.removeFromTop(labelH));

    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
    slider.setBounds(area);
}

} // namespace drift::ui
