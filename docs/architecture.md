# M1 architecture

All numerical choices below are **engineering decisions** unless explicitly identified as sourced. References and experimental caveats are in `research_notes.md`.

## Flow and ownership

`PluginProcessor` handles JUCE buses/state and loads nine cached atomic parameter pointers once per block. `DriftEngine` smooths controls per sample, measures a linked original-input envelope, splits each channel into four bands, builds shared/independent/stereo control signals, processes eight contained `ModulatedDelayVoice`s (the legacy `DelayPath` name is an alias), recombines bands and mixes with the unfiltered original. Mono/mono and stereo/stereo are supported; mono/stereo duplicates the original before the same stereo engine. Mono output uses the left motion system without summing phase-inverted wet paths.

Preparation allocates all delay storage; processing uses fixed arrays and preallocated vectors. There are no locks, filesystem operations, logging, allocations or throwing operations in the audio callback. Parameter reads use relaxed float atomics, which are lock-free on the supported desktop targets. The engine assumes prepare precedes processing; reset is a lifecycle operation and must not run concurrently with processing. Modulation is deterministic given seed, input and sample-timed automation; bit identity is promised within a toolchain, not across libm implementations. Host automation is sampled once per callback, so differing host automation timestamps can still produce differing results.

## Complementary bank and latency

Three parallel bilinear/TPT one-pole lowpasses at 250/1000/4000 Hz form `L0`, `L1`, `L2`; bands are `L0`, `L1-L0`, `L2-L1`, `input-L2`. The sum telescopes to the original with only floating-point roundoff, including phase. This gentle, overlapping bank favors exact zero-latency reconstruction; it is not a brick-wall separator. Differential delay intentionally breaks cancellation between bands. Identical delays recover the fullband delayed signal when feedback limiting is inactive. The filterbank concept is **sourced from paper** (Fernández-Cid/Casajús-Quirós pp. 4–5); this topology is our decision. Crossovers are centralized in `MultiscaleBank.h`.

Reported host latency is zero: dry audio is untouched and wet center delay is an effect parameter, not a compensated processing latency. Mix=0 is exact once the smoothed Mix has reached zero (or starts at zero). During automation toward zero the smoothing tail remains audible by design.

## Delay boundary and feedback

`DelayPath::process(input, delaySeconds, feedback)` contains a normal digital `DigitalFractionalDelay`. Four-point cubic Hermite interpolation reads wrapped neighbors with at least three samples of margin; engine minimum is four samples, capacity is 60 ms plus eight samples. Center and depth remain in seconds until the delay boundary. Smoothed macros and a final delay slew cap of 0.25 sample/sample prevent read-pointer jumps, including large Center changes. This introduces a short pitch glide instead of an instantaneous time jump. No fixed band center spread is used.

Feedback returns the previous interpolated wet sample to the delay input; a symmetric hard limiter clamps the write to +/-16 (unity below that threshold). Hermite coefficient absolute sum is at most 1.25; maximum loop gain is 0.75, so their product is below unity. The limiter additionally bounds stored values under arbitrary finite float input. This is a transparent emergency mechanism, **not BBD saturation**. Per-path 5 Hz DC blocking occurs after the feedback tap; it cannot destabilize the loop and slightly attenuates sub-audio wet content. Dry audio bypasses it. The feedback path includes one explicit sample of recursion delay.

## OrganicModulator and Chaos

**Sourced from paper:** random control points at low rate, raised-cosine interpolation and flat transition endpoints (Fernández-Cid/Casajús-Quirós pp. 3–4).

**Engineering decision:** xorshift32 maps targets uniformly to [-1,1], with new points at twice Motion. A fractional segment phase is never reset by rate changes. Adjacent targets interpolate with `h(t)=(1-cos(pi*t))/2`: bounded between endpoints, C1 across random segments, generally not C2. A continuous sine phase runs concurrently. Chaos `c` produces `((1-c)*sin + c*random)*sqrt(0.5/(0.5*(1-c)^2+0.25*c^2))`. Uniform-target interpolated noise has theoretical variance 1/4; sine variance is 1/2, and independent phases make the cross term zero in expectation. Normalization stabilizes long-term RMS (not every short-window peak). The morph is on control signals, not between audio effects. Analytic source bound is sqrt(3). Public rate/Chaos setters have 50 ms smoothing; the engine uses the already-smoothed macro path.

M1.1 names that unchanged path `Wander`. Internal-only `PaperNarrowband` multiplies a smooth random envelope (one target per Motion cycle) by the continuous Motion carrier and blends it with the sine. `PhaseDrift` updates its smooth random target at half Motion and integrates `Motion*(1+0.75*Chaos*random)`; it changes timing, never oscillator amplitude. All three return the identical sine at Chaos=0. These enum choices are neither serialized nor exposed to the plug-in, whose default remains Wander.

Their analytical source bounds are respectively `sqrt(3)`, `2*sqrt(2)`, and `1`. The engine retains a conservative combined bound of twice the selected source bound: each of the coherence and stereo variance-preserving weighted sums can contribute at most a `sqrt(2)` magnitude factor. Bounds therefore remain variant-specific rather than weakening Wander/PhaseDrift qualification to B's larger range.

## Coherence and stereo

For each band, `base=C*common+sqrt(1-C*C)*independent`. Band-independent sources have rates 0.91/1.037/1.083/0.967 times Motion and distinct seeded phases/targets. At C=1 all left bands move identically; at C=0 they use independent streams. At intermediate C, pairwise correlation tends toward C² for statistically independent components. Expected variance is preserved. The independent/shared perceptual contrast is **sourced from paper** (pp. 4–5); this law and ratios are ours.

Left uses `base`; right uses `rho*base+sqrt(1-rho*rho)*side`, where `rho=1-0.35*Width`. Side streams have independently seeded motion and slight rate offsets. Width=0 uses identical L/R controls; Width=1 gives partial, not antiphase, decorrelation. With stereo input Width=0 does not force mono audio. Mono output uses identical motion for both internal paths. Coherence=1 is perfectly shared per band on the left; on the right nonzero Width adds band-dependent variation, so use Width=0 for a fully locked stereo flange comparison. The combined control magnitude is bounded by sqrt(12).

## Depth guide

The physical nominal excursion is rate-dependent; 4–9 Hz uses Martens/Marui's lower and upper fits **sourced from paper**, p. 4. Depth 0–25% goes linearly from zero to lower; 25–75% is geometric between lower and upper; 75–100% reaches twice upper. Below 4 Hz the result blends from an independent 0–5 ms mapping at 0.05 Hz into the 4 Hz mapping, without evaluating the upper equation below its measured domain. Above 9 Hz we hold the 9 Hz mapping. Thus maximum nominal excursion is 5 ms at slowest Motion and 366.67 us at 9–10 Hz. There is no discontinuity at 4 or 9 Hz. These are design extensions, not predictions from the experiment.

Nominal excursion is additionally clamped to `(center - 4/sampleRate)*0.85/sqrt(12)` before multiplying by the bounded modulation. This is deliberately conservative: short-center depth is reduced while coherence/width changes retain headroom without changing the depth law. Raw-depth comparison uses 0–5 ms before the same safety restriction. An effective per-band read delay also has a 55 ms upper guard. Faster rates may move less in time yet remain salient in pitch. Listening is needed to decide whether the mapping is preferable for different instruments.

## Dynamics

**Sourced from paper:** original-input envelope can shape wet contribution, randomness and feedback (pp. 4–5). We do not use its proposed envelope-to-center variation or per-scale envelope tracking.

The original linked stereo peak follows 15 ms attack and 250 ms release. `intensity=envelope/(envelope+0.1)` is additionally smoothed at 50 ms. Effective Chaos is `min(1,Chaos+0.2*Dynamics*intensity)`, effective Feedback is `min(0.75,Feedback+0.08*Dynamics*intensity)`, and wet prominence is `1-0.55*Dynamics*(1-intensity)`. Output is `(1-Mix)*dry+Mix*wetProminence*wet`. Quiet input reduces only the processed term; the original is not gated. Dynamics=0 restores level-independent motion. At Mix=1 the user has explicitly removed the dry contribution. Time constants and amounts are centralized; absence of audible pumping cannot be proven by the detector test alone.

## Parameters and state

| Parameter | Range | Default | Mapping |
|---|---|---|---|
| Motion | 0.05–10 Hz | 0.7 Hz | logarithmic |
| Depth | 0–100% | 50% | rate-guided time excursion |
| Center | 0.3–30 ms | 8 ms | logarithmic |
| Chaos | 0–100% | 55% | normalized control morph |
| Coherence | 0–100% | 45% | shared amplitude weight |
| Dynamics | 0–100% | 25% | envelope influence |
| Feedback | 0–65% | 12% | positive loop gain; dynamic ceiling 75% |
| Mix | 0–100% | 50% | linear dry/wet |
| Width | 0–100% | 60% | partial stereo decorrelation |

All macro controls are smoothed at 50 ms. Parameter ranges/defaults/IDs are centralized in `params/ParameterSpecs.h`; IDs and version hints remain stable when product name changes. APVTS serializes them to XML with schema=1. Loading replaces state, with smoothing applied on the next audio callback. Unknown schema/malformed XML is ignored. Seed defaults to a fixed constant and is reset by prepare/reset; transport position is not followed or saved. State contains parameters, not delay memory or evolving PRNG position. Restoring settings and replaying from prepare with the same input is repeatable.

## Experimental M2.0 delay-core boundary

The voice boundary is input conditioning -> delay core -> reconstruction -> feedback routing. Production instantiates the digital core and retains exactly the M1.1 processing/defaults. Separately, internal `ClockedBBDCore` implements the experimental input-filter hook, fixed circular stage transport, double-precision event scheduler and temporary zero-order output hold. It accepts physical seconds through `setDelaySeconds`; it is not selectable by parameters or UI and is not placed in `DriftEngine` yet because clocked feedback and circuit-filter placement are not behaviorally interchangeable with the production loop.

For N stages, `fclock=N/(2D)` and event rate is `2*fclock`. Each host sample adds `eventRate/hostRate` to phase, executes `floor(phase)` transfers, and retains the fractional remainder. Thus zero, one, or many events occur without integer ratio truncation, and block calls cannot affect state evolution. Each transfer retires the circular head (the final stage), writes the conditioned input there (new stage zero), and rotates the head. Storage and ownership are fixed after `prepare`; processing allocates nothing. Full details and qualification are in `m2_0_bbd_transport.md`.
