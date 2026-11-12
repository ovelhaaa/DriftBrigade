#include "ui/MotionField.h"
#include "ui/DriftLookAndFeel.h"
#include "dsp/DepthMapping.h"
#include "dsp/DriftEngine.h"
#include <cmath>

namespace drift::ui {

namespace {
constexpr std::array<double, 4> kBandIndepRates {0.91, 1.037, 1.083, 0.967};
constexpr std::array<double, 4> kBandStereoRates {1.113, 0.943, 1.057, 0.887};
constexpr std::array<const char*, 4> kBandNames {"B1 LOW", "B2 LO-MID", "B3 HI-MID", "B4 HIGH"};

double hashTarget(std::uint32_t seed, std::int64_t segmentIndex) noexcept {
    std::uint64_t x = static_cast<std::uint64_t>(segmentIndex) * 0x9E3779B97F4A7C15ull
                    ^ (static_cast<std::uint64_t>(seed) << 1);
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    x = x ^ (x >> 31);
    const double u = static_cast<double>(x & 0xFFFFFFFFull) / 4294967295.0;
    return 2.0 * u - 1.0;
}

bool parametersDiffer(const EngineParameters& a, const EngineParameters& b) noexcept {
    for (std::size_t i = 0; i < Count; ++i) {
        if (std::abs(a.values[i] - b.values[i]) > 1e-5)
            return true;
    }
    return false;
}
} // namespace

MotionField::MotionField(juce::AudioProcessorValueTreeState& state,
                         std::function<void()> onParametersChanged)
    : reader(state),
      snapshot(reader.read()),
      parameterChangeCallback(std::move(onParametersChanged)) {
    freezeButton.setButtonText("LIVE");
    freezeButton.setClickingTogglesState(true);
    freezeButton.setToggleState(true, juce::dontSendNotification);
    freezeButton.setTooltip("Alterna entre animacao em tempo real (30 FPS) e visualizacao estatica.");
    freezeButton.onClick = [this] {
        setAnimationEnabled(freezeButton.getToggleState());
    };
    addAndMakeVisible(freezeButton);

    recomputeTrajectories();
    startTimerHz(30);
}

MotionField::~MotionField() {
    stopTimer();
}

void MotionField::setAnimationEnabled(bool enabled) {
    animationEnabled = enabled;
    freezeButton.setToggleState(enabled, juce::dontSendNotification);
    freezeButton.setButtonText(enabled ? "LIVE" : "HOLD");
    if (enabled && isShowing())
        startTimerHz(30);
    else
        stopTimer();
    repaint();
}

void MotionField::visibilityChanged() {
    if (isShowing() && animationEnabled)
        startTimerHz(30);
    else
        stopTimer();
}

double MotionField::evaluateWander(std::uint32_t seed, double cycles, double chaos) noexcept {
    const double c = std::clamp(chaos, 0.0, 1.0);
    const double sine = std::sin(2.0 * 3.14159265358979323846 * cycles);
    const double segPos = 2.0 * cycles;
    const double segFloor = std::floor(segPos);
    const auto segIdx = static_cast<std::int64_t>(segFloor);
    const double frac = segPos - segFloor;
    const double rc = 0.5 - 0.5 * std::cos(3.14159265358979323846 * frac);
    const double startVal = hashTarget(seed, segIdx);
    const double endVal = hashTarget(seed, segIdx + 1);
    const double randomVal = startVal + (endVal - startVal) * rc;
    const double norm = std::sqrt(0.5 / (0.5 * (1.0 - c) * (1.0 - c) + 0.25 * c * c));
    return ((1.0 - c) * sine + c * randomVal) * norm;
}

void MotionField::stepSimulation(double deltaSeconds) noexcept {
    const auto latest = reader.read();
    const bool changed = parametersDiffer(snapshot, latest);
    snapshot = latest;
    if (changed && parameterChangeCallback)
        parameterChangeCallback();

    if (animationEnabled) {
        // Perceptual visual scroll rate: smooth and readable from 0.05 Hz to 10 Hz at 30 FPS
        const double motionHz = std::clamp(snapshot[Motion], 0.05, 10.0);
        const double visualCyclesPerSec = 0.18 + 1.35 * std::pow(motionHz / 10.0, 0.55);
        phaseCycles += deltaSeconds * visualCyclesPerSec;
        if (phaseCycles > 10000.0)
            phaseCycles -= 10000.0;
    }

    recomputeTrajectories();
}

void MotionField::timerCallback() {
    if (!isShowing()) return;
    stepSimulation(1.0 / 30.0);
    repaint();
}

void MotionField::recomputeTrajectories() noexcept {
    const double motion = std::clamp(snapshot[Motion], 0.05, 10.0);
    const double depth = std::clamp(snapshot[Depth], 0.0, 1.0);
    const double centerMs = std::clamp(snapshot[Center], 0.3, 30.0);
    const double chaos = std::clamp(snapshot[Chaos], 0.0, 1.0);
    const double coherence = std::clamp(snapshot[Coherence], 0.0, 1.0);
    const double dynamics = std::clamp(snapshot[Dynamics], 0.0, 1.0);
    const double feedback = std::clamp(snapshot[Feedback], 0.0, 0.65);
    const double width = std::clamp(snapshot[Width], 0.0, 1.0);

    // Effective chaos includes a subtle representation of dynamic sensitivity
    const double effectiveChaos = std::clamp(chaos + 0.12 * dynamics, 0.0, 1.0);
    const double indepWeight = std::sqrt(std::max(0.0, 1.0 - coherence * coherence));
    const double stereoShared = 1.0 - 0.35 * width;
    const double stereoWeight = std::sqrt(std::max(0.0, 1.0 - stereoShared * stereoShared));

    // Visual spatial wave density across the horizontal history window
    const double windowCycles = 0.85 + 2.85 * std::pow((motion - 0.05) / 9.95, 0.48);
    // Center shifts baseline phase offset subtly and controls lane spacing
    const double centerNorm = std::log(centerMs / 0.3) / std::log(30.0 / 0.3);
    const double laneSpread = (0.14 + 0.42 * (1.0 - coherence)) * (0.78 + 0.32 * centerNorm);
    const double excursionAmp = depth * (0.34 + 0.38 * (1.0 - 0.25 * std::pow(motion / 10.0, 0.4)));
    const double stereoLaneOffset = width * 0.14;
    const double feedbackPhaseLag = 0.16 + 0.22 * (feedback / 0.65);

    for (std::size_t i = 0; i < kHistoryPoints; ++i) {
        const double tNorm = static_cast<double>(i) / static_cast<double>(kHistoryPoints - 1);
        const double localPhase = phaseCycles + (tNorm - 1.0) * windowCycles + centerNorm * 0.35;
        const double echoPhase = localPhase - feedbackPhaseLag;

        const double commonVal = evaluateWander(0x44524946u, localPhase, effectiveChaos);
        const double commonEcho = evaluateWander(0x44524946u, echoPhase, effectiveChaos);

        for (std::size_t b = 0; b < kNumBands; ++b) {
            const double bandIndexCentered = (static_cast<double>(b) - 1.5) / 1.5; // -1, -1/3, +1/3, +1
            const double bandBaseline = bandIndexCentered * laneSpread;

            const std::uint32_t indepSeed = 0x44524946u ^ (0x9E3779B9u * static_cast<std::uint32_t>(b + 1));
            const std::uint32_t sideSeed  = 0x44524946u ^ (0x85EBCA6Bu * static_cast<std::uint32_t>(b + 5));

            const double sepVal = evaluateWander(indepSeed, localPhase * kBandIndepRates[b] + 0.19 * static_cast<double>(b), effectiveChaos);
            const double sideVal = evaluateWander(sideSeed, localPhase * kBandStereoRates[b] + 0.31 * static_cast<double>(b), effectiveChaos);

            const double baseMod = coherence * commonVal + indepWeight * sepVal;
            const double rightMod = stereoShared * baseMod + stereoWeight * sideVal;

            // Normalize by conservative Wander bound (~1.732) so values fit [-1, +1]
            const double modL = std::clamp(baseMod / 1.85, -1.0, 1.0);
            const double modR = std::clamp(rightMod / 1.85, -1.0, 1.0);

            const double yL = std::clamp(bandBaseline - stereoLaneOffset + modL * excursionAmp, -0.94, 0.94);
            const double yR = std::clamp(bandBaseline + stereoLaneOffset + modR * excursionAmp, -0.94, 0.94);

            trajectories[0][b][i] = static_cast<float>(yL);
            trajectories[1][b][i] = static_cast<float>(yR);

            // Feedback echo trail
            const double sepEcho = evaluateWander(indepSeed, echoPhase * kBandIndepRates[b] + 0.19 * static_cast<double>(b), effectiveChaos);
            const double sideEcho = evaluateWander(sideSeed, echoPhase * kBandStereoRates[b] + 0.31 * static_cast<double>(b), effectiveChaos);
            const double baseEcho = coherence * commonEcho + indepWeight * sepEcho;
            const double rightEcho = stereoShared * baseEcho + stereoWeight * sideEcho;
            const double echoScale = excursionAmp * (0.55 + 0.35 * (feedback / 0.65));

            feedbackTrails[0][b][i] = static_cast<float>(std::clamp(
                bandBaseline - stereoLaneOffset + (baseEcho / 1.85) * echoScale, -0.94, 0.94));
            feedbackTrails[1][b][i] = static_cast<float>(std::clamp(
                bandBaseline + stereoLaneOffset + (rightEcho / 1.85) * echoScale, -0.94, 0.94));
        }
    }
}

void MotionField::resized() {
    const int btnW = 50;
    const int btnH = 20;
    freezeButton.setBounds(getWidth() - btnW - 10, 6, btnW, btnH);
}

void MotionField::paint(juce::Graphics& g) {
    const auto outer = getLocalBounds().toFloat().reduced(1.0f);
    constexpr float corner = 6.0f;

    // 1. Viewport recessed frame
    juce::ColourGradient bgGrad(Palette::viewportBg, outer.getX(), outer.getY(),
                                Palette::viewportInner, outer.getX(), outer.getBottom(), false);
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(outer, corner);

    g.setColour(Palette::borderCrisp);
    g.drawRoundedRectangle(outer, corner, 1.0f);

    const float headerH = 28.0f;
    const auto headerArea = outer.withHeight(headerH);
    g.setColour(Palette::panelBg.withAlpha(0.75f));
    g.fillRoundedRectangle(headerArea.getX() + 1.0f, headerArea.getY() + 1.0f,
                           headerArea.getWidth() - 2.0f, headerH, corner);
    g.setColour(Palette::borderSubtle);
    g.drawHorizontalLine(static_cast<int>(outer.getY() + headerH), outer.getX() + 1.0f, outer.getRight() - 1.0f);

    // Header title + honest parametric tag
    g.setColour(Palette::accentCyan);
    g.fillRoundedRectangle(outer.getX() + 10.0f, outer.getY() + 8.0f, 3.0f, 12.0f, 1.5f);

    g.setFont(DriftLookAndFeel::makeTechnicalFont(11.5f, true));
    g.setColour(Palette::textPrimary);
    g.drawText("MOTION FIELD", static_cast<int>(outer.getX() + 18.0f), static_cast<int>(outer.getY()),
               110, static_cast<int>(headerH), juce::Justification::centredLeft, false);

    g.setFont(DriftLookAndFeel::makeTechnicalFont(10.0f, false));
    g.setColour(Palette::textMuted);
    const juce::String subHeader = getWidth() < 860 ? "4-BAND STEREO TRAJECTORY"
                                                    : "PARAMETRIC 4-BAND STEREO TRAJECTORY";
    const int subHeaderW = getWidth() < 860 ? 155 : 240;
    g.drawText(subHeader, static_cast<int>(outer.getX() + 122.0f),
               static_cast<int>(outer.getY()), subHeaderW, static_cast<int>(headerH),
               juce::Justification::centredLeft, false);

    // Live parametric readout summary in header (right side before LIVE button)
    const double motionHz = snapshot[Motion];
    const double depthVal = snapshot[Depth];
    const double centerMs = snapshot[Center];
    const double feedbackVal = snapshot[Feedback];
    const double mixVal = snapshot[Mix];
    const double widthVal = snapshot[Width];
    const double dynamicsVal = snapshot[Dynamics];

    const double requestedExcursionSec = depthSeconds(motionHz, depthVal, true);
    const double safetyMaxSec = std::max(0.0, (centerMs * 0.001 - 4.0 / 48000.0) * 0.85
                                         / DriftEngine::combinedModulationBound(OrganicVariant::Wander));
    const double effectiveExcursionMs = std::min(requestedExcursionSec, safetyMaxSec) * 1000.0;

    const juce::String telemetryText =
        juce::String("CENTER ") + juce::String(centerMs, 2) + " ms  |  "
        + "EXCURSION +/-" + juce::String(effectiveExcursionMs, 2) + " ms  |  "
        + "L: CYAN  R: AMBER";

    g.setFont(DriftLookAndFeel::makeTechnicalFont(10.0f, true));
    g.setColour(Palette::textSecondary);
    const int telemetryLeft = static_cast<int>(outer.getX()) + 126 + subHeaderW;
    const int telemetryRight = getWidth() - 68;
    if (telemetryRight > telemetryLeft + 80) {
        g.drawFittedText(telemetryText, telemetryLeft, static_cast<int>(outer.getY()),
                         telemetryRight - telemetryLeft, static_cast<int>(headerH),
                         juce::Justification::centredRight, 1, 0.85f);
    }

    // Split main canvas into Left (76% Scrolling Multiband Trajectory Field) & Right (24% Stereo & Delay Horizon Scope)
    const auto bodyArea = outer.withTrimmedTop(headerH + 1.0f).reduced(8.0f, 6.0f);
    if (bodyArea.getWidth() < 120.0f || bodyArea.getHeight() < 60.0f) return;

    const float scopeWidth = std::clamp(bodyArea.getWidth() * 0.235f, 155.0f, 235.0f);
    const auto waveArea = bodyArea.withTrimmedRight(scopeWidth + 8.0f);
    const auto scopeArea = bodyArea.withLeft(waveArea.getRight() + 8.0f);

    // Vertical separator between wave field and stereo/delay scope
    g.setColour(Palette::borderSubtle);
    g.drawVerticalLine(static_cast<int>(waveArea.getRight() + 4.0f), bodyArea.getY(), bodyArea.getBottom());

    // =========================================================================
    // A. LEFT REGION: 4-BAND STEREO TRAJECTORY CANVAS
    // =========================================================================
    {
        // Technical graticule lines
        g.setColour(Palette::gridLine);
        for (int row = 1; row <= 3; ++row) {
            if (row == 2) continue;
            const float y = waveArea.getY() + waveArea.getHeight() * (static_cast<float>(row) / 4.0f);
            g.drawHorizontalLine(static_cast<int>(y), waveArea.getX(), waveArea.getRight());
        }
        constexpr int vDivs = 8;
        for (int col = 1; col < vDivs; ++col) {
            const float x = waveArea.getX() + waveArea.getWidth() * (static_cast<float>(col) / static_cast<float>(vDivs));
            g.drawVerticalLine(static_cast<int>(x), waveArea.getY(), waveArea.getBottom());
        }

        const float midY = waveArea.getCentreY();
        const float halfH = waveArea.getHeight() * 0.46f;

        // Dry reference horizon line at center (prominent when Mix is low)
        const float dryAlpha = static_cast<float>(0.25 + 0.65 * (1.0 - mixVal));
        g.setColour(Palette::textSecondary.withAlpha(dryAlpha));
        const float dashLengths[2] = {4.0f, 4.0f};
        g.drawDashedLine(juce::Line<float>(waveArea.getX(), midY, waveArea.getRight(), midY),
                         dashLengths, 2, 1.2f);

        // Dynamic sensitivity envelope corridor (honest parametric representation of Dynamics)
        if (dynamicsVal > 0.01) {
            const float dynBandHalfH = static_cast<float>(dynamicsVal) * halfH * 0.55f;
            const juce::Rectangle<float> dynRect(waveArea.getX(), midY - dynBandHalfH,
                                                 waveArea.getWidth(), dynBandHalfH * 2.0f);
            juce::ColourGradient dynGrad(Palette::accentCyan.withAlpha(0.0f), waveArea.getX(), midY - dynBandHalfH,
                                         Palette::accentCyan.withAlpha(static_cast<float>(0.07 * dynamicsVal)),
                                         waveArea.getX(), midY, false);
            g.setGradientFill(dynGrad);
            g.fillRect(dynRect);

            g.setColour(Palette::accentCyan.withAlpha(static_cast<float>(0.16 * dynamicsVal)));
            g.drawHorizontalLine(static_cast<int>(midY - dynBandHalfH), waveArea.getX(), waveArea.getRight());
            g.drawHorizontalLine(static_cast<int>(midY + dynBandHalfH), waveArea.getX(), waveArea.getRight());
        }

        // Wet prominence factor from Mix
        const float wetAlpha = static_cast<float>(0.18 + 0.82 * mixVal);
        const float fbNorm = static_cast<float>(feedbackVal / 0.65);

        // Band colors for Left (Mineral Cyan / Mint shades) and Right (Warm Amber / Gold shades)
        const std::array<juce::Colour, kNumBands> leftColors {{
            juce::Colour(0xff26b5aa),
            juce::Colour(0xff38d4c8),
            juce::Colour(0xff5ce0b8),
            juce::Colour(0xff7ce8dc)
        }};
        const std::array<juce::Colour, kNumBands> rightColors {{
            juce::Colour(0xffcc8838),
            juce::Colour(0xffe5a44c),
            juce::Colour(0xffedb868),
            juce::Colour(0xfff4ca85)
        }};

        // Draw feedback ghost trails first (when Feedback > 2%)
        if (fbNorm > 0.02f) {
            for (std::size_t b = 0; b < kNumBands; ++b) {
                for (std::size_t ch = 0; ch < 2; ++ch) {
                    if (ch == 1 && widthVal < 0.01) continue;
                    juce::Path trailPath;
                    for (std::size_t i = 0; i < kHistoryPoints; ++i) {
                        const float px = waveArea.getX() + (static_cast<float>(i) / static_cast<float>(kHistoryPoints - 1)) * waveArea.getWidth();
                        const float py = midY - feedbackTrails[ch][b][i] * halfH;
                        if (i == 0) trailPath.startNewSubPath(px, py);
                        else trailPath.lineTo(px, py);
                    }
                    const auto baseCol = (ch == 0) ? leftColors[b] : rightColors[b];
                    g.setColour(baseCol.withAlpha(wetAlpha * fbNorm * 0.34f));
                    g.strokePath(trailPath, juce::PathStrokeType(1.1f + fbNorm * 0.9f,
                                                                 juce::PathStrokeType::curved,
                                                                 juce::PathStrokeType::rounded));
                }
            }
        }

        // Draw primary Left and Right band trajectories + stereo ribbon fill between L and R
        for (std::size_t b = 0; b < kNumBands; ++b) {
            juce::Path leftPath;
            juce::Path rightPath;
            juce::Path ribbonPath;

            for (std::size_t i = 0; i < kHistoryPoints; ++i) {
                const float px = waveArea.getX() + (static_cast<float>(i) / static_cast<float>(kHistoryPoints - 1)) * waveArea.getWidth();
                const float pyL = midY - trajectories[0][b][i] * halfH;
                const float pyR = midY - trajectories[1][b][i] * halfH;
                if (i == 0) {
                    leftPath.startNewSubPath(px, pyL);
                    rightPath.startNewSubPath(px, pyR);
                    ribbonPath.startNewSubPath(px, pyL);
                } else {
                    leftPath.lineTo(px, pyL);
                    rightPath.lineTo(px, pyR);
                    ribbonPath.lineTo(px, pyL);
                }
            }

            if (widthVal > 0.02) {
                for (std::size_t i = kHistoryPoints; i > 0; --i) {
                    const std::size_t idx = i - 1;
                    const float px = waveArea.getX() + (static_cast<float>(idx) / static_cast<float>(kHistoryPoints - 1)) * waveArea.getWidth();
                    const float pyR = midY - trajectories[1][b][idx] * halfH;
                    ribbonPath.lineTo(px, pyR);
                }
                ribbonPath.closeSubPath();
                g.setColour(leftColors[b].interpolatedWith(rightColors[b], 0.5f)
                                         .withAlpha(wetAlpha * static_cast<float>(0.04 + 0.06 * widthVal)));
                g.fillPath(ribbonPath);
            }

            // Right channel trajectory (Amber)
            if (widthVal > 0.005) {
                g.setColour(rightColors[b].withAlpha(wetAlpha * 0.82f));
                g.strokePath(rightPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
            }

            // Left channel trajectory (Cyan)
            g.setColour(leftColors[b].withAlpha(wetAlpha * 0.94f));
            g.strokePath(leftPath, juce::PathStrokeType(1.75f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            // Leading head nodes at the right edge of the trajectory window
            const float headX = waveArea.getRight() - 2.0f;
            const float headYL = midY - trajectories[0][b][kHistoryPoints - 1] * halfH;
            const float headYR = midY - trajectories[1][b][kHistoryPoints - 1] * halfH;

            if (widthVal > 0.01) {
                g.setColour(rightColors[b].withAlpha(wetAlpha * 0.9f));
                g.fillEllipse(headX - 3.0f, headYR - 3.0f, 6.0f, 6.0f);
            }
            g.setColour(leftColors[b].withAlpha(wetAlpha));
            g.fillEllipse(headX - 3.4f, headYL - 3.4f, 6.8f, 6.8f);
        }

        // Left-margin band labels with dark pill backdrop & Mix=0 indicator
        g.setFont(DriftLookAndFeel::makeTechnicalFont(9.0f, true));
        for (std::size_t b = 0; b < kNumBands; ++b) {
            const float labelY = waveArea.getY() + 4.0f + static_cast<float>(3 - b) * (waveArea.getHeight() - 20.0f) / 3.0f;
            const juce::Rectangle<float> tagRect(waveArea.getX() + 4.0f, labelY, 64.0f, 14.0f);
            g.setColour(Palette::viewportBg.withAlpha(0.84f));
            g.fillRoundedRectangle(tagRect, 3.0f);
            g.setColour(leftColors[b].withAlpha(0.88f));
            g.drawText(kBandNames[b], tagRect.reduced(4.0f, 0.0f).toNearestInt(),
                       juce::Justification::centredLeft, false);
        }

        if (mixVal < 0.01) {
            g.setColour(Palette::panelElevated.withAlpha(0.88f));
            const juce::Rectangle<float> dryBadge(waveArea.getCentreX() - 68.0f, midY - 11.0f, 136.0f, 22.0f);
            g.fillRoundedRectangle(dryBadge, 4.0f);
            g.setColour(Palette::borderCrisp);
            g.drawRoundedRectangle(dryBadge, 4.0f, 1.0f);
            g.setColour(Palette::textPrimary);
            g.setFont(DriftLookAndFeel::makeTechnicalFont(10.0f, true));
            g.drawText("MIX 0% (DRY REFERENCE)", dryBadge.toNearestInt(), juce::Justification::centred, false);
        }
    }

    // =========================================================================
    // B. RIGHT REGION: STEREO BAND ORBIT + DELAY HORIZON BAR
    // =========================================================================
    {
        const float meterBarH = 38.0f;
        const auto radarArea = scopeArea.withTrimmedBottom(meterBarH + 4.0f);
        const auto delayBarArea = scopeArea.withTop(radarArea.getBottom() + 4.0f);

        // 1. 2D Stereo Band Orbit Scope
        const float radarRadius = std::min(radarArea.getWidth(), radarArea.getHeight()) * 0.42f;
        const auto radarCentre = radarArea.getCentre();

        g.setColour(Palette::gridLine);
        g.drawEllipse(radarCentre.x - radarRadius, radarCentre.y - radarRadius,
                      radarRadius * 2.0f, radarRadius * 2.0f, 1.0f);
        g.drawEllipse(radarCentre.x - radarRadius * 0.55f, radarCentre.y - radarRadius * 0.55f,
                      radarRadius * 1.1f, radarRadius * 1.1f, 1.0f);
        g.drawHorizontalLine(static_cast<int>(radarCentre.y), radarCentre.x - radarRadius, radarCentre.x + radarRadius);
        g.drawVerticalLine(static_cast<int>(radarCentre.x), radarCentre.y - radarRadius, radarCentre.y + radarRadius);

        // Stereo width sector cone
        const float spreadAngle = static_cast<float>(0.08 + 0.72 * widthVal);
        juce::Path widthCone;
        widthCone.addPieSegment(radarCentre.x - radarRadius, radarCentre.y - radarRadius,
                                radarRadius * 2.0f, radarRadius * 2.0f,
                                -spreadAngle, spreadAngle, 0.1f);
        g.setColour(Palette::accentCyan.withAlpha(0.08f));
        g.fillPath(widthCone);

        g.setFont(DriftLookAndFeel::makeTechnicalFont(9.0f, true));
        g.setColour(Palette::textMuted);
        g.drawText("L", static_cast<int>(radarCentre.x - radarRadius - 14.0f), static_cast<int>(radarCentre.y - 6.0f),
                   12, 12, juce::Justification::centred, false);
        g.drawText("R", static_cast<int>(radarCentre.x + radarRadius + 2.0f), static_cast<int>(radarCentre.y - 6.0f),
                   12, 12, juce::Justification::centred, false);

        // Plot the 4 band stereo locus nodes
        for (std::size_t b = 0; b < kNumBands; ++b) {
            const float yL = trajectories[0][b][kHistoryPoints - 1];
            const float yR = trajectories[1][b][kHistoryPoints - 1];
            const float rawDiff = (yR - yL) - static_cast<float>(2.0 * widthVal * 0.14);
            const float bandSign = (b % 2 == 0) ? -1.0f : 1.0f;
            const float stereoSpread = rawDiff * 1.45f + bandSign * static_cast<float>(widthVal) * 0.28f;
            const float avgMod = 0.5f * (yL + yR);

            const float nx = radarCentre.x + std::clamp(stereoSpread, -0.92f, 0.92f) * radarRadius;
            const float ny = radarCentre.y - std::clamp(avgMod, -0.92f, 0.92f) * radarRadius;

            // Feedback resonance halo around each band node
            const float fbHalo = static_cast<float>(feedbackVal / 0.65) * 7.5f;
            if (fbHalo > 0.5f) {
                g.setColour(Palette::accentAmber.withAlpha(0.24f));
                g.drawEllipse(nx - 3.5f - fbHalo, ny - 3.5f - fbHalo,
                              (3.5f + fbHalo) * 2.0f, (3.5f + fbHalo) * 2.0f, 1.1f);
            }

            g.setColour(Palette::accentCyanBright);
            g.fillEllipse(nx - 3.2f, ny - 3.2f, 6.4f, 6.4f);
        }

        // 2. Delay Horizon Bar (0.3 ms .. 30 ms logarithmic scale)
        g.setFont(DriftLookAndFeel::makeTechnicalFont(9.0f, true));
        g.setColour(Palette::textMuted);
        g.drawText("DELAY WINDOW (0.3 - 30 ms)", static_cast<int>(delayBarArea.getX()),
                   static_cast<int>(delayBarArea.getY()), static_cast<int>(delayBarArea.getWidth()), 12,
                   juce::Justification::centredLeft, false);

        const juce::Rectangle<float> barTrack(delayBarArea.getX(), delayBarArea.getY() + 15.0f,
                                              delayBarArea.getWidth(), 8.0f);
        g.setColour(Palette::knobTrack);
        g.fillRoundedRectangle(barTrack, 3.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(barTrack, 3.0f, 1.0f);

        const float centerLogNorm = static_cast<float>(std::log(centerMs / 0.3) / std::log(30.0 / 0.3));
        const float lowMs = std::clamp(centerMs - effectiveExcursionMs * 1.732, 0.3, 30.0);
        const float highMs = std::clamp(centerMs + effectiveExcursionMs * 1.732, 0.3, 30.0);
        const float lowNorm = static_cast<float>(std::log(lowMs / 0.3) / std::log(30.0 / 0.3));
        const float highNorm = static_cast<float>(std::log(highMs / 0.3) / std::log(30.0 / 0.3));

        const float spanX1 = barTrack.getX() + lowNorm * barTrack.getWidth();
        const float spanX2 = barTrack.getX() + highNorm * barTrack.getWidth();
        g.setColour(Palette::accentCyan.withAlpha(0.45f));
        g.fillRoundedRectangle(spanX1, barTrack.getY() + 1.0f,
                               std::max(3.0f, spanX2 - spanX1), barTrack.getHeight() - 2.0f, 2.0f);

        const float centerX = barTrack.getX() + centerLogNorm * barTrack.getWidth();
        g.setColour(Palette::accentAmber);
        g.fillRoundedRectangle(centerX - 1.5f, barTrack.getY() - 2.0f, 3.0f, barTrack.getHeight() + 4.0f, 1.5f);

        // Dynamics & Feedback micro status row below bar
        g.setFont(DriftLookAndFeel::makeTechnicalFont(9.0f, false));
        g.setColour(Palette::textSecondary);
        const juce::String dynFbText = "DYN " + juce::String( static_cast<int>( std::round(dynamicsVal * 100.0))) + "%  |  FB "
                                     + juce::String(static_cast<int>(std::round(feedbackVal * 100.0))) + "%";
        g.drawText(dynFbText, static_cast<int>(delayBarArea.getX()), static_cast<int>(barTrack.getBottom() + 2.0f),
                   static_cast<int>(delayBarArea.getWidth()), 12, juce::Justification::centred, false);
    }
}

} // namespace drift::ui
