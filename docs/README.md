# Documentation

[Architecture](architecture.md) · [Research ledger and papers](research_notes.md) · [Repository audit](repository_consolidation.md)

## Milestones and decisions

- M1 digital architecture and listening — [M1](m1_report.md), [M1.1](m1_1_report.md), [M1.1 protocol](m1_1_listening_guide.md).
- M2 experimental BBD — [transport](m2_0_bbd_transport.md), [linear sampling](m2_1_bbd_linear_resampling.md), [device character](m2_2_bbd_device_character.md), [runtime](m2_3_bbd_runtime_optimization.md), [nonlinearity](m2_4_bbd_nonlinearity.md), [compander](m2_5_bbd_compander.md), [headroom](m2_6_bbd_gain_headroom.md), [engine](m2_7_bbd_engine_integration.md), [realtime hardening](m2_8_bbd_realtime_hardening.md), [operating envelope](m2_9_bbd_product_envelope.md).
- M3 listening protocols — [M3.0](m3_0_listening_guide.md), [M3.1B.1](m3_1b_organic_voice_listening_guide.md). In generated packages, record scores before opening the answer key, manifest or technical reports.
- Technical reports and historical listening record — [M3.0](m3_0_bbd_listening_bakeoff.md), [M3.1B.1](m3_1b_organic_voice_retest.md), [listening decisions](m3_1_listening_decisions.md). The project owner selected Wander and closed the comparison. PaperNarrowband and PhaseDrift remain research references.
- M3.1 product architecture — [musical architecture](m3_1_musical_architecture.md), [implementation plan](m3_1_implementation_plan.md), [admission and fallback](m3_1c_bbd_product_admission.md). DigitalFractional/Wander/Gentle is the public configuration. Balanced1024 is experimental and still requires callback and DAW qualification.

## Validation

Run the full DSP suite with the README commands. Use `ctest --test-dir build-dsp -C Release -R bbd_product_admission` for admission and `-R 'dsp_qualification|bbd_listening_numerical|bbd_engine_numerical'` for Digital regressions. The JUCE build adds `plugin_state_and_routing`. The numerical suites include frozen references and C++ callback allocation tracking. For ASan/UBSan, use a separate Debug build with `-DDRIFT_SANITIZE=ON -DDRIFT_BUILD_PLUGIN=OFF` and a Clang/GCC toolchain with the sanitizer runtimes.

`drift_analysis` exports audio and telemetry for optional Python plotting. `drift_bakeoff` renders M1.1. `drift_bbd_*_qualification` targets generate DSP qualification reports. For production-path performance use `drift_bbd_realtime_production_timing output/realtime --comparison --ci` and the M2.9 report's production timing procedure. Remove the CI workload option for dedicated runs and record machine conditions.

## Reproduce the blind listening packages

Run from a Release DSP build. Append `.exe` on Windows and add the configuration subdirectory for multi-config builds.

```sh
build-dsp/drift_bbd_listening_bakeoff output/m30
build-dsp/drift_bbd_listening_bakeoff output/m30-repeat
python tools/verify_bbd_listening_bakeoff.py output/m30 --compare output/m30-repeat
build-dsp/drift_bbd_listening_bakeoff --voice-retest output/m31b1
build-dsp/drift_bbd_listening_bakeoff --voice-retest output/m31b1-repeat
python tools/verify_bbd_listening_bakeoff.py output/m31b1 --voice-retest --compare output/m31b1-repeat --original output/m30
```

Open the packages in the [Listening Lab](../tools/listening-lab/README.md). GitHub Actions publishes the M1.1, M2 qualification and M3 listening bundles with repository-default retention, verified at 90 days on the consolidation workflow. Build trees, WAVs and generated CSVs belong under ignored directories such as `build-dsp/` or `output/`.

## M3.1D host qualification

See the [host validation report](m3_1d_host_validation.md) and [manual DAW procedure](m3_1d_daw_test_guide.md). The internal VST3 uses a separate build flag and plug-in identity.

## Offline M1 telemetry

Run `drift_analysis output/organic.csv 0.45 1 1` in the Release DSP build. Historical comparisons use `1 0.7 0 flange` for locked, `0 0.7 0 flange` for diffuse, `0.45 0 0` for periodic, `0.45 0.3 1` for dynamics and `0.45 1 1 raw-depth` for the internal depth-mapping comparison. Each run exports eight seconds of stereo WAV and 1 kHz telemetry. The WAV has a fixed 0.4 gain. Use `python tools/plot_analysis.py output/organic.csv` with matplotlib to inspect the telemetry.
