# M3.0 listening guide

Listen first, record your preference, then open `ANSWER_KEY.csv`. The manifest
and objective reports also reveal identities; keep them closed during blind
listening. No subjective winner was selected.

Start with `level_matched/`. Compare files with the same group ID, for example
`SET_A01_X.wav` and `SET_A01_Y.wav`. X/Y/Z are local labels: a label does not
identify the same architecture across groups. Use the corresponding files in
`raw/` afterwards to assess the actual output-gain difference. Keep playback
volume fixed within a group. Repeat in a different listening order and take
breaks. Headphones and speakers can expose different stereo impressions.

The three dry references are a sustained polyphonic pad (A), a plucked harmonic
phrase (B), and a mono voice-like lead (C). All last ten seconds, with a half
second of initial silence and 2.5 seconds after excitation. Source C has
identical left/right dry samples and enters the real stereo engine as a
duplicated mono signal. Listen to the dry reference when assessing how well an
attack or pitch remains attached to the original material. Group C01 uses dry A
at one eighth of its reference amplitude for both members, exposing quiet
material and startup behavior.

| Group | Source / scene | Question | Listen for |
|---|---|---|---|
| A01 | Pad / subtle chorus | Backend | Tonal integration, smoothness, noise at onset |
| A02 | Pad / wide chorus | Backend | Movement, grain/metallicity, stereo image |
| A03 | Pluck / feedback-rich | Backend | Transient attachment, smear, feedback decay |
| A04 | Mono lead / slow drift | Backend | Pitch continuity, stereo generation, naturalness |
| B01 | Pad / wide chorus | Stage count | Top-end character, darkness, density, modulation texture |
| B02 | Pluck / short delay | Stage count | Combing, transient shape, perceived BBD character |
| C01 | Quiet pad / subtle chorus | Operating gain | Noise, body, compander startup |
| C02 | Pluck / feedback-rich | Operating gain | Transient aggression, distortion, tail behavior |
| D01 | Pad / wide chorus, Chaos .5 | Organic modulation | Periodicity, wandering, motion continuity |
| D02 | Mono lead / wide chorus, Chaos 1 | Organic modulation | Naturalness, seasick pitch sensation, static repetition |
| E01 | Pad / wide chorus | Multiscale bank | Cohesiveness, spectral movement, multiband separation |
| E02 | Pluck / subtle chorus | Multiscale bank | Metallic quality, separation, attack integration |

Each group changes one principal factor. Do not use files from different groups
as an architecture comparison: their source, scene or source level may differ.
Neither stage nor bank comparisons are EQ-matched. Organic generators retain
their native waveforms and the engine's existing bound-aware admission.

Fill one row per group in `LISTENING_SCORECARD.csv`. For `preferred_candidate`,
use X, Y or Z, **no preference**, or **needs more listening**. Leave uncertain
quality dimensions blank. Preference strength is 1 (weak) to 5 (strong).
Naturalness, stereo, tonal, transient and tail quality use 1 (poor) to 5 (good).
Artifacts and fatigue use 1 (little/none) to 5 (strong/objectionable). Describe
specific moments in comments, including onset, chord transitions and late tails.
You can note RAW and level-matched impressions separately in the same comment.

Level matching uses a single fixed gain across each complete file, measured on
the common active region from 0.5 to 7.5 seconds. This also changes the audible
noise/tail level by that same gain. No limiter, dynamic normalization or spectral
compensation is used. Silence and tails remain available for listening even
though they are excluded from the matching measurement.

After recording blind preferences, inspect the answer key and optional objective
reports. RMS, peak, DC, correlation and spectral measurements explain measured
differences; they do not determine musical preference. Confidence may differ
between questions. Report your backend, stage, gain, modulation and bank
preferences independently, including no preference or more listening where
appropriate. These decisions will be frozen in a later milestone; this bake-off
does not apply them to the plugin.

BBD remains an internal **PRODUCTIZATION CANDIDATE / NOT SHIPPING DEFAULT**.
DigitalFractional remains the production/default backend.
