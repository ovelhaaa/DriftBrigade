#include "PluginEditor.h"
DriftEditor::DriftEditor(DriftProcessor& processor) : AudioProcessorEditor(processor) {
    for (std::size_t i=0; i<drift::Count; ++i) {
        const auto& spec = drift::parameterSpecs[i];
        sliders[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sliders[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 100, 24);
        sliders[i].setTextValueSuffix(juce::String(" ")+spec.unit);
        sliders[i].setTooltip(juce::String(spec.name)+" — "+spec.unit);
        sliders[i].setName(spec.name);
        labels[i].setText(spec.name, juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(sliders[i]); addAndMakeVisible(labels[i]);
        attachments[i] = std::make_unique<Attachment>(processor.state, spec.id, sliders[i]);
    }
    setResizable(true, true); setResizeLimits(540, 540, 1000, 900); setSize(660, 600);
}
void DriftEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1b2025));
    g.setColour(juce::Colours::whitesmoke); g.setFont(24.0f);
    g.drawText(drift::productName, 20, 12, getWidth()-40, 32, juce::Justification::centred);
    g.setFont(14.0f); g.setColour(juce::Colour(0xffb6c3cc));
    g.drawText("Multiscale Organic Modulator", 20, 45, getWidth()-40, 24, juce::Justification::centred);
}
void DriftEditor::resized() {
    auto area = getLocalBounds().reduced(16); area.removeFromTop(62);
    // Keep controls clear of JUCE's default lower-right attribution overlay.
    area.removeFromBottom(66);
    const int width = area.getWidth()/3, height = area.getHeight()/3;
    for (std::size_t i=0; i<drift::Count; ++i) {
        auto cell = juce::Rectangle<int>(area.getX()+static_cast<int>(i%3)*width,
                                       area.getY()+static_cast<int>(i/3)*height, width, height).reduced(8);
        labels[i].setBounds(cell.removeFromTop(24)); sliders[i].setBounds(cell);
    }
}
