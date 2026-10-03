# M2.2: BBD device character

Implemented on merged M2.1 (`261ed6f`, merge of `4a22c4a`). Production still instantiates `DigitalFractionalDelay`; no plugin parameters, routing, UI, presets or M1.1 choices change. Table 1 and all M2.1 modes remain intact. Acceptance uses numerical qualification only.

## Source evidence and boundaries

**PAPER-BACKED:** Raffel/Smith, *Practical Modeling of Bucket-Brigade Device Circuits*, supplied `bbd modeling (1).pdf`, PDF p.4 section 3.1 and p.5 sections 4.2–4.3 describe variable insertion gain, stage/clock-dependent degradation, insertion filtering, and noise immediately before/after the delay. Noise can seed feedback. Approximately 60 dB SNR is contextual, near maximum input, without a digital full-scale conversion or device calibration.

**MEASURED / FIT FROM PAPER / FIT FROM PAPER DATA:** Holters/Parker, supplied `BBD filters model (1).pdf`, PDF p.6, reports approximately +2.3 dB measured Juno-60 BBD gain. Set `insertionDb=2.3` in `LossOnly` to exercise this optional constant observation. It is tested, but is neither the default nor a universal clock/stage law. No curve was digitized or fit.

**PAPER-BACKED:** Holters/Parker p.5 Table 1 describes the surrounding fifth-order low-pass filters. p.6 equations 34–38 explicitly derive the rectangular hold sinc. M2.1 already supplies both filters, variable-clock capture/transport, signal Nyquist and this hold.

Raffel/Smith p.5 quotes roughly 0–2 dB low-frequency gain and -4 to -6 dB near Nyquist, but does not de-embed the rectangular hold. Its sampling loss alone is -3.92 dB at Nyquist. These ranges cannot justify fitting another full sinc or another -4 to -6 dB device filter. No source supplies a residual HF curve, noise coloration, quantitative stage-noise law, clock-noise law or phase/capacitor mismatch distribution. These uncertainties are preserved.

Clock feedthrough and alternating-phase mismatch are deferred. Existing edge/capture/output telemetry provides a clock measurement scaffold; no audio oscillator is added. Nonlinear material in Raffel/Smith sections 4.4–4.5 is deliberately outside this implementation.

## Architecture and internal configuration

`ClockedBBDCore` owns `BBDDeviceCharacter`, which composes `BBDDeviceLoss`, `BBDNoiseModel` and `BBDTransferImperfection`. Input-referred noise is added to the filtered captured sample, then travels through the bucket queue. The exited sample passes through aggregate device loss and fixed mismatch; output-referred noise is added before the existing rectangular hold and asynchronous output filter.

Set `BBDCharacterConfig` before `prepare`; configuration changes require reprepare. `Ideal`, `LossOnly`, `NoiseOnly`, and `FullLinearCharacter` are internal qualification modes. Every strength defaults to zero, insertion gain defaults to unity, and the mode defaults to Ideal. Individual loss, pole, input/output noise and mismatch controls can independently remain zero. Both existing circuit-filter profiles and TransportOnly can be used with this compact configuration. There are no new product-facing modes.

No allocation, lock, exception, `std::random`, or per-stage loop occurs in processing. Storage allocates only during prepare. The loss memory clears values below 1e-280; ideal bypass never changes M2.1 filter arithmetic. Existing analogue filters can decay to bounded subnormals; M2.2 does not change their tail behavior. Numerical safety covers maximum float, minimum/maximum clock bounds, invalid configuration, and finite character state.

## Transfer derivation

**ENGINEERING APPROXIMATION:** For physical stage count N and instantaneous clock c:

```
g = 10^(insertionDb/20) * exp(-N*(lossPerStage + leakagePerStageSecond/(2*c)))
a = exp(-1024/(residualPolePer1024*N))  [a=0 for zero strength]
m[k] = (1-a)*exited[k] + a*m[k-1]
Hdevice(f) = g*(1-a)/(1-a*exp(-j*2*pi*f/c))
```

The first loss term represents a hypothetical independent per-stage attenuation; the second represents a hypothetical residence-time charge leakage. The pole models only an independently controlled residual transfer response, at BBD output update rate. Its physical cutoff scales with clock, and strength scales with stages. Its value is not inferred from the quoted Nyquist response. A zero pole returns the input directly, and unity gain performs no multiply.

This aggregate applies at exit using the current clock. It does not integrate every bucket's historical clock trajectory or simulate charge loss at each stage. Modulated-clock loss is a continuous instantaneous equivalent, not a calibrated residence-history model. All coefficients change smoothly with clock and no delay, pole, filter, RNG or mismatch state resets during modulation.

At constant clock, neglecting folded images:

```
Htotal(f) = Hin(f)
          * exp(-j*2*pi*f*N/(2*c)) * sinc(f/c)
          * Hdevice(f) * fixedMismatchGain
          * Hout(f)
```

The core implements the hold by event timing. The character code contains no sinc. The single analytical sinc exists only in measurement prediction. Neither surrounding circuit filter is replaced or augmented. The artifact separates input filter, hold, device, output filter, analytical product and measured combined response. Combined-tone measurement uses 384 kHz host rate and coherent 100 ms observations after transport plus 50 ms settling. Tolerance is 0.002 absolute magnitude to allow residual aliasing. Device-only measurement checks its discrete response to 1e-9.

## Noise, stage dependence and fixed mismatch

**ENGINEERING APPROXIMATION:** Separate xorshift32 streams supply bounded uniform, zero-centered source samples normalized to configured RMS. Input noise advances only on capture; output noise advances only on output updates. Zero RMS returns exactly zero without advancing RNG. Output noise therefore receives hold and reconstruction-filter shaping; it is not white host-rate noise added after the filters. No unexplained extra noise-color filter is present.

A hypothetical independent-stage accumulation scales source RMS by `sqrt(N/1024)`. No extra delay/noise mapping or clock-dependent variance is asserted. Clock dependence in the observed spectrum comes from source update rate, hold and reconstruction filter. This separates clock effects from stage effects in the artifact. Input/output noise sources are alternatives to qualify, not a measured partition of real device noise; per-transfer stage noise is deferred.

A seed of zero maps to one. Input stream uses the seed, output uses seed XOR 0x9e3779b9; mismatch uses seed XOR 0x85ebca6b (each zero result maps to one). Stereo callers can supply different seeds. Reset and prepare reproduce output; reset preserves the prepared mismatch. Mismatch is a bounded uniform aggregate gain deviation drawn once in prepare, with zero default and at most 10% configured deviation. It is a sensitivity model, not a capacitor tolerance fit, phase mismatch or nonlinear transfer curve.

Noise qualification uses one second settling and 65536 samples at 48 kHz, two seeds, three noise modes, all five stage counts and clocks 8/16/32 kHz. Artifact RMS includes DC; band RMS excludes DC and Nyquist. Broad bands are 20–200, 200–2000, 2000–8000 and 8000–24000 Hz. The expected one-sided analogue PSD is:

```
S(f) = 2*sourceRms^2/c * sinc(f/c)^2 * |Hout(f)|^2
```

Expected sampled bands sum host aliases through +/-4 host rates, then integrate at 1 Hz spacing. The squared sinc here describes the existing noise hold, not additional filtering. Finite rectangular-window FFT statistics are compared within 25%, avoiding FFT-bin overfitting. Identical seeds are sample-identical; different seeds must differ but agree in energy within 10% in the unit probe. Fixed-clock stage scaling and input noise transit are tested separately.

## Qualification and artifacts

Build `drift_bbd_character_tests` and `drift_bbd_character_qualification`, then run:

```
ctest --test-dir build --output-on-failure
./build/drift_bbd_character_qualification artifact
```

The artifact is `DriftBrigade-M2.2-BBD-Character-Qualification`, with all seven requested CSVs and README.txt, plus bbd_cpu.csv. All streams are opened before measurements, every flush and close is checked, and failures return nonzero. The existing fail-safe CMake harness now accepts a filename so both M2.1 and M2.2 test blocked directories/streams and Linux /dev/full finalization. Tone, band-spectrum and feedback bounds also fail the generator on qualification errors. No listening is needed.

The nonzero sensitivity fixture is explicitly synthetic: lossPerStage=1e-5, leakagePerStageSecond=0.1, residualPolePer1024=0.25, outputNoiseRms=1e-4, mismatchFraction=0.001. These are test stimulus strengths, never vintage constants or production defaults. Measurements below apply to this fixture only.

Feedback uses Table 1, 1024 stages, 16 kHz clock, gains 0/0.5/0.9, with and without noise, and a 10 ms, 0.1-amplitude input pulse followed by five seconds of observation. CSV exports one-second RMS, peak, DC, RMS change in dB/second and peak-growth ratio. Decay columns include the noise floor and first-window stimulus; they are not a fitted physical decay constant. The qualification is for these profiles only: a positive insertion gain or resonant response can make even a nominal gain below one unstable. Production feedback is untouched.

CPU measures one second of 48 kHz stereo at 10 ms delay: per-channel physical edges are 102400/s (1024 stages) and 409600/s (4096 stages). Timings include host/event processing, and relative CPU is versus TransportOnly at the same stage count. Events/s combines stereo measured events divided by wall time. Exact exponential calls are counted analytically: two five-pole filter advances per host segment/event. Independent varying-argument analogue-advance and exp-only microbenchmarks prevent constant hoisting. They are cost proxies, not exclusive profiler attribution, and can exceed the actual loop due to different arguments/overhead. Constant-clock CPU does not include per-host `setDelaySeconds` coefficient recomputation. No LUT or exponential approximation is introduced.

CI preserves M1/M1.1, bake-off, M2.1, sanitizer and plugin jobs, adds the M2.2 generator/upload job gated on DSP and sanitizer success, and renames only the workflow and Windows artifact. Binary and product IDs do not change.

## Limitations and M2.3 recommendations

No device-specific residual frequency response or noise magnitude has been calibrated. Stage/clock loss, residual bandwidth, stage-noise accumulation and fixed mismatch remain opt-in engineering hypotheses. Audible feedthrough, phase mismatch, stochastic individual capacitor tolerances and historical leakage need independent measurements before stronger physical claims.

For M2.3, measure a specific device at several clocks and stage counts, de-embed circuit filters and hold, and then fit only residual loss/noise. Establish voltage-to-digital normalization and confidence intervals. Qualify nonlinear transfer and any compander as separate mechanisms with amplitude/frequency sweeps, including closed-loop stability across total path gain. Retain independent clock feedthrough control and headless acceptance. Profile asynchronous filters on intended target hardware before deciding whether exact exponentials need optimization. Production routing, stage-count product selection, feedback coloration, UI, presets and listening decisions remain deferred.

## Local results (2026-10-03)

Windows MinGW GCC Release, 48 kHz host unless noted. These values characterize the synthetic fixture, not a real chip.

- Independent comparison compiled the merged M2.1 source alongside M2.2 and found **900000 bit-identical output samples** across 256/1024/4096 stages, TransportOnly/prototype/Table 1, and continuously changing clocks. The permanent test also checks neutral FullLinearCharacter against Ideal.
- Final full CTest run: **9/9 passed** (50.14 seconds), including existing DSP/transport/filter/hold/artifact/plugin tests, character unit tests and the positive measurement generator. VST3 and Standalone rebuilt successfully. Process/sweep allocation counts were unchanged; maximum-float, invalid-config and minimum/maximum-clock probes stayed finite. The new positive measurement CTest runs the same generator under sanitizer CI as well as release CI.
- Combined frequency magnitude maximum absolute error: **1.21134e-6**, with input/filter, single hold sinc, device and output/filter separated. At fixed 16 kHz clock, DC device gain is -0.02918 dB (256 stages) and -0.46695 dB (4096). At 0.40*clock it is -0.02919 and -6.83286 dB respectively; this deliberately exercises the residual pole and is not fitted to paper data.
- At 1024 stages, 8/16/32 kHz clocks, DC gain is 0.983498/0.986650/0.988230. The coefficient sweep is continuous and preserves event counters/history and seeded noise. These dependencies arise from the documented approximation.
- NoiseOnly, seed 1, 1024 stages, 16 kHz clock: RMS **7.86715e-5** (-82.0837 dB relative to unity amplitude), peak 1.73024e-4, DC 7.15920e-7, crest 2.1993. Band RMS: 1.38896e-5 / 4.22130e-5 / 6.15940e-5 / 1.99778e-5 for the four declared bands. Largest expected-versus-measured band error over the entire matrix is **6.3762%**.
- At fixed 16 kHz, output noise RMS rises 3.93357e-5 to 1.57343e-4 from 256 to 4096 stages, exactly 4x for the same output-source seed; this is the square-root stage assumption. At fixed 1024 stages it falls 8.54293e-5 to 6.27388e-5 from 8 to 32 kHz because of hold/filter shaping, without changing source variance.
- Feedback stays bounded at 0/0.5/0.9. With no noise and gain 0.9, one-second RMS reaches 1.27550e-15 in the final window, with approximately -65 dB/s in the late tail. With noise, final RMS is 1.05840e-4 and peak 3.58522e-4; DC is -3.70007e-7. Noise seeds a stable floor, not growth. No physical feedback calibration is claimed.

Median of five one-second stereo timing trials, measured by the artifact generator:

| Stages | Profile | Physical edges/s/channel | Host seconds | Relative to transport | Stereo events/wall second |
|---:|---|---:|---:|---:|---:|
| 1024 | TransportOnly | 102400 | 0.002652 | 1.00 | 77232719 |
| 1024 | M2.1 Table1 | 102400 | 0.237449 | 89.55 | 862493 |
| 1024 | M2.2 LossOnly | 102400 | 0.220735 | 83.24 | 927801 |
| 1024 | M2.2 FullLinearCharacter | 102400 | 0.233384 | 88.01 | 877514 |
| 4096 | TransportOnly | 409600 | 0.002852 | 1.00 | 287206114 |
| 4096 | M2.1 Table1 | 409600 | 0.646520 | 226.67 | 1267089 |
| 4096 | M2.2 LossOnly | 409600 | 0.669507 | 234.73 | 1223583 |
| 4096 | M2.2 FullLinearCharacter | 409600 | 0.670901 | 235.21 | 1221042 |

Full character/M2.1 timing ratios are about 0.98x (1024) and 1.04x (4096); the small 1024 difference is timer/system variation, not a speedup claim. Exact complex exponentials remain the obvious dominant cost: 3.00798 million/9.15198 million estimated calls per stereo second, with exp-only proxies about 0.186/0.564 seconds versus M2.1 host times 0.237/0.647 seconds. Approximately 78%/87% comparable cost is indicative, not exclusive attribution. Analog-advance proxies include arithmetic and differ in interval distribution; consult bbd_cpu.csv rather than subtracting them from total time. More target-hardware profiling is appropriate before optimizing.

**Sanitizer status: locally unverified.** The installed MinGW Clang 19 linker cannot find its AddressSanitizer import/runtime libraries. Linux ASan/UBSan CI remains enabled, includes the positive measurement test, and must pass before sanitizer-clean acceptance. Remote CI/upload has been configured but has not been run from this local task. Local fail-safe checks passed; /dev/full finalization is exercised on Linux CI.

Local artifact directory: `output/DriftBrigade-M2.2-BBD-Character-Qualification`. CI upload name is identical. No listening decision, compander, nonlinear saturation, clock-whine source, product stage choice or production BBD routing was introduced.
