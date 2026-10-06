# M1 research notes

## M2.5 compander ledger (recorded before implementation)

- **PAPER-BACKED:** Raffel/Smith, supplied `bbd modeling (1).pdf`, PDF pp.4-5, sections 3.1-3.4: limited BBD dynamic range; separate feedback compressor and feedforward expander, nominal ratio 2; full-wave rectification followed by RC averaging. Internal resistance is approximately 10000 ohms; typical external capacitance 0.22-1 uF gives 2.2-10 ms. Filter and feedback placement varies between circuits. Equations: expander `y=avg(abs(x))*x`; compressor `y=x/avg(abs(y))`; averaging `L[n]=dt/(RC+dt)*abs(signal[n])+RC/(RC+dt)*L[n-1]`. Section 4.5 notes that companding raises the operating level and reduces the practical impact of the cubic model's low-level harmonic mismatch. This does not establish device calibration.
- **DERIVED FROM PAPER EQUATIONS:** For current-sample feedback, substitution gives `L^2-beta*Lprev*L-alpha*abs(x)=0`. Its nonnegative root is `L=(beta*Lprev+sqrt((beta*Lprev)^2+4*alpha*abs(x)))/2`; no iteration or detector delay. Constant DC gives `abs(y)=sqrt(abs(x))` and expander `abs(y)=abs(x)^2`, slopes 0.5 and 2. With matched initial states and an ideal unity channel, the separate expander detector observes exactly the compressor output and obeys the same recurrence, so round-trip identity is expected within floating-point error, even for transients. Delays, filters, noise and time warping can break this correspondence.
- **ENGINEERING APPROXIMATION:** Dimensionless normalization, detector startup at unity (both independent states), a binary numerical floor `2^-500` and flushing double subnormal input magnitudes. The floor square is normal (`2^-1000`) and reciprocal finite, selecting a precision boundary rather than an audible threshold. Startup unity avoids the zero-state first-sample reciprocal singularity. Float-range signal admission follows the existing core's numerical boundary; no audio gain limiter. Physical config validation and host-rate OutsideFilters qualification shell are engineering policies. Terminal placement is deferred because asynchronous nonlinear input conditioning requires a separate event-time design. Non-typical capacitors, non-10k resistance and independent stereo qualification are engineering experiments; no production selection.
- Production remains DigitalFractionalDelay. No musical feedback, IC noise/distortion, volts calibration, final capacitor, UI, presets or subjective tuning in M2.5.
- **ENGINEERING APPROXIMATION (numerical-review amendment):** flush subnormal intermediate products and final outputs before arithmetic, with a fast normal-product bound at 2^-450. The RC/root equations are unchanged in their qualified operating range; the extreme floor neighborhood is explicitly approximate. This complements, rather than substitutes for, the detector floor and input admission policy.

All five supplied PDFs were inspected before DSP implementation. Page numbers below are PDF pages (not conference pagination). This is a paraphrased implementation ledger, not a claim that our complete algorithm appears in any paper.

## Modulated delay and interpolation

**PAPER-BACKED BEHAVIOR:** `modulation and time based effects(1).pdf`, Disch/Zölzer, pp. 1–2, classifies variable delay as phase modulation with a frequency-modulation consequence. Section 2.2 decomposes delay into integer and fractional portions and discusses linear, spline and all-pass interpolation. `chorus-flange-quality-enhancements(1).pdf`, Fernández-Cid/Casajús-Quirós, pp. 1–3, connects chorus/flange through variable delay, short flange centers (roughly 0–10 ms), longer chorus centers (10–40 ms), feedback resonance and interpolation error. Windowed sinc has an accuracy/cost/minimum-delay tradeoff.

**ENGINEERING INTERPRETATION / DESIGN CHOICE:** Four-point cubic Hermite (Catmull-Rom) is a low-cost M1 compromise, not the paper's windowed-sinc implementation. It is not ideally bandlimited, especially near Nyquist. Delay is expressed in seconds at a contained engine boundary; reads use bounded sample positions. Center spans 0.3–30 ms. Slewed controls avoid pointer jumps; a hard emergency limiter is transparent below its threshold and is not analog coloration.

## Organic motion

**PAPER-BACKED BEHAVIOR:** `chorus-flange-quality-enhancements(1).pdf`, pp. 3–4, section 5 generates random points at a low adjustable rate, uses a raised-cosine transition to audio rate, and optionally multiplies this process by a cosine to position its spectrum. Flat transition endpoints avoid value and slope jumps and uncontrolled polynomial overshoot.

**ENGINEERING INTERPRETATION / DESIGN CHOICE:** We use deterministic xorshift32 uniform targets, a continuous phase accumulator (two random targets per nominal Motion cycle), and `(1-cos(pi*t))/2`. Rate changes preserve phase. Chaos morphs the control itself between a sine and the interpolated process, with a theoretical variance normalization. We do not implement the paper's spectral-shifting multiplication. Independent sources have seeded phases and slightly different rates. Note a mathematical correction to the paper's broad smoothness wording: arbitrary adjacent cosine segments are C1, generally not C2 or infinitely differentiable. Only C1 is promised and tested.

**M1.1 PAPER-BACKED BEHAVIOR:** Experimental Variant B exercises the paper's optional multiplication of the DC-centered, raised-cosine random process by a cosine carrier. Its carrier phase, one-random-target-per-cycle relationship, clean-sine blend, and RMS scaling are engineering choices, not values specified by the paper.

**M1.1 ENGINEERING EXPERIMENT:** Variant C integrates a smoothly perturbed instantaneous rate. This phase-drift strategy is ours and is not attributed to either paper. Variant A remains the unchanged production default; B and C are offline bake-off choices only.

## Rate/depth evidence and scope

**PAPER-BACKED BEHAVIOR:** `CLASSIFICATION OF MODULATION EFFECTS (1).pdf` is actually Martens/Marui's *Categories of perception for vibrato, flange, and stereo chorus* (2006). Page 2 describes one compressed/overdriven guitar note, rates 2, 3, 4, 6, 9 Hz, five peak depths 40–1000 us, 1.4 s stimuli and simple sinusoidal modulation. Page 3 describes 25 young computer-science students without strong musical backgrounds and a fixed effect-task order. Page 4, figure 2, fits lower depth `814/rate - 66` us (2–9 Hz) and upper `4800/rate - 350` us only at 4, 6, 9 Hz. No upper boundary was observed at 2 and 3 Hz. The authors explicitly limit generalization by stimulus and participant scope.

**ENGINEERING INTERPRETATION / DESIGN CHOICE:** Motion 0.05–10 Hz is a musical extension. At 4–9 Hz the middle Depth range interpolates geometrically between fitted bounds; endpoints allow zero and twice the upper bound. Below 4 Hz an independently chosen maximum blends continuously from 5 ms at slow rates to twice the 4 Hz upper fit; no upper fit is evaluated below 4 Hz. Above 9 Hz the 9 Hz mapping is held constant. Short centers and the analytic modulation bound further restrict physical excursion. These equations guide nominal excursion, not perception of a non-sinusoidal, multiband effect. A raw mapping switch is available only for offline comparison.

## Multiscale and coherence

**PAPER-BACKED BEHAVIOR:** `chorus-flange-quality-enhancements(1).pdf`, pp. 4–5, section 7 contrasts multiple fullband voices with one delayed version distributed across a filterbank. Their bank is dyadic/octave based. Independent modulation per scale reduces harmonically locked metallic movement; a shared law restores flange-like character. Per-scale envelope tracking is also explored.

**ENGINEERING INTERPRETATION / DESIGN CHOICE:** Four bands use complementary residuals of parallel one-pole TPT lowpasses at 250, 1000, 4000 Hz. Their telescoping sum reconstructs the input sample exactly, without an all-pass phase error or latency. Slopes are intentionally gentle and bands overlap; this differs from the paper's bank and from Linkwitz–Riley. Coherence weights are `C` and `sqrt(1-C²)`, preserving expected variance of independent zero-mean sources. A separately seeded right-channel component provides controlled stereo decorrelation. Neither weighting law is from the paper. No center spread is used in M1.

## Envelope interaction

**PAPER-BACKED BEHAVIOR:** `chorus-flange-quality-enhancements(1).pdf`, p. 4, section 6 recommends using original-input envelope to reduce processed contribution at low level while retaining the original signal, in contrast to gating the whole effect. Page 5, section 8 proposes envelope-driven randomness, feedback and center delay.

**ENGINEERING INTERPRETATION / DESIGN CHOICE:** A stereo-linked peak envelope with 15 ms attack / 250 ms release plus 50 ms control smoothing changes Chaos by at most 0.2, feedback by at most 0.08, and wet prominence by at most 55%. Original dry gain remains `1-Mix`. We use one broadband detector, not per-band tracking, and do not modulate center from the envelope. These amounts and time constants are ours; freedom from perceptible pumping still requires listening.

## M2.0 BBD transport source boundary

**PAPER-BACKED BEHAVIOR:** `bbd modeling (1).pdf`, Raffel/Smith (2010), pp. 1–2 covers stages, two-phase clock, input/output filters and delay `N/(2*fclock)`; pp. 3–5 covers filter modeling, companding, clocked resampling, insertion gain, noise and nonlinearity; pp. 6–7 discusses nonlinear fitting and its limitations. `BBD filters model (1).pdf`, Holters/Parker (2018), pp. 1–4 develops fixed-stage variable-rate sampling with surrounding filters used for asynchronous resampling; p. 6 shows why abruptly changing BBD clock differs from abruptly moving a digital read pointer; pp. 5–8 compares Juno-60 response, gain, aliasing and oversampling. These are distinct physical models, not ordinary digital delay with saturation added.

**PAPER-BACKED BEHAVIOR USED IN M2.0:** From Raffel/Smith pp. 1–2, M2.0 uses a fixed sequence of storage stages, a two-phase clock, the relationship `delay=N/(2*fclock)`, and explicit input/output filter boundaries. From Holters/Parker pp. 1–4 and p. 6, it uses fixed-stage variable-rate transport, asynchronous sampling/holding, retained transport history, and clock variation rather than movement of an interpolated read pointer. These sources support the architecture and the expected timing/pitch mechanism; they do not specify our host-sample scheduler, data structure, limits, or placeholder reconstruction.

**ENGINEERING INTERPRETATION / DESIGN CHOICE:** `ClockedBBDCore` treats each half-clock transfer as one abstract whole-chain event at rate `2*fclock`. A double phase accumulator schedules those events; a circular stage array makes each event O(1). Input conditioning is identity and output reconstruction is zero-order hold. The 128-events-per-host-sample ceiling and 1 Hz clock floor are deterministic computational bounds. They are not device measurements. M2.0 deliberately omits circuit filters, spectral refinement, noise, feedthrough, companding, gain loss, nonlinearity and oversampling. The existing production `ModulatedDelayVoice` still contains `DigitalFractionalDelay`; no M1.1 bake-off choice was selected or changed.

## M2.2 device-character ledger (before implementation)

- **PAPER-BACKED:** Raffel/Smith, PDF p.5 sections 4.2–4.3: insertion response depends on clock and stage count; noise can be inserted immediately before/after the delay and seeds feedback. p.4 section 3.1 reports approximately 60 dB SNR near maximum input and variable gain. These are broad observations, not calibrated transfer/noise laws.
- **MEASURED / FIT FROM PAPER / FIT FROM PAPER DATA:** Holters/Parker PDF p.6 reports approximately +2.3 dB measured BBD gain in the Juno-60. Use only as an optional constant reference gain, not a universal device specification; no curve fitting is justified.
- **PAPER-BACKED:** Holters/Parker p.6 equations 34–38 specify the rectangular-output sinc. Already implemented in M2.1. Raffel/Smith p.5 quotes -4 to -6 dB near Nyquist but does not de-embed that hold. Do not fit an extra filter to this range: it could double-count sinc.
- **ENGINEERING APPROXIMATION:** Opt-in separable aggregate transfer: exp(-N*(lossPerStage + leakagePerStageSecond/(2*clock))) times a discrete one-pole at BBD output-update rate. Pole strength scales with N; all strengths default zero. No measured residual HF curve is claimed.
- **ENGINEERING APPROXIMATION:** Uniform deterministic noise sampled at capture and/or output updates; independent source streams; RMS proportional to sqrt(N/1024). The placement is paper-backed; distribution, independent-stage accumulation and clock-independent source variance are assumptions. Holding and output filters shape the spectrum; no added colored-noise fit or direct delay-to-noise mapping.
- **ENGINEERING APPROXIMATION:** Fixed aggregate gain mismatch, drawn once in prepare from seed, bounded and neutral by default; not a physical capacitor distribution fit. No alternating-phase mismatch, DC leakage source, per-bucket stochastic stage simulation or audible clock oscillator is justified by these papers.
- Production, Table 1 and all existing profiles remain unchanged. No compander/nonlinear or listening choices.

## M2.4 nonlinear ledger

- **PAPER-BACKED:** Raffel/Smith PDF pp.5–7 §§4.4–4.5 reports approximately `1.01^(N/1024)-1` THD, fairly amplitude-independent device distortion and asymmetric harmonic spectra; proposes a cubic `x-a*x²-b*x³+a`, `a=1/8,b=1/18`, whose low-level harmonics underestimate measurements. Printed outer branches are discontinuous with that offset; do not assume its prose establishes derivative continuity. Measurements are after reconstruction, and distortion follows input filtering.
- **FIT FROM PAPER DATA:** none in M2.4. Published illustrative coefficients are not a new fit; no calibrated voltage mapping, source-point residual error or measured DC offset is available.
- **ENGINEERING APPROXIMATION:** zero-offset cubic variant,zero-default strength,aggregate transfer at BBD output updates before loss/noise/hold,C1 rational bounded overload continuation; neutral stage/clock dependence. No per-stage equivalence is claimed. Input voltage normalization,DC behavior and overload policy are engineering choices. Insertion gain is separate,not hidden in coefficients.
- Detailed evidence,printed-equation discrepancy,alias measurements,feedback limitations and qualification method: `m2_4_bbd_nonlinearity.md`. Production remains DigitalFractionalDelay; companding is deferred to M2.5.

## M2.6 operating-level source review (before profile qualification)

Re-read the supplied papers, with focused extraction of Raffel/Smith PDF pp.4-7
and Holters/Parker PDF pp.1-6. The three modulation/perception papers supply no
BBD terminal operating-point or supply-voltage calibration.

- **PAPER-BACKED:** Raffel/Smith p.4 sections 3.1-3.2: low distortion requires
  input to remain a small fraction of supply; near-maximum inputs typically give
  approximately 60 dB SNR. Companding brings the signal near that maximum.
  Neither the fraction, supply nor maximum volts is specified. Insertion gain is
  typically 0-2 dB, variable with device/clock/stages. These are broad observations,
  not a threshold for this normalized model or a guaranteed dynamic range.
- **PAPER-BACKED:** p.4 sections 3.3-3.4: 2:1 compressor/expander, 10 kohm,
  0.22-1 uF. Both outside-filter and terminal placements and different feedback
  placements are reported, without a universal feedback gain or terminal timebase.
- **PAPER-BACKED:** pp.5-6: approximate stage-dependent THD, and distortion figures
  taken after reconstruction at different amplitudes. Figures 9-12 use relative
  amplitude/normalized axes, without physical voltage, supply, bias, exact source
  amplitude or measured IC operating point sufficient to calibrate x=1.
- **DEVICE-SPECIFIC / NOT GENERALIZED:** Holters/Parker p.6 measured Juno-60 BBD
  insertion gain approximately +2.3 dB, minimum clock about 26 kHz; not a universal
  gain, headroom or terminal voltage. Table 1 filter residues/poles describe the
  example linear response, not a general unit-gain assumption.
- **DERIVED FROM PAPER DATA:** inverse gains g and 1/g across an ideal linear
  transport cancel; when used before/after the filters, scalar gain commutes with
  linear filtering. The separate detectors recover their original units only if
  gain is inverted BEFORE expansion. Pre-compressor scaling a requires an external
  inverse a, not simply 1/sqrt(a), for steady ideal roundtrip.
- **ENGINEERING NORMALIZATION:** x=1 is the existing nominal nonlinear boundary;
  |x|=2 is an overload reporting boundary, not promised physical headroom. Useful
  band 0.1-1 is a reporting convention. Profile terminal gains 1, 1/8, 1/4, 1/2
  explore fixed attenuation and its reciprocal; no voltage mapping or new default.
  Detector startup unity, .47 uF fixture, fixed existing noise RMS, SNR-scaled
  noise experiments and candidate SNR/THD/occupancy thresholds remain engineering.
  No clipping, limiter or coefficient retuning is introduced.

## M2.7 actual engine integration

**QUALIFIED PREVIOUS MILESTONE:** M2.6 merged as d726052 after green complete CI, Linux ASan/UBSan and headroom artifact. Its gain-staged OutsideFilters signal path is reused under the neutral `BBDFullPath` name.

**ENGINEERING INTEGRATION CHOICE / NOT PRODUCT DEFAULT:** internal pre-prepare BBD backend, 1024-stage Table1/.47uF/10kohm/full synthetic character/M2.4/M2.6 Nominal fixture, ExternalWetReturn before the output DC blocker and per-band seeds shared across L/R. Digital plugin routing and all macro equations stay unchanged. Engine admission uses actual physical minimum and preserves quarter-sample slew.

Stage feasibility and actual eight-voice performance are measured, without selecting product stages or compensating the multiband/BBD transfer. The complete qualification grid is an onset admission check; long modulation tracking is separate. Feedback's .75 guard is not actually attained by current .65+.08 parameter formulas; the .75 direct voice gate is separate. Timing is host-specific and instrumented. See [M2.7 integration](m2_7_bbd_engine_integration.md). User-facing selection, final calibration, solver, topology and subjective winners remain deferred.
# M2.8 qualification note

The actual eight-voice engine now has a separate realtime stress harness covering
the model's 128-edge boundary, callback distributions, feedback/DC evolution,
startup, fault/reset recovery, automation and subnormal tails. These are
ENGINEERING ROBUSTNESS and ENGINEERING PRODUCT CRITERIA, not new sound-design
defaults. Noise-off/nonlinearity-off comparisons are labelled measurement
fixtures. Minimum-delay high-rate deadline failures prevent an unrestricted BBD
realtime claim. DigitalFractional remains production/default. See
[M2.8 report](m2_8_bbd_realtime_hardening.md) for provenance and limits.

# M2.9 product-envelope qualification

M2.8 is QUALIFIED PREVIOUS MILESTONE evidence for model robustness, including
uncomfortable maximum-clock realtime corners. M2.8.1 fixes immediate hostile
state-recovery reporting (0 frames) without changing detector recovery or DSP.
M2.9 investigates 512/1024/2048/4096 stages with event ceilings below the
128-edge MODEL CAPABILITY. Repeated CPU, response/noise/distortion, onset
headroom, parameter coverage and feedback/DC evidence stay separately visible.
Only the qualification harness contains the experimental feedback blocker.
ENGINEERING PRODUCT ENVELOPE and PRODUCTIZATION CANDIDATE labels do not imply
FINAL SHIPPING DEFAULT. All prior CI jobs/artifacts remain; new hosted timing
is informational. See [M2.9 report](m2_9_bbd_product_envelope.md).

M2.9 local recommendation (6 October 2026): first listen to Balanced/1024,
Nominal, 44.1/48/88.2 kHz, clock <= min(384 kHz, 8*hostRate), with the 1.451/
1.333 ms floors and block64 qualification reference. Five production modulation
runs at 48 kHz/block64 had median/worst p99 .363/.389 ms and zero misses.
96 kHz remains experimental because the additional repeated block128 cohort
showed unresolved tails; do not discard that cohort to advertise support.
Only 41.67% of the existing short-center-heavy grid is fully preserved, so
explicit future range/depth admission remains a blocker. Nominal onset peak
and loop/output DC evidence support further listening, not public readiness.
