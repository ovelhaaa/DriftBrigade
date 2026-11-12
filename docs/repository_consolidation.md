# Repository consolidation audit

Snapshot on 2026-11-12. The rows below list the remote heads with the common prefix `codex/` omitted. Preserve the historical refs. Use one merge PR into main from the M3.1C.1 lineage, retaining all five original commits.

| Branch | HEAD | Purpose and action |
|---|---|---|
| main | dedf4cb | Integrated through M2.9 via PR #10 |
| add-organic-modulation-bake-off | c9ac933 | M1.1, integrated in #1 |
| implement-transport-foundation-for-bbd-delay | e5eb925 | M2.0, integrated in #2 |
| m2-1-bbd-linear-resampling | 4a22c4a | M2.1, integrated in #3 |
| m2-2-bbd-device-character | 9bf785e | M2.2, integrated in #4 |
| m2-3-exact-async-bbd-runtime | 3a8d794 | Historical SHA outside main ancestry, patch equivalent to integrated 166c568 per git cherry |
| m2-4-bbd-nonlinearity | 10e724f | M2.4, ancestor of main |
| m2-5-bbd-compander | 6746927 | M2.5, integrated in #5 |
| m2-6-bbd-headroom | 1ceb348 | M2.6, integrated in #6 |
| m2-7-bbd-engine | a0f359e | M2.7, integrated in #7 |
| m2-7-1-instrumentation-reset | 822cf90 | M2.7.1, integrated in #8 |
| m2-8-bbd-realtime-hardening | cc73eed | M2.8, integrated in #9 |
| m2-9-bbd-product-envelope | ca41d73 | M2.9, integrated in #10 |
| m3-0-bbd-listening-bakeoff | 9876796 | First commit above main, draft PR #11. Keep until its commit reaches main |
| m3-1b1-organic-voice-retest | 9157382 | Includes M3.0 and retest, ancestor of consolidation |
| m3-1c-bbd-product-admission | 6e8492e | Includes retest, FMA fix 9fe7b22, admission 6565083 and exact-rate fix 6e8492e |

The M3.1A decisions are recorded in the M3 documentation on this lineage. PR #11 was the only open PR at audit time. The M3.1B.1 Linux job failed `bbd_listening_numerical`, `bbd_listening_voice_numerical`, `bbd_performance_numerical` and `bbd_engine_numerical`. The Windows matrix sibling was cancelled, which also left qualification jobs skipped. The next commit aligned FMA contraction across DSP and frozen references, and all 16 check runs succeeded on 6e8492e. Disabling contraction can increase arithmetic instruction count on FMA targets. Performance comparisons should record the compiler and flags.

CI runs DSP on Windows, Linux and macOS, Linux ASan/UBSan, and Windows JUCE tests. The DSP matrix gates all artifact jobs, and sanitizer success additionally gates M2.2–M3.1. The listening job renders both packages twice, compares their artifacts, and exercises the Listening Lab package loader. The old root README described the M1 feature set. The current index distinguishes internal BBD admission, historical preference records and public defaults.
