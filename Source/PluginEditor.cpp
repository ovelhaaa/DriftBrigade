#include "PluginEditor.h"
#include <algorithm>
#include <array>

DriftEditor::DriftEditor(DriftProcessor& processor)
    : AudioProcessorEditor(processor),
      quickStartSelector(processor.state),
      motionField(processor.state,
                  [this] { quickStartSelector.syncHighlightWithParameters(); },
                  [&processor] { return processor.effectiveSampleRate(); }),
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

    // 1. Warm off-white precision instrument chassis (#F0EDE6)
    g.fillAll(Palette::chassisBg);
    g.setColour(Palette::chassisBorderCrisp);
    g.drawRect(getLocalBounds(), 1);

    // 2. Top header bar on off-white chassis
    const float headerH = std::clamp(bounds.getHeight() * 0.092f, 46.0f, 58.0f);
    g.setColour(Palette::chassisBorder);
    g.drawHorizontalLine(static_cast<int>(headerH), 16.0f, bounds.getWidth() - 16.0f);

    // Small orange registration bar on header divider below emblem
    g.setColour(Palette::accentOrange);
    g.fillRect(16.0f, headerH - 1.5f, 22.0f, 2.0f);

    // Brand emblem (4-band stereo geometric bars in Graphite #292B2D + Orange #F36B2B)
    const float emblemX = 16.0f;
    const float emblemCY = headerH * 0.48f;
    const std::array<float, 4> barHeights {11.0f, 19.0f, 23.0f, 15.0f};
    for (std::size_t i = 0; i < 4; ++i) {
        const float bx = emblemX + static_cast<float>(i) * 4.8f;
        const float bh = barHeights[i];
        g.setColour(i == 2 ? Palette::accentOrange : Palette::textOnChassis);
        g.fillRoundedRectangle(bx, emblemCY - bh * 0.5f, 3.0f, bh, 1.0f);
    }

    // Product title: DRIFT BRIGADE (Graphite #292B2D on Off-White #F0EDE6)
    const int titleX = 42;
    g.setFont(DriftLookAndFeel::makeTechnicalFont(std::clamp(headerH * 0.36f, 15.5f, 20.0f), true));
    g.setColour(Palette::textOnChassis);
    g.drawText(drift::productName, titleX, static_cast<int>(headerH * 0.11f),
               270, static_cast<int>(headerH * 0.46f), juce::Justification::centredLeft, false);

    // Subtitle: ORGANIC STEREO MOTION (with subtle orange index square)
    g.setFont(DriftLookAndFeel::makeTechnicalFont(std::clamp(headerH * 0.20f, 9.5f, 11.0f), true));
#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
    const auto& processor = static_cast<const DriftProcessor&>(*getAudioProcessor());
    using Outcome = drift::BBDPreparationOutcome;
    const auto outcome = processor.hostTestOutcome();
    juce::String status = outcome == Outcome::BBD ? "Balanced1024 admitted"
        : outcome == Outcome::FallbackToDigital ? "Digital fallback" : "Audio unavailable";
    if (!processor.hostTestParametersAccepted()) status += " | parameter snapshot rejected";
    g.setColour(Palette::accentOrange);
    g.drawText(status, titleX, static_cast<int>(headerH * 0.55f),
               360, static_cast<int>(headerH * 0.36f), juce::Justification::centredLeft, false);
#else
    const float subY = headerH * 0.56f;
    const float subH = headerH * 0.34f;
    g.setColour(Palette::accentOrange);
    g.fillRect(static_cast<float>(titleX), subY + (subH - 4.5f) * 0.5f, 4.5f, 4.5f);
    g.setColour(Palette::textOnChassisSecondary);
    g.drawText("ORGANIC STEREO MOTION", titleX + 9, static_cast<int>(subY),
               240, static_cast<int>(subH), juce::Justification::centredLeft, false);
#endif

    // 3. Footer technical bar on off-white chassis (60 px height keeps controls clear of JUCE attribution badge)
    const float footerH = 60.0f;
    const float footerY = bounds.getHeight() - footerH;
    g.setColour(Palette::chassisBorder);
    g.drawHorizontalLine(static_cast<int>(footerY), 16.0f, bounds.getWidth() - 16.0f);

    // Signal flow modules on the left side of the footer (leaving rightmost 155 px clear for JUCE badge)
    const float pillY = footerY + 11.0f;
    const float pillH = 22.0f;
    const bool compactFooter = bounds.getWidth() < 860.0f;
    const std::array<const char*, 4> chainSteps {
        compactFooter ? "4-BAND GENTLE (250/1k/4k)" : "GENTLE 4-BAND SPLIT (250 / 1k / 4k Hz)",
        compactFooter ? "WANDER MODULATOR" : "WANDER ORGANIC MODULATOR",
        compactFooter ? "8x FRACTIONAL DELAY" : "8x DIGITAL FRACTIONAL DELAY",
        compactFooter ? "DYNAMIC ENVELOPE" : "DYNAMIC WET PROMINENCE"
    };
    const float availFooterW = bounds.getWidth() - 172.0f;
    const float pillGap = 6.0f;
    const float boxW = std::clamp((availFooterW - pillGap * 3.0f) / 4.0f, 128.0f, 198.0f);
    float curX = 16.0f;
    g.setFont(DriftLookAndFeel::makeTechnicalFont(compactFooter ? 8.6f : 9.2f, true));
    for (std::size_t i = 0; i < chainSteps.size(); ++i) {
        const juce::String stepText(chainSteps[i]);
        if (curX + boxW > bounds.getWidth() - 152.0f) break;

        const juce::Rectangle<float> pill(curX, pillY, boxW, pillH);
        g.setColour(Palette::chassisSurface);
        g.fillRoundedRectangle(pill, 3.5f);
        g.setColour(Palette::chassisBorderCrisp);
        g.drawRoundedRectangle(pill, 3.5f, 1.0f);

        g.setColour(Palette::accentOrange);
        g.fillEllipse(pill.getX() + 6.5f, pill.getCentreY() - 2.3f, 4.6f, 4.6f);

        g.setColour(Palette::textOnChassis);
        g.drawFittedText(stepText,
                         juce::Rectangle<int>(static_cast<int>(pill.getX() + 14.0f), static_cast<int>(pill.getY()),
                                              static_cast<int>(pill.getWidth() - 17.0f), static_cast<int>(pill.getHeight())),
                         juce::Justification::centredLeft, 1, 0.85f);
        curX += boxW + pillGap;
    }

    g.setFont(DriftLookAndFeel::makeTechnicalFont(9.0f, false));
    g.setColour(Palette::textOnChassisMuted);
    const juce::String footerHelp =
        juce::String("DOUBLE-CLICK KNOB: RESET DEFAULT   |   CTRL/ALT + DRAG: FINE ADJUST   |   CLICK VALUE: NUMERIC INPUT   |   v")
        + drift::releaseVersion;
    g.drawText(footerHelp, 16, static_cast<int>(footerY + 37.0f),
               static_cast<int>(bounds.getWidth() - 172.0f), 16,
               juce::Justification::centredLeft, false);
}

void DriftEditor::resized() {
    auto area = getLocalBounds();
    const int headerH = std::clamp(static_cast<int>(area.getHeight() * 0.092f), 46, 58);
    auto headerArea = area.removeFromTop(headerH).reduced(16, 0);

    // QuickStart selector placed on the right side of the off-white header
    const int qsWidth = std::clamp(static_cast<int>(headerArea.getWidth() * 0.38f), 268, 340);
    const int qsHeight = std::clamp(headerH - 18, 26, 32);
    quickStartSelector.setBounds(headerArea.getRight() - qsWidth,
                                 (headerH - qsHeight) / 2,
                                 qsWidth, qsHeight);

    // Keep all controls clear of the 60 px footer and JUCE's bottom-right attribution overlay
    area.removeFromBottom(60);

    // Visible off-white chassis margins and breathing room around dark modules
    auto body = area.reduced(16, 10);

    // Central Motion Field takes ~46% of body height
    const int fieldH = std::clamp(static_cast<int>(body.getHeight() * 0.46f), 150, 400);
    motionField.setBounds(body.removeFromTop(fieldH));

    // Off-white chassis separation between Motion Field and the three parameter bays
    body.removeFromTop(10);

    // Three dark parameter bays separated by visible off-white channels
    const int gap = 12;
    const int bayW = (body.getWidth() - gap * 2) / 3;
    movementSection.setBounds(body.removeFromLeft(bayW));
    body.removeFromLeft(gap);
    spaceSection.setBounds(body.removeFromLeft(bayW));
    body.removeFromLeft(gap);
    characterSection.setBounds(body);
}
