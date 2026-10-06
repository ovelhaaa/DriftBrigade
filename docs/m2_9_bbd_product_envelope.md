# M2.9 — internal BBD product envelope

**ENGINEERING PRODUCT ENVELOPE · PRODUCTIZATION CANDIDATE · NOT FINAL SHIPPING DEFAULT**

M2.8 is the **QUALIFIED PREVIOUS MILESTONE** for numerical robustness. M2.8.1
corrects hostile-state recovery telemetry without modifying DSP. The model's
**MODEL CAPABILITY** remains 128 physical edges per host sample per voice,
4096 stages and the existing host-rate domain. These are model/safety limits,
not realtime promises for a future product.

## Fixtures and admission mathematics

The qualification-only `BBDProductEnvelope` lives in the tool, not plugin state
or parameter code. Its hypotheses are:

| Candidate | Stages | Clock cap | Edges/sample cap | Qualification reference block |
|---|---:|---:|---:|---:|
| Economy | 512 | 192 kHz | 8 | 64 |
| Balanced | 1024 | 384 kHz | 16 | 64 |
| Extended | 2048 | 384 kHz | 16 | 64 |
| LargeStageResearch | 4096 | 384 kHz | 16 | 64 |

For host rate `Fs`, the admitted hypothesis is `fclock=min(clockCap,edgeCap*Fs/2)`.
The resulting minimum delay is `N/(2*fclock)`, and eight paths generate
`8*2*fclock` physical edges per second. At 48 kHz these delay floors are
1.333, 1.333, 2.667 and 5.333 ms, respectively. At 44.1 kHz they are 1.451,
1.451, 2.902 and 5.805 ms. The raw model floors remain `N/(128*Fs)`.

Clock caps are hypotheses until repeated measurements admit them; the finalized
clock CSV distinguishes qualified candidate caps from unqualified hypotheses.
The core's `maximumEventsPerHostSample` remains unchanged. No production
admission helper, limiter, automatic fallback or adaptive quality is introduced.

## Measurement and budget methodology

Engineering criteria are p99 <=50% of deadline as the target, 50–70% as tight,
and >70% as product risk. These thresholds are judgment, not physical laws.
The nearest-rank p50/p95/p99/p99.9, maximum and misses remain inspectable.
All timing is informational; hosted CI never fails because of these values.

The local static-floor matrix uses five independent 10,000-callback trials at
64 frames for every candidate and all six rates. Blocks 16/32/128/256/512/1024
use 512-callback exploratory observations. Their tails do not establish equally
strong support. The CI matrix uses one 128-callback observation per block.
Small blocks do not change DSP; exact buffer segmentation is tested separately.

Timing encloses only `process` on stereo float buffers. Input generation,
buffer restoration, percentile calculation and raw-file writes are outside the
timer. Each fresh engine gets 2048 warmup frames. Operating-statistic collection
is disabled during timers; the instrumented build retains minimal numerical
counters. A separate production-counter build removes those types/counters.
Independent production modulation trials use five 10,000-callback runs per
candidate at 44.1/48/88.2/96 kHz and block64, plus block128 for Balanced to
investigate whether a larger deadline resolves small-block tail risk. They place center at 1.2 times
the candidate floor and invert the current Depth mapping to keep the complete
conservative modulation bound above that floor. Static clocks alone do not
qualify continuously changing asynchronous transitions.

The event sweep exercises 4/8/12/16/24/32/48/64/96/128 physical edges per sample
per voice at each host rate, using 1024 stages and block128. Local observations
use 2048 callbacks; CI uses 128. Actual scheduler counts must agree with requested
load, including capture/output accounting. Raw repeated callback measurements,
machine/compiler/build metadata and all report CSVs live in ignored artifacts.
The stronger production repetition must run without simultaneous qualification
or build workloads. Cross-machine qualification remains a product blocker.

## Parameter coverage and stage tradeoff

The coverage report reuses the existing M2.7 grid: Motion .05/.2/.7/2/6/10,
Depth 0/.25/.5/.75/1 and Center .3/.5/1/2/5/10/20/30 ms (240 points), default Wander's conservative magnitude bound,
and the existing perceptual mapping. Fractions are mutually exclusive: center
below the product floor; additional excursion reduction relative to M2.8's
already-admitted excursion under a proposed 15% excursion reserve above the new floor; otherwise fully reproducible. This reserve is an engineering admission policy, not a requirement of the clock ceiling. Separate `clock_cap_only_*` CSV columns omit that extra reserve. This is a parameter
grid fraction, not a perceptual weighting or a probability of user settings.
The CSV also provides effective maximum Depth in 0.001 steps at short centers.
Width, coherence and dynamics change observed trajectories within the conservative
bound; coverage describes the admitted excursion. Other M1.1 modulation variants require separate listening decisions.

Current Center starts at 0.3 ms. All these candidate floors remove part of that
range. Larger stage counts need proportionally higher clock for equal short
delay; at equal clock their longer minimum delay narrows flange/short-chorus
coverage. Their synthetic per-stage loss, nonlinear accumulation and noise
also differ. A CPU-only stage winner would therefore be misleading.

Stage and gain reports measure the complete eight-voice research path at
100/500/5000/10000 Hz, with paired noise-on/off engines, Conservative/Nominal/
HighDrive gains and integer-cycle harmonic projection. The exact tone is
`Fs/round(Fs/requestedFrequency)`; requested frequencies label the CSV.
Only harmonics below Nyquist contribute. Local measurements cover approximately
0.5 seconds after 0.5 seconds settling; CI is explicitly shortened. These are
level-dependent response/THD and paired-noise SNR estimates, not IC calibration.
No nonlinear coefficient changes are made.

## Startup and feedback

Each gain profile is measured after 0/0.1/2 seconds silence, using an impulse,
full-scale 500 Hz tone and a percussive fixture at the candidate delay floor.
Summed wet peak is measured per channel, separately from post-mix peak. Internal
peak, nonlinear input peak, nominal-region occupancy and headroom relative to
the existing engineering level 2 are retained. The steady-state gain report
supplies the separate SNR/THD tradeoff. Noise is the full research fixture.

Lower fixed operating gain is an engineering option; its inverse terminal gain
restores detector units. Detector initialization, a short startup gain ramp and
a wet ramp after reset remain **ENGINEERING PRODUCT OPTIONS**, not historical
IC claims, and are not adopted or claimed measured. They are needed only if
the measured operating-level/headroom margin cannot satisfy a future decision.
No limiter is added. Ten-second silence and all M2.8 hostile onsets remain
previous evidence, not newly repeated M2.9 fixtures.

Feedback 0/.25/.5/.65 and the macro maximum are tested on all candidates/rates.
The maximum request is admitted by the existing Feedback parameter and Dynamics
macro; the CSV records actual effective feedback, rather than assuming .75.
Local windows run 30 seconds: 20 seconds of 500 Hz excitation and 10 seconds
decay. DC/RMS/detector bias/output THD use the final 5 seconds of excitation;
bucket mean is sampled at the excitation endpoint. Early/late decay RMS are
separate. Occupancy and internal peaks cover the entire run. CI's 0.25-second
windows are smoke evidence and do not establish steady-state DC or THD.

The optional feedback-block comparison has its own state in the harness and
adds 1/2/5 Hz blocking only to the expanded return of a direct BBD path.
The existing output blocker remains independent. It compares .65 feedback,
100 Hz excitation and 30-second windows at 48/96 kHz. The report retains loop
DC, bucket mean, output DC, steady peak and decay, plus exact blocker magnitude
and phase at 1/5/20/100/500/5000 Hz. Those transfer values describe the added
blocker, not an entire nonlinear closed-loop transfer measurement. No pole is
selected for production, and these direct-voice results cannot alone establish
the eight-voice engine's final topology.

## Cost and possible future work

Eight independently allocated direct paths are timed through transport,
asynchronous filters, loss/noise, nonlinearity and compander, followed by the
complete engine. The table gives absolute p99, percentage of full-engine p99
and signed differences between independently timed layers. Fixtures differ
between direct paths and the engine's multiband inputs; these are informative
costs, not an additive profiler attribution. Negative differences can reflect
timing variability. All layers retain the existing exact model.

The eight voice inputs, clocks, filter memories and nonlinear detector states
are independent. Sharing these stateful components has no demonstrated exact
mathematical equivalence. The architecture report records the eight-path cost
and explicitly leaves shared/reduced-path work unqualified; no approximation
or engine redesign is introduced. The measured frontier should decide whether
a future optimization milestone is justified.

## Support, recommendation and deferred decisions

`summarize_bbd_product_envelope.py` finalizes the support matrix and clock-cap
status from the generated evidence. Persistent median p99 above 70% in repeated
production modulation trials disqualifies that machine/configuration. A worst
run above 70% without persistent median overload, incomplete repetition or
repeated production runs with misses stays experimental. Static instrumented
tails remain separately visible; production-counter trials decide support.
176.4/192 kHz remain experimental
unless the measurements show computational overload, in which case the tested
candidate is unsupported for BBD. DigitalFractional remains available at all
plugin rates; no whole-plugin rate is disabled.

A timing-eligible Balanced/1024 candidate can be recommended for listening only
after production modulation evidence is available. The machine-specific
recommendation and raw values are in `SUMMARY.json` and recommendation/frontier
CSVs. These preserve quality, startup, gain and DC findings separately rather
than collapsing them into an arbitrary score. A recommendation is **NOT FINAL
SHIPPING DEFAULT** and does not establish universal realtime safety.

Future prepare-time choices could restrict the delay range, select a lower stage
profile, fall back to DigitalFractional, or reject an unsupported BBD request.
All require documented product behavior. CPU probing on the audio thread,
adaptive sound based on current utilization and automatic switching are deferred.

Remaining decisions include cross-machine timing, tiny-block admission, listening
and level calibration, feedback topology, startup margin, parameter/UI semantics
and high-rate fallback. Public BBD/stage controls, switching/crossfade/state
migration, production feedback blockers, presets, feedthrough/whine, calibrated
device coefficients, BBDTerminals and the final M1.1 modulation winner remain
explicitly deferred. Production DSP and DigitalFractional source are unchanged.

## Reproduction

Build `drift_bbd_product_envelope_qualification` in Release and run it with an
artifact directory and `--local --timing-only`, then `--local --quality-only`.
Operating measurements can alternatively run with `--local --measurements-only
--candidate=NAME` in independent candidate subdirectories. This mode performs
no callback timing; merge those reports with the summary script and run
`--local --cost-only` in the main artifact directory separately.
Build `drift_bbd_product_envelope_production_timing` and run the same directory
with `--production-reference`; its trials are always five by 10,000 callbacks.
Run `--coverage-only` to regenerate coverage without altering timing files,
then run `python tools/summarize_bbd_product_envelope.py ARTIFACT_DIRECTORY`.
Default artifact generation is CI_SHORT and must not be interpreted as local
repeated qualification. Numerical and stream-failure tests are registered in
CTest; the existing Linux sanitizer job covers the new executable.

## Local findings — 6 October 2026

Qualification machine: AMD Ryzen 7 7730U, 8 cores/16 logical processors,
Windows 10 Pro 10.0.19045, Balanced power scheme, GNU 14.2.0 Release (-O3).
All operating measurements completed; callback/decomposition passes ran
separately from the parallel candidate measurement workers. Static instrumented
trials show appreciable OS/frequency variability; they are not interchangeable
with the separate production-counter modulation trials. Every cohort retains
its raw measurements and misses rather than filtering scheduling interruptions.

### Repeated production modulation timing

Each row is five independent runs of 10,000 callbacks. All values are milliseconds.
The deadline is the actual host block duration. The complete repeated CSV also
contains all four candidates and the static-floor matrix.

| Balanced rate | Block | Deadline | Median / worst p99 | Median / worst p99.9 | Total misses / runs with misses |
|---|---:|---:|---:|---:|---:|
| 44.1 kHz | 64 | 1.451 | .362 / .460 | .447 / .569 | 0 / 0 |
| 44.1 kHz | 128 | 2.902 | .721 / .730 | .892 / 1.048 | 1 / 1 |
| 48 kHz | 64 | 1.333 | .363 / .389 | .451 / .543 | 0 / 0 |
| 48 kHz | 128 | 2.667 | .722 / .724 | .855 / .950 | 0 / 0 |
| 88.2 kHz | 64 | .726 | .338 / .397 | .403 / .502 | 0 / 0 |
| 88.2 kHz | 128 | 1.451 | .669 / .695 | .897 / .936 | 0 / 0 |
| 96 kHz | 64 | .667 | .345 / .352 | .414 / .458 | 1 / 1 |
| 96 kHz | 128 | 1.333 | .664 / .999 | .785 / 1.115 | 3 / 2 |

48 kHz meets the 50% target with margin. 88.2 kHz has a tight worst p99 at
block64 (54.7%), while block128 remains below 50%. 96 kHz has favorable median
cost but an unresolved block128 tail: its worst p99 is 74.9%, with misses in
two runs. It remains experimental rather than choosing the favorable block64
cohort and discarding contrary evidence. These measurements do not prove that
96 kHz is intrinsically computationally overloaded. The corresponding isolated
44.1 kHz miss is retained; it does not by itself establish persistent overload.

### Sample-rate support matrix

R means **SUPPORTED_WITH_REDUCED_BBD_RANGE**, E **EXPERIMENTAL**, and U
**UNSUPPORTED_FOR_BBD** for the tested candidate on this machine. No candidate
reproduces the complete current parameter range, so none receives unrestricted
SUPPORTED. R is engineering qualification at the reference block; larger blocks
without repeated production measurements and all tiny blocks retain explicit
risk. All rates remain available with DigitalFractional.

| Rate | Economy 512 | Balanced 1024 | Extended 2048 | LargeStageResearch 4096 |
|---|---|---|---|---|
| 44.1 kHz | R | R | R | R |
| 48 kHz | R | R | R | R |
| 88.2 kHz | E | R | R | R |
| 96 kHz | R | E | E | E |
| 176.4 kHz | U | U | E | E |
| 192 kHz | U | U | U | U |

High-rate classifications use repeated static median evidence where production
modulation was not repeated. The U cases are disqualified candidate envelopes,
not model failures or whole-plugin rate restrictions. Additional machine/host
qualification could change this matrix; it is not a universal hardware policy.

### Event curve and clock ceiling

At 48 kHz/block128, the exploratory p99 deadline utilization for
4/8/12/16/24/32/48/64/96/128 edges was approximately
17.4/23.4/23.2/21.6/24.8/27.8/34.3/38.8/49.9/59.1 percent.
At 192 kHz it was 76.1/81.1/108.1/109.7/140.7/150.0/134.9/169.8/205.1/236.6
percent; the 48-edge and higher points missed all 2048 observed deadlines.
The non-monotonic points show why one sweep cannot establish a hard CPU law.

The recommended Balanced cap is **min(384000 Hz, 8*Fs)**, equivalent to at most
16 physical edges/sample/voice, far below the unchanged 128-edge model limit.
It is a qualified admission ceiling, not an estimate of the largest clock this
CPU can ever process. The ceiling combines useful short-delay range with the
repeated variable-clock margin; raising it toward the model maximum lacks the
same repeated product evidence. Eight voices generate at most 6.144 million
physical edges/s (5.645 million at 44.1 kHz). No exact-core rewrite is justified
for the currently recommended low-rate envelope.

The resulting floor is 1.451 ms at 44.1 kHz and 1.333 ms at 48/88.2 kHz.
This excludes near-zero/very-short flange requests from the generic 0.3 ms
Center range, while preserving useful chorus and positive short-delay territory.
A future BBD control/admission range must make that limitation visible.

### Quality/CPU frontier and stage choice

The following 48 kHz response/SNR/THD values use the complete engine at 8 ms
with Nominal gain and .2 input tone; startup values use the separate full-scale
onset fixtures at the product floor. They are normalized synthetic-fixture data.

| Candidate | Floor ms | Clock kHz | Production block64 worst p99 utilization | 500 / 5000 / 10000 Hz gain dB | Paired SNR dB | THD H2-H5 % | Startup output peak | Max loop-return DC |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Economy | 1.333 | 192 | 33.1% | -2.30 / -7.43 / -27.61 | 63.91 | .1513 | .7806 | .001000 |
| Balanced | 1.333 | 384 | 29.2% | -2.39 / -7.07 / -25.85 | 63.49 | .1514 | .7726 | .000984 |
| Extended | 2.667 | 384 | 50.0% | -2.56 / -7.18 / -25.82 | 63.32 | .1514 | .7568 | .000717 |
| LargeStageResearch | 5.333 | 384 | 28.1% | -2.92 / -7.54 / -26.02 | 63.17 | .1514 | .7260 | .000884 |

The lower observed worst p99 of a particular higher-stage cohort is not proof
that additional stages reduce computational cost; independent trial tails vary.
Economy saves clock/event work and is a valid alternative for listening, with
more upper-band attenuation in this matched-delay fixture. Balanced preserves
the already qualified 1024-stage research path, has adequate repeated CPU
margin, and retains more upper-band response than Economy here. The larger
stage profiles do not demonstrate a useful SNR/THD advantage in these fixtures
and raise the short-delay floor substantially. Thus **1024 stages are recommended
for the first listening/productization candidate**, with 512 retained as the
lower-event alternative and 2048/4096 as longer-delay character research options.
This remains a technical nomination, not a subjective sound winner.

On the existing 240-point grid at 48 kHz, Economy/Balanced fully reproduce
41.67% under the proposed 15% reserve, require additional excursion reduction for 20.83%, and require center
admission for 37.5%. With only the physical clock cap, Economy/Balanced reproduce 48.33% and need excursion reduction for 14.17%; center admission remains 37.5%. Extended/LargeStageResearch fully reproduce 37.08/29.58%.
For Balanced, Motion .7 and Center 8 ms imply effective maximum Depth .382 under that reserve; the already-admitted M2.8 excursion at Depth .5 fits the bare clock cap. Thus the additional reserve must be evaluated as a product policy, not presented as physical necessity;
Center 2 ms leaves substantially less depth, and Center below 1.333 ms needs
admission. The percentages are deliberately based on the actual older grid
containing many short centers, rather than a new grid dominated by long delays.
The generic controls should not be exposed unchanged as an unrestricted BBD mode.

### Headroom, gain and feedback decision

Across all candidates/rates, Nominal's largest startup output peak was .78063
and internal peak 1.61718, leaving about 1.84 dB below the engineering headroom
boundary 2. The complete profile CSV retains per-candidate and per-rate values.
HighDrive reaches beyond that internal boundary; it is not recommended for this
initial envelope. Conservative provides roughly 6 dB more internal margin but
loses roughly 6 dB paired-noise SNR compared with Nominal. At Balanced/48 kHz/
500 Hz the Nominal SNR is 63.49 dB and THD .1514%; the gain report keeps the
corresponding Conservative/HighDrive tradeoffs visible.

**Nominal remains the candidate for listening**, with an explicit operating-level
and full-scale-feedback review before public exposure. These tested onsets do
not justify a limiter or extra detector/reset/wet-ramp mechanism. They do not
establish unrestricted upstream overload headroom, and the earlier 10-second
M2.8 startup/direct-path ratio remains distinct evidence.

The 30-second feedback fixtures remained finite and unclamped with zero callback
allocations. Maximum absolute steady loop-return DC over the matrix was .002974,
while output DC was <=3.08e-8. The worst final decay RMS was <=6.01e-8, near the
noise tail. Nominal occupancy remained inside the chosen region in the ordinary
.2-tone feedback fixtures, with substantial internal margin. No harmful DC
accumulation or growing tail was observed in those windows; mixed/full-scale
materials and operating-level calibration remain separate product decisions.

In the direct Balanced/48 kHz/.65-feedback/100 Hz experiment, no internal blocker
left return DC around -.004104; 1/2/5 Hz blockers reduced it to approximately
1.26e-8/1.28e-8/5.20e-9. At 20 Hz their added-filter magnitudes were
-.0103/-.0421/-.2604 dB and phases .0500/.0997/.2450 radians. Late decay stayed
near 7.1e-9. This establishes DC rejection and the added filter's transfer, not
closed-loop perceptual equivalence. **No blocker pole or final topology is selected.**

### Remaining cost and validation

Balanced/48 kHz/block128 direct-layer p99 was approximately .0492 ms transport,
.4969 ms with async filters, .4909 ms with character/noise, .5026 ms with
nonlinearity and .5390 ms with compander, versus .5650 ms for the full engine.
Those are about 8.7/87.9/86.9/89.0/95.4% of full-engine p99. The negative
character increment reflects independently timed variation; it is not a claim
of free noise processing. Async filtering/event transport is the dominant
remaining cost. Sharing stateful filters/detectors has no demonstrated exact
multiband equivalence, so no reduced-voice architecture is adopted.

Local Release CTest passed 22/22; final new numerical/stream tests passed in
Release and Windows UBSan. Linux CI at 203d4b2 passed ASan+UBSan and all three
DSP platforms, including the fixed `/dev/full` flush gate. Windows plugin build
and state/default tests passed. The final-head workflow is tracked in PR #10.
All 48 M2.8.1 hostile CSV rows preserve every non-recovery field exactly;
immediate recovery is now 0 and detector recovery remains independent.
`git diff` against f9a1eb5 shows no Source changes: DigitalFractional, model
coefficients, clock ceiling, public parameters and serialization are unchanged.

**RECOMMENDED FOR LISTENING / PRODUCTIZATION:** Balanced 1024, Nominal,
44.1/48/88.2 kHz, the clock cap/floors above, and block64 reference qualification
with tiny-block risk kept explicit. **NOT FINAL SHIPPING DEFAULT.** 96 kHz stays
experimental for Balanced pending repeated-tail review; 176.4/192 kHz are not
admitted by this recommended envelope. Remaining blockers are cross-machine and
host timing (including unpredictable tiny blocks), listening, operating-level/
startup margin, full-scale feedback materials, final DC topology and explicit
BBD parameter-admission/fallback semantics. These must be resolved before public
exposure; no BBD mode, limiter, fallback or new production topology is implemented.
