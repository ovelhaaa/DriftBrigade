# Manual DAW qualification — M3.1D

## Build and identify

Use a separate build directory and install the VST3 only in the test machine's scan path.

```sh
cmake -S . -B build-host-internal -DDRIFT_INTERNAL_BBD_HOST_TEST=ON -DDRIFT_BUILD_VST3_HOST_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-host-internal --config Release --parallel 2
ctest --test-dir build-host-internal -C Release -R 'plugin_internal|vst3_loaded' --output-on-failure
```

Use the platform options from the root README. Locate `DRIFT BRIGADE BBD HOST TEST.vst3` under `build-host-internal/DriftBrigade_artefacts/Release/VST3/`. Confirm both names in the DAW after a rescan if the public build is also installed. The internal bundle ID is `audio.driftbrigade.bbdhosttest` and its JUCE plug-in code is `DbHt`. The public build retains `audio.driftbrigade.m1` and `DrBg`. Save a separate session for qualification. The DAW sees the two builds as distinct effects even though the serialized parameter schema is shared.

## Session and test steps

1. Record DAW version, OS, CPU, audio interface and driver, build revision, power settings, buffer, rate, and whether other plug-ins are running. Record xruns from the host or driver if it exposes them.
2. Use a mono track and a stereo track. Include a sustained chord, plucks or drums, bass, test noise, a near-full-scale segment and at least two seconds of silence followed by another transient. Start with Center 24 ms, Depth 50%, Motion 0.7 Hz, Chaos 55%, Coherence 45%, Dynamics 25%, Feedback 12%, Mix 50% and Width 60%. The internal editor should show **Balanced1024 admitted** at 44.1, 48 and 88.2 kHz.
3. At each admitted rate, try 16, 32, 64, 128 and 512 samples as supported by the driver. Play through the scene for at least five minutes per supported configuration, with one and then several instances. Record dropout count, audible clicks, worst reported audio load and whether load was measured per effect or for the project. Record unavailable buffer sizes as unsupported by the test environment.
4. Automate Motion, Depth, Center, Chaos, Feedback, Mix and Width individually, then simultaneously. Include Center 22–30 ms, Depth 0–85% and Feedback up to 65%. At a stable audio point, move Center below 22 ms while changing Mix. The editor should report a rejected snapshot and the audio should retain the last accepted target tuple. Return Center to range and confirm updates resume. An initial saved Center outside the envelope selects the declared initial scene for audio until the first valid tuple arrives. The same test-mode policy applies during Digital fallback.
5. Engage and disengage host bypass. Confirm that the bypass output is dry, note whether the DAW suspends the callback or processes the tail, and record clicks or discontinuities. With bypass off, test Mix 0 and allow its smoother to settle before comparing dry output.
6. Stop transport before changing rate, buffer or routing. Test 44.1 → 48 → 88.2 → 96 → 44.1 kHz. Confirm **Digital fallback** at 96 kHz and BBD admission on return. Verify mono, stereo and mono-to-stereo where the DAW exposes that route. Record any host restrictions.
7. Save, close and reopen the session with valid controls. Verify values and admission status and repeat with out-of-envelope controls. Render a fixed clip offline and capture playback from the same reset and automation timeline. Compare alignment and levels, and record any automation or bypass timing differences.
8. Repeat the session with the public VST3 as a Digital control, using the same audio, parameter values, layout, rates and buffers.

## Record

| Host / driver | Build SHA | Rate / buffer | Layout | Backend shown | Instances | Play time | XRuns / peaks | State / automation / bypass | Audio observations |
|---|---|---|---|---|---|---|---|---|---|
| | | | | | | | | | |

A passing row requires the expected backend, correct routing and state, safe handling of rejected snapshots, zero observed dropouts and no crash. Record unsupported combinations and failures explicitly. The separate dedicated callback benchmark targets p99 utilization ≤50% and zero deadline misses. Complete both types of evidence before considering a public BBD build.
