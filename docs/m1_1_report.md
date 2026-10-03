# M1.1 organic-modulation listening bake-off

## Implementation summary

M1's modulator is preserved as internal `OrganicVariant::Wander`. Two explicitly selectable offline alternatives, `PaperNarrowband` and `PhaseDrift`, share the allocation-free enum/switch implementation. The plug-in has no new parameter and constructs Wander/Gentle/Current by default. The renderer produces deterministic 48 kHz stereo PCM WAVs, control CSVs, fixed-gain synthetic sources, and a guide. No BBD behavior was added.

`BankMode::Selective` is an offline alternative made from three sequential two-pole low-pass/residual splits at 250/1000/4000 Hz. Each split defines `high=input-low`, so reconstruction telescopes exactly with zero processing latency. `DynamicsMode::MotionOnly` retains input-driven Chaos and feedback increases but fixes wet prominence at one.

## Exact control formulas

Let `s=sin(2*pi*phase)`, `r` be uniform targets in `[-1,1]` joined by `h(u)=(1-cos(pi*u))/2`, and `c=Chaos`.

- **A / Wander:** targets update at `2*Motion`; `y=((1-c)s+c*r)*sqrt(0.5/(0.5(1-c)^2+0.25c^2))`. This is unchanged M1 behavior.
- **B / Paper Narrowband:** the carrier is `s` (cosine with a fixed phase offset), envelope targets update at `Motion`, shifted control is `q=2*r*s`, and `y=((1-c)s+c*q)*sqrt(0.5/(0.5((1-c)^2+c^2)))`. Factor two matches the theoretical RMS of independent interpolated-uniform envelope times sine. Finite captures retain honest covariance/rate differences.
- **C / Phase Drift:** targets update at `0.5*Motion`; `instantaneousRate=Motion*(1+0.75*c*r)`, phase integrates `instantaneousRate/sampleRate`, and `y=sin(2*pi*phase)`. Rate stays positive and its expected long-term value is Motion.

All phases and random segment positions continue through parameter changes. At `c=0`, all variants are sample-identical. A/B/C analytical source magnitudes are bounded by `sqrt(3)`, `2*sqrt(2)`, and `1`; no control limiter or audio-rate noise is used.

## Evidence boundary

**PAPER-BACKED BEHAVIOR:** low-rate random points, raised-cosine interpolation, and optional multiplication of the DC-centered smooth random process by a cosine carrier come from Fernández-Cid/Casajús-Quirós. Independent multiscale modulation and envelope interactions are also motivated there.

**ENGINEERING INTERPRETATION:** B's update ratio, scaling, carrier phase and Chaos blend are ours. C in its entirety is our experiment, not a paper algorithm. Both bank topologies, crossover realization, dynamics amounts, PRNG and normalization are implementation decisions.

## Measured modulation statistics

Release GCC 13.3, 48 kHz, fixed seed, 20-second windows. The renderer prints the complete 60-row matrix for Motion 0.2/0.7/2/6 Hz and Chaos 0/.25/.5/.75/1. Representative Motion=0.7 Hz results:

| Variant | Chaos | centered RMS | peak | maximum step |
|---|---:|---:|---:|---:|
| A | 0 / .5 / 1 | .707 / .688 / .677 | 1.000 / 1.595 / 1.414 | .0000916 / .0000967 / .000116 |
| B | 0 / .5 / 1 | .707 / .910 / .805 | 1.000 / 1.927 / 1.733 | .0000916 / .000194 / .000183 |
| C | 0 / .5 / 1 | .707 / .707 / .707 | 1.000 / 1.000 / 1.000 | .0000916 / .000126 / .000160 |

Across the full matrix, B's worst observed RMS was .910 and A/C remained within .677–.795/.706–.708. B is intentionally not compressed; its residual up-to-34% RMS difference at a slow finite capture is a listening caveat. Absolute mean reached .401 in a short, slow A capture and trends toward zero over longer seeded trajectories. Maximum observed steps remained below .0022 at 6 Hz. Raised-cosine transition-neighborhood steps stayed comparable to ordinary steps; automated tests cover boundaries and smoothed changes.

## Filterbank metrics

Both Gentle and Selective reconstructed 100,000 impulse/random samples at 44.1/48/88.2/96 kHz with measured peak error below `1e-14` and RMS below `1e-15`. Both are zero-latency algebraic residual banks. Selective has steeper two-pole low branches, but individual bands carry causal filter phase; exact summed reconstruction does not imply isolated-band linear phase.

## CPU comparison

Isolated Release timing for 480,000 stereo frames (I/O excluded) was A 0.302 s, B 0.306 s, C 0.287 s: **A 1.00x, B 1.01x, C 0.95x**. This is a local observation, not a cross-machine promise; no disproportionate regression was found.

## Tests and CI

Headless tests cover variant/seed determinism, Chaos-zero identity, bounds, RMS sanity, transition and automation continuity, all-variant block segmentation, both banks' reconstruction, and the existing no-process-allocation invariant. Existing depth, delay, dynamics, stereo, feedback, DC, rate and sample-rate coverage remains. CI retains cross-platform DSP, Linux sanitizer, and Windows plug-in jobs and adds a dependent Ubuntu bake-off artifact job. Hosted CI status was unavailable from this local run and is not claimed.

## Artifact manifest

`01_A_Wander.wav`, `02_B_PaperNarrowband.wav`, `03_C_PhaseDrift.wav`, `04_A_Wander_HighChaos.wav`, `05_B_PaperNarrowband_HighChaos.wav`, `06_C_PhaseDrift_HighChaos.wav`, `07_Coherence_0.wav`, `08_Coherence_50.wav`, `09_Coherence_100.wav`, `10_Bank_Gentle_Coh0.wav`, `11_Bank_Selective_Coh0.wav`, `12_Bank_Gentle_Coh100.wav`, `13_Bank_Selective_Coh100.wav`, `14_Dynamics_Current.wav`, and `15_Dynamics_MotionOnly.wav`.

Files 01–06 also have same-base-name CSV telemetry containing time, organic control, four left band modulation/delay values, envelope, effective Chaos/feedback, carrier, random envelope, instantaneous rate and phase derivative. `README.txt` and `LISTENING_GUIDE.md` accompany them in CI.

## Unresolved concerns and default confirmation

- B's finite-window depth varies more than A/C and may sound amplitude-modulated; this is deliberately exposed rather than hidden by limiting.
- Selective's greater isolation may color independently delayed bands despite exact unprocessed reconstruction.
- Whether Current pumps, Coherence is sufficiently distinct, and which motion is musical are listening questions. No preference is asserted.
- Generated WAVs are not committed. PCM is deterministic within a build; floating math identity across standard libraries is not promised.
- The normal plug-in remains **Variant A / Wander, Gentle bank, Current dynamics**, preserving M1 defaults.

## Listening checklist

- Organic preference: **A / B / C**
- Preferred bank: **Gentle / Selective**
- Preferred dynamics: **Current / MotionOnly**
- Coherence contrast: **Too weak / Good / Too strong**
