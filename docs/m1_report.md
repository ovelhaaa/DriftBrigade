# M1 qualification and delivery

## Delivered files

- Foundation: `CMakeLists.txt`, `.gitignore`, `README.md`, `.github/workflows/build.yml`, generated-name template `Source/ProductConfig.h.in`.
- JUCE glue/editor: `Source/PluginProcessor.{h,cpp}`, `Source/PluginEditor.{h,cpp}`.
- Parameter specification/state glue: `Source/params/ParameterSpecs.h`, `Parameters.{h,cpp}`.
- JUCE-free DSP: `Source/dsp/DriftEngine.{h,cpp}`, `OrganicModulator.{h,cpp}`, `MultiscaleBank.h`, `FractionalDelay.h`, `EnvelopeFollower.h`, `ParameterSmoother.h`, `DepthMapping.h`, `DspMath.h`.
- Qualification: `tests/DspTests.cpp`, `tests/PluginTests.cpp`, `tests/AllocationTracker.h`.
- Offline analysis: `tools/Analysis.cpp`, optional `tools/plot_analysis.py`.
- Design/research: `docs/research_notes.md`, `docs/architecture.md`, this report. Original five PDFs are unchanged.

Builds, extracted PDF text, plots, CSV and WAV outputs are ignored generated artifacts; regenerate them using the README commands.

## Architecture and controls

Linked input envelope -> four complementary frequency bands per channel -> one principal digital fractional-delay path per band -> recombination -> dry/wet. Shared, band-independent and stereo-independent seeded modulation sources feed delay times in seconds. Feedback stays within each contained delay path. The input dry path is never hard-gated. Host latency is zero.

OrganicModulator generates low-rate uniform random targets and interpolates them by raised cosine at audio rate. Chaos morphs a continuous periodic source into that random process with expected-variance normalization. Coherence uses `C*common+sqrt(1-C²)*independent`; Width adds partially correlated right motion. Dynamics uses a 15/250 ms envelope plus 50 ms control smoothing for moderate Chaos/feedback increases and quiet wet reduction. See architecture for formulas, conservative headroom and future BBD boundary.

| Parameter | Exact user range | Default |
|---|---|---|
| Motion | 0.05–10 Hz, logarithmic | 0.7 Hz |
| Depth | 0–100% | 50% |
| Center | 0.3–30 ms, logarithmic | 8 ms |
| Chaos | 0–100% | 55% |
| Coherence | 0–100% | 45% |
| Dynamics | 0–100% | 25% |
| Feedback | 0–65%, dynamic total <=75% | 12% |
| Mix | 0–100% | 50% |
| Width | 0–100% | 60% |

Depth represents a rate-guided nominal time excursion (zero to 5 ms at slowest Motion, zero to 366.67 us at 9–10 Hz), further limited by Center and the worst-case control magnitude. The UI does not promise a constant physical depth across rate/center. Every macro has 50 ms smoothing and saved/restored APVTS state.

## Measured core qualification

Local Windows x64, CMake 4.1.1, GCC 14.2 Release and Clang 19.1.1 Debug. All core tests pass. Supported rates **44.1 / 48 / 88.2 / 96 kHz** were actually exercised, including mono processing, 512 parameter-endpoint combinations per rate, maximum feedback with silence/impulse/full-scale sine and float-range safety. Parameter-event timing is held constant in block-segmentation comparisons.

| Sample rate | Reconstruction RMS error | Peak error | Peak observed in safety tests |
|---|---|---|---|
| 44.1 kHz | 1.53910e-17 | 2.22045e-16 | 2.71275 |
| 48 kHz | 1.52441e-17 | 2.22045e-16 | 2.83352 |
| 88.2 kHz | 1.68363e-17 | 1.11022e-16 | 2.72779 |
| 96 kHz | 1.68841e-17 | 1.11022e-16 | 2.85563 |

The bank test directly recombines impulse/broadband input without delay processing. These near-machine-precision errors are relative to the actual input sample, not an all-pass reference. Separate locked-multiscale/fullband equivalence confirms that identical moving delay paths reconstruct the fullband modulated delay to roundoff. Feedback output can exceed unity; boundedness is not a promise of automatic loudness normalization or a final output brick-wall limiter.

- Random endpoint slopes: numerical endpoint differences below 3e-6 per normalized phase unit. Actual random-segment boundary step <=1.42268e-6 in the automation capture. Maximum morph sample step under abrupt target changes: 0.00135909. No audio-rate white-noise injection is used.
- Chaos centered RMS over a 20 s capture at 9 Hz: 0.707107 / 0.708974 / 0.709568 / 0.705484 / 0.700404 at Chaos 0 / .25 / .5 / .75 / 1. This verifies approximate depth stability, not short-window perceptual equivalence.
- Left band 0/1 correlation over 20 s, Motion=8 Hz, Chaos=1: 0.0749599 / 0.311618 / 1.000000 at Coherence 0 / .5 / 1. Short or slow captures need not match these estimates.
- Same seeds produce bit-identical output within each build; different seeds differ. Seeds reset with lifecycle prepare/reset, not transport.
- Blocks **1, 17, 64, 127, 256, 511, 1024, 16384** produce bit-identical core output at each rate, including sample-timed Motion/Center/Chaos/Coherence/Depth automation.
- Ordinary C++ allocation counting detects no allocation during core processing. Delay interpolation reproduces affine content across repeated ring wraps; all endpoint delay reads satisfy interpolation/capacity guards.
- Maximum-feedback silence is quiet; impulse and sine tails decay below test threshold after three seconds. Wet DC blocker rejects steady DC without changing dry output. Output remains finite for finite float-range input.
- Dynamics loud test: envelope 0.725264, effective Chaos 0.375765 from base 0.2, effective feedback 0.190306 from base 0.12. Maximum per-sample wet-prominence movement during release is 1.13497e-5. Dynamics=0 is level independent; Width=0 gives identical wet controls/output for identical stereo input.
- Mix=0 reproduces dry float samples exactly when initialized at zero. Automation to zero settles through the specified smoothing, rather than bypassing instantly.

## Build and plugin qualification

JUCE is pinned to **7.0.12** by commit. JUCE 8.0.6 was initially attempted but rejects the only usable installed compiler family, MinGW; using 7.0.12 permits a native local plugin build without modifying framework sources. The DSP remains independent of JUCE and uses C++17.

Both **VST3 and Standalone built successfully locally** with GCC/MinGW Release. `ctest --test-dir build-plugin --output-on-failure` passes **2/2 targets**: complete core qualification and JUCE integration. Integration checks normalized mapping, saved/restored state, invalid state handling, mono/mono and stereo/stereo at all four rates, mono/stereo dry duplication, blocks 0/1/17/127/1024/8192, finite audio and editor construction/resizing. Callback-thread allocation counting detects no ordinary C++ heap allocation during `processBlock`; a global counter initially counted unrelated JUCE timer-thread activity and was corrected to thread-local measurement. The editor snapshot was visually inspected; a footer reserve keeps controls clear of JUCE's default attribution overlay.

Artifacts: `build-plugin/DriftBrigade_artefacts/Release/VST3/DRIFT BRIGADE.vst3/` and `build-plugin/DriftBrigade_artefacts/Release/Standalone/DRIFT BRIGADE.exe`. MinGW binaries require its runtime DLLs on PATH; MSVC CI artifacts use that toolchain's runtime conventions. JUCE 7 emits a MinGW DirectWrite warning and uses its fallback text renderer; this did not prevent building or rendering the editor.

GitHub Actions additionally covers core Windows/macOS/Linux, Linux ASan/UBSan and a Windows MSVC plugin build. CI configuration is not itself evidence those hosted jobs passed. Third-party host loading was not exercised locally.

Local sanitizer linking was attempted, but this installed Clang package omits its Windows ASan runtime libraries. Therefore **no local ASan/UBSan pass is claimed**. Ordinary Clang Debug core qualification passes; sanitizers are configured for the Linux CI job.

## Offline artifacts and CPU

`drift_analysis` exports eight seconds of organic control, raw raised-cosine control, all eight band modulation/delay streams, input envelope, effective dynamics controls and sampled audio to CSV, plus audio-rate stereo WAV. Fixed seed and unchanged output gain make comparisons reproducible. `locked` and `diffuse` runs use the same short Center and zero Width; `organic` / `periodic` compare motion styles; `dynamics` captures changing playing intensity with headroom for Chaos to increase. Optional plots have no plugin dependency.

GCC Release isolated timing, 480 k stereo frames at 48 kHz, excludes file I/O: approximately **0.125–0.167 s for 10 s audio**, or **1.25–1.67% of one CPU core** on this machine while other build work was running. This is an observation, not a cross-machine guarantee or measured host/GUI overhead. Debug builds and denormal handling differ; the JUCE callback uses ScopedNoDenormals.

## Required design questions and unresolved concerns

**A. More locked/comb-like at Coherence=1?** The control correlation and direct fullband-equivalence test establish locked behavior at Width=0. Independent bands break a common delay law at Coherence=0, as the supplied multiscale paper describes. WAV A/B is available. Perceptual clarity on real instruments still requires listening; no unperformed listening verdict is claimed.

**B. High Chaos irregular without steps/noise?** Numerically nonperiodic and C1 with bounded trajectories, small boundary differences and stable RMS. There is no white-noise audio path. Subjective motion character, high-frequency interpolation artifacts and maximum-depth sound require listening.

**C. Dynamics responds without pumping?** Envelope and controls visibly respond smoothly to input intensity; tests verify release and level independence at zero. Audible pumping is a listening criterion and remains to be checked with playing material.

**D. Transparent recombination?** Yes numerically, with the reconstruction errors above and no latency/phase mismatch. The wet DC blocker and cubic fractional interpolation are separate intentionally non-identity operations. The gentle bank has broad overlap, so band independence is less sharply localized than with steeper crossovers.

**E. Stable across blocks?** Yes: bit identity for the listed core segmentation sizes and sample-timed automation. Hosts that quantize automation differently per callback can still differ because JUCE macro values are read once per block.

**F. Ready for BBD replacement?** The contained physical-time `DelayPath` boundary keeps filterbank/modulators independent and needs no bank rewrite. A clocked implementation must own transit/clock history, filters and latency; it is not accomplished by replacing an interpolation formula alone.

Remaining musical qualification: real-instrument listening, mono fold-down across Width, preferred depth mapping, envelope timing, and third-party host scans/session automation. M1 is numerically qualified; the claim that its identity is musically more interesting than a normal chorus/flanger is intentionally not asserted without that audition.

## Intentionally deferred to M2 or later

Real BBD stages/variable clock and asynchronous sampling; sample-and-hold, alias/images, clock-dependent bandwidth, detailed anti-alias/reconstruction circuits and insertion gain; compander, transistor nonlinearities, BBD noise; SSB elastic modulation, rotary speaker and pitch transposer; elaborate preset browser and final visual identity. No dead controls for these systems were added.
