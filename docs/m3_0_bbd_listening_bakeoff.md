# M3.0 — deterministic BBD listening and productization bake-off

**PRODUCTIZATION CANDIDATE · NOT SHIPPING DEFAULT**

M2.9 PR #10 was merged into main as
`dedf4cb76e15a335159cb4c227f66b9ae69224bb` before this branch was created.
Final head `ca41d73` passed all 15 CI jobs, including Linux ASan/UBSan, and
published `DriftBrigade-M2.9-BBD-Product-Envelope`. Final Codex and CodeRabbit
reviews completed without actionable findings. Local Release CTest passed
22/22. This milestone builds on that merged baseline.

M2.9 provides a numerical and machine-specific realtime envelope. M3.0 now
provides musical material for human decisions. It does not choose subjective
winners or change any Source DSP, plugin parameters, state, UI or presets.
DigitalFractional remains the production/default backend.

## Frozen listening candidate

`BalancedListeningCandidate` uses the real eight-voice DriftEngine with 1024
physical stages, HoltersParkerTable1 asynchronous filters, the full M2.2
synthetic character fixture, M2.4 strength-1 engineering polynomial transfer,
enabled M2.5 compander (0.47 uF / 10 kohm), M2.6 Nominal gain, ExternalWetReturn,
and the existing independent 5 Hz output DC blocker. The engine seed is 77 and
BBD fixture seed is 570; existing `bbdBandSeed` derivation is retained.

The Balanced product clock cap is `min(384000, 8*Fs)` with at most 16 physical
edges/sample/voice. Economy uses 512 stages and `min(192000, 4*Fs)`, at most 8
edges/sample/voice. Both have a 1.333333 ms floor at 48 kHz and a 1.451247 ms
floor at 44.1 kHz. 44.1/48/88.2 kHz are sanity-tested here. M2.9 classifies
Economy at 88.2 kHz as experimental; M3.0's short numerical check does not
upgrade that realtime classification. Listening WAVs are only 48 kHz.

## Deterministic dry material

Three sources are generated completely before processing, with fixed analytic
amplitudes and hashed partial phases derived from bake-off seed `0x4d333042`.
No external or copyrighted recordings or audio dependencies are required.

* A: two overlapping four-note chords, ten partials per note, smooth attacks
  and releases, with fixed stereo phase offsets.
* B: repeating eight-note plucked phrases at 0.4375 s spacing, fourteen
  decaying harmonic partials with a 1.2 ms attack.
* C: an eight-note voice-like harmonic lead, eighteen partials shaped around
  700/1700 Hz formants, moderate deterministic dynamics and identical dry
  left/right samples.

Each source lasts 10 seconds (480000 frames): 0.5 seconds silence, excitation
through 7.5 seconds, and 2.5 seconds tail. Dry references are PCM 24-bit encodings
of the same precomputed double fixtures supplied to every candidate. Only PCM
rounding distinguishes a decoded dry reference from the internal fixture.
The mono source is duplicated into a stereo host input so the real stereo
engine generates width. The engine's mono-host flag would instead share the
right modulation with the left; it is not used for this stereo listening task.
C01 applies the same constant source gain 0.125 to both members. Every other
source gain is 1. Sources are finite and peak-safe before processing.

## Scenes and conservative admission

All scenes use real EngineParameters, set before prepare/reset. There is no
center override, product control change or automation during rendering.

| Scene | Motion Hz | Depth | Center ms | Chaos | Coherence | Dynamics | Feedback | Mix | Width |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| subtle | .35 | .22 | 8 | .3 | .65 | .15 | .05 | .35 | .65 |
| wide | .7 | .25 | 12 | .5 | .3 | .15 | .1 | .5 | .95 |
| slow | .15 | .55 | 18 | .8 | .35 | .15 | .12 | .5 | .85 |
| short | .9 | derived, about .0637 | 2.5 | .5 | .45 | .15 | .12 | .5 | .8 |
| feedback | .55 | .28 | 10 | .5 | .45 | .1 | .45 | .5 | .8 |
| wide_high_chaos | .7 | .25 | 12 | 1 | .3 | .15 | .1 | .5 | .95 |

Exact values, including derived Depth, are in `scene_parameters.csv` and the
artifact README and manifest. For short delay, binary inversion of the existing
`depthSeconds` mapping finds the greatest Depth satisfying

`depthSeconds(Motion, Depth) * combinedModulationBound(Wander)
 <= .85 * (CenterSeconds - productFloor)`.

The .85 multiplier is an explicit optional excursion reserve above the product
floor, not a physical clock law. The frozen short scene derives Depth using
44.1 kHz, the most restrictive of the three sanity rates, so the same scene is
valid everywhere. The CSV also records the 48 kHz maximum (about .0708).
Every scene is checked against the COMPLETE conservative bound of its actual
organic variant, including coherence/stereo combination factors, and the .055 s
maximum. Every observed sample/channel/band is also checked against the product
floor and the actual core clock/event cap. A violation fails rendering; it is
never silently repaired by core clamping.

## Independent comparison sets and trajectory fairness

The bake-off contains 12 groups and 26 processed candidates:

| Set | Groups | Processed files | Sole principal factor |
|---|---|---:|---|
| A | A01 pad/subtle, A02 pad/wide, A03 pluck/feedback, A04 mono lead/slow | 8 | DigitalFractional vs full Balanced BBD |
| B | B01 pad/wide, B02 pluck/short | 4 | Economy 512 vs Balanced 1024, with corresponding M2.9 clock budget |
| C | C01 quiet pad/subtle, C02 pluck/feedback | 4 | Conservative vs Nominal operating gain |
| D | D01 pad/wide Chaos .5, D02 mono lead/wide Chaos 1 | 6 | Wander, PaperNarrowband, PhaseDrift |
| E | E01 pad/wide, E02 pluck/subtle | 4 | Gentle vs Selective bank |

All unvaried candidate fields and every scene/source/seed are asserted equal
within a group. A uses current Wander and Gentle defaults. B uses Nominal,
Wander and Gentle. C uses Balanced, Wander and Gentle. D uses Balanced,
Nominal and Gentle. E uses Balanced, Nominal and Wander. HighDrive and optional
feedback DC blocking are excluded. Stage/bank spectra are not compensated.

Center/Depth are fully admitted by both Digital and BBD. Requested and actual
excursions must match exactly; complete `telemetry.delaySeconds[ch][band]`
trajectories must match at every sample, channel and band, including startup
slew. This is tested over full listening files and over every group at
44.1/48/88.2 kHz. Non-modulation sets B/C/E are also trajectory-checked exactly.
Organic variants retain native waveforms and existing bound-aware engine
admission; their waveforms are not independently rescaled. Their measured
modulation RMS/peak, excursions and delay bounds remain in the manifest.

## Level matching and blind labels

Both RAW and LEVEL_MATCHED files are emitted. Matching measures pooled stereo
RMS over the fixed active interval `[0.5,7.5)` seconds, excluding initial silence
and the decay/noise tail consistently. The initial target is the smallest member
RMS. For every member with raw whole-file peak P and active RMS R, the target is
also bounded by `.98*R/P`. The minimum over all members therefore lowers the
COMMON target as necessary. Each whole file receives exactly one constant
scalar `target/R`; no dynamic normalization, limiter or EQ is applied.

Pre-PCM matching error must be <=0.1 dB. The independent Python verifier checks
actual 24-bit decoded RMS agreement to <=0.1 dB and verifies the matched samples
equal a constant-scaled raw file within the proven combined PCM rounding bound.
Raw RMS/peak, applied gain, matched RMS/peak and matching error are recorded.

Blind seed `0x4d333042` (1295200322 decimal) selects a cyclic Latin rotation,
incremented per group within each set. Every pair alternates ordering; the two
three-way groups distribute identities across slots with count difference <=1.
This avoids implementation-dependent library shuffling. X/Y/Z have only local
meaning, with filenames `SET_A01_X.wav`, etc. `ANSWER_KEY.csv` maps every name
to its identity for both raw and matched directories. Manifest/objective files
also disclose identities and must remain closed during blind listening.
The scorecard has empty subjective fields and permits no preference or needs
more listening. The guide does not identify candidate behavior by blind label.

## Telemetry, regression and deterministic gates

`listening_render_manifest.csv` has one row for every WAV, including dry
references. Candidate rows include every control/configuration/seed, source
gain, requested/actual excursion, modulation RMS/peak, delay min/max, clock
min/max and budget, maximum/total physical events, hidden clamps, numerical
guards, callback allocations, internal peak and nominal-domain occupancy, active
output RMS, whole-file output peak/DC, stereo correlation and crest factor.
Digital-only nonapplicable BBD fields are `NA`, not fabricated zeros.

DC is the whole-file pooled arithmetic mean; correlation removes each channel's
mean. Occupancy is total nominal samples divided by total instrumented samples
across all eight cores. Internal peak is the maximum instrumented core operating
peak. Spectral support uses analysis-only one-pole filters at 500/4000 Hz:
low=LP500, mid=LP4000-LP500, high=output-LP4000. These overlapping RMS measures
are not FFT band powers, calibration, EQ compensation or subjective scores.
Five objective comparison CSVs repeat the corresponding raw manifest rows.
Approximate realtime factor is elapsed instrumented render time / audio duration
and includes telemetry; it is informational, not another CPU qualification.

All BBD samples must be finite, unclipped, within the product envelope, with
zero callback-thread C++ allocations, zero hidden clamps and zero numerical
guards. Allocation checks use the existing AllocationTracker around only the
processing loop, including measurement aggregation but excluding preparation,
source generation and file I/O. The existing frozen M2.6 Digital oracle is
unchanged and bit-compared over all full-length Set A cases and their three-rate
sanity checks. M2.7–M2.9 preserve valid finite Digital behavior, making this the
existing merged-baseline reference machinery. No Source files are modified.

Every configuration is repeated in three-rate 0.75 s numerical sanity checks
(including 0.5 s silence and 0.25 s musical onset). Full-length in-process repeats
cover both A01 backends, B02 Economy and all D02 variants. Audio doubles are
bit-identical; trajectories are exact. Additionally, CI and local acceptance
render two complete independent artifacts and compare SHA-256 of ALL 55 WAVs.
PCM24 encoding rounds by `llround(x*8388608)` with explicit finite/range checks;
it never saturates or truncates. WAV headers, exact frame counts, byte lengths,
decoded peak/RMS/DC and constant-gain behavior are independently verified.
Byte determinism is tested within the same build/platform; cross-platform
libm/compiler bit identity is not asserted.

The local 48 kHz artifact measured maximum decoded PCM matching error
`3.4394562e-7 dB`, decoded peak `0.2895331383`, BBD clock `253693.0457 Hz`,
11 physical events/sample/voice and minimum observed BBD delay `2.018187 ms`.
Maximum internal operating peak was `0.2460918572`, with nominal-domain
occupancy 1. All hidden clamps, guards and processing allocations were zero;
the common trajectories were exact. These measurements select no musical winner.

## Build, artifact and CI

```text
cmake -S . -B build -DDRIFT_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target drift_bbd_listening_bakeoff
ctest --test-dir build -R bbd_listening_ --output-on-failure
drift_bbd_listening_bakeoff output/DriftBrigade-M3.0-BBD-Listening-Bakeoff
drift_bbd_listening_bakeoff output/m3_0_determinism_repeat
```

The renderer copies the guide and technical notes from the build's repository
docs directory into each artifact. Then run:

```text
python tools/verify_bbd_listening_bakeoff.py output/DriftBrigade-M3.0-BBD-Listening-Bakeoff --compare output/m3_0_determinism_repeat
```

The CLI accepts an output directory or `--tests-only`. Optional external WAV
input is deferred; generated sources are the acceptance path. CI preserves all
existing jobs/artifacts and adds `bbd-listening-bakeoff`, after DSP/sanitizers,
to build, numerically test, render twice, verify and publish
`DriftBrigade-M3.0-BBD-Listening-Bakeoff`. The existing sanitizer job includes
the new numerical and stream-failure CTests.

The artifact contains 55 stereo PCM24 WAVs (about 158.4 MB uncompressed audio),
`README.txt`, `LISTENING_GUIDE.md`, empty `LISTENING_SCORECARD.csv`,
`ANSWER_KEY.csv`, `listening_render_manifest.csv`, `scene_parameters.csv`, five
objective comparison CSVs, `objective/sanity_rates.csv`, `SHA256SUMS.txt`, and
`VERIFICATION.json`. No build logs are needed to use it.

## Human decisions and later work

The listener chooses independently: Digital or BBD; Economy or Balanced;
Conservative or Nominal; Wander/PaperNarrowband/PhaseDrift; Gentle or Selective.
Different confidence levels, no preference and additional listening are valid.
**No subjective winner was selected.** After the listener reports preferences,
a later milestone freezes the selected musical architecture. M3.0 does not
automatically implement those choices.

Public backend controls, runtime switching/crossfades, state migration, fallback,
sample-rate/tiny-block host policy, production feedback blockers/startup ramps,
limiters, device-calibrated coefficients, clock feedthrough, UI, factory presets
and shipping defaults remain deferred.
