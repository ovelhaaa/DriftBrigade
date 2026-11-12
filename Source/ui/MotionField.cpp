#include "ui/MotionField.h"
#include "ui/DriftLookAndFeel.h"
#include "dsp/DepthMapping.h"
#include "dsp/DriftEngine.h"
#include <algorithm>
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
                         std::function<void()> onParametersChanged,
                         std::function<double()> sampleRateProvider)
    : reader(state),
      snapshot(reader.read()),
      parameterChangeCallback(std::move(onParametersChanged)),
      sampleRateCallback(std::move(sampleRateProvider)),
      currentSampleRate(querySampleRate()) {
    freezeButton.setButtonText("LIVE");
    freezeButton.setClickingTogglesState(true);
    freezeButton.setToggleState(true, juce::dontSendNotification);
    freezeButton.getProperties().set("isHoldButton", true);
    freezeButton.setTooltip("Alterna entre animacao em tempo real (LIVE) e trajetoria congelada com indicadores ativos (HOLD).");
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

double MotionField::querySampleRate() const noexcept {
    if (sampleRateCallback) {
        const double sr = sampleRateCallback();
        if (std::isfinite(sr) && sr > 0.0) {
            return std::clamp(sr, 44100.0, 192000.0);
        }
    }
    return 48000.0;
}

double MotionField::estimatedExcursionMs() const noexcept {
    const double sr = (std::isfinite(currentSampleRate) && currentSampleRate > 0.0)
        ? std::clamp(currentSampleRate, 44100.0, 192000.0)
        : 48000.0;
    const double motionHz = snapshot[Motion];
    const double depthVal = snapshot[Depth];
    const double centerMs = snapshot[Center];
    const double minimumDelaySec = 4.0 / sr;
    const double requestedExcursionSec = depthSeconds(motionHz, depthVal, true);
    const double safetyMaxSec = std::max(0.0, (centerMs * 0.001 - minimumDelaySec) * 0.85
                                         / DriftEngine::combinedModulationBound(OrganicVariant::Wander));
    return std::min(requestedExcursionSec, safetyMaxSec) * 1000.0;
}

bool MotionField::pollParameters() noexcept {
    const auto latest = reader.read();
    const double latestSr = querySampleRate();
    const bool paramChanged = parametersDiffer(snapshot, latest);
    const bool srChanged = std::abs(currentSampleRate - latestSr) > 1e-3;

    snapshot = latest;
    currentSampleRate = latestSr;

    if (paramChanged && parameterChangeCallback) {
        parameterChangeCallback();
    }
    return paramChanged || srChanged;
}

void MotionField::setAnimationEnabled(bool enabled) {
    animationEnabled = enabled;
    freezeButton.setToggleState(enabled, juce::dontSendNotification);
    freezeButton.setButtonText(enabled ? "LIVE" : "HOLD");
    pollParameters();
    if (enabled) {
        recomputeTrajectories();
    }
    if (isShowing()) {
        startTimerHz(enabled ? 30 : 15);
    } else {
        stopTimer();
    }
    repaint();
}

void MotionField::visibilityChanged() {
    if (isShowing())
        startTimerHz(animationEnabled ? 30 : 15);
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
    pollParameters();

    if (animationEnabled) {
        // Perceptual visual scroll rate: smooth and readable from 0.05 Hz to 10 Hz at 30 FPS
        const double motionHz = std::clamp(snapshot[Motion], 0.05, 10.0);
        const double visualCyclesPerSec = 0.18 + 1.35 * std::pow(motionHz / 10.0, 0.55);
        phaseCycles += deltaSeconds * visualCyclesPerSec;
        if (phaseCycles > 10000.0)
            phaseCycles -= 10000.0;
        recomputeTrajectories();
    }
}

void MotionField::timerCallback() {
    if (!isShowing()) return;
    if (animationEnabled) {
        stepSimulation(1.0 / 30.0);
        repaint();
    } else if (pollParameters()) {
        repaint();
    }
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
    const int btnW = 54;
    const int btnH = 20;
    freezeButton.setBounds(getWidth() - btnW - 10, 5, btnW, btnH);
}

void MotionField::paint(juce::Graphics& g) {
    const auto outer = getLocalBounds().toFloat().reduced(0.5f);
    constexpr float corner = 5.5f;

    // 1. Outer graphite structural module (#292B2D)
    g.setColour(Palette::panelBg);
    g.fillRoundedRectangle(outer, corner);

    const float headerH = 30.0f;
    const juce::Rectangle<float> headerRect(outer.getX() + 1.0f, outer.getY() + 1.0f,
                                            outer.getWidth() - 2.0f, headerH);
    g.setColour(Palette::panelRecessed);
    g.fillRoundedRectangle(headerRect, corner);
    g.fillRect(outer.getX() + 1.0f, outer.getY() + headerH - corner, outer.getWidth() - 2.0f, corner);

    g.setColour(Palette::panelBorder);
    g.drawRoundedRectangle(outer, corner, 1.0f);
    g.drawHorizontalLine(static_cast<int>(outer.getY() + headerH), outer.getX() + 1.0f, outer.getRight() - 1.0f);

    // Header title + honest parametric tag
    g.setColour(Palette::accentOrange);
    g.fillRoundedRectangle(outer.getX() + 10.0f, outer.getY() + 9.0f, 3.0f, 12.0f, 1.2f);

    g.setFont(DriftLookAndFeel::makeTechnicalFont(11.5f, true));
    g.setColour(Palette::textOnPanel);
    g.drawText("MOTION FIELD", static_cast<int>(outer.getX() + 18.0f), static_cast<int>(outer.getY()),
               108, static_cast<int>(headerH), juce::Justification::centredLeft, false);

    g.setFont(DriftLookAndFeel::makeTechnicalFont(9.8f, false));
    g.setColour(Palette::textOnPanelMuted);
    const juce::String subHeader = getWidth() < 860 ? "PARAMETRIC 4-BAND STEREO"
                                                    : "PARAMETRIC 4-BAND STEREO TRAJECTORY";
    const int subHeaderW = getWidth() < 860 ? 150 : 225;
    g.drawText(subHeader, static_cast<int>(outer.getX() + 120.0f),
               static_cast<int>(outer.getY()), subHeaderW, static_cast<int>(headerH),
               juce::Justification::centredLeft, false);

    // Parametric readout summary in header (always updated in both LIVE and HOLD)
    const double centerMs = snapshot[Center];
    const double feedbackVal = snapshot[Feedback];
    const double mixVal = snapshot[Mix];
    const double widthVal = snapshot[Width];
    const double dynamicsVal = snapshot[Dynamics];
    const double effectiveExcursionMs = estimatedExcursionMs();

    juce::String telemetryText =
        juce::String("CENTER ") + juce::String(centerMs, 2) + " ms   |   "
        + "EST. EXCURSION +/-" + juce::String(effectiveExcursionMs, 2) + " ms";
    if (!animationEnabled) {
        telemetryText = "HOLD (FROZEN TRACE)   |   " + telemetryText;
    }

    g.setFont(DriftLookAndFeel::makeTechnicalFont(10.0f, true));
    g.setColour(animationEnabled ? Palette::textOnPanelSecondary : Palette::accentOrange);
    const int telemetryLeft = static_cast<int>(outer.getX()) + 124 + subHeaderW;
    const int telemetryRight = getWidth() - 72;
    if (telemetryRight > telemetryLeft + 80) {
        g.drawFittedText(telemetryText, telemetryLeft, static_cast<int>(outer.getY()),
                         telemetryRight - telemetryLeft, static_cast<int>(headerH),
                         juce::Justification::centredRight, 1, 0.85f);
    }

    // 2. Recessed almost-black display interior (#17191B) inside the #292B2D module frame
    const auto displayFrame = outer.withTrimmedTop(headerH).reduced(6.0f, 6.0f);
    if (displayFrame.getWidth() < 120.0f || displayFrame.getHeight() < 60.0f) return;

    g.setColour(Palette::displayBg);
    g.fillRoundedRectangle(displayFrame, 4.0f);
    g.setColour(Palette::panelBorder);
    g.drawRoundedRectangle(displayFrame, 4.0f, 1.0f);

    const auto bodyArea = displayFrame.reduced(6.0f, 4.0f);
    const float scopeWidth = std::clamp(bodyArea.getWidth() * 0.24f, 160.0f, 240.0f);
    const auto waveArea = bodyArea.withTrimmedRight(scopeWidth + 10.0f);
    const auto scopeArea = bodyArea.withLeft(waveArea.getRight() + 10.0f);

    // Vertical separator between wave field and stereo/delay instrument
    g.setColour(Palette::gridAxis);
    g.drawVerticalLine(static_cast<int>(waveArea.getRight() + 5.0f), displayFrame.getY() + 2.0f, displayFrame.getBottom() - 2.0f);

    // =========================================================================
    // A. LEFT REGION: 4-BAND STEREO TRAJECTORY CANVAS (Monochromatic Orange System)
    // =========================================================================
    {
        // Neutral technical graticule lines
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
        const float halfH = waveArea.getHeight() * 0.45f;

        // Dry reference horizon line at center (prominent when Mix is low)
        const float dryAlpha = static_cast<float>(0.24 + 0.62 * (1.0 - mixVal));
        g.setColour(Palette::textOnPanelMuted.withAlpha(dryAlpha));
        const float refDashLengths[2] = {4.0f, 4.0f};
        g.drawDashedLine(juce::Line<float>(waveArea.getX(), midY, waveArea.getRight(), midY),
                         refDashLengths, 2, 1.1f);

        // Dynamic sensitivity envelope corridor (same orange family, subtle opacity)
        if (dynamicsVal > 0.01) {
            const float dynBandHalfH = static_cast<float>(dynamicsVal) * halfH * 0.54f;
            const juce::Rectangle<float> dynRect(waveArea.getX(), midY - dynBandHalfH,
                                                 waveArea.getWidth(), dynBandHalfH * 2.0f);
            g.setColour(Palette::accentOrange.withAlpha(static_cast<float>(0.045 * dynamicsVal)));
            g.fillRect(dynRect);

            g.setColour(Palette::accentOrange.withAlpha(static_cast<float>(0.15 * dynamicsVal)));
            g.drawHorizontalLine(static_cast<int>(midY - dynBandHalfH), waveArea.getX(), waveArea.getRight());
            g.drawHorizontalLine(static_cast<int>(midY + dynBandHalfH), waveArea.getX(), waveArea.getRight());
        }

        // Wet prominence factor from Mix
        const float wetAlpha = static_cast<float>(0.20 + 0.80 * mixVal);
        const float fbNorm = static_cast<float>(feedbackVal / 0.65);

        // Band differentiation within the single #F36B2B orange family:
        // Low bands have slightly heavier stroke and higher opacity; High bands are finer.
        constexpr std::array<float, kNumBands> bandAlphaScale {1.00f, 0.92f, 0.84f, 0.76f};
        constexpr std::array<float, kNumBands> bandLeftStroke {2.20f, 1.95f, 1.72f, 1.52f};
        constexpr std::array<float, kNumBands> bandRightStroke{1.65f, 1.50f, 1.35f, 1.22f};

        // 1. Draw feedback persistence trails first (when Feedback > 2%)
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
                    const float chScale = (ch == 0) ? 0.30f : 0.20f;
                    g.setColour(Palette::accentOrange.withAlpha(wetAlpha * bandAlphaScale[b] * fbNorm * chScale));
                    g.strokePath(trailPath, juce::PathStrokeType(1.0f + fbNorm * 0.6f,
                                                                 juce::PathStrokeType::curved,
                                                                 juce::PathStrokeType::rounded));
                }
            }
        }

        // 2. Draw primary Left (solid orange) and Right (segmented/dashed orange) band trajectories
        const float rightDashLengths[2] = {5.5f, 3.5f};

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

            // Subtle stereo ribbon between L and R
            if (widthVal > 0.02) {
                for (std::size_t i = kHistoryPoints; i > 0; --i) {
                    const std::size_t idx = i - 1;
                    const float px = waveArea.getX() + (static_cast<float>(idx) / static_cast<float>(kHistoryPoints - 1)) * waveArea.getWidth();
                    const float pyR = midY - trajectories[1][b][idx] * halfH;
                    ribbonPath.lineTo(px, pyR);
                }
                ribbonPath.closeSubPath();
                g.setColour(Palette::accentOrange.withAlpha(wetAlpha * bandAlphaScale[b] * static_cast<float>(0.03 + 0.05 * widthVal)));
                g.fillPath(ribbonPath);
            }

            // Right channel trajectory: segmented/dashed orange with distinct opacity
            if (widthVal > 0.005) {
                juce::Path dashedRight;
                juce::PathStrokeType(bandRightStroke[b], juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                    .createDashedStroke(dashedRight, rightPath, rightDashLengths, 2);
                g.setColour(Palette::accentOrange.withAlpha(wetAlpha * bandAlphaScale[b] * 0.62f));
                g.fillPath(dashedRight);
            }

            // Left channel trajectory: solid orange with primary stroke weight
            g.setColour(Palette::accentOrange.withAlpha(wetAlpha * bandAlphaScale[b] * 0.96f));
            g.strokePath(leftPath, juce::PathStrokeType(bandLeftStroke[b], juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            // Leading head nodes at the right edge of the trajectory window:
            // Left = solid filled circle, Right = hollow ring
            const float headX = waveArea.getRight() - 3.5f;
            const float headYL = midY - trajectories[0][b][kHistoryPoints - 1] * halfH;
            const float headYR = midY - trajectories[1][b][kHistoryPoints - 1] * halfH;

            if (widthVal > 0.01) {
                g.setColour(Palette::displayBg);
                g.fillEllipse(headX - 3.2f, headYR - 3.2f, 6.4f, 6.4f);
                g.setColour(Palette::accentOrange.withAlpha(wetAlpha * bandAlphaScale[b] * 0.85f));
                g.drawEllipse(headX - 3.2f, headYR - 3.2f, 6.4f, 6.4f, 1.4f);
            }
            g.setColour(Palette::accentOrange.withAlpha(wetAlpha * bandAlphaScale[b]));
            g.fillEllipse(headX - 3.4f, headYL - 3.4f, 6.8f, 6.8f);
        }

        // Left-margin band labels with dark tag backdrop + orange index bar
        g.setFont(DriftLookAndFeel::makeTechnicalFont(9.0f, true));
        for (std::size_t b = 0; b < kNumBands; ++b) {
            const float labelY = waveArea.getY() + 4.0f + static_cast<float>(3 - b) * (waveArea.getHeight() - 20.0f) / 3.0f;
            const juce::Rectangle<float> tagRect(waveArea.getX() + 4.0f, labelY, 66.0f, 14.0f);
            g.setColour(Palette::panelRecessed.withAlpha(0.90f));
            g.fillRoundedRectangle(tagRect, 2.5f);
            g.setColour(Palette::panelBorder);
            g.drawRoundedRectangle(tagRect, 2.5f, 0.8f);

            g.setColour(Palette::accentOrange.withAlpha(bandAlphaScale[b]));
            g.fillRect(tagRect.getX() + 2.0f, tagRect.getY() + 3.0f, 2.0f, tagRect.getHeight() - 6.0f);

            g.setColour(Palette::textOnPanelSecondary);
            g.drawText(kBandNames[b], tagRect.withTrimmedLeft(6.0f).toNearestInt(),
                       juce::Justification::centredLeft, false);
        }

        // Graphical L / R channel coding legend pill in top-right of wave canvas
        {
            const float legW = 148.0f;
            const float legH = 16.0f;
            const juce::Rectangle<float> legRect(waveArea.getRight() - legW - 6.0f, waveArea.getY() + 4.0f, legW, legH);
            g.setColour(Palette::panelRecessed.withAlpha(0.92f));
            g.fillRoundedRectangle(legRect, 3.0f);
            g.setColour(Palette::panelBorder);
            g.drawRoundedRectangle(legRect, 3.0f, 0.8f);

            const float cy = legRect.getCentreY();
            // L: solid line + filled dot
            g.setColour(Palette::accentOrange);
            g.drawLine(legRect.getX() + 8.0f, cy, legRect.getX() + 24.0f, cy, 2.0f);
            g.fillEllipse(legRect.getX() + 24.0f, cy - 2.6f, 5.2f, 5.2f);
            g.setFont(DriftLookAndFeel::makeTechnicalFont(9.0f, true));
            g.setColour(Palette::textOnPanel);
            g.drawText("LEFT", static_cast<int>(legRect.getX() + 33.0f), static_cast<int>(legRect.getY()),
                       34, static_cast<int>(legH), juce::Justification::centredLeft, false);

            // R: dashed line + hollow dot
            g.setColour(Palette::accentOrange.withAlpha(0.75f));
            const float rDash[2] = {3.5f, 2.5f};
            g.drawDashedLine(juce::Line<float>(legRect.getX() + 74.0f, cy, legRect.getX() + 92.0f, cy), rDash, 2, 1.4f);
            g.setColour(Palette::displayBg);
            g.fillEllipse(legRect.getX() + 92.0f, cy - 2.6f, 5.2f, 5.2f);
            g.setColour(Palette::accentOrange.withAlpha(0.85f));
            g.drawEllipse(legRect.getX() + 92.0f, cy - 2.6f, 5.2f, 5.2f, 1.3f);
            g.setColour(Palette::textOnPanelSecondary);
            g.drawText("RIGHT", static_cast<int>(legRect.getX() + 101.0f), static_cast<int>(legRect.getY()),
                       42, static_cast<int>(legH), juce::Justification::centredLeft, false);
        }

        if (mixVal < 0.01) {
            g.setColour(Palette::panelBg.withAlpha(0.94f));
            const juce::Rectangle<float> dryBadge(waveArea.getCentreX() - 72.0f, midY - 11.0f, 144.0f, 22.0f);
            g.fillRoundedRectangle(dryBadge, 4.0f);
            g.setColour(Palette::accentOrange);
            g.drawRoundedRectangle(dryBadge, 4.0f, 1.0f);
            g.setColour(Palette::textOnPanel);
            g.setFont(DriftLookAndFeel::makeTechnicalFont(10.0f, true));
            g.drawText("MIX 0% (DRY REFERENCE)", dryBadge.toNearestInt(), juce::Justification::centred, false);
        }
    }

    // =========================================================================
    // B. RIGHT REGION: MINIMALIST STEREO ORBIT + DELAY WINDOW
    // =========================================================================
    {
        const float meterBarH = 42.0f;
        const auto radarArea = scopeArea.withTrimmedBottom(meterBarH + 4.0f);
        const auto delayBarArea = scopeArea.withTop(radarArea.getBottom() + 4.0f);

        // 1. Minimalist Stereo Orbit Instrument
        g.setFont(DriftLookAndFeel::makeTechnicalFont(8.8f, true));
        g.setColour(Palette::textOnPanelMuted);
        g.drawText("STEREO ORBIT", static_cast<int>(radarArea.getX()), static_cast<int>(radarArea.getY()),
                   static_cast<int>(radarArea.getWidth()), 11, juce::Justification::centred, false);

        const auto orbitBounds = radarArea.withTrimmedTop(11.0f);
        const float radarRadius = std::min(orbitBounds.getWidth(), orbitBounds.getHeight()) * 0.43f;
        const auto radarCentre = orbitBounds.getCentre();

        g.setColour(Palette::gridAxis);
        g.drawEllipse(radarCentre.x - radarRadius, radarCentre.y - radarRadius,
                      radarRadius * 2.0f, radarRadius * 2.0f, 1.0f);
        g.setColour(Palette::gridLine);
        g.drawEllipse(radarCentre.x - radarRadius * 0.52f, radarCentre.y - radarRadius * 0.52f,
                      radarRadius * 1.04f, radarRadius * 1.04f, 1.0f);
        g.drawHorizontalLine(static_cast<int>(radarCentre.y), radarCentre.x - radarRadius, radarCentre.x + radarRadius);
        g.drawVerticalLine(static_cast<int>(radarCentre.x), radarCentre.y - radarRadius, radarCentre.y + radarRadius);

        // Minimalist stereo width rim arc (top of reticle)
        const float spreadAngle = static_cast<float>(0.06 + 0.72 * widthVal);
        if (widthVal > 0.01) {
            juce::Path widthArc;
            widthArc.addCentredArc(radarCentre.x, radarCentre.y, radarRadius, radarRadius,
                                   0.0f, -spreadAngle, spreadAngle, true);
            g.setColour(Palette::accentOrange.withAlpha(0.55f));
            g.strokePath(widthArc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }

        g.setFont(DriftLookAndFeel::makeTechnicalFont(9.0f, true));
        g.setColour(Palette::textOnPanelSecondary);
        g.drawText("L", static_cast<int>(radarCentre.x - radarRadius - 14.0f), static_cast<int>(radarCentre.y - 6.0f),
                   12, 12, juce::Justification::centred, false);
        g.drawText("R", static_cast<int>(radarCentre.x + radarRadius + 2.0f), static_cast<int>(radarCentre.y - 6.0f),
                   12, 12, juce::Justification::centred, false);

        // Plot the 4 band stereo locus nodes in orange
        for (std::size_t b = 0; b < kNumBands; ++b) {
            const float yL = trajectories[0][b][kHistoryPoints - 1];
            const float yR = trajectories[1][b][kHistoryPoints - 1];
            const float rawDiff = (yR - yL) - static_cast<float>(2.0 * widthVal * 0.14);
            const float bandSign = (b % 2 == 0) ? -1.0f : 1.0f;
            const float stereoSpread = rawDiff * 1.45f + bandSign * static_cast<float>(widthVal) * 0.28f;
            const float avgMod = 0.5f * (yL + yR);

            const float nx = radarCentre.x + std::clamp(stereoSpread, -0.90f, 0.90f) * radarRadius;
            const float ny = radarCentre.y - std::clamp(avgMod, -0.90f, 0.90f) * radarRadius;

            // Subtle vector line from origin to node
            g.setColour(Palette::accentOrange.withAlpha(0.22f));
            g.drawLine(radarCentre.x, radarCentre.y, nx, ny, 1.0f);

            // Discreet feedback persistence ring in the same orange family
            const float fbRing = static_cast<float>(feedbackVal / 0.65) * 4.5f;
            if (fbRing > 0.4f) {
                g.setColour(Palette::accentOrange.withAlpha(0.28f));
                g.drawEllipse(nx - 3.0f - fbRing, ny - 3.0f - fbRing,
                              (3.0f + fbRing) * 2.0f, (3.0f + fbRing) * 2.0f, 1.0f);
            }

            const float nodeR = 3.2f - static_cast<float>(b) * 0.2f;
            g.setColour(Palette::accentOrange);
            g.fillEllipse(nx - nodeR, ny - nodeR, nodeR * 2.0f, nodeR * 2.0f);
        }

        // 2. Delay Window Bar (0.3 ms .. 30 ms logarithmic scale)
        g.setFont(DriftLookAndFeel::makeTechnicalFont(8.8f, true));
        g.setColour(Palette::textOnPanelSecondary);
        g.drawText("DELAY WINDOW (0.3 - 30 ms)", static_cast<int>(delayBarArea.getX()),
                   static_cast<int>(delayBarArea.getY()), static_cast<int>(delayBarArea.getWidth()), 12,
                   juce::Justification::centredLeft, false);

        const juce::Rectangle<float> barTrack(delayBarArea.getX(), delayBarArea.getY() + 14.0f,
                                              delayBarArea.getWidth(), 9.0f);
        g.setColour(Palette::panelRecessed);
        g.fillRoundedRectangle(barTrack, 3.0f);
        g.setColour(Palette::panelBorder);
        g.drawRoundedRectangle(barTrack, 3.0f, 1.0f);

        // Reference scale ticks on the track (0.3 ms, 3.0 ms, 30.0 ms)
        g.setColour(Palette::gridAxis);
        const float midScaleX = barTrack.getX() + 0.5f * barTrack.getWidth();
        g.drawVerticalLine(static_cast<int>(midScaleX), barTrack.getY() + 1.0f, barTrack.getBottom() - 1.0f);

        const float centerLogNorm = static_cast<float>(std::log(centerMs / 0.3) / std::log(30.0 / 0.3));
        const double maxModBound = DriftEngine::combinedModulationBound(OrganicVariant::Wander);
        const double lowMs = std::clamp(centerMs - effectiveExcursionMs * maxModBound, 0.3, 30.0);
        const double highMs = std::clamp(centerMs + effectiveExcursionMs * maxModBound, 0.3, 30.0);
        const float lowNorm = static_cast<float>(std::log(lowMs / 0.3) / std::log(30.0 / 0.3));
        const float highNorm = static_cast<float>(std::log(highMs / 0.3) / std::log(30.0 / 0.3));

        const float spanX1 = barTrack.getX() + lowNorm * barTrack.getWidth();
        const float spanX2 = barTrack.getX() + highNorm * barTrack.getWidth();
        const float spanW = std::max(3.0f, spanX2 - spanX1);

        // Predicted excursion interval in translucent orange with crisp orange boundary ticks
        g.setColour(Palette::accentOrange.withAlpha(0.38f));
        g.fillRoundedRectangle(spanX1, barTrack.getY() + 1.0f, spanW, barTrack.getHeight() - 2.0f, 2.0f);
        g.setColour(Palette::accentOrange.withAlpha(0.85f));
        g.drawVerticalLine(static_cast<int>(spanX1), barTrack.getY() + 1.0f, barTrack.getBottom() - 1.0f);
        g.drawVerticalLine(static_cast<int>(spanX1 + spanW), barTrack.getY() + 1.0f, barTrack.getBottom() - 1.0f);

        // Selected Center marker: high-contrast light graphite/off-white needle with orange cap
        const float centerX = barTrack.getX() + centerLogNorm * barTrack.getWidth();
        g.setColour(Palette::accentOrange);
        g.fillRoundedRectangle(centerX - 2.5f, barTrack.getY() - 2.5f, 5.0f, barTrack.getHeight() + 5.0f, 1.5f);
        g.setColour(Palette::textOnPanel);
        g.fillRect(centerX - 1.0f, barTrack.getY() - 1.5f, 2.0f, barTrack.getHeight() + 3.0f);

        // Dynamics, Feedback & Sample Rate micro status row below bar
        g.setFont(DriftLookAndFeel::makeTechnicalFont(8.8f, true));
        g.setColour(Palette::textOnPanelMuted);
        const juce::String srKhz = juce::String(currentSampleRate * 0.001, 1) + "k";
        const juce::String dynFbText = "DYN " + juce::String(static_cast<int>(std::round(dynamicsVal * 100.0))) + "%  |  FB "
                                     + juce::String(static_cast<int>(std::round(feedbackVal * 100.0))) + "%  |  " + srKhz;
        g.drawText(dynFbText, static_cast<int>(delayBarArea.getX()), static_cast<int>(barTrack.getBottom() + 3.0f),
                   static_cast<int>(delayBarArea.getWidth()), 13, juce::Justification::centred, false);
    }
}

} // namespace drift::ui
