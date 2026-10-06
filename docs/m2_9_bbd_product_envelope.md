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
already-admitted excursion; otherwise fully reproducible. This is a parameter
grid fraction, not a perceptual weighting or a probability of user settings.
The CSV also provides effective maximum Depth in 0.001 steps at short centers.
Default dynamics can reduce excursion further; it cannot break the conservative
bound. Other M1.1 modulation variants require separate listening decisions.

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
status from the generated evidence. Excessive static or production modulation
p99 disqualifies that machine/configuration; incomplete repetition or repeated
production runs with misses stays experimental. 176.4/192 kHz remain experimental
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
