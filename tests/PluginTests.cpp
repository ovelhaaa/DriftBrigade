#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include "AllocationTracker.h"

namespace {
void check(bool ok, const char* msg) {
    if (!ok) throw std::runtime_error(msg);
}

void saveSnapshotPng(juce::Component& comp, const juce::File& destination, float scaleFactor = 1.0f) {
    destination.getParentDirectory().createDirectory();
    auto snapshot = comp.createComponentSnapshot(comp.getLocalBounds(), true, scaleFactor);
    check(snapshot.isValid() && snapshot.getWidth() > 0 && snapshot.getHeight() > 0, "invalid component snapshot");
    auto stream = destination.createOutputStream();
    check(stream != nullptr, "snapshot output stream");
    stream->setPosition(0);
    check(stream->truncate().wasOk(), "snapshot truncate");
    juce::PNGImageFormat format;
    check(format.writeImageToStream(snapshot, *stream), "snapshot encode");
    stream->flush();
    check(destination.existsAsFile() && destination.getSize() > 1024, "snapshot file empty or missing");
}
} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiser;
    try {
        // 0. Verify version metadata consistency
        check(std::string(drift::productVersion) == "0.2.0", "CMake productVersion matches 0.2.0");
        check(std::string(drift::releaseVersion) == "0.2.0-beta.1", "releaseVersion matches 0.2.0-beta.1");

        DriftProcessor a, b;
        for (std::size_t i = 0; i < drift::Count; ++i) {
            auto* parameter = a.state.getParameter(drift::parameterSpecs[i].id);
            parameter->setValueNotifyingHost(static_cast<float>(i + 1) / 10.0f);
            check(std::abs(parameter->convertTo0to1(parameter->convertFrom0to1(0.37f)) - 0.37f) < 1e-5f,
                  "normalized round trip");
        }
        juce::MemoryBlock saved;
        a.getStateInformation(saved);
        b.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        for (const auto& s : drift::parameterSpecs)
            check(a.state.getRawParameterValue(s.id)->load() == b.state.getRawParameterValue(s.id)->load(),
                  "state round trip");
        const char bad[] = "invalid";
        b.setStateInformation(bad, sizeof(bad));

#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
        using Outcome = drift::BBDPreparationOutcome;
        b.prepareToPlay(48000, 17);
        check(b.hostTestOutcome() == Outcome::BBD && !b.hostTestParametersAccepted(),
              "internal scene starts admitted with out-of-envelope saved parameters");
        b.releaseResources();
        auto* center = b.state.getParameter("center");
        center->setValueNotifyingHost(center->convertTo0to1(24.f));
#endif

        for (double sr : {44100., 48000., 88200., 96000.}) {
            for (bool stereo : {false, true}) {
                const auto set = stereo ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono();
                juce::AudioProcessor::BusesLayout layout;
                layout.inputBuses.add(set);
                layout.outputBuses.add(set);
                check(b.setBusesLayout(layout), "mono/stereo routing");
                b.prepareToPlay(sr, 17);
                check(std::abs(b.effectiveSampleRate() - sr) < 1e-3, "effectiveSampleRate updated on prepareToPlay");
#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
                check(b.hostTestOutcome() == (sr == 96000 ? Outcome::FallbackToDigital : Outcome::BBD)
                          && b.hostTestParametersAccepted(), "internal host rate and restored controls");
#endif
                for (int n : {0, 1, 17, 127, 1024, 8192}) {
                    juce::AudioBuffer<float> audio(stereo ? 2 : 1, n);
                    audio.clear();
                    if (n > 0) audio.setSample(0, 0, 1.0f);
                    juce::MidiBuffer midi;
                    const auto before = allocations.load();
                    b.processBlock(audio, midi);
                    check(allocations.load() == before, "plugin callback allocation");
                    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                        for (int i = 0; i < n; ++i)
                            check(std::isfinite(audio.getSample(ch, i)), "plugin finite output");
                }
                b.releaseResources();
            }
        }

        juce::AudioProcessor::BusesLayout upmix;
        upmix.inputBuses.add(juce::AudioChannelSet::mono());
        upmix.outputBuses.add(juce::AudioChannelSet::stereo());
        check(b.setBusesLayout(upmix), "mono to stereo routing");
        b.state.getParameter("mix")->setValueNotifyingHost(0.0f);
        b.prepareToPlay(48000, 17);
        juce::AudioBuffer<float> audio(2, 17);
        audio.clear();
        audio.setSample(0, 0, 0.5f);
        audio.setSample(1, 0, -0.8f);
        juce::MidiBuffer midi;
        b.processBlock(audio, midi);
        check(audio.getSample(0, 0) == 0.5f && audio.getSample(1, 0) == 0.5f,
              "mono input duplicated before DSP");

#ifdef DRIFT_INTERNAL_BBD_HOST_TEST
        center->setValueNotifyingHost(0);
        b.state.getParameter("mix")->setValueNotifyingHost(1);
        audio.clear();
        audio.setSample(0, 0, .5f);
        b.processBlock(audio, midi);
        check(!b.hostTestParametersAccepted() && audio.getSample(0, 0) == .5f,
              "internal callback atomically rejects out-of-envelope snapshot");
#endif

        // Offline render + simultaneous multi-parameter automation test
        {
            DriftProcessor offlineProc;
            juce::AudioProcessor::BusesLayout stLayout;
            stLayout.inputBuses.add(juce::AudioChannelSet::stereo());
            stLayout.outputBuses.add(juce::AudioChannelSet::stereo());
            check(offlineProc.setBusesLayout(stLayout), "offline stereo layout");
            offlineProc.setNonRealtime(true);
            offlineProc.prepareToPlay(48000.0, 256);
            for (int block = 0; block < 16; ++block) {
                const float norm = static_cast<float>(block) / 15.0f;
                for (std::size_t p = 0; p < drift::Count; ++p)
                    offlineProc.state.getParameter(drift::parameterSpecs[p].id)->setValueNotifyingHost(norm);
                juce::AudioBuffer<float> buf(2, 128);
                for (int n = 0; n < 128; ++n) {
                    buf.setSample(0, n, 0.3f * std::sin(0.05f * static_cast<float>(block * 128 + n)));
                    buf.setSample(1, n, 0.3f * std::cos(0.04f * static_cast<float>(block * 128 + n)));
                }
                const auto before = allocations.load();
                offlineProc.processBlock(buf, midi);
                check(allocations.load() == before, "offline automation callback allocation");
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < 128; ++n)
                        check(std::isfinite(buf.getSample(ch, n)), "offline automation finite output");
            }
            offlineProc.releaseResources();
        }

        // Repeated editor open/close lifecycle test
        for (int cycle = 0; cycle < 6; ++cycle) {
            std::unique_ptr<juce::AudioProcessorEditor> tempEditor(b.createEditor());
            check(tempEditor != nullptr, "editor creation in lifecycle loop");
            tempEditor->setSize(768, 480);
            tempEditor->setSize(960, 600);
        }

        // Detailed GUI component, QuickStart, Knob & MotionField tests
        DriftProcessor uiProc;
        std::unique_ptr<DriftEditor> editor(static_cast<DriftEditor*>(uiProc.createEditor()));
        check(editor != nullptr, "DriftEditor creation");
        check(editor->getWidth() == 960 && editor->getHeight() == 600, "DriftEditor default dimensions 960x600");

        // Verify all 9 knobs, units, tooltips, double-click defaults, and presentation-layer formatting/parsing
        for (std::size_t i = 0; i < drift::Count; ++i) {
            const auto param = static_cast<drift::Param>(i);
            const auto& spec = drift::parameterSpecs[i];
            auto& knob = editor->knobFor(param);
            auto& slider = knob.sliderComponent();
            check(slider.isDoubleClickReturnEnabled(), "knob double-click reset enabled");
            check(std::abs(slider.getDoubleClickReturnValue() - static_cast<double>(spec.initial)) < 1e-5,
                  "knob double-click default matches spec");
            check(slider.getTooltip().isNotEmpty(), "knob tooltip non-empty");
            check(slider.getTextValueSuffix() == juce::String(" ") + spec.unit, "knob unit suffix");
            check(std::abs(slider.getValue() - static_cast<double>(spec.initial)) < 1e-4,
                  "knob initial value matches spec");
        }

        // Validate reading, typing, and rounding on Motion, Center, Feedback, and 0-100% percentage controls
        {
            auto& motionSlider = editor->knobFor(drift::Motion).sliderComponent();
            check(motionSlider.getTextFromValue(0.70) == "0.70 Hz", "Motion default text formatting");
            check(motionSlider.getTextFromValue(2.346) == "2.35 Hz", "Motion rounding to 2 decimals");
            check(std::abs(motionSlider.getValueFromText("3.25 Hz") - 3.25) < 1e-5, "Motion parsing with unit");
            check(std::abs(motionSlider.getValueFromText("1,50") - 1.50) < 1e-5, "Motion parsing comma decimal");
            check(std::abs(motionSlider.getValueFromText("99.0") - 10.0) < 1e-5, "Motion upper bound clamp");

            auto& centerSlider = editor->knobFor(drift::Center).sliderComponent();
            check(centerSlider.getTextFromValue(8.0) == "8.00 ms", "Center default text formatting");
            check(centerSlider.getTextFromValue(14.284) == "14.28 ms", "Center rounding to 2 decimals");
            check(std::abs(centerSlider.getValueFromText("11.5 ms") - 11.5) < 1e-5, "Center parsing with unit");
            check(std::abs(centerSlider.getValueFromText("0.05") - 0.30) < 1e-5, "Center lower bound clamp");

            auto& feedbackSlider = editor->knobFor(drift::Feedback).sliderComponent();
            check(feedbackSlider.getTextFromValue(0.12) == "12 %", "Feedback default 12% formatting");
            check(feedbackSlider.getTextFromValue(0.65) == "65 %", "Feedback max 65% formatting");
            check(feedbackSlider.getTextFromValue(0.2405) == "24 %", "Feedback fractional percentage rounding down");
            check(feedbackSlider.getTextFromValue(0.246) == "25 %", "Feedback fractional percentage rounding up");
            check(std::abs(feedbackSlider.getValueFromText("38 %") - 0.38) < 1e-5, "Feedback parsing 38%");
            check(std::abs(feedbackSlider.getValueFromText("38%") - 0.38) < 1e-5, "Feedback parsing 38% without space");
            check(std::abs(feedbackSlider.getValueFromText("95") - 0.65) < 1e-5, "Feedback clamp at 65% (0.65)");

            auto& depthSlider = editor->knobFor(drift::Depth).sliderComponent();
            check(depthSlider.getTextFromValue(0.50) == "50 %", "Depth 50% formatting");
            check(depthSlider.getTextFromValue(0.354) == "35 %", "Depth rounding 35.4% -> 35%");
            check(depthSlider.getTextFromValue(0.356) == "36 %", "Depth rounding 35.6% -> 36%");
            check(std::abs(depthSlider.getValueFromText("78 %") - 0.78) < 1e-5, "Depth parsing 78%");
        }

        // Verify Quick Starts (SUBTLE, WIDE, DRIFT) apply, sync, persist, and allow user edits afterward
        auto& qs = editor->quickStartComponent();
        auto& mf = editor->motionFieldComponent();
        for (std::size_t q = 0; q < drift::ui::quickStartPresets.size(); ++q) {
            qs.applyPreset(q);
            mf.stepSimulation(1.0 / 30.0);
            check(qs.activePresetIndex() == static_cast<int>(q), "Quick Start active preset highlight");

            const auto& preset = drift::ui::quickStartPresets[q];
            for (std::size_t p = 0; p < drift::Count; ++p) {
                const float rawVal = uiProc.state.getRawParameterValue(drift::parameterSpecs[p].id)->load();
                check(std::abs(rawVal - preset.values[p]) < 1e-3f, "Quick Start APVTS value applied");
                const auto& slider = editor->knobFor(static_cast<drift::Param>(p)).sliderComponent();
                check(std::abs(static_cast<float>(slider.getValue()) - preset.values[p]) < 1e-3f,
                      "Quick Start updated attached knob");
            }

            // Verify host state save/restore after Quick Start
            juce::MemoryBlock qsState;
            uiProc.getStateInformation(qsState);
            DriftProcessor restoredProc;
            restoredProc.setStateInformation(qsState.getData(), static_cast<int>(qsState.getSize()));
            for (std::size_t p = 0; p < drift::Count; ++p) {
                const float orig = uiProc.state.getRawParameterValue(drift::parameterSpecs[p].id)->load();
                const float rest = restoredProc.state.getRawParameterValue(drift::parameterSpecs[p].id)->load();
                check(std::abs(orig - rest) < 1e-5f, "Quick Start state round-trip");
            }
        }

        // Modifying a parameter after Quick Start clears active Quick Start highlight
        uiProc.state.getParameter("mix")->setValueNotifyingHost(0.11f);
        mf.stepSimulation(1.0 / 30.0);
        check(qs.activePresetIndex() == -1, "Quick Start highlight clears on manual edit");

        // Verify MotionField allocation-free simulation and response to parameter changes in LIVE
        const float sampleBefore = mf.trajectorySample(0, 0, 48);
        uiProc.state.getParameter("depth")->setValueNotifyingHost(1.0f);
        uiProc.state.getParameter("chaos")->setValueNotifyingHost(1.0f);
        uiProc.state.getParameter("width")->setValueNotifyingHost(1.0f);
        const auto allocBeforeStep = allocations.load();
        mf.stepSimulation(1.0 / 30.0);
        check(allocations.load() == allocBeforeStep, "MotionField stepSimulation zero allocations");
        const float sampleAfter = mf.trajectorySample(0, 0, 48);
        check(std::isfinite(sampleAfter) && std::abs(sampleAfter - sampleBefore) > 1e-4f,
              "MotionField responds to parameter changes in LIVE");

        // Verify HOLD mode behavior:
        // 1. Trajectories freeze when HOLD is active.
        // 2. Parameter snapshots, excursion readouts, and Quick Start highlights still update during HOLD.
        mf.setAnimationEnabled(false);
        check(!mf.isAnimationEnabled(), "MotionField animation pause (HOLD)");
        const float frozenSample = mf.trajectorySample(0, 0, 48);
        const float frozenRightSample = mf.trajectorySample(2, 1, 72);

        qs.applyPreset(0); // Apply SUBTLE while in HOLD
        const auto allocBeforeHoldPoll = allocations.load();
        mf.stepSimulation(1.0 / 30.0);
        check(allocations.load() == allocBeforeHoldPoll, "MotionField HOLD poll zero allocations");
        check(qs.activePresetIndex() == 0, "Quick Start highlight synced during HOLD");
        check(std::abs(mf.currentParameters()[drift::Center] - 6.50) < 1e-3, "MotionField snapshot updated during HOLD");
        check(mf.trajectorySample(0, 0, 48) == frozenSample && mf.trajectorySample(2, 1, 72) == frozenRightSample,
              "MotionField trajectories remain frozen during HOLD");

        // Manual parameter edit during HOLD clears Quick Start highlight and updates readout while keeping trace frozen
        auto* centerParam = uiProc.state.getParameter("center");
        centerParam->setValueNotifyingHost(centerParam->convertTo0to1(20.0f));
        mf.pollParameters();
        check(qs.activePresetIndex() == -1, "Quick Start highlight clears on manual edit during HOLD");
        check(std::abs(mf.currentParameters()[drift::Center] - 20.0) < 1e-2, "Center readout updated during HOLD");
        check(mf.trajectorySample(0, 0, 48) == frozenSample, "Trajectories still frozen after pollParameters in HOLD");

        // Re-enabling LIVE immediately updates trajectories to match current parameters
        mf.setAnimationEnabled(true);
        check(mf.isAnimationEnabled(), "MotionField animation resume (LIVE)");
        check(std::abs(mf.trajectorySample(0, 0, 48) - frozenSample) > 1e-4f,
              "Re-enabling LIVE syncs trajectories to updated parameters");

        // Verify real sample rate in MotionField across 44.1, 48, 88.2, and 96 kHz
        {
            // Fallback when invalid sample rate is passed
            uiProc.prepareToPlay(0.0, 256);
            mf.pollParameters();
            check(std::abs(mf.effectiveSampleRate() - 48000.0) < 1e-3, "Safe fallback sample rate 48 kHz");

            // At minimum Center (0.30 ms) and max Depth (100%), DigitalFractional's 4.0 / sr minimum delay
            // constrains the excursion differently at each host sample rate.
            centerParam->setValueNotifyingHost(centerParam->convertTo0to1(0.30f));
            uiProc.state.getParameter("depth")->setValueNotifyingHost(1.0f);
            auto* motionParam = uiProc.state.getParameter("motion");
            motionParam->setValueNotifyingHost(motionParam->convertTo0to1(0.5f));

            double prevExcursionMs = 0.0;
            for (double sr : {44100.0, 48000.0, 88200.0, 96000.0}) {
                uiProc.prepareToPlay(sr, 256);
                mf.pollParameters();
                check(std::abs(mf.effectiveSampleRate() - sr) < 1e-3, "MotionField reflects host sample rate");

                const double expectedSafetyMs =
                    ((0.30 * 0.001 - 4.0 / sr) * 0.85
                     / drift::DriftEngine::combinedModulationBound(drift::OrganicVariant::Wander)) * 1000.0;
                const double actualExcursionMs = mf.estimatedExcursionMs();
                check(std::abs(actualExcursionMs - expectedSafetyMs) < 1e-4,
                      "MotionField excursion matches DigitalFractional 4/sr limit at sample rate");
                check(actualExcursionMs > prevExcursionMs,
                      "Higher sample rate lowers 4/sr floor and increases available short-center excursion");
                prevExcursionMs = actualExcursionMs;
            }

            // Restore 48 kHz default preparation
            uiProc.prepareToPlay(48000.0, 256);
            mf.pollParameters();
        }

        // Resize bounds tests
        editor->setSize(760, 520);
        editor->setSize(1280, 800);
        editor->setSize(960, 600);

        if (argc > 1) {
            const std::string arg1(argv[1]);
            if (arg1 == "--visual-suite" && argc > 2) {
                const juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile(
                    juce::String::fromUTF8(argv[2]));
                outDir.createDirectory();

                // Reset to factory defaults at 48 kHz in LIVE mode
                uiProc.prepareToPlay(48000.0, 256);
                mf.setAnimationEnabled(true);
                for (std::size_t p = 0; p < drift::Count; ++p) {
                    auto* param = uiProc.state.getParameter(drift::parameterSpecs[p].id);
                    param->setValueNotifyingHost(param->convertTo0to1(drift::parameterSpecs[p].initial));
                }
                qs.syncHighlightWithParameters();
                mf.stepSimulation(0.25);

                // 1. Default 960x600
                editor->setSize(960, 600);
                saveSnapshotPng(*editor, outDir.getChildFile("01_default_960x600.png"));

                // 2. Minimum supported size 760x520
                editor->setSize(760, 520);
                saveSnapshotPng(*editor, outDir.getChildFile("02_minimum_760x520.png"));

                // 3. Expanded size 1280x800
                editor->setSize(1280, 800);
                saveSnapshotPng(*editor, outDir.getChildFile("03_expanded_1280x800.png"));

                // 4-6. Restore 960x600 for DPI scale tests (125%, 150%, 200%)
                editor->setSize(960, 600);
                saveSnapshotPng(*editor, outDir.getChildFile("04_scale_125pct.png"), 1.25f);
                saveSnapshotPng(*editor, outDir.getChildFile("05_scale_150pct.png"), 1.50f);
                saveSnapshotPng(*editor, outDir.getChildFile("06_scale_200pct.png"), 2.00f);

                // 7. Minimum parameter values (Mix 0% Dry Reference)
                for (std::size_t p = 0; p < drift::Count; ++p)
                    uiProc.state.getParameter(drift::parameterSpecs[p].id)->setValueNotifyingHost(0.0f);
                mf.stepSimulation(0.25);
                saveSnapshotPng(*editor, outDir.getChildFile("07_controls_minimum.png"));

                // 8. Maximum parameter values
                for (std::size_t p = 0; p < drift::Count; ++p)
                    uiProc.state.getParameter(drift::parameterSpecs[p].id)->setValueNotifyingHost(1.0f);
                mf.stepSimulation(0.25);
                saveSnapshotPng(*editor, outDir.getChildFile("08_controls_maximum.png"));

                // 9-11. Quick Starts: SUBTLE, WIDE, DRIFT
                qs.applyPreset(0);
                mf.stepSimulation(0.25);
                saveSnapshotPng(*editor, outDir.getChildFile("09_quickstart_subtle.png"));

                qs.applyPreset(1);
                mf.stepSimulation(0.25);
                saveSnapshotPng(*editor, outDir.getChildFile("10_quickstart_wide.png"));

                qs.applyPreset(2);
                mf.stepSimulation(0.25);
                saveSnapshotPng(*editor, outDir.getChildFile("11_quickstart_drift.png"));

                // 12. HOLD with modified controls (proves frozen trajectory from SUBTLE while controls/readouts update)
                qs.applyPreset(0);
                mf.stepSimulation(0.25);
                mf.setAnimationEnabled(false); // Freeze trace on SUBTLE
                uiProc.prepareToPlay(96000.0, 256);
                uiProc.state.getParameter("center")->setValueNotifyingHost(
                    uiProc.state.getParameter("center")->convertTo0to1(22.50f));
                uiProc.state.getParameter("depth")->setValueNotifyingHost(0.88f);
                uiProc.state.getParameter("feedback")->setValueNotifyingHost(
                    uiProc.state.getParameter("feedback")->convertTo0to1(0.52f));
                uiProc.state.getParameter("dynamics")->setValueNotifyingHost(0.64f);
                mf.pollParameters();
                saveSnapshotPng(*editor, outDir.getChildFile("12_hold_modified_controls.png"));

                mf.setAnimationEnabled(true);
                uiProc.prepareToPlay(48000.0, 256);
            } else {
                // Reset to factory defaults for single snapshot
                uiProc.prepareToPlay(48000.0, 256);
                mf.setAnimationEnabled(true);
                for (std::size_t p = 0; p < drift::Count; ++p) {
                    auto* param = uiProc.state.getParameter(drift::parameterSpecs[p].id);
                    param->setValueNotifyingHost(param->convertTo0to1(drift::parameterSpecs[p].initial));
                }
                qs.syncHighlightWithParameters();
                mf.stepSimulation(0.25);
                editor->setSize(960, 600);
                saveSnapshotPng(*editor, juce::File::getCurrentWorkingDirectory().getChildFile(
                    juce::String::fromUTF8(argv[1])));
            }
        }

        std::cout << "PASS: JUCE state, version 0.2.0, percentage/unit presentation, editor, Quick Starts, MotionField LIVE/HOLD, real sample-rate excursion, mono/stereo/upmix, variable blocks and zero callback allocations\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
