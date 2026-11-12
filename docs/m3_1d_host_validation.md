# M3.1D host validation

Balanced1024 was exercised using the same production DSP library that the VST3 links. The harness prepares a product scene through `prepareProduct()`, uses an identical parameter and input schedule for the DigitalFractional control, and changes sample rate only between renders. The isolated JUCE variant adds a second layer of testing for host routing and state.

## Scene and matrix

- Balanced1024, nominal operating gain, Wander, Gentle and a 384 kHz requested clock ceiling, further bounded by eight times the host rate. Initial settings use the normal DSP defaults with Center at 24 ms. The automation envelope covers the public ranges except Center 22–30 ms and Depth 0–85%. All other controls use their existing public limits. The timing run changes the entire tuple from minimum to maximum and back at 40 ms boundaries, quantized to the next block start.
- Each repeating stimulus contains 20 ms each of sustained 220 Hz tone, 0.98 impulse, 330/440 Hz harmonic material, 47 Hz bass, deterministic noise and near-full-scale 440 Hz tone, then 60 ms of silence. Noise uses a fixed hash of the sample index, channel and seed. Both backend and input use seed `0x44524946`.
- The full matrix includes 44.1, 48, 88.2 and 96 kHz, both mono and stereo, and 16, 32, 64, 128, 512 and variable blocks. The variable sequence is 16, 127, 32, 511, 17, 512, 63 and 128. At 96 kHz the product request prepares Digital fallback. The separate admission suite covers additional incompatible rates, invalid rates, failure injection and recovery.
- Each case starts with 128 unmeasured callbacks. Input generation, finite-output validation, statistics and CSV writes occur outside the timed section. Timed work is one control update plus one DSP block. The adjacent pre-DSP clock pair estimates timestamp overhead without subtracting it. Wall time per row includes warmup and the untimed loop work. The benchmark calls DSP in a tight loop. Driver scheduling, VST3 wrapper overhead, APVTS reads and host audio-thread priority are outside the benchmark.

## Reproduce

Use a Release Ninja or Makefiles build that exports a compilation database.

```sh
cmake -S . -B build-dsp -G Ninja -DDRIFT_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-dsp --target drift_bbd_host_qualification drift_bbd_admission_tests --parallel 2
ctest --test-dir build-dsp -C Release -R 'bbd_host|bbd_product_admission' --output-on-failure
python tools/run_bbd_host_qualification.py build-dsp/drift_bbd_host_qualification output/m31d-host --compile-commands build-dsp/compile_commands.json --environment workstation
```

Append `.exe` on Windows. The runner writes per-callback durations, deadlines, utilization and clock overhead to `host_callbacks.csv`. `host_summary.csv` contains nearest-rank p50/p95/p99/p99.9 and maximum in microseconds and deadline percentages, plus misses and miss rates. `host_metadata.json` records OS, CPU, compiler, compiler commands, workload and machine conditions. Use `--environment dedicated --callbacks 20000 --conditions "…"` for extended runs and record audio-machine settings. Check every admitted row for p99 ≤50% and zero misses. CI runs the matrix and retains reports for 14 days. Timing misses are diagnostic in CI.

The new CI timing job waits for the three-platform DSP matrix. The sanitizer job keeps Debug assertions and adds `-O1` to shorten numerical runs, with UBSan configured to fail on the first finding. Historical qualification and listening jobs retain their test dependencies.

## Lifecycle and compatibility

- The DSP lifecycle test starts Digital, then prepares BBD at 44.1 → 48 → 88.2 kHz, falls back at 96 kHz and returns to 44.1 kHz. It checks exact block partition equality in mono and stereo, exact 96 kHz fallback equality against a direct Digital render, reset, finite output with high feedback and a two-second tail, resumption, and exact dry output at Mix 0 with finite float extremes.
- At each admitted rate, the instrumented admission suite tests the same scene's simultaneous automation extrema, out-of-envelope and NaN/Inf updates, and compares output with a control engine that received only the valid tuples. It checks clock bounds, physical limits, hidden clamps, guards and finite voice state. The timing and JUCE processor tests check ordinary C++ allocations on the callback thread. They do not instrument OS allocators or the host wrapper.
- The optional internal VST3 uses the admitted scene and a distinct bundle name, ID and class code. If the entire APVTS tuple is valid on prepare it starts from that tuple. Otherwise it starts from the scene and rejects invalid updates until a complete valid tuple arrives. The editor displays admission, fallback or unavailable status and snapshot rejection. The same policy applies to the internal fallback. There is no callback logging or runtime backend switch. The public target still uses the existing parameter IDs, defaults, schema, identity and Digital backend.
- The JUCE host test loads a built VST3 bundle, switches mono/stereo routing while stopped, changes rates and maximum block size, renders variable blocks, exercises VST3 bypass when advertised, and compares a restored offline render to the corresponding normal-mode render. It uses JUCE's host API with synthetic buffers.

On Windows, both VST3 bundles passed the JUCE loaded-host tests. Their generated VST3 manifests have different audio module and controller class IDs. The internal processor test confirmed admission status and fallback, state restoration, snapshot rejection and no ordinary C++ allocation on the processor callback. Windows Clang UBSan passed the host lifecycle and product admission tests. The Windows MinGW toolchain lacks the ASan runtime required for linking. Combined ASan/UBSan passed in the Linux workflow.

## REAPER 7.81 on Windows

An isolated REAPER profile scanned and instantiated both VST3 variants. A ReaScript generated and saved stereo projects using the same three-second harmonic and transient fixture, Center 30 ms, Depth 60%, Mix 65% and Feedback at 45% of its public range. Each offline render reopened its project in a separate REAPER process. The renderer produced 24-bit stereo PCM at the requested rate. Internal-versus-public RMS differences were 0.015317 at 44.1 kHz, 0.015374 at 48 kHz and 0.015663 at 88.2 kHz. At 96 kHz the outputs were sample-identical, corroborating Digital fallback. The bypassed internal effect was sample-identical to the no-effect render at 48 kHz.

A reopened internal project restored the four saved controls within `1e-5` in normalized units. A second 48 kHz render added simultaneous Depth, Mix and Feedback envelopes and differed from the fixed-control render by 0.022868 RMS. The automated project also played in REAPER with Dummy Audio at 48 kHz/128 and WASAPI at 48 kHz/512. The master fader was at zero during playback and track meters confirmed signal. Both short runs advanced the transport through 1.8 seconds and reported no change in the audio or media underrun timestamps. This check did not capture realtime audio for sample comparison to the offline render.

The same stereo automation project then looped for 300.03 seconds at 48 kHz/512 using WASAPI, with 99 observed wraps and a track peak of 0.3712. REAPER's reported audio and media underrun timestamps stayed at zero. This is a transport and driver smoke test for one instance on this workstation. It does not supply per-callback deadlines.

## Workstation timing

Windows 10 build 19045, AMD Ryzen 7 7730U, GNU 14.2.0 (MinGW-w64), Release C++17, `-O3 -DNDEBUG -ffp-contract=off`, with the production DSP configuration and ordinary C++ allocation checks. Each of the 96 rows contains 2,000 measured callbacks. The run shared the machine with plug-in builds and listening renders. There was no CPU affinity or realtime priority.

Stereo product request results. Callback percentiles and maxima are in microseconds. Relative p99 compares the same rate, channel count and buffer with direct Digital.

| Rate (Hz) | Buffer | Backend | p50 | p95 | p99 | p99.9 | Max | p99 deadline % | p99 vs Digital | Misses |
|---:|:---|:---|---:|---:|---:|---:|---:|---:|---:|---:|
| 44100 | 16 | BBD | 58.6 | 61.3 | 75.2 | 90.1 | 92.9 | 20.73 | 8.17x | 0 |
| 44100 | 32 | BBD | 119.4 | 180.7 | 208.5 | 263.7 | 819.8 | 28.73 | 11.46x | 1 |
| 44100 | 64 | BBD | 243.3 | 363.6 | 402.9 | 439.2 | 453.0 | 27.76 | 12.51x | 0 |
| 44100 | 128 | BBD | 474.9 | 761.2 | 818.6 | 1194.6 | 1742.9 | 28.20 | 9.93x | 0 |
| 44100 | 512 | BBD | 1954.8 | 3006.2 | 3180.6 | 3339.2 | 3748.9 | 27.40 | 9.85x | 0 |
| 44100 | variable | BBD | 459.4 | 3128.5 | 3332.2 | 3570.2 | 3803.7 | 31.13 | 15.67x | 0 |
| 48000 | 16 | BBD | 58.6 | 67.1 | 89.6 | 98.0 | 109.8 | 26.88 | 8.45x | 0 |
| 48000 | 32 | BBD | 115.1 | 164.3 | 178.3 | 191.3 | 239.7 | 26.75 | 9.64x | 0 |
| 48000 | 64 | BBD | 235.2 | 374.9 | 408.9 | 429.6 | 798.5 | 30.67 | 7.69x | 0 |
| 48000 | 128 | BBD | 462.7 | 622.6 | 730.7 | 803.5 | 1010.7 | 27.40 | 9.33x | 0 |
| 48000 | 512 | BBD | 1865.8 | 2899.5 | 3107.8 | 3366.3 | 3627.3 | 29.14 | 10.62x | 0 |
| 48000 | variable | BBD | 378.4 | 1919.2 | 2377.3 | 3224.3 | 3958.9 | 28.35 | 7.98x | 0 |
| 88200 | 16 | BBD | 48.2 | 73.4 | 77.1 | 85.8 | 99.2 | 42.50 | 8.03x | 0 |
| 88200 | 32 | BBD | 95.0 | 117.9 | 155.1 | 164.5 | 183.0 | 42.75 | 9.18x | 0 |
| 88200 | 64 | BBD | 200.1 | 324.6 | 349.7 | 362.7 | 395.9 | 48.19 | 8.17x | 0 |
| 88200 | 128 | BBD | 565.3 | 707.9 | 768.8 | 1258.5 | 2332.1 | 52.98 | 8.91x | 2 |
| 88200 | 512 | BBD | 1617.3 | 2675.3 | 2838.8 | 3761.4 | 5160.2 | 48.90 | 8.19x | 0 |
| 88200 | variable | BBD | 349.8 | 1863.6 | 2560.3 | 2832.8 | 2973.1 | 48.69 | 8.74x | 0 |
| 96000 | 16 | DigitalFallback | 5.9 | 7.9 | 9.0 | 17.8 | 44.5 | 5.40 | 1.08x | 0 |
| 96000 | 32 | DigitalFallback | 11.7 | 14.5 | 16.4 | 18.1 | 50.9 | 4.92 | 0.86x | 0 |
| 96000 | 64 | DigitalFallback | 23.1 | 33.5 | 36.2 | 51.6 | 51.8 | 5.43 | 0.85x | 0 |
| 96000 | 128 | DigitalFallback | 48.3 | 74.0 | 81.8 | 96.5 | 113.7 | 6.13 | 1.33x | 0 |
| 96000 | 512 | DigitalFallback | 188.0 | 289.3 | 307.4 | 345.8 | 374.9 | 5.76 | 0.99x | 0 |
| 96000 | variable | DigitalFallback | 34.2 | 194.4 | 205.1 | 243.9 | 281.6 | 4.98 | 0.67x | 0 |

The shared-load run recorded five BBD misses in 72,000 measured BBD callbacks (0.00694%). Stereo recorded one at 44.1 kHz/32 and two at 88.2 kHz/128. Mono recorded one at 48 kHz/variable and one at 88.2 kHz/16. The 88.2 kHz/128 stereo row had the highest BBD p99 utilization at 52.98%. The largest BBD utilization was 245.14% at 88.2 kHz/16 mono. Digital and Digital fallback each recorded zero misses. All timing rows passed finite-output, legal automation and C++ allocation checks.

The repeat used the same executable and workload, the Windows Balanced power plan and default scheduler, with no concurrent project builds or listening renders. The working tree differed from the measured revision only in documentation and workflow files. Each row below takes the worst utilization across that rate's six buffers and both layouts. The per-case percentiles and distributions for both runs are in their CSVs.

| Rate (Hz) | Backend | Worst p99, shared load | Worst p99, idle | Max utilization, idle | Misses, shared / idle |
|---:|:---|---:|---:|---:|---:|
| 44100 | BBD | 31.13% | 24.32% | 29.16% | 1 / 0 |
| 48000 | BBD | 39.69% | 22.93% | 47.02% | 1 / 0 |
| 88200 | BBD | 52.98% | 34.66% | 131.02% | 3 / 1 |
| 96000 | Digital fallback | 6.50% | 4.95% | 19.86% | 0 / 0 |

The idle repeat met the 50% p99 objective across all admitted cases. Its single miss was 1,901.4 µs against a 1,451.2 µs deadline at 88.2 kHz/128 mono, so it fell short of the zero-miss objective. Direct Digital recorded zero misses in both runs. In both runs the median adjacent-clock estimate was 0 ns or 100 ns, consistent with the timer resolution.

### Floating-point contraction experiment

On the same workstation, two local Release builds added `-march=znver3` and differed only in `-ffp-contract=off` versus `fast`. Disassembly found 253 fused arithmetic instructions in the fast DSP archive and zero in the off archive. The run order was off, fast, fast, off with 2,000 callbacks per case. For each matrix case, p99 utilization was averaged across its two repetitions before comparing the flags. The median fast/off ratio was 1.028 for BBD (range 0.661–1.575) and 1.007 for direct Digital (range 0.728–1.524). The off runs observed 0 and 2 BBD misses, and the fast runs observed 15 and 0. Variation was larger than the median difference, so this workstation experiment established no repeatable timing advantage from contraction. The checked-in build retains `-ffp-contract=off` to preserve the frozen numerical regressions.

### Linux CI timing

The GitHub Actions Linux runner used GNU 15.2.0 on an AMD EPYC 9V74 with four logical CPUs available. Across the same 96 cases it recorded zero misses, with a worst BBD p99 utilization of 19.75% at 88.2 kHz/16 stereo and a maximum utilization of 27.27% across admitted cases. The runner and full CSVs are identified by [the CI artifact](https://github.com/ovelhaaa/DriftBrigade/actions/runs/43300196909/artifacts/13981588219).

## Candidate support and remaining evidence

| Sample rate | DSP result | Status |
|---|---|---|
| 44.1, 48, 88.2 kHz | Balanced1024 admitted in the declared scene | Candidate for the [manual DAW procedure](m3_1d_daw_test_guide.md) |
| 96 kHz | Digital fallback | Fallback coverage only |
| Other rates | Admission rejects Balanced1024. Digital fallback requires a valid rate in the engine's Digital domain | Admission unit coverage, outside the timing matrix |

The DSP benchmarks cover one admitted scene. They do not stress all legal BBD operating points near the physical clock ceiling. The REAPER results cover stereo and one workstation. Other DAWs, mono in a DAW, realtime rates and buffers beyond the reported cases, and multi-instance sessions remain open. Dedicated audio-machine timing still needs to reproduce the p99 and zero-miss objectives.
