#include "PluginEditor.h"

DriftEditor::DriftEditor(DriftProcessor& processor)
    : AudioProcessorEditor(processor),
      quickStartSelector(processor.state),
      motionField(processor.state, [this] { quickStartSelector.syncHighlightWithParameters(); }),
      movementSection(processor.state, "MOVEMENT", "RATE / EXCURSION / DRIFT",
                      {drift::Motion, drift::Depth, drift::Chaos}, 0),
      spaceSection(processor.state, "SPACE", "DELAY / BANDS / STEREO",
                   {drift::Center, drift::Coherence, drift::Width}, 1),
      characterSection(processor.state, "CHARACTER", "ENVELOPE / LOOP / BLEND",
                       {drift::Dynamics, drift::Feedback, drift::Mix}, 2) {
    setLookAndFeel(&lookAndFeel);
    tooltipWindow.setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(quickStartSelector);
    addAndMakeVisible(motionField);
    addAndMakeVisible(movementSection);
    addAndMakeVisible(spaceSection);
    addAndMakeVisible(characterSection);

    setResizable(true, true);
    setResizeLimits(760, 520, 1600, 1000);
    setSize(960, 600);

#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
    startTimerHz(4);
#endif
}

DriftEditor::~DriftEditor() {
#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
    stopTimer();
#endif
    tooltipWindow.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

drift::ui::DriftKnob& DriftEditor::knobFor(drift::Param param) noexcept {
    switch (param) {
        case drift::Motion:    return movementSection.knobAt(0);
        case drift::Depth:     return movementSection.knobAt(1);
        case drift::Chaos:     return movementSection.knobAt(2);
        case drift::Center:    return spaceSection.knobAt(0);
        case drift::Coherence: return spaceSection.knobAt(1);
        case drift::Width:     return spaceSection.knobAt(2);
        case drift::Dynamics:  return characterSection.knobAt(0);
        case drift::Feedback:  return characterSection.knobAt(1);
        case drift::Mix:
        case drift::Count:
        default:               return characterSection.knobAt(2);
    }
}

void DriftEditor::paint(juce::Graphics& g) {
    using namespace drift::ui;
    const auto bounds = getLocalBounds().toFloat();

    // 1. Chassis dark graphite gradient background
    juce::ColourGradient bgGrad(Palette::chassisTop, 0.0f, 0.0f,
                                Palette::chassisBottom, 0.0f, bounds.getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // 2. Top header bar
    const float headerH = std::clamp(bounds.getHeight() * 0.092f, 44.0f, 58.0f);
    const juce::Rectangle<float> headerRect(0.0f, 0.0f, bounds.getWidth(), headerH);
    g.setColour(Palette::headerBg);
    g.fillRect(headerRect);
    g.setColour(Palette::borderSubtle);
    g.drawHorizontalLine(static_cast<int>(headerH), 0.0f, bounds.getWidth());

    // Brand emblem (4-band stereo geometric bars)
    const float emblemX = 16.0f;
    const float emblemCY = headerH * 0.5f;
    const std::array<float, 4> barHeights {10.0f, 18.0f, 14.0f, 22.0f};
    for (std::size_t i = 0; i < 4; ++i) {
        const float bx = emblemX + static_cast<float>(i) * 4.5f;
        const float bh = barHeights[i];
        g.setColour(i < 2 ? Palette::accentCyan : Palette::accentAmber);
        g.fillRoundedRectangle(bx, emblemCY - bh * 0.5f, 2.8f, bh, 1.2f);
    }

    // Product title: DRIFT BRIGADE
    const int titleX = 40;
    g.setFont(DriftLookAndFeel::makeTechnicalFont(std::clamp(headerH * 0.36f, 15.0f, 20.0f), true));
    g.setColour(Palette::textPrimary);
    g.drawText(drift::productName, titleX, static_cast<int>(headerH * 0.12f),
               260, static_cast<int>(headerH * 0.46f), juce::Justification::centredLeft, false);

    // Subtitle: ORGANIC STEREO MOTION (or internal BBD host status when compiled with DRIFT_INTERNAL_BBD_HOST_TEST)
    g.setFont(DriftLookAndFeel::makeTechnicalFont(std::clamp(headerH * 0.21f, 9.5f, 11.5f), true));
#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
    const auto& processor = static_cast<const DriftProcessor&>(*getAudioProcessor());
    using Outcome = drift::BBDPreparationOutcome;
    const auto outcome = processor.hostTestOutcome();
    juce::String status = outcome == Outcome::BBD ? "Balanced1024 admitted"
        : outcome == Outcome::FallbackToDigital ? "Digital fallback" : "Audio unavailable";
    if (!processor.hostTestParametersAccepted()) status += " | parameter snapshot rejected";
    g.setColour(Palette::accentAmber);
    g.drawText(status, titleX, static_cast<int>(headerH * 0.54f),
               360, static_cast<int>(headerH * 0.36f), juce::Justification::centredLeft, false);
#else
    g.setColour(Palette::accentCyan);
    g.drawText("ORGANIC STEREO MOTION", titleX, static_cast<int>(headerH * 0.54f),
               240, static_cast<int>(headerH * 0.36f), juce::Justification::centredLeft, false);
#endif

    // 3. Footer technical bar (60 px height keeps controls clear of JUCE's bottom-right attribution badge)
    const float footerH = 60.0f;
    const float footerY = bounds.getHeight() - footerH;
    g.setColour(Palette::headerBg.withAlpha(0.75f));
    g.fillRect(0.0f, footerY, bounds.getWidth(), footerH);
    g.setColour(Palette::borderSubtle.withAlpha(0.8f));
    g.drawHorizontalLine(static_cast<int>(footerY), 14.0f, bounds.getWidth() - 14.0f);

    // Signal flow pills on the left side of the footer (leaving rightmost 155 px clear for JUCE badge)
    const float pillY = footerY + 12.0f;
    const float pillH = 22.0f;
    const std::array<const char*, 4> chainSteps {
        "GENTLE 4-BAND SPLIT (250 / 1k / 4k Hz)",
        "WANDER ORGANIC MODULATOR",
        "8x DIGITAL FRACTIONAL DELAY",
        "DYNAMIC WET PROMINENCE"
    };
    float curX = 16.0f;
    g.setFont(DriftLookAndFeel::makeTechnicalFont(9.5f, true));
    for (std::size_t i = 0; i < chainSteps.size(); ++i) {
        const juce::String stepText(chainSteps[i]);
        const float boxW = std::clamp(bounds.getWidth() * 0.19f, 138.0f, 200.0f);
        if (curX + boxW > bounds.getWidth() - 155.0f) break;

        const juce::Rectangle<float> pill(curX, pillY, boxW, pillH);
        g.setColour(Palette::panelBg);
        g.fillRoundedRectangle(pill, 4.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(pill, 4.0f, 1.0f);

        g.setColour(i % 2 == 0 ? Palette::accentCyan : Palette::accentAmber);
        g.fillEllipse(pill.getX() + 7.0f, pill.getCentreY() - 2.5f, 5.0f, 5.0f);

        g.setColour(Palette::textSecondary);
        g.drawFittedText(stepText,
                         juce::Rectangle<int>(static_cast<int>(pill.getX() + 16.0f), static_cast<int>(pill.getY()),
                                              static_cast<int>(pill.getWidth() - 20.0f), static_cast<int>(pill.getHeight())),
                         juce::Justification::centredLeft, 1, 0.8f);
        curX += boxW + 8.0f;
    }

    g.setFont(DriftLookAndFeel::makeTechnicalFont(9.0f, false));
    g.setColour(Palette::textMuted);
    g.drawText("DOUBLE-CLICK KNOB: RESET DEFAULT   |   CTRL/ALT + DRAG: FINE ADJUST   |   CLICK VALUE: NUMERIC INPUT",
               16, static_cast<int>(footerY + 38.0f), static_cast<int>(bounds.getWidth() - 170.0f),
               16, juce::Justification::centredLeft, false);
}

void DriftEditor::resized() {
    auto area = getLocalBounds();
    const int headerH = std::clamp(static_cast<int>(area.getHeight() * 0.09f), 44, 56);
    auto headerArea = area.removeFromTop(headerH).reduced(14, 0);

    // QuickStart selector placed on the right side of the header
    const int qsWidth = std::clamp(static_cast<int>(headerArea.getWidth() * 0.34f), 240, 320);
    const int qsHeight = std::clamp(headerH - 18, 24, 30);
    quickStartSelector.setBounds(headerArea.getRight() - qsWidth,
                                 (headerH - qsHeight) / 2,
                                 qsWidth, qsHeight);

    // Keep all controls clear of the 60 px footer and JUCE's bottom-right attribution overlay
    area.removeFromBottom(60);
    auto body = area.reduced(14, 8);

    // Central Motion Field takes ~46% of body height
    const int fieldH = std::clamp(static_cast<int>(body.getHeight() * 0.46f), 150, 400);
    motionField.setBounds(body.removeFromTop(fieldH));

    body.removeFromTop(8);

    // Three musical parameter bays side by side
    const int gap = 10;
    const int bayW = (body.getWidth() - gap * 2) / 3;
    movementSection.setBounds(body.removeFromLeft(bayW));
    body.removeFromLeft(gap);
    spaceSection.setBounds(body.removeFromLeft(bayW));
    body.removeFromLeft(gap);
    characterSection.setBounds(body);
}
