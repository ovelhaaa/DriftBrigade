# M2.6 BBD gain staging, headroom and topology qualification

Status: implementation and local numerical qualification complete; **Linux ASan/
UBSan acceptance remains pending CI**. Base is merged M2.5
`5361be512570eb6340c2d67841bee02eda5252d2`. Production remains
`DigitalFractionalDelay`. No final profile, topology, capacitor, stages, feedback,
UI or subjective choice is selected.

## Evidence and motivation

M2.5's compressor raises weak signals and can substantially exceed the M2.4
nominal nonlinear domain after silence. Signal normalization must be explicit
before a complete BBD path can be considered for production.

The source ledger in [research_notes.md](research_notes.md) records the focused
re-reading and classification. Raffel/Smith, supplied `bbd modeling (1).pdf`,
PDF p.4 sections 3.1-3.4: low-distortion input is a small fraction of supply;
near-maximum input gives about 60 dB typical SNR; companders drive near maximum;
insertion gain is typically 0-2 dB; detector resistor is 10 kohm with external
0.22-1 uF. None supplies an absolute input voltage, calibrated normalized level,
clipping voltage, exact operating point or universal feedback gain. Figures 9-12
pp.6-7 contain relative amplitudes and illustrative cubic coefficients, not a
voltage calibration. Holters/Parker, supplied `BBD filters model (1).pdf`, p.6
measures approximately +2.3 dB insertion gain in one Juno-60: **DEVICE-SPECIFIC /
NOT GENERALIZED**. Its filter response includes the circuit's gain; it cannot be
assumed to be universally unity. Modulation/perception papers add no terminal
level or headroom measurements.

## Domain and architecture

**ENGINEERING NORMALIZATION:** BBD-domain amplitude 1 is the existing nominal
maximum of the M2.4 nonlinear model, not 1 volt, full supply or a device clipping
point. Nominal domain is `abs(x/reference)<=1`; reference defaults to 1.
`headroomLevel=2` is an overload histogram boundary, not guaranteed linear
headroom. Useful band 0.1-1 is a descriptive convention.

Explicit linear gains are configured before processing, without plugin parameters:

```
external -> preCompressorGain -> compressor (host dt)
 -> input analog filter -> compressorToBBDGain
 -> BBD capture + intrinsic input noise -> buckets
 -> reference normalization -> unchanged M2.4 transfer
 -> inverse reference normalization -> loss/mismatch/intrinsic output noise
 -> held terminal -> output analog filter -> bbdToExpanderGain
 -> expander (host dt) -> postExpanderGain -> external
```

The gain after the linear output filter commutes with that filter. It preserves
held terminal and filter state in native BBD units, and restores original detector
units before expansion. No signal gains are hidden in nonlinear coefficients,
noise or loss. `transferAfterNonlinear` splits existing character operations only
so the core can perform reference scaling around the nonlinear element without
scaling intrinsic output noise. Reference 1 takes the old arithmetic path.
Disabled nonlinearity ignores the reference exactly.

Profiles differ only by terminal scalar gain and its reciprocal. Pre/post gains
and nonlinear reference remain unity. All nonlegacy values are **ENGINEERING
NORMALIZATION**, exact binary attenuations chosen to explore headroom, not fits:

| Profile | Compressor to BBD | dB | BBD to expander |
|---|---:|---:|---:|
| LegacyM25 | 1 | 0 | 1 |
| Conservative | 0.125 | -18.0618 | 8 |
| Nominal | 0.25 | -12.0412 | 4 |
| HighDrive | 0.5 | -6.0206 | 2 |

Configuration validates finite positive gains/reference in [2^-20,2^20], falling
back to unity for invalid fields. It changes no state or allocation. Runtime
parameter changes/smoothing are not introduced or qualified. The inherited float
numerical admission boundary remains separate from ordinary signal behavior;
there is no added safety limiter, clamp, tanh or coefficient change.

## Legacy, roundtrip and realtime tests

`tests/reference_m25` freezes the merged M2.5 sources with namespace relocation
and renamed instrumentation macros only. Fixed, sinusoidal, cosine and abrupt
clock tests compare bit identity at every host sample for output, compressor and
expander detector/gain, every bucket, held output, both filter snapshots,
scheduler phase and event count. Seeded input/output noise and full character are
active, so the compared bucket/output sequences exercise RNG preservation.
Clock configuration is also checked for unchanged immediate state.

Always-run tests cover inverse gains with the 2-stage exact identity transport,
including DC, impulses and transients, nonlinear reference scaling against an
independent transfer oracle, long silence recovery and callback allocation counts.
Binary gain profiles also reproduce the ungained ideal asynchronous output and
both detector gains bit for bit under a modulated clock. Maximum identity
roundtrip peak error in the artifact is 5.78e-15. The additional roundtrip CSV
reports DC/RMS gain and RMS/peak errors; asynchronous 1024-stage delayed errors
are explicitly unaligned, not interpreted as gain-cancellation failure.

Processing adds scalar arithmetic and comparisons, no heap allocation, locks,
logging, filesystem, exceptions or lazy initialization. The allocation test counts
ordinary C++ new/new[] on the callback thread. Existing numerical floor/normal
product handling remains in the detectors; gain ranges and finite-state tests do
not replace sanitizers. Local release CTest: 16/16 passed; Windows plugin state/routing also passed. Local Clang UBSan trap
build: M2.6 numerical test passed. Combined local ASan/UBSan could not link because
this MinGW installation lacks `libclang_rt.asan_dynamic` and runtime thunk.
Linux ASan/UBSan job includes the new executable/tests; a successful CI run is
still required before claiming sanitizer acceptance.

## Measurement method and startup

Default fixture: 48 kHz, 1024 stages, 10 ms, Holters/Parker Table 1 filters,
0.47 uF/10 kohm, nonlinear strength 1, noise RMS 1e-4 at each intrinsic source,
seed 570. The intrinsic noise fixture is engineering data, not calibrated IC noise.
External levels: -80,-60,-48,-36,-24,-18,-12,-9,-6,-3,0 dB peak. Six stimuli:
500 Hz sine, 25 Hz sine, decaying burst, impulse, stepped envelope, bounded
multi-tone program signal. Distribution rows contain actual external RMS/peak
and every capture-event magnitude, not host snapshots. Steady sines discard
500 ms; transient measurements include onset. Histograms have mutually exclusive
bins <.01, .01-.1, .1-1 inclusive, >1-2 inclusive, >2.

Startup sweeps 0,10,100,500,2000,10000 ms silence, followed by -60/-24/0 dB
500 Hz sine, unity DC, impulse and burst. The CSV includes pre-onset detector/gain,
compressor peak, all-event nonlinear input/output maxima, expander detector peak,
final output, capture distribution and capture-weighted overload duration.
Duration is sum(1/clock) over overloaded captures, not exact continuous analog
time. Onset is observed for 250 ms, including transport latency.

| Profile | Worst startup internal peak | 10s silence -> DC nominal occupancy |
|---|---:|---:|
| LegacyM25 | 8.94590 | 97.3828% |
| Conservative | 1.11824 | 99.9844% |
| Nominal | 2.23648 | 99.9297% |
| HighDrive | 4.47295 | 99.7344% |

High occupancy does not hide severe short excursions. With these four profiles,
gain staging alone does not ensure every startup remains inside the nominal
range. The current-sample compressor root after a completely decayed detector
has an impulse-like bound proportional to sqrt(abs(input)/alpha); this explains
why fixed attenuation cannot resolve arbitrary capacitor/onset combinations.
A lower gain could contain this finite fixture, at further SNR cost; no automatic
limiter, reset rule or final drive selection is justified here.

## SNR, distortion and usable region

Frontier sweeps drive -24,-18,-12,-9,-6,-3,0,+3,+6 dB at every external level.
Inverse terminal gains cancel; noise sources stay fixed. Sine measurements use
250 ms settling and 250 ms coherent 500 Hz DFT observation. THD is H2-H5 relative
to H1 of the clean full chain. Nonlinearity CSV also exports linear baseline THD
and complex-spectrum differences for generated H2-H5. These isolate additional
harmonics without attributing detector distortion to the nonlinear element.

Effective SNR uses paired deterministic noisy and clean chains with identical
nonlinear/loss configuration. Their difference includes signal-dependent noise
interaction in the expander; it is not an additive noise-only output experiment.
The noise table includes an uncompanded baseline, improvement, and a separately
labelled SNR-oriented experiment scaling intrinsic noise with drive. Only fixed
noise preserves M2.2 semantics. Neither experiment changes defaults.

At external 0 dB steady sine:

| Profile | Internal RMS | Peak | Nominal occupancy | SNR dB | THD % |
|---|---:|---:|---:|---:|---:|
| LegacyM25 | .85504 | 1.20864 | 61.7188% | 80.50 | 8.223 |
| Conservative | .10688 | .15122 | 100% | 62.36 | .956 |
| Nominal | .21376 | .30228 | 100% | 68.38 | 1.920 |
| HighDrive | .42752 | .60440 | 100% | 74.39 | 3.909 |

At -24 dB Nominal: RMS .05369, peak .07605, 100% nominal, SNR 56.39 dB,
THD .480%. Its lower internal level reduces distortion while sacrificing roughly
12 dB effective SNR compared with Legacy. All four program0 capture RMS/peak
pairs are respectively .52634/1.52947, .06579/.19118, .13159/.38237,
.26317/.76474; Legacy occupancy 94.0586%, others 100%.

Candidate dynamic-range gates are SNR >=20/40/60 dB, THD <=1/3/10%, occupancy
>=95/99/100%. They are **ENGINEERING NORMALIZATION**, not industry truths.
Each row exports the largest contiguous passing interval of tested steady sine
levels, with no interpolation or extrapolation. For >=40 dB SNR, <=3% THD,
>=99% nominal occupancy, intervals are:

| Profile | Tested external interval dB | Span dB |
|---|---:|---:|
| LegacyM25 | -80 to -18 | 62 |
| Conservative | -36 to 0 | 36 |
| Nominal | -48 to 0 | 48 |
| HighDrive | -60 to -6 | 54 |

This provides an objectively usable region (Nominal -48 to 0 dB for this sine
fixture and candidate criteria), not a new product default. Startup, frequencies,
capacitors and arbitrary program material are not covered by this dynamic-range
claim. No scalar score or best-sounding choice is made.

## Topology result

OutsideFilters retains host-rate detector updates with dt=1/hostRate. Input
filter propagation, capture/noise, chronological bucket operations, nonlinear
output updates and reconstruction remain asynchronous.

BBDTerminals is **deferred**, not implemented at host rate or treated as equivalent.
The compressor would observe the continuously evolving filtered waveform; its
feedback detector responds to abs(compressed output) between captures. Capture
instant sampling with dt_event integrates a held approximation, not the actual
rectified filter trajectory. Filter superposition cannot propagate this coupled
nonlinear detector exactly. On output, the terminal is piecewise held, but the
continuous expander detector changes during the hold, making its expanded output
nonconstant. Existing output transitions assume constant held drive. Expanding
only once per terminal event would introduce a new zero-order-held nonlinear
approximation. Sources report placement but do not select these discretizations.
A coupled continuous solver or explicitly qualified approximation is a separate
milestone. The comparison CSV states the missing timebase and reason; it contains
no invented response/CPU comparison for an unimplemented topology.

## Clock, feedback and stereo

All profiles run fixed delay, sine modulation, seeded deterministic pseudo-random
raised-cosine target interpolation, and abrupt 10-to-3 ms clock changes. Changing
clock leaves gains, detectors, held output and counts unchanged before the next
process operation. Full character and seeded noise remain finite; legacy oracle
and ideal gain-only equivalence demonstrate scheduler/bucket/filter/detector and
noise-sequence preservation. Maximum measured internal peak across these clock
fixtures is 1.09460 (Legacy). Explicit configured gains have no time-varying
zipper mechanism; no runtime gain automation is claimed.

Qualification feedback returns reconstructed output to the external input, with
one host-observation delay. It is not a selected musical loop. 0,.5,.8,.9,.95 are
swept for linear, nonlinear, nonlinear+noise, full-character+compander at every
profile. A .1 peak sine excites for 100 ms, followed by silence to 2 s. Peak/RMS/DC,
H2-H5 projections, late/early RMS growth and occupancy are exported. Maximum loop
peak .33015; no finite-state failure and no noise-free limit-cycle candidate in
this finite observation. Noise-seeded late energy is not labelled a limit cycle.
The flag is a heuristic candidate (>1e-8 late RMS, within 1 dB/s decay); it is not
proof of stability for arbitrary loop placement, input or duration. No limiter.

Stereo applies the same gain config to independent L/R detector/core states and
asymmetric program/stepped inputs. Nominal balance shift -0.22543 dB, correlation
-.01464, BBD peaks .27364/.17947. Independent compressor gain divergence is
expected with asymmetric input; it is reported along with expander divergence.
Conservative/HighDrive balance shifts -.21033/-.28647 dB versus Legacy -.53278.
No linked detector policy is selected.

## Performance and M2.7

The artifact benchmarks frozen M2.5, explicit unity gains, Nominal gain staging,
and Nominal with nonlinear reference 2: stereo/eight cores at 48 kHz,
512/1024/2048/4096 stages and 3/10/30 ms. Five repeated 500 ms audio runs use
median time; detector updates and event/nonlinear rates are analytically derived
for the fixed clock. No hosted-runner timing gate.

On this Windows release run, median overhead across configurations is +1.16%
for explicit unity, +1.81% for gain staging, +2.09% for reference scaling.
Individual relative results span approximately -2.56% to +6.09%; negative
overhead is measurement variation, not a speedup claim. Eight-core realtime
factors for staging are .0363-.2111 and for reference scaling .0362-.2139.
This supports a small measured overhead on this host. It does not establish a
universal timing bound; re-run on target hardware for integration budgeting.

M2.7 should first obtain green Linux ASan/UBSan, then propose an internal full-path
integration with a documented operating envelope and explicit startup policy.
OutsideFilters is the qualified starting architecture; select no production
profile or capacitor automatically. Preserve fixed intrinsic noise semantics,
M2.4 coefficients and digital production routing until integration acceptance.
Final stage count, compander capacitor, drive, topology, musical feedback/limiter,
feedthrough, MN300x/pedal calibration, UI, presets and subjective listening
remain intentionally unselected.

## Reproduce

```sh
cmake -S . -B build -DDRIFT_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target drift_bbd_headroom_qualification
./build/drift_bbd_headroom_qualification --tests-only
./build/drift_bbd_headroom_qualification output/DriftBrigade-M2.6-BBD-Headroom-Qualification
```

The CI job `bbd-headroom-qualification` publishes
`DriftBrigade-M2.6-BBD-Headroom-Qualification`; all prior jobs remain.
Thirteen requested CSVs and README.txt are generated, plus bbd_gain_roundtrip.csv.
Artifacts are ignored build outputs, regenerated by the committed executable.
