# M2.3 exact asynchronous BBD runtime optimization

Baseline: M2.2 commit `9bf785e28542be8511fdcf3e9e86ecf78c8f1ad5`.
Branch: `codex/m2-3-exact-async-bbd-runtime`. Production continues to use
`DigitalFractionalDelay`; this work changes the qualification-only BBD path.

## Baseline and frozen oracle

M2.2 advanced both analogue filters at every physical half-clock edge and at
the host interval end. Each filter independently evolved five complex poles
using `std::exp(std::complex<double>)`, including both members of each conjugate
pair. For E physical edges and S host samples, the Table 1 core performed
`10*(E+S)` complex exponential calls. M2.2's same-machine proxies attributed
most comparable runtime to these exponentials, especially at high clock rates.

`tests/reference/` preserves the original filter, core and device character in
`drift_reference`. The only changes are namespace/include integration and
read-only state snapshots behind the qualification macro. Its equations and
operation order are frozen. The oracle is linked only into the new performance
qualification executable, never into `drift_dsp` or the plugin.

## Real pole and conjugate pair representation

The unchanged Table 1 profiles contain one real pole and two conjugate pairs.
The new runtime stores a real scalar state and two pairs of double states.
Poles/residues remain complex for configuration and `response()`, while impulse
injection, state propagation and output summation use ordinary real arithmetic.
The prototype remains a single real pole.

For a real pole p and residue r:

```
e = exp(p*dt)
x' = e*x + (r/p)*(e-1)*u
```

For a stored member `p=a+jb`, `r=c+jd`, precompute `q=r/p=qr+jqi`.
Let `er=exp(a*dt)*cos(b*dt)` and `ei=exp(a*dt)*sin(b*dt)`:

```
kr = qr*(er-1) - qi*ei
ki = qr*ei + qi*(er-1)
xr' = er*xr - ei*xi + kr*u
xi' = er*xi + ei*xr + ki*u
pair output = 2*xr
```

An impulse of area A adds `r*A` to the state. `AsyncAnalogTransition` stores
`e`, `(r/p)*(e-1)`, and each pair's `er,ei,kr,ki`. Applying it uses only
arithmetic. Three real exponentials and two standard sin/cos pairs construct a
Table 1 transition, instead of five independent complex exponentials. Fixed
arrays provide all storage. A transition belongs to its configured profile;
configuration invalidates the core's cached transitions.

## Independent filter event ownership

At each host start, inject the unchanged input impulse `sample*hostDt`.
The input filter advances directly to captures, then to the host end. It does
not advance for output-phase edges. The output filter advances under its
previous held input directly to output updates, changes its driving hold after
that advancement, then advances to the host end. It does not advance at
captures. Thus each process call still finishes with both filters at the host
boundary, preserving impulse ownership and variable-clock history.

Physical scheduling, phase accumulation, event timestamps, capture/output
counts, ring storage, N-1 edge onset, one-period hold and `N/(2*fBBD)` delay are
unchanged. Combining successive zero-input or constant-input intervals follows
the exact continuous-time semigroup; floating-point operation order changes
are measured against the frozen oracle.

## Period, host and clock caches

Within one host call the physical clock is constant. After the first event of
each phase, reuse the cached transition for `1/fBBD`. Only the first and last
relevant boundary intervals construct arbitrary transitions. At most four
boundary transitions are constructed per host call, regardless of edge count.

`prepare()` caches `1/hostSampleRate` transitions. A filter with no relevant
events uses its host transition. Configuration rebuilds profile-dependent
transitions. `setDelaySeconds()` always updates requested/effective telemetry;
only an exact double change in effective clock rebuilds the two period
transitions and updates clock-dependent character gain. Clamped requests that
map to the same clock therefore reuse caches. No epsilon or clock quantization
is used. Reset preserves valid coefficients/caches while deterministically
clearing signal/character/RNG state as before.

## Character coefficient hoisting

At character prepare, precompute
`10^(insertionDb/20)*exp(-N*lossPerStage)` and the residual pole
`exp(-1024/(residualPolePer1024*N))`. Clock updates multiply the constant gain
by `exp(-N*leakagePerStageSecond/(2*clock))` only when leakage is enabled.
Zero controls avoid pow/exp entirely and retain exact bypass. Gain factorization
is algebraically equivalent to the old combined exponent; its tiny rounding
difference is covered by differential gain/audio/state checks. Seeded draws,
noise scaling, mismatch and residual memory recurrences are unchanged.

## Numerical qualification

The tool writes `async_filter_reference_error.csv` for all three filter profiles,
impulse/DC/sine/seeded random/alternating maximum audio inputs, and host, BBD,
fractional, tiny, long and seeded random intervals. It alternates explicit
transition application and convenience `advance()`. The unit-amplitude
absolute gate is **1e-11**. It records absolute, relative and RMS differences.

Maximum finite *audio* means float max, matching the existing core's input
clamp. Absolute roundoff at that amplitude cannot meet a unit-amplitude 1e-11
gate; stress cases separately report raw absolute/RMS errors and gate the
error divided by float max at the same 1e-11 tolerance. This is an explicit
amplitude-scaled stress criterion, not a loosened normal-signal gate. Double
max would overflow the frozen filter's residue arithmetic and is outside the
core's accepted input amplitude.

`bbd_core_equivalence.csv` covers 1120 cases: four host rates, five stage counts,
two asynchronous profiles, four character modes and seven controls (the five
requested trajectories plus low-edge and near-maximum-edge probes). Ordinary
cases run 8192 samples; maximum-edge cases run 1024, enough to fill even the
4096-stage history many times. The **1e-9** gate covers output, hold, filter
outputs and every expanded real/imaginary pole state; gain uses **1e-12**.
All event counts, last capture/output timestamps, phase and delay/clock telemetry
must match exactly. Every bucket is compared periodically. Identical noise
seeds and draws are required. Values and filter states are checked for finiteness.

## Structural acceptance and instrumentation

`AsyncOperationCounts` is enabled only in a separately compiled qualification
executable. Production macros compile away, with no counters, atomics, allocation,
logging or added state. The harness counts state advances, transition builds,
period applications, arbitrary builds, real exp and sin/cos pairs, plus separate
character exp/pow calls. The old filter count is derived exactly from its frozen
loop; old character update costs are separately reported.

The deterministic probes cover 0.1/1/4/16/127.9/128 edges per sample, fixed and
changing clocks. Acceptance bounds are four builds per fixed host sample,
six including a clock change, and 12/18 filter real exp calls respectively.
State propagation may grow with events; construction does not. Same-clock
identity and neutral-character zero-transcendental checks are additional gates.

For 4096 host samples at 4, 16 and 128 edges/sample, fixed-clock builds stay
**16384**, real exp calls **49152**, sin/cos pairs **32768**. Old complex exp
calls grow **204800 / 696320 / 5283840**. The call units differ: one complex exp
contains its own scalar/transcendental work, while a sin/cos pair is two calls.
The relevant structural result is bounded construction, not treating these
units as equivalent. The maximum-edge old loop advances filters 1056768 times,
versus 532480 in the new loop; the new loop reuses period transitions 516096 times.

## Analytical and realtime validation

All original M2.1/M2.2 tests remain: impulse, step, convolution, Table 1 response,
hold/alias/image measurements, segmentation/history, character/noise spectrum,
feedback, allocation and stream fail-safe checks. New differential evidence
supplements these mathematical tests. The new harness additionally counts
allocations around 10000 changing-clock process calls. Its positive numerical
CTest runs under the existing sanitizer configuration.

MinGW Clang's native TLS access to the existing inline thread-local allocation
counter crashed inside `operator new`, confirmed with a debugger. Allocation-test
executables now use `-femulated-tls` for MinGW Clang only. This workaround changes
no production DSP and restored the Clang tests. GCC/MSVC/other Clang toolchains
are unaffected.

Local ASan/UBSan remains **unverified**: installed MinGW Clang lacks the ASan
import/runtime libraries, as already documented for M2.2. Linux sanitizer CI
must pass before sanitizer-clean milestone acceptance. MSVC/macOS validation
is configured in preserved CI jobs and has not been executed locally.

## Benchmarks and eight-core scaling

`bbd_performance.csv` and `bbd_clock_modulation_performance.csv` compare frozen
old/new paths on the same machine: 48kHz stereo, 256–4096 stages, 3/10/30ms,
transport/Table1 ideal/loss/full, fixed or slow continuous modulation. Each
result is the median of five 250ms audio trials, including per-sample delay
calls. Realtime factor is **wall/audio**, so lower is better. The benchmark
checksum prevents removal of the audio work. Qualification counters add some
overhead to the new implementation; no fast-math is enabled.

`bbd_multivoice_performance.csv` directly measures eight independent cores at
512–4096 stages, 10/30ms, ideal/full. A matching stereo benchmark reports the
actual CPU scaling ratio. This measures the future eight-core BBD workload;
**extrapolation** to a complete four-band stereo engine must add crossover,
modulation and mixing cost to this measured floor. No final stage count is
selected. Timing is machine-dependent evidence and is never a CI gate.

## Artifacts and CI

Target: `drift_bbd_performance_qualification`.
Artifact: **DriftBrigade-M2.3-BBD-Performance-Qualification**.
Files: the six requested CSVs plus `README.txt`. Streams are checked at open,
flush and close; blocked output paths/artifacts and Linux `/dev/full` are tested.
`--numerical-only` skips timing, never numerical or structural gates. The existing
DSP Windows/macOS/Linux, sanitizer, Windows plugin, bake-off, M2.1 and M2.2 jobs
are preserved. The new M2.3 job depends on DSP and sanitizer acceptance and
uploads the complete artifact, with no wall-time threshold.

## Remaining bottlenecks and M2.4

At high clocks, event scheduling, bucket/character recurrence and cached state
propagation still grow with physical events. Exact boundary transition builds
remain a fixed cost; continuously modulated clocks add two period builds and
possibly one leakage exp per core per sample. No host-rate biquad replacement,
interpolation substitute, reduced precision, LUT, polynomial exp, fitted poles
or fast-math was used, because this milestone preserves the exact model.

For M2.4, profile this exact path on intended target hardware and investigate
additional exact boundary/cache reuse or event-loop simplifications before
considering approximate transcendental backends. Keep any future approximation
as an explicitly separate, differentially qualified experiment. Device
calibration and any nonlinear/compander mechanisms require separate validation;
production routing, feedback coloration, stage selection, UI and subjective
choices remain deferred.

## Local results — 2026-10-03

Machine: Windows, AMD Ryzen 7 7730U with Radeon Graphics. MinGW GCC 14.2 Release. Benchmark run completed without other build/test jobs running. Each row is the median of five 250ms trials.

- Unit-scale maximum filter absolute error: **4.440892098500626e-16**.
- Maximum core output error: **1.7430501486614958e-14**; RMS in that worst-peak case: **6.7672835732446054e-15**.
- Maximum expanded pole-state absolute error: **2.93653990013354E-14**. Scheduling telemetry matched exactly in all 1120 cases.
- Alternating float-max stress: raw maximum absolute error **1.5111572745182865e23**, normalized by float max **4.440892363198438e-16**. Finite state maintained.
- Realtime allocation counts unchanged during process and continuously changing delay.

Table 1 ideal speedup, fixed / continuously modulated:

| Stages | 3ms | 10ms | 30ms |
|---:|---:|---:|---:|
| 256 | 5.30x / 3.45x | 8.42x / 3.61x | 15.57x / 3.39x |
| 512 | 7.38x / 4.86x | 6.24x / 3.54x | 10.48x / 3.34x |
| 1024 | 12.94x / 8.35x | 5.37x / 3.52x | 7.50x / 3.57x |
| 2048 | 21.98x / 14.43x | 8.52x / 5.68x | 5.67x / 3.52x |
| 4096 | 35.58x / 26.32x | 14.63x / 9.66x | 6.42x / 4.15x |

Stereo wall seconds per 250ms audio, Table 1 ideal:

| Stages | Delay | Old fixed | New fixed | Old modulated | New modulated |
|---:|---:|---:|---:|---:|---:|
| 256 | 3ms | 0.041823 | 0.007898 | 0.039783 | 0.011524 |
| 256 | 10ms | 0.023851 | 0.002832 | 0.023388 | 0.006478 |
| 256 | 30ms | 0.021272 | 0.001366 | 0.018566 | 0.005472 |
| 512 | 3ms | 0.068287 | 0.009258 | 0.063889 | 0.013147 |
| 512 | 10ms | 0.031283 | 0.005017 | 0.029929 | 0.008447 |
| 512 | 30ms | 0.020693 | 0.001974 | 0.020655 | 0.006177 |
| 1024 | 3ms | 0.121691 | 0.009404 | 0.112072 | 0.013420 |
| 1024 | 10ms | 0.047216 | 0.008790 | 0.044513 | 0.012647 |
| 1024 | 30ms | 0.026032 | 0.003470 | 0.025177 | 0.007057 |
| 2048 | 3ms | 0.225976 | 0.010281 | 0.208852 | 0.014471 |
| 2048 | 10ms | 0.080867 | 0.009497 | 0.077571 | 0.013663 |
| 2048 | 30ms | 0.036602 | 0.006452 | 0.035016 | 0.009937 |
| 4096 | 3ms | 0.448541 | 0.012607 | 0.421726 | 0.016023 |
| 4096 | 10ms | 0.146307 | 0.009997 | 0.135277 | 0.014002 |
| 4096 | 30ms | 0.059440 | 0.009252 | 0.055901 | 0.013466 |

Eight-core wall/audio realtime factor, ideal / full character:

| Stages | 10ms | 30ms |
|---:|---:|---:|
| 512 | 0.0796 / 0.0810 | 0.0315 / 0.0321 |
| 1024 | 0.1418 / 0.1469 | 0.0562 / 0.0576 |
| 2048 | 0.1476 / 0.1512 | 0.1033 / 0.1105 |
| 4096 | 0.1604 / 0.1683 | 0.1493 / 0.1778 |

Measured eight-core/stereo CPU ratios span 3.95–4.52; most are close to 4.0. **Extrapolation:** at 4096 stages/10ms, a four-band stereo engine would cost approximately 0.1604 (ideal) or 0.1683 (full character) wall/audio for its BBD cores, plus separately unmeasured crossover/modulation/mixing overhead. This is not a production-engine measurement or a stage-count recommendation.

Full CSVs include all transport/loss/full rows, per-second events/builds/transcendentals and character counts. Local artifact directory: `output/DriftBrigade-M2.3-BBD-Performance-Qualification`.


Final local checks: **GCC Release 10/10 CTests**, **Clang Debug 10/10**,
**Windows plugin Release 11/11** passed. Standalone and VST3 rebuilt. All
independent analytical and allocation checks passed. ASan/UBSan linking remains
blocked by missing local runtime libraries; Linux CI is required for that
acceptance criterion. Remote CI has not been run in this task. The frozen oracle
was compared back to the baseline commit after stripping only the documented
integration/read-only observation changes; all four files matched exactly.
