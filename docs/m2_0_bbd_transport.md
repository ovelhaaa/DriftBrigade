# M2.0 clocked BBD transport

## Scope and architecture

M2.0 adds an internal, objective transport foundation; it does not select a sound. The production `ModulatedDelayVoice` continues to own `DigitalFractionalDelay`, its existing feedback limiter, and DC blocker. `ClockedBBDCore` is compiled and tested but is not reachable from plug-in parameters, presets, or UI. No Organic, bank, or Dynamics bake-off decision changes.

The experimental flow is `BBDInputFilter` (identity placeholder) -> fixed-stage `ClockedBBDCore` -> `BBDOutputHold` (temporary zero-order hold). These named boundaries reserve the clock-dependent filtering/reconstruction work without pretending it exists in M2.0. Feedback remains a surrounding-voice responsibility.

## Timing and two-phase interpretation

The paper-backed relation is

`D = N / (2 f_clock)`, hence `f_clock = N / (2 D)`.

A physical BBD advances charge using two non-overlapping clock phases. M2.0 abstracts each half-clock advancement into one whole-chain transfer event, so `f_event=2*f_clock` and `D=N/f_event`. This explicitly represents, rather than hides, the factor of two. The abstraction does not model phase overlap, charge-transfer loss, or clock feedthrough.

For host sample `k`, scheduler phase is updated as `p'=p+f_event/f_host`; `floor(p')` events execute and `p=p'-floor(p')`. Phase is a double and is never reset by `setDelaySeconds`. A delay change therefore affects future event spacing while stage memory remains untouched. Delay is clamped only to the architecture's 128 events/host-sample real-time ceiling and a 1 Hz non-stopped clock floor.

## Stage representation and real-time properties

A preallocated circular array is logically equivalent to shifting N cells. Its head is the oldest/final stage. An event retires that value, replaces the cell with current conditioned input, and rotates the head. Cost and copied data are O(1) per event independent of N. `prepare` owns allocation; `process` is `noexcept`, takes no locks, performs no I/O/logging, and allocates nothing. Telemetry distinguishes the raw finite caller request from the architecture-limited instantaneous nominal delay and exposes a clamp flag, effective clock, N, remainder phase, per-sample events, and total events. The instantaneous nominal delay is derived from the current clock; it is not labeled as an actual historical transit time under clock modulation.

## Objective qualification

`drift_bbd_tests` covers N=256/512/1024/2048/4096 at 44.1, 48, 88.2 and 96 kHz, using 3, 10, and 30 ms constant-clock targets. Measured leading-edge transit agrees with `N/(2*f_clock)` to at most one host sample (the strict quantization bound of the temporary host-rate observation). Block sizes 1, 17, 64, 127, 256, 511 and 1024 produce bit-identical output for a sample-timed delay sweep. Fixed-clock million-sample runs accept at most one-event floating-point boundary ambiguity and show no accumulating drift. Tests also cover event rates below, equal to, and far above host rate, variable-clock event/phase continuity, retained stage contents on a delay step, raw-versus-effective delay telemetry, finite silence/impulse/sine/deterministic broadband/maximum-float processing, and zero process allocations. Actual historical marker transit time during a variable-clock sweep is not currently measured; only scheduler continuity and the instantaneous clock-derived nominal delay are qualified.

The headless `drift_bbd_qualification` emits `bbd_constant_clock.csv`, `bbd_delay_sweep.csv`, `bbd_event_timing.csv`, `bbd_stage_matrix.csv`, and `README.txt`. It reports failure unless the output directory is valid and every artifact opens, writes, flushes, and closes successfully. The sweep is a 440 Hz sine with a 3–30 ms linear delay ramp and records the requested delay, instantaneous clock-derived nominal delay, clock, event count, and held output; it does not claim to measure historical transit timing and is not an auditory rating. Constant-clock CSV rows report 48 kHz stereo wall time, combined event throughput, and relative CPU for each N. Since a 10 ms target makes event rate proportional to N, total cost rises with event count, while the transport operation itself remains O(1) in stage count. Exact wall times are machine-dependent and intentionally live in the generated artifact rather than this document.

A representative Linux Release run of the fixed 10-second stereo workload produced the following (CI artifacts are authoritative for their runner):

| stages | processing seconds | combined events/second | relative CPU |
|---:|---:|---:|---:|
| 256 | 0.0140 | 36.6 M | 1.00 |
| 512 | 0.0144 | 71.1 M | 1.03 |
| 1024 | 0.0159 | 128.7 M | 1.14 |
| 2048 | 0.0166 | 246.7 M | 1.19 |
| 4096 | 0.0194 | 422.8 M | 1.38 |

Each row's accumulated event-count error was -1 event after 480,000 host samples, the bounded binary floating-point boundary ambiguity documented by the test; it does not grow as drift. Measured delay error was bounded by one host sample at every qualified rate/stage/target combination. All tested block segmentations were bit-identical.

## Limitations and deferred M2.x work

Deferred exactly: final anti-alias input filter; final reconstruction filter; sample/hold spectral refinement; BBD noise; clock feedthrough; insertion loss; compander; transistor, per-stage, and other nonlinear distortion; saturation; analog feedback coloration; oversampling policy; final product stage count; user Clock control; production BBD mode; presets; UI; and every M1.1 subjective selection. The host-rate input is held across events and the output is a temporary ZOH, so M2.0 makes no final sound-quality or alias-rejection claim. Macro smoothing remains outside the core; intentional clock steps are not smoothed internally.
