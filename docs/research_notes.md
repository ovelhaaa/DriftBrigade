# M1 research notes

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
