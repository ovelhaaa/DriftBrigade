# M2.7 — internal eight-voice BBD engine integration

## Scope and evidence labels

M2.6 was merged as d72605212421c56b47d4b1377f5bbe3bc7b188e6 after the full Actions matrix, Linux ASan/UBSan and headroom artifact passed. M2.7 starts from that commit.

**PAPER-BACKED:** fixed-stage variable-clock transport, Holters/Parker Table 1 asynchronous filters and 570/571 detector architecture, with the source qualifications and discretization limitations in M2.1/M2.5.

**QUALIFIED PREVIOUS MILESTONE:** M2.2 synthetic character/noise, M2.4 normalized nonlinear transfer, M2.5 OutsideFilters host-observation compander and M2.6 internal gain staging. No second qualification-only DSP implementation exists: `BBDFullPath` is the neutral name of the same reusable path; the old qualification name is a compatibility alias.

**ENGINEERING INTEGRATION CHOICE / NOT PRODUCT DEFAULT:** backend selection, 1024-stage full research fixture, per-band shared L/R seeds, ExternalWetReturn feedback topology and conservative excursion admission.

## Backend and signal flow

The production/default `DigitalFractional` branch retains `ModulatedDelayVoice`, Hermite interpolation, its existing ±16 safety clamp and 5 Hz output DC blocker. Its arithmetic order and dynamics/modulation/filterbank laws are preserved. The plugin never calls the internal BBD selector. Parameter IDs, defaults, presets, schema and UI are unchanged.

`setDelayBackend(ExperimentalBBD, config)` is accepted only before the first `prepare()`. Subsequent attempts return false without changing configuration; create another engine to choose another backend. Repeated prepare/reset of the same backend is supported. Stage storage allocates only in prepare. No lazy preparation, stage resize, switching, crossfade or reconstruction happens in processing.

Each of four bands on each channel has a dedicated `BBDModulatedDelayVoice`:

band input + feedback × previous wet → compressor → asynchronous input filter → compressorToBBDGain → capture/noise → fixed-stage variable-clock transport → reference-normalized M2.4 transfer → loss/mismatch/output noise → one-clock hold → asynchronous output filter → bbdToExpanderGain → expander → previous-wet memory → 5 Hz output DC blocker → engine wet sum.

ExternalWetReturn uses the reconstructed/expanded signal **before** the DC blocker, with one host-observation feedback memory, matching M2.6. The DC blocker remains outside the loop. This is an engineering qualification topology, not a universal historical circuit claim. No digital ±16 clamp was copied. Existing qualified finite-domain numerical handling is retained; the voice sanitizes nonfinite final wet values and counts such events only in instrumented builds. Qualified tests require zero numerical-guard activations.

## Explicit fixture

`BBDVoiceConfig::fullResearchFixture()` selects 1024 physical stages, Table1 filters, compander enabled, .47 uF / 10 kohm, engineering polynomial strength 1, the shared M2.2 synthetic fixture (output noise RMS 1e-4, input noise zero, stage loss 1e-5, leakage .1 per stage-second, residual pole .25 per 1024 and mismatch .001), with M2.6 Nominal gain staging (.25 into BBD / 4 out). Seed 570 combines with the engine seed. This is a **QUALIFICATION FIXTURE / ENGINEERING NORMALIZATION / NOT PRODUCT DEFAULT**. `linearReference()` disables character, nonlinearity and companding while retaining the asynchronous filters. Neither is a shipping BBD sound.

## Delay capabilities and admission

For N physical stages and host rate Fs, fclock = N/(2D), physical edge rate = 2fclock, and the scheduler ceiling of 128 edges per host sample gives Dmin = N/(128Fs). Core Dmax = N/2 seconds (1 Hz minimum edge rate); the engine retains its 55 ms guard. The digital engine retains its existing interpolation-safe 4/Fs minimum and 55 ms maximum.

| Stages | 44.1 kHz ms | 48 kHz ms | 88.2 kHz ms | 96 kHz ms | 192 kHz ms |
|---|---:|---:|---:|---:|---:|
| 512 | .090703 | .083333 | .045351 | .041667 | .020833 |
| 1024 | .181406 | .166667 | .090703 | .083333 | .041667 |
| 2048 | .362812 | .333333 | .181406 | .166667 | .083333 |
| 4096 | .725624 | .666667 | .362812 | .333333 | .166667 |

512/1024 stages admit all existing centers at every qualified rate. 2048 limits centers at 44.1/48 kHz; 4096 limits them at 44.1/48/88.2/96 kHz. Center support alone is not full trajectory feasibility. The three right-modulation coefficients have squared sum one, so Cauchy-Schwarz gives a tighter magnitude bound sqrt(3) times the source bound. The complete pre-M2.7 protected envelope is conservatively bounded below by .3ms − .85×(.3ms−4/Fs)×sqrt(3)/2. Against this envelope: 512 is FULL_RANGE at all rates; 1024 is FULL_RANGE at 88.2/96/192 kHz and LIMITED_SHORT_DELAY at 44.1/48; 2048 is FULL_RANGE at 192 kHz only; 4096 is LIMITED_SHORT_DELAY at all listed rates. Outside this conservative guarantee, unrestricted trajectories can be EVENT_LIMIT_EXCEEDED; the BBD engine admits them explicitly. The feasibility CSV distinguishes center support from this envelope guarantee. The 1024 fixture is qualified for the admitted engine range, with explicit short-excursion reduction, not exact replication of every digital trajectory.

For BBD only, the engine first admits Center into the physical range, then limits excursion to max(0, min(requested, .85×(center-min)/bound)). `bound=combinedModulationBound(variant)` is unchanged. The existing 55 ms target guard is preserved; supported stage counts have a much larger core maximum. The same sqrt(3) coefficient bound keeps the current protected modulation envelope below 55 ms, so no additional upper-excursion reduction is introduced. Reset admits initial delay history too. The existing quarter-host-sample slew is preserved, with no additional BBD clock smoother. Requested/actual excursion and actual delay telemetry describe the admitted trajectory. `physicalLimitSamples` reports samples with a reduced excursion or admitted center; this is envelope limiting, not a count of pointwise hard clipping. Core clamp is only the final safeguard.

## Modulation, dynamics, mono and noise

Wander remains production default. The same Motion, Depth, Chaos, Coherence, Width and Dynamics laws drive all eight BBD voices. Depth=0 with a fixed Center keeps the effective clock constant. The engine supplies delay once per sample; clock tracking is checked against N/(2×trace.delaySeconds), without another slew stage.

A deterministic integer hash derives each band seed from engine seed XOR fixture base seed, band index and a fixed BBD tag. No channel index enters the hash. Complete matching band input/delay histories therefore give matching L/R character streams; different bands use different streams. Width=0 and mono input remain strictly mono. This is an **ENGINEERING INTEGRATION CHOICE**, not final stereo decorrelation. Reset reseeds both intrinsic noise sources and mismatch deterministically without allocation.

Dynamics uses the unchanged external envelope, independently of the two compander detectors. Chaos=.2×Dynamics×intensity added to Chaos; Feedback=.08×Dynamics×intensity added to Feedback; wet prominence=1−.55×Dynamics×(1−intensity). The Dynamics CSV compares these scalars directly against digital and requires zero difference. Mix=0 remains exact dry for finite qualified inputs.

The prompt's .75 is the engine guard ceiling. Current user Feedback is at most .65 and Dynamics adds less than .08, so actual parameter-driven operation approaches .73 rather than .75. The engine CSV records attained feedback; a separate direct voice test covers .75 without changing the product parameter domain. A sustained program is not a decay test; impulse/transient/burst fixtures include silence afterward. H2–H5 in the feedback table are whole-observation projections, not stationary THD. Noise-seeded late energy does not prove a limit cycle. Stability conclusions are limited to the listed stimuli, amplitudes and durations.

## Multiband interaction and measurements

Gentle/Selective banks are unchanged. The eight asynchronous filter paths make algebraic unprocessed band reconstruction insufficient to predict processed reconstruction. The band CSV measures per-band input/output RMS, summed wet RMS/peak/DC for impulse, low/mid/high sine, deterministic white noise, percussion, harmonic/program signals and a logarithmic sweep. Complex sine transfer is measured across low/mid/high and crossover frequencies; it includes delay phase and fixture nonlinearity/noise. No compensating EQ or bank winner is selected.

Coherence/Width are swept at 0,.25,.5,.75,1, with delay/clock/wet/noise-only correlation and peak/RMS balance. The 8×8 noise matrix is measured with zero input; matching L/R band histories are checked for exact equality, and different band streams for nonidentity. Stereo modulation gets a separate matrix. Correlation values and timing are measurements, not product targets.

## Tests, telemetry, performance and artifact

The immutable pre-M2.7 engine oracle is copied from merged M2.6; only class/telemetry/dynamics type names and header include paths changed. Production digital dependencies remain unchanged. Regression checks bit identity with changing all nine parameters, mono/stereo, Gentle/Selective and all three organic variants. Both backends render blocks 1,17,32,64,127,256,511,1024 identically and replay after reset. Always-run tests cover finite states, physically admitted delays, zero hidden clamps, quarter-sample slew under Center automation, Mix=0, Depth=0, mono fixtures and callback allocations. No tolerance replaces bit equality.

All added measurement counters and per-band taps are behind `DRIFT_BBD_INSTRUMENT`; production does not collect operating distributions. Per-voice telemetry exports requested/effective delay, clock/events, capture RMS/peak/occupancy, both detectors and held output. Engine aggregation tracks max clock/events/internal peak, minimum nominal occupancy and total edges. CPU benchmarks execute actual stereo `process()` including bank, modulation, eight companders/filter/character/nonlinear/feedback paths and wet summing. Rates 44.1/48/96 kHz, blocks 32/64/128/256/512, stages 512/1024/2048, normal and heavy fixtures are included; matched digital timing supplies the CPU ratio. Timing retains clock/event counters but disables operating distributions and bucket timestamps; bit identity with instrumentation enabled/disabled is checked. Timing is not an absolute CI gate.

The delay grid has 4,800 cells, each with a 256-sample onset observation. It checks admission across the requested Motion/Depth/Center grid, not full-period extrema for each cell. The range/tracking CSVs also record actual capture-to-output bucket residence using measurement-only timestamp storage allocated in prepare. Nominal N/(2fclk) is a control quantity, distinct from historical variable-clock residence and full-path group delay. Eq.1 yields (N-1)/(2fclk) at fixed clock; this is explicitly tested. Residence is nan when an onset observation has not transported a bucket yet. Tracking separately observes at least two Motion cycles in all variants (minimum 2 s, 40 s at .05 Hz). Conservative analytical amplitude bounds ensure trajectory admission beyond those finite observations. CPU records 250 ms of audio plus 1024 warmup samples per case; single-run timings need replication on target hardware. Clock/event extrema and averages include warmup.

`drift_bbd_engine_qualification --tests-only` runs numerical gates; supplying an output directory generates the twelve requested CSVs, per-voice telemetry, fixed-delay spectral/direct-feedback measurements and README. `bbd-engine-qualification` uploads **DriftBrigade-M2.7-BBD-Engine-Qualification**. All previous CI jobs remain, and the numerical executable is included in the Linux ASan/UBSan matrix. Local combined ASan/UBSan is not claimed for the existing MinGW installation; acceptance requires green Linux CI.

```sh
cmake -S . -B build -DDRIFT_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/drift_bbd_engine_qualification output/DriftBrigade-M2.7-BBD-Engine-Qualification
```

## Measured local qualification results

**ENGINEERING INTEGRATION CHOICE / NOT PRODUCT DEFAULT:** Windows Release measurements on an AMD Ryzen 7 7730U, with the exact fixture above. These are finite observations, not calibration to a physical IC. The complete artifact has 15 CSVs plus README; Linux/macOS/Windows CI independently regenerates/gates the relevant results.

- Digital regression: 12 changing-parameter bank/variant/mono configurations, 12,000 samples each, bit-identical to merged M2.6. Both backends: eight block sizes, exact reset replay, repeated prepare/state replay and zero callback allocations. All 18 DSP tests and the plugin state/routing test passed locally.
- Delay admission: 4,800 onset cells; zero hidden scheduler clamps. Tracking: 18 variant/Motion cases, two cycles each; maximum clock formula error 0 Hz and maximum 9 edges per host sample. Nominal tracking delays span 2.4524–13.0226 ms; measured historical bucket residence spans 2.4500–13.0099 ms. Fixed Center with Depth=0 has constant clock.
- Engine feedback: all 60 noise-on/off, feedback/stimulus cases remained finite with zero hidden clamps and zero numerical-guard activations. Maximum attained coefficient .722727 (DC burst); maximum summed-wet peak 1.05147, internal capture peak 1.60868 and minimum nominal occupancy .9999478. The DC burst exposes a pre-blocker loop DC of .17004 pooled across voices: the output DC blocker does not remove DC inside ExternalWetReturn. Its noise-off late/early output energy falls 65.06 dB in the measured windows; this does not prove unrestricted stability. Direct full-path voice feedback .75 also passes: maximum sine-burst output peak .78515 and internal peak .30967; no limiter or guard was activated.
- Noise: identical band histories at Width=0 yield correlation 1 and maximum L/R difference 0. Maximum absolute cross-band correlation .02599 in the two silence matrices. Shared band seeds do not force equality when L/R clock histories diverge. Stereo grid Width=0 gives delay/clock/noise-only correlation 1 even though its program inputs deliberately differ; mono equality is separately tested with identical inputs. At Coherence=Width=1, measured delay/clock correlation is .16613/.14455, wet correlation −.06483. These values are fixture observations, not universal stereo targets.
- Dynamics macro comparisons against digital have zero error at 0/.5/1. Actual wet output differs because the backend changes.
- Fixed-delay multiband spectral result: BBD wet gain is .7623/.7597/.6038 at 100/500/3000 Hz in Gentle, versus approximately 1 in digital. Gentle H2–H5 THD is .738%/.151%/.116%; Selective is .276%/.0415%/.615%. Paired-noise SNR ranges 61.52–64.04 dB across these sine cases. Complex transfer/crossover/sweep measurements are in the artifact. No EQ compensation or bank winner was chosen.

Actual eight-voice `process()` CPU, 512/1024/2048 stages, all five block sizes and both normal/heavy configurations:

| Rate | CPU seconds per audio second, min–max | Median BBD/Digital ratio |
| --- | --- | --- |
| 44.1 kHz | .1744–.2397 | 16.71× |
| 48 kHz | .1868–.3915 | 16.52× |
| 96 kHz | .2660–.4606 | 13.24× |

The 1024-stage, block-128 normal/heavy CPU factors are .2121/.2112 at 44.1, .3169/.2341 at 48 and .3560/.3620 at 96 kHz. Timing retains instrumentation clock counters, disables capture distributions/timestamps, and includes every audible DSP stage. Each case times only 250 ms at the default Center (8 ms), after 1024 warmup samples; host scheduling variance is visible. These measurements are not the maximum-clock short-delay worst case, a callback-deadline guarantee or a cross-platform performance claim. Profile sustained runs and short-delay corners before exposing BBD in the plugin.

## Remaining product decisions

User-facing backend mode, plugin parameters/state, live switching/migration/crossfade, final stage count/headroom/gain/capacitor/feedback topology, feedback limiter, coupled continuous BBDTerminals solver, clock feedthrough/whine, IC/pedal calibration, UI/presets and subjective Digital/BBD or M1.1 variant/bank winner remain deferred. Next milestone should refine the measured operating envelope and startup/feedback behavior on target hardware before proposing any user-facing mode. Numerical finite fixtures and headroom occupancy do not establish an unrestricted circuit model or product-ready sound.
