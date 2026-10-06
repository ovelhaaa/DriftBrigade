# M2.8 — Realtime worst case, recovery and long-run hardening

**ENGINEERING ROBUSTNESS / ENGINEERING PRODUCT CRITERIA / NOT PRODUCT DEFAULT.**
The base is merged M2.7.1, `1e54be5a9e6889e81869533397c2f57cd2a8519f` (PR #8).
Its explicit instrumentation setting survives reset and remains independently
qualified. M2.1–M2.7 signal models are **QUALIFIED PREVIOUS MILESTONE** fixtures.
DigitalFractional remains the production/default backend. No parameter, UI,
preset, final stage count, gain profile, capacitor, noise calibration or feedback
topology is selected here.

## Scope and reproduction

Build Release with `DRIFT_BUILD_PLUGIN=OFF`, then run:

```text
drift_bbd_realtime_qualification --tests-only
drift_bbd_realtime_qualification artifact --ci
drift_bbd_realtime_qualification local-artifact --local
drift_bbd_realtime_production_timing local-artifact --comparison --local
python tools/summarize_bbd_realtime.py local-artifact
```

`--timing-only`, `--reports-only`, `--denormals-only` and
`--denormals-high`, `--feedback-only`, `--startup-only`, `--recovery-only`
allow independent investigations. A partial run's README
describes the selected duration policy, not confirmation that every report
finished. A nonzero exit means numerical or artifact I/O failure. Every output
stream enables fail/bad exceptions and explicitly flushes. README is flushed
before expensive processing; CTest exercises blocked directory, blocked stream
and Unix `/dev/full` failures.

Local provenance: Windows, AMD Ryzen 7 7730U (8 cores/16 logical processors),
MinGW GCC, `-O3 -DNDEBUG`, seed 77. CPU observations concern this machine and
load at measurement time. Audio duration is simulated sample time; processing
does not sleep to synchronize with wall time. GitHub runners provide separate,
short, informational measurements.

## 1. True model clock boundary

All six rates (44.1, 48, 88.2, 96, 176.4 and 192 kHz), all four physical stage
fixtures (512, 1024, 2048, 4096), and Dmin, 1.05Dmin, 1.25Dmin, 2Dmin, 1, 3,
8 ms are measured where valid. Dmin = N/(Fs*128), clock = N/(2D).
At Dmin, 128 physical edges/sample/voice are reached. At 192 kHz this is
12.288 MHz per voice and 196.608 million edges/s across eight voices, half
capture and half output edges. Numerical tests require caps+outs=edges and no
hidden clamp. These are physical edge counts, not 128 audio captures.

A `DRIFT_BBD_REALTIME_QUALIFY`-only, pre-prepare center hook reaches Dmin below
the public Center minimum. It changes no parameter range and does not bypass
the engine's physical admission or quarter-sample slew. Thus model-boundary
measurements are explicitly broader than current host-selectable settings.

## 2. Callback distributions

The actual stereo `DriftEngine::process()` is timed, after 1024 warm-up samples.
Input generation, allocation checks, state scanning, CSV writing and percentile
sorting are outside the stopwatch. Buffers and duration arrays are preallocated.
Blocks 16/32/64/128/256/512/1024 are crossed with every rate, stage and load.
Full-matrix runs use 32 local or 8 CI callbacks per cell; they identify expensive
regions, but their tail percentiles are coarse. Six representative cases per
backend use 10,000 local or 256 CI callbacks. Percentiles use nearest rank
`ceil(p*n)-1`. Raw records retain deadline, duration/ deadline, event delta,
capture/output delta, maximum observed voice clock and events/sample.

## 3. Timing overhead and loads

Detailed core operating distributions are explicitly disabled, including after
reset; tests assert their count/transport count remain zero. Minimal qualification
builds retain engine clock/event aggregation and guard/clamp counters. The
auxiliary executable omits `DRIFT_BBD_INSTRUMENT`, its types and aggregation;
intrinsic production scheduler counters remain. Its clock is sampled at callback
end, not claimed to be the intrablock maximum. Counter mode and observation
method appear in every timing file. Operating distributions are never used to
time the DSP.

Normal uses the requested .7/.5/8/.55/.45/.6/.25/.12/1 macro fixture; heavy uses
Motion 10, Depth1, Chaos1, Coherence0, Width1, Dynamics 1, Feedback .65, Mix1.
Max clock uses Depth 0, Feedback 0 at Dmin; max clock+feedback uses Depth 0,
Feedback .65, Dynamics 1 at 1.05Dmin. Max clock+modulation uses the heavy controls
at 1.05Dmin, which admits nonzero excursion. There is no unique shortest real
number greater than Dmin; 5% headroom is this explicit engineering fixture.

## 4. Sustained operation

All six materials (silence, 500 Hz sine, deterministic broadband, program,
repeated transient, alternating loud/quiet) run at nominal, high clock and high
feedback. Local duration is 60 s per cell; CI is 0.1 s and labelled CI_SHORT.
Monitoring and callback timing repeat the input/seed in separate passes.
The monitor uses double samples; callback input is quantized to float. The
material formula and seed are shared, rather than claiming bit-identical states
between precision variants. Strict reset/segmentation comparisons use matched
input precision.
Reports include output DC/RMS/peak, internal/nonlinear peaks, detector peaks,
events, phase, maximum callback, clamps and guards. A separate noise-free decay
observes five minutes locally; CI's 0.1 s does not substitute for that evidence.

## 5–6. Feedback and DC

Feedback 0/.25/.5/.65 and maximum macro settings (requested .73, admitted
parameter .65 plus Dynamics 1) run through the real engine. Effective feedback
is measured; the envelope-dependent term is bounded by .08 and the general
guard by .75. A direct full-path voice qualifies .75 separately. Eight
excitations include impulse, positive/negative DC, asymmetric pulse, 100/500 Hz
bursts, broadband and silence. Excitation is finite (1 s locally, .01 s CI),
followed by silence. The silence fixture is an unexcited control.
The maximum-macro fixture scales excitation by500 to approach the engine's
envelope-dependent .73 asymptote through its normal envelope and smoothing,
using admitted finite audio. Effective feedback remains a measured CSV value.

Local checkpoints are cumulative 2/10/30 s; CI .02/.1/.25 s. Noise-on,
noise-off and noise-off/nonlinearity-strength-zero research comparisons isolate
the contribution of asymmetry without changing production coefficients. Return
DC is measured before the existing output-only 5 Hz blocker. Output DC, return
RMS/peak, DC/RMS, bucket-mean snapshots, levels/gains, occupancy and H1–H5
projections are separate columns. Harmonic projections of bursts are not
stationary THD. Window evolution provides DC/loop decay evidence; a strictly
increasing absolute DC over the last eight windows is a review flag, not an
arbitrary absolute DC acceptance threshold. Noise-on decay is compared against
its noise floor. Stable nonzero internal DC remains a product/topology concern.
Additional per-window CSVs retain all eight voices' DC/RMS, bucket snapshots,
gains and RMS/DC change rates. The first window has rate_valid=0; silent
zero-to-zero RMS change reports0 rather than an undefined logarithm. Ordinary
compressor release toward the existing numerical floor is distinguished from
drift under steady excitation.
Continuous growth, detector drift, failure to decay without noise, or approach
to numerical limits is a blocker requiring investigation, not a topology change
hidden in this milestone.

## 7. Eight-voice startup

Silence0/.01/.1/.5/2/10 s precedes -60/-24/0 dB sine, impulse, broadband burst
or program transient. Noise on/off and nominal/max clock are separate. Local
observation is .2 s; CI is .02 s, with exact requested silence durations.
All eight pre-onset detector levels/gains and post-onset internal/nonlinear,
return/wet/expander peaks are retained. Summed wet and post-mix peaks accompany
an M2.6 `BBDFullPath`-based direct voice baseline. The latter receives full-band
input instead of a bank output and independent fixture RNG seed; it is a
comparison of headroom, not a claim of identical waveforms. The baseline
retains a return peak before the DC blocker as well as its
post-blocker output, so the M2.6 path comparison does not hide this distinction.
sum_wet_peak adds all eight voices including L/R; the engine sums four per
channel, and post_mix_peak reports the actual per-channel output peak.
Counters clear at onset; DSP/detector/RNG state does not reset.

## 8–10. Hostile input and recovery

Both signs of 1/2/16/100/1e6/float_max run as impulse/DC/alternating/burst,
followed by valid input and a deterministic reset comparison. Recovery frames
in that CSV mean state finiteness, not completion of ordinary detector release.
All output/state/phase remain finite, and processing/reset allocate nothing.
Both processSample and actual float callbacks are exercised. After extreme
input, 20,000 valid frames verify detector release below the existing unity
startup level. detector_recovery_frames measures that explicit engineering
criterion across all eight voices, separately from immediate state finiteness;
it is not an audio-fidelity tolerance.

The previous generic `bounded(nonfinite, -float_max, float_max)` selected the
negative bound for NaN/Inf, producing a huge excitation instead of silence.
This was reproduced by a failing zero-substitution test. External NaN/±Inf now
admit zero before shared DSP. For both backends, left/right/both injection is
bit identical to an explicitly zero-substituted reference for 2048 frames.
Valid finite DigitalFractional output retains its existing regression oracle.

Qualification-only hooks inject feedback, held output, a live bucket and both
detectors. Hooks are absent from ordinary production builds. Each reports
AUTOMATIC_RECOVERY, VOICE_RESET_REQUIRED or ENGINE_RESET_REQUIRED after 1024 samples.
Artificial state corruption does not justify changing detector equations.
Explicit engine reset is always compared with a fresh seeded engine.

## 11. Reset contract

`reset(seed)` restores bucket memory, scheduler phase, filters, detector state,
character/modulator RNG, feedback and DC state. The existing target parameters,
backend/configuration and explicit instrumentation setting remain configured.
After hostile or injected stress, 512 reference frames match a fresh engine bit
for bit. Segmentation stress separately proves no callback-boundary state reset.

## 12. Prepare failure

Zero, odd, oversized and size_t-max stages reject with the current core policy.
Invalid/nonfinite sample rates and invalid character/gains follow existing
validators. A reproduced inactive-object bug allowed `processSample()` after
failed prepare. Preparation now first clears `prepared`; inactive sample
processing returns silence. Valid re-prepare restores exact fresh-engine
behavior. Prepare may allocate/throw; callback entry remains noexcept and does
not allocate. No partial configuration enters active processing.

## 13–15. Rates, stages and automation

The full timing matrix observes every rate and stage independently, including
176.4 kHz. A paired noise-on/off, fixed8ms, 500 Hz spectral experiment reports
H1–H5, projected THD and noise difference. Local settling/observation is .5/1 s;
CI .02/.02 s observes only10 tone cycles after short settling and is not an
audio-quality decision. Stage selection remains deferred; at Dmin the common event cap, rather
than N, dominates cost, while fixed delay scales clock with N.

All nine controls undergo deterministic steps and ramps through existing
ParameterSmoother architecture. Delay stays admitted and slew <=.25 sample per
sample, with no hidden clamp, reset or allocation. High clock, high feedback,
startup and aggressive automation are strictly bit-identical across
1/7/16/17/32/64/127/256/511/1024 partitions; updates split blocks at the same
sample indices.

## 16. Denormal hardening

Noise-free decay exposed persistent double subnormals in asynchronous filters,
bank, DC and envelope state. Filters reached subnormal tails within 1 s; DC by
25 s and envelope in the five-minute run. Detector/feedback subnormal counts
were zero. Expanded filter snapshots include conjugate views, so their count is
an observation count, not a count of unique stored scalars.

The BBD voice clears only magnitudes below IEEE double minimum-normal after
each sample; the engine does likewise for its BBD bank/control/envelope tails.
Normal values, signed zero, equations, poles and gain/noise coefficients remain
unchanged. No global FTZ/DAZ or arbitrary sonic floor is introduced. Standalone
historical core oracles do not invoke this BBD-voice tail cleanup. Injected
underflow-state tests and five-minute decay validate clearing and finiteness.
Before/after probes on this machine showed nominal block128 median about
.545/.443 ms late in decay and max-clock .001785/.001342 s. These sequential
observations show a cost reduction, not a hardware-independent percentage claim.

## 17–18. Deadline and readiness criteria

Deadline is block/Fs; utilization is duration/deadline; margin its reciprocal.
Bands use p99: <25%, 25–50%, 50–75%, 75–100%, >100%. These are **ENGINEERING
PRODUCT CRITERIA**, not a published paper's guarantee. Zero misses and p99<50%
is REALTIME_COMFORTABLE; zero misses with higher p99 REALTIME_TIGHT; one observed
miss DEADLINE_RISK; at least two DEADLINE_FAIL. Numerical gates abort with
failure (NUMERICAL_FAIL) rather than publishing a false finite row. Even a
comfortable finite run establishes observations, not unrestricted realtime safety.
CI never gates on duration/miss counts. High-rate/minimum-delay deadline failures
are explicit remaining product blockers; no default is altered to hide them.

## 19. Artifact, CI and realtime audit

`DriftBrigade-M2.8-BBD-Realtime-Hardening` contains all 14 required CSVs, README,
raw callbacks, direct/engine feedback evolution, segmentation summary and
production-counter comparison. The standard-library Python summary checks that
all required reports completed, verifies their numerical/allocation gates,
exports all-voice late-tail trends and paired nonlinear asymmetry contribution,
and writes SUMMARY.json. It never rejects a CPU duration or miss count. CI
short tails explicitly lack eight late windows; absence of their trend flag is
not a long-run stability result. Existing milestone jobs/artifacts are preserved.
The new artifact job depends on dsp/sanitize. The short numerical suite is in
CTest, with ASan/UBSan on its instrumented target. Plugin/default-route tests
remain in the existing Windows job. No absolute CPU acceptance is in CI.

Ordinary C++ new/new[] tracking surrounds actual callbacks, sample processing
and reset. The tracker does not intercept every platform allocation API; source
audit complements it: vectors resize only in prepare, no file/log I/O, mutex or
exception originates in processing. Harness files/vectors are outside DSP and
outside timed/allocation-tracked regions. Fault injection is build-only.

## 20. Remaining work and next milestone

Keep BBD internal. A later product-budget milestone should choose a supported
rate/block/delay envelope using measured worst-case deadlines, then separately
address stable loop DC and startup headroom. Backend parameter/crossfade/live
switching, migration, final N/gains/capacitor/noise, DC correction, limiter,
feedthrough, device calibration, solver, UI/presets and listening winner remain
deferred. Local measured tables and CI status are appended after verification.

## Local callback observations

These representative BBD cases each contain 10,000 callbacks. Times below are
milliseconds; misses are observed counts, including OS scheduling interruptions.

| Rate / N / block / load | p50 | p95 | p99 | p99.9 | max | misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 48k / 1024 / 128 / normal | .639 | .650 | .666 | .883 | 1.006 | 0 |
| 48k / 1024 / 128 / heavy | .630 | .647 | .881 | 1.041 | 7.504 | 2 |
| 48k / 512 / 16 / max clock | .184 | .189 | .230 | .320 | .510 | 3 |
| 192k / 4096 / 16 / max clock | .183 | .189 | .208 | .276 | .318 | 10000 |
| 44.1k / 1024 / 32 / max clock feedback | .360 | .367 | .389 | .543 | .787 | 1 |
| 88.2k / 2048 / 16 / max clock modulation | .201 | .206 | .223 | .306 | .416 | 10000 |

The worst full-matrix observed utilization was 386.4%, at176.4k/N1024/block1024
max clock, in a 32-callback cell. All 32 missed. It is a coarse worst observed
sample, not a 10,000-callback percentile or a guaranteed bound. The detailed
raw files and matrix keep misses separated by rate, block, stage and load.
All instrumented cells had zero hidden clamps and numerical guards.
The production-counter companion measured primary/companion median ratios
1.010/1.011/1.024/1.027/1.040/1.011 for these six cases, respectively. These
sequential observations do not isolate frequency/OS variability. 192 kHz and
88.2k boundary cases still missed 100% of deadlines in the companion build;
48k maximum-clock block16 had 63 misses, despite a lower median. Tail differences
must not be attributed solely to instrumentation overhead. The companion has
no instrumented clamp/guard counters; their reserved zero CSV fields represent
compiled-out measurement, with admission verified by the instrumented counterpart.

The 18 sustained 60 s cells finished finite with zero processing allocations,
hidden clamps and numerical guards. Worst internal input peak was1.45835,
output peak.70498 and absolute post-blocker cumulative DC6.28147e-5. Maximum
observed sustained callback was9.0613ms (deadline2.6667ms at48k/block128),
reinforcing that short nominal comfort does not establish worst-case safety.
The sustained CSV's detector maxima concern voice0; all voices undergo finite
state checks, and separate feedback/startup CSVs retain per-voice detector data.

Local CTest passed 20/20, including all previous milestone numerical oracles,
the extended hostile detector release tests and artifact stream failures.
Additional final numerical checks confirmed immediate corruption by each fault
hook and exact fresh wet output after aggressive automation at all six rates.
VST3/Standalone built, and plugin state/default routing passed. Numerical gates
require no hidden clamp for admitted trajectories and zero allocation in the
tested realtime operations. Raw backend 0 denotes DigitalFractional, 1 BBD.

Feedback produced 432 checkpoint rows (120 eight-voice fixtures and24 direct
voice fixtures, each 2/10/30 s). Maximum effective engine feedback was.729963;
the direct voice reached.75. Across all eight engine voices, final noise-free
window DC/RMS were0; noise-on maxima were1.178e-9 absolute DC and4.155e-8 RMS.
No case had strictly growing DC or RMS across its final eight windows. This
supports these tested trajectories, not unrestricted feedback stability.
Paired positive/negative DC burst comparisons found a maximum nonlinear even
contribution.022065 for ordinary-level fixtures (feedback<=.65), versus zero in
the linear fixture. The deliberately scaled100-level maximum-feedback burst
had contribution-11.0208; that extreme is not an ordinary audio calibration.

Startup produced 1152 per-voice rows across 144 fixtures. Largest actual
per-channel post-mix peak was.83908, against.84226 for its direct path fixture;
the corresponding raw direct return peak was.83450. However, some individual
fixtures exceeded their matching direct-path output: the largest ratio1.45605
occurred with a broadband burst after 10 s at nominal clock with noise enabled
(.08092 engine versus.05558 direct). This is a measured multiband/startup
headroom concern for later product work, not a new limiter/gain decision.
Maximum internal/nonlinear startup input peak was1.45854. Adding the pre-DC
baseline measurement preserved every pre-existing startup CSV value exactly.

Host NaN/±Inf recovered automatically with exact zero-substitution equivalence.
Of 15 artificial internal faults, 10 recovered automatically and 5 required voice
reset; none required engine reset after voice reset. Explicit engine reset
always restored fresh seeded output. Worst hostile detector recovery into the
unity startup range was 10032 samples / 209 ms. Both denormal tails (300 s nominal,
60 s max clock) reported zero subnormal observations; interval-median block128
durations were approximately.449ms and1.363ms, respectively.

All 14 CI jobs passed before this artifact-summary/pre-DC-baseline update
(run 37417857581). The final-head CI status and artifact link are recorded in the
PR; CPU misses are informational. No numerical blocker was observed in these
fixtures. High-rate minimum-delay deadline failure and configuration-dependent
startup amplification remain explicit barriers to unrestricted BBD readiness.

### M2.8.1 reporting correction
The hostile CSV recovery_frames field (state_recovery_frames conceptually) is 0 when state was already finite at the end of the hostile segment, positive for the first valid frame restoring finiteness, and -1 if unresolved. detector_recovery_frames independently measures return to the chosen detector operating region. This correction changes qualification reporting only; DSP is unchanged.

