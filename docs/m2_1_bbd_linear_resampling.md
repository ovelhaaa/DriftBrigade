# M2.1 linear BBD transport and asynchronous resampling

## Scope

Internal, objective qualification only. Production `ModulatedDelayVoice` still
owns `DigitalFractionalDelay`; no plugin routing, parameter, UI, preset, feedback,
Organic, Dynamics or M1.1 selection changes. No noise or nonlinear coloration.

## Paper-backed model

Primary reference: Martin Holters and Julian D. Parker, *A Combined Model for a
Bucket Brigade Device and its Input and Output Filters*, DAFx-18 (2018), sections
2-3, Eq.1, Eqs.2-25, Table 1 and Eqs.34-38.
[Publisher PDF](https://dafx.de/paper-archive/2018/papers/DAFx2018_paper_12.pdf).
The supplied local copy is `docs/BBD filters model (1).pdf`. The other supplied
paper, `docs/bbd modeling (1).pdf`, is contextual background, not the source of
this implementation's coefficients.

* Physical transfer edges occur at **2*f_BBD**.
* Input capture occurs on alternating edges, at **f_BBD**.
* Physical stage count N is even; only half the capacitors carry signal.
* Combined output updates on the opposite alternating phase and is held for a
  full BBD period, **1/f_BBD**.
* Signal Nyquist is **f_BBD/2**, not f_BBD.
* Nominal delay is **D=N/(2*f_BBD)=(N/2)/f_BBD**.
* Input and output low-pass filters surround a fixed-length, variable-sample-rate
  signal delay. Their continuous-time state supports asynchronous rate conversion.

There is a significant aperture convention in Eq.1: a capture at edge t_n
appears at the combined output at t_(n+N-1), remains through t_(n+N+1), and
therefore has **N-1 edges of onset transit**. At constant clock the centre of
this rectangular pulse is N edges after capture. Its baseband response is
`exp(-j*2*pi*f*D)*sinc(f/f_BBD)` with `sinc(x)=sin(pi*x)/(pi*x)`.
Claiming both exactly N-edge onset transit and opposite-phase output for even N
would be inconsistent. Tests explicitly separate onset transit, hold width,
nominal centre delay, and host observation quantization. Under variable clock,
centre transit computed with the current half-period is diagnostic only: a future
clock change could alter the remaining hold width.

Known examples are automated: 1024 physical stages / 10 ms means 512 logical
buckets, 51.2 kHz signal sampling and 25.6 kHz Nyquist. 4096 stages / 300 ms means
2048 buckets, 6826.6667 Hz signal sampling and 3413.3333 Hz Nyquist.

## Engineering representation and equivalence

`ClockedBBDCore` preallocates N/2 doubles in a circular array. On a capture edge,
write the current input at head and rotate head. On the following output edge,
read the current head (oldest signal) into the hold. Initial memory is zero.
For L=N/2 and the first input at edge index 0, head advances once every two
edges; that input becomes oldest on capture number L-1, and is read on edge
2*(L-1)+1=N-1. Subsequent inputs and outputs are two edges apart. This exactly
reproduces Eq.1 without representing empty capacitors. For arbitrary clock
spacing the same edge-index proof applies; stored values never move or change
when clock frequency changes. Each edge costs O(1) storage operations, with no
O(N) shift, read-pointer retuning or metadata in audio memory.

The fractional-edge scheduler retains a remainder in [0,1), plus an explicit
capture/output phase bit. Each host interval adds `2*f_BBD/f_host`; every edge is
processed at its actual fractional offset, in order. Changing delay changes
future edge spacing, without resetting this remainder, phase bit, buckets, hold,
filter states or counters. Boundary edges execute before the host observation.
Host input sample k is supplied at k/f_host; process returns output evaluated at
(k+1)/f_host. This end-of-interval observation convention is explicit in CSVs.
TransportOnly holds host input between captures and has no input LPF; this is an
engineering representation of the discrete host source, not ideal bandlimited
reconstruction.

Supported host rates are **[8000,384000] Hz inclusive**. Non-finite or unsupported
values normalize to 48000 Hz *before* any clock calculation and set telemetry's
`hostRateWasNormalized`. Physical stages must be even in [2,65536]; invalid stage
counts throw only during prepare. Delay clamps to a 1 Hz BBD floor and at most
128 physical edges per host sample. Invalid delay requests safely use the maximum
delay. Unprepared processing returns zero. Non-finite input becomes zero, and
finite input is bounded to float range. These are engineering limits, not
claims about a specific device. Finite telemetry/state and process allocation
checks include maximum float input at the edge ceiling. DSP process is noexcept,
with no locks, logging, exceptions, filesystem access or dynamic allocation.

## Asynchronous LTI shell

`AsyncAnalogFilter` represents `H(s)=sum r_m/(s-p_m)` with at most five
preallocated complex states. For a time step dt with held input u:

`x_m(t+dt)=exp(p_m*dt)*x_m(t)+(r_m/p_m)*(exp(p_m*dt)-1)*u`.

An impulse of area A adds `r_m*A`; evaluation sums the real parts. Stable poles
and conjugate pairs are fixed by profile setup. Complex arithmetic directly
retains the documented residues, avoiding a new circuit approximation.

Following the paper, the asynchronous input source is a train of host Dirac
impulses weighted by input/f_host. At each host interval start it injects the
impulse, advances exactly to each capture time, and samples the state there.
This is the impulse-invariant resampling assumption of the paper; it can retain
host-rate image residuals when input-filter rejection is inadequate. It is not a
host biquad followed by a nearest sample. The output filter integrates the
piecewise-constant BBD output up to each edge, changes its driving value on output
edges, and continues to the host observation. All intermediate edges contribute,
even when many fit inside one host sample.

Qualification modes and profiles:

* **TransportOnly**: corrected phases and physical rectangular hold, no LPF.
* **AsyncLinearReference / ValidationPrototype**: input and output
  `H(s)=w/(s+w)`, w=2*pi*2000 rad/s. This deliberately simple first-order analog
  prototype validates machinery; it is not a vintage tonal choice. Its relatively
  weak suppression of host impulse-train images produces measurable differences
  from an ideal analog sine (up to about 0.014 absolute magnitude in the fixed
  qualification setup). The exact impulse-train reference, rather than the ideal
  sine, is used to validate this implementation.
* **AsyncLinearReference / HoltersParkerTable1**: fifth-order input/output poles
  and residues transcribed verbatim from Table 1 and visually checked against the
  supplied PDF. No invented coefficients, gain normalization, measured 2.3 dB
  insertion gain or omitted bias high-pass stages are added. This is a documented
  circuit filter reference, not a complete device accuracy claim.

Mode/profile setup is intended before prepare/reset. Clock updates never retune
coefficients. Exact exponentials are evaluated for each time advance; lookup
approximations, conjugate-pair real optimizations and oversampling policy are
left for later work. Process complexity is bounded by the edge ceiling and fixed
pole count; prototype/reference CPU can be substantially higher than transport.
No performance claim equates the transport benchmark to the async profile.

## Qualification and tolerances

CTest retains DSP and BBD transport coverage and adds separate async filter and
aliasing/hold tests. The all-tests executable also prints maximum errors.

* All requested stages 256/512/1024/2048/4096, rates 44.1/48/88.2/96 kHz, and
  delays 3/10/30 ms: step arrival from the reset epoch agrees with nominal D to
  one host observation interval. Low-edge-rate cases also compare capture/output
  timestamps to exactly N-1 edges. The 300 ms known example is included.
* One-edge-per-host tests assert alternating capture and output updates, held
  value preservation on capture, historical values and even-stage validation.
* Long-run edge counts retain the M2.0 bounded one-edge boundary tolerance.
* Block sizes 1,17,64,127,256,511,1024 give bit-identical output under a delay sweep
  in both modes. Tests exercise NaN, +/-Inf, zero, negative/unsupported host
  rates and both valid inclusive extremes; invalid delay clamps remain tested.
* Prototype impulse/step references at arbitrary time intervals use analytical
  exponential kernels (1e-12 absolute tolerance). Sine input compares to the
  exact geometric-series impulse-train gain including phase (1e-10 tolerance).
* Complete async shell output compares to independently summed analytical
  input impulse and output step convolutions at fixed and stepped clock,
  including fractional event offsets and retained history (1e-10 tolerance).
* Fixed-clock spectral measurement uses 384 kHz host, 8 kHz BBD, 256 stages,
  100 ms settling and one second of coherent sine correlation. Ratios .1,.25,
  .45,.55,.75 explicitly cross the **4 kHz** BBD Nyquist. Raw folded-tone
  magnitudes match ZOH sinc to **0.002 absolute linear magnitude**; the small
  residual comes from observing continuous holds on a finite host grid.
* Table 1 baseband sine predictions use the paper's filter transfer functions,
  delay and sinc: magnitude tolerance 2e-5 absolute, phase 5e-5 rad. This check
  applies below BBD Nyquist and tolerates finite host-rate image residuals.
* Marker/clock-step tests require exact onset departure according to the later
  edge trajectory, plus unchanged filter states, memory, phase and hold at a
  clock change. Qualification IDs are scalar test stimuli with timestamps held
  by the non-real-time tool; no DSP metadata representation is introduced.
* Allocation tracking covers process, while sanitizers cover memory/undefined
  behavior in CI. Existing Windows plugin and M1.1 bake-off jobs remain.

Representative native Release measurements:

| Metric | Maximum error |
|---|---:|
| nominal delay observed on host grid | 20.8333 us (one 48 kHz interval) |
| ZOH linear magnitude | 0.000101017 |
| prototype analytical filter | 3.79e-13 |
| full async analytical convolution, fixed/stepped clock | 1.01e-13 |
| Table 1 baseband magnitude | 1.084e-6 |
| Table 1 baseband phase | 1.468e-6 rad |

At 8 kHz BBD, input tones 4.4 and 6 kHz fold to 3.6 and 2 kHz. Raw folded
amplitudes are about 0.698748 and 0.900356. Prototype-filtered values are 0.142782
and 0.204729 (13.79/12.86 dB attenuation). Table 1 values are 0.499020 and
0.583760 (2.92/3.76 dB attenuation). These levels reflect a deliberately low BBD
clock relative to the circuit filter bandwidth, not an alias-free requirement.
Upper output images are measured as well. Desired and folded components coincide
below Nyquist; above Nyquist the desired tone is itself an output image. CSV and
README explicitly identify this overlap rather than double-counting energy.

## Artifact

`DriftBrigade-M2.1-BBD-Linear-Qualification` contains:
`bbd_phase_semantics.csv`, `bbd_constant_clock.csv`,
`bbd_frequency_response.csv`, `bbd_aliasing_sweep.csv`, `bbd_hold_response.csv`,
`bbd_delay_sweep.csv`, `bbd_event_timing.csv`, and `README.txt`.

Run `drift_bbd_qualification <output-directory>`. The constant-clock theoretical
column is `per_channel_transfer_edge_rate_hz`; timed throughput is explicitly
`stereo_measured_edges_per_wall_second`. CPU benchmarking here is TransportOnly.
Files use 17-digit precision. Opening all streams is required, and every stream
is flushed and closed with both statuses checked without short-circuiting later
finalization. Invalid paths or failed writes return failure. CI uploads the new
artifact and retains DSP, sanitizer, Windows plugin and M1.1 bake-off jobs.

## Relationship to M2.0 and deferred M2.2 work

M2.0 correctly established nominal delay timing, preallocation and clock-driven
history preservation. Its N-cell/every-edge signal abstraction was not physical
BBD sampling. M2.1 refines that abstraction into separate physical phases and
signal capture instants and adds the asynchronous filter/resampling foundation.

Deferred: noise; clock feedthrough/whine; insertion-loss modeling beyond these
linear references; nonlinear charge transfer; saturation; compander; feedback
coloration; final filter/circuit/product stage choices; real-time CPU
optimization and oversampling decisions; production BBD routing or controls;
UI; presets; subjective tuning and all M1.1 listening decisions.
