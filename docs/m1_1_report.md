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

For delay safety, each coherence/stereo variance-preserving weighted sum has a conservative `sqrt(2)` magnitude factor. Applying both gives `2*sourceBound`: A `2*sqrt(3)`, B `4*sqrt(2)`, and C `2`. This preserves the original A margin while keeping tighter C qualification and the required larger B bound.

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

### A/B/C listening fairness qualification

Files 01–06 now use Motion .7 Hz, Depth .5, and Center 15 ms. Requested nominal excursion is 2.14486 ms; B's tightest safety ceiling is 2.24138 ms, so **none of A/B/C is safety-clamped**. Previous settings requested 3.04063 ms at Center 7 ms and produced unequal actual excursions.

| Render | Previous actual excursion / delay RMS (ms) | Qualified actual excursion / delay RMS (ms) |
|---|---:|---:|
| A, Chaos .5 | 1.69717 / 1.22762 | 2.14486 / 1.55124 |
| B, Chaos .5 | 1.03930 / .78437 | 2.14486 / 1.58279 |
| C, Chaos .5 | 2.93958 / 1.14798 | 2.14486 / 1.52965 |
| A, high Chaos | 1.69717 / 1.20324 | 2.14486 / 1.51946 |
| B, high Chaos | 1.03930 / .70964 | 2.14486 / 1.50628 |
| C, high Chaos | 2.93958 / 1.05596 | 2.14486 / 1.34044 |

The same requested and actual excursion is used for each comparison; remaining RMS differences are produced by control character rather than safety clamping.

## Filterbank metrics

Both Gentle and Selective reconstructed 100,000 impulse/random samples at 44.1/48/88.2/96 kHz with measured peak error below `1e-14` and RMS below `1e-15`. Both are zero-latency algebraic residual banks. Selective has steeper two-pole low branches, but individual bands carry causal filter phase; exact summed reconstruction does not imply isolated-band linear phase.

## CPU comparison

Isolated Release timing for 480,000 stereo frames (I/O excluded) was A 0.302 s, B 0.306 s, C 0.287 s: **A 1.00x, B 1.01x, C 0.95x**. This is a local observation, not a cross-machine promise; no disproportionate regression was found.

## Tests and CI

Headless tests cover variant/seed determinism, Chaos-zero identity, variant-specific bounds, RMS sanity, transition and automation continuity, all-variant block segmentation, both banks' reconstruction, and the existing no-process-allocation invariant. Existing depth, delay, dynamics, stereo, feedback, DC, rate and sample-rate coverage remains.

Current PR qualification: **Linux sanitizer PASS; DSP Ubuntu PASS; DSP Windows PASS; DSP macOS PASS; Windows plugin PASS; bake-off PASS.** The `DriftBrigade-M1.1-Bakeoff` artifact was successfully generated. These are objective build/render results and make no subjective listening claim.

### WAV headroom qualification

The renderer measures samples before PCM clamping and fails if any exceeds unity. A single common output gain of .55 remains in use; files are not independently normalized.

| WAV | peak | samples exceeding `[-1,1]` |
|---|---:|---:|
| 01 A Wander | .325934 | 0 |
| 02 B Paper Narrowband | .332587 | 0 |
| 03 C Phase Drift | .361426 | 0 |
| 04 A Wander High Chaos | .327550 | 0 |
| 05 B Paper Narrowband High Chaos | .358855 | 0 |
| 06 C Phase Drift High Chaos | .276569 | 0 |
| 07 Coherence 0 | .254164 | 0 |
| 08 Coherence 50 | .249275 | 0 |
| 09 Coherence 100 | .202270 | 0 |
| 10 Bank Gentle Coh0 | .239521 | 0 |
| 11 Bank Selective Coh0 | .270636 | 0 |
| 12 Bank Gentle Coh100 | .183645 | 0 |
| 13 Bank Selective Coh100 | .183645 | 0 |
| 14 Dynamics Current | .424727 | 0 |
| 15 Dynamics MotionOnly | .432510 | 0 |

## Artifact manifest

`01_A_Wander.wav`, `02_B_PaperNarrowband.wav`, `03_C_PhaseDrift.wav`, `04_A_Wander_HighChaos.wav`, `05_B_PaperNarrowband_HighChaos.wav`, `06_C_PhaseDrift_HighChaos.wav`, `07_Coherence_0.wav`, `08_Coherence_50.wav`, `09_Coherence_100.wav`, `10_Bank_Gentle_Coh0.wav`, `11_Bank_Selective_Coh0.wav`, `12_Bank_Gentle_Coh100.wav`, `13_Bank_Selective_Coh100.wav`, `14_Dynamics_Current.wav`, and `15_Dynamics_MotionOnly.wav`.

Files 01–06 also have same-base-name CSV telemetry containing time, organic control, four left band modulation/delay values, requested and actual excursion, envelope, effective Chaos/feedback, carrier, random envelope, instantaneous rate and phase derivative. Renderer output reports effective delay RMS and pre-clamp WAV peak/clipping for every file. `README.txt` and `LISTENING_GUIDE.md` accompany them in CI.

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
