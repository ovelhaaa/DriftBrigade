# M2.4 BBD nonlinear transfer (internal qualification)

## Source ledger and model boundary

The supplied Raffel/Smith, *Practical Modeling of Bucket-Brigade Device Circuits* (DAFx 2010), PDF pp.5–7, sections 4.4–4.5, was reread before implementation; p.6 was also rendered to verify the equation signs. Holters/Parker, *A Combined Model for the Bucket Brigade Device and its Input and Output Filters* (DAFx 2018), was reread for asynchronous sample/hold placement. The latter supplies the linear resampling framework, not a calibrated nonlinear law.

- **PAPER-BACKED:** distortion belongs to BBD charge transfer, after input filtering and before output reconstruction. The papers do not uniquely locate a lumped nonlinear element within the physical stage chain. Raffel/Smith measures harmonics after reconstruction because the raw sampled output is unreliable for this purpose; it treats the low-amplitude transistor filters as linear.
- **PAPER-BACKED:** p.5 estimates `THD = 1.01^(N/1024)-1`, approximately one percent per 1024 stages. This broad estimate is not a per-stage transfer equation, voltage calibration, or amplitude-specific coefficient law. Section 3.1 says device imperfections increase with stages and vary with clock; it does not isolate a nonlinear clock law.
- **PAPER-BACKED:** measured distortion changes relatively little with input amplitude; it is not described as clipping. Figures 9–10 show H2 and H3 with much lower H4+. The example is asymmetric. The paper does not establish a universal measured DC bias.
- **PAPER-BACKED:** p.6 proposes `x-a*x²-b*x³+a` inside `(-1,1)` with `a=1/8,b=1/18`; printed outer constants are `1-a-b` and `-1-a+b`. Its example coefficients attempt to match high-amplitude harmonic spectra and underestimate low-amplitude harmonics. The printed endpoint constants do not equal the inner endpoint limits when `a!=0`, despite the text's smooth-transition description. We do not carry that discontinuity into runtime. Transfer CSV retains the printed outer branches with inclusive inner endpoints as an explicitly offline reference.
- **FIT FROM PAPER DATA:** none. No digitized measured points or least-squares fit is claimed. The published illustrative coefficients are transcribed, not newly fitted. Maximum measured fit error is unavailable. A fit tool would imply data precision that is absent here.
- **ENGINEERING APPROXIMATION:** aggregate output-event placement before existing linear loss/mismatch and output noise. Constant-clock memoryless distortion commutes with ideal delay; changing clock does not make this a distributed charge-transfer simulation. Input-referred noise passes through the element; output noise does not. Processing once per output update (`f_BBD`) avoids double evaluation at half edges.
- **ENGINEERING APPROXIMATION:** no stage or clock scaling of coefficients. The broad THD estimate cannot justify iterating or scaling this cubic per physical stage. Stage dependence is neutral at every count. Insertion gain remains the separate M2.2 loss block; no hidden gain normalization is fitted into distortion.

## Runtime transfer and domain

`BBDNonlinearTransfer` composes with every existing `BBDCharacterMode` through `BBDCharacterConfig::nonlinear`; no new linear-mode combinations or plugin parameters. Disabled is the default and strength defaults to zero. Strength `s` is finite and bounded to `[0,1]` during configuration.

For `|x|<=1`, `T(x)=x-s*(x²/8+x³/18)` and `T'(x)=1-s*(x/4+x²/6)`. The +1/8 paper offset is deliberately removed: `T(0)=0`, small-signal slope is one. This is an engineering variant, not the exact paper fit. At strength one, derivative on the nominal domain is at least 7/12; the model is strictly monotonic. It is memoryless, deterministic, allocation-free and contains no hysteresis.

Digital peak amplitude one denotes our normalized nominal BBD input range, **not volts**, supply voltage, or an identified device's headroom. Spectral levels use `20*log10(peak)` relative to one; +3/+6 dB test extrapolation. No physical input/output gain calibration is inferred.

For `|x|>1`, let `z=sign(x)` and `t=|x|-1`. Use `T(z)+sign(x)*T'(z)*t/(1+t)`. This engineering continuation is C1 at both boundaries, monotonic, finite and bounded; it is not hard clipping. It flattens outside the nominal range and can bound qualification feedback. That behavior is an explicit safety extension, **not a measured self-limiting BBD property or feedback limiter**. Nominal cubic conclusions do not extend to overload. Enabled nonfinite input returns zero. Core input sanitization remains before bucket/filter state, including disabled mode.

## Spectral expectations and measurement

For an isolated sine of peak A<=1 at strength one: fundamental amplitude `A-A³/24`, H2 `A²/16`, H3 `A³/72`, H4/H5 zero, DC `-A²/16`. The sign and DC reflect the retained quadratic asymmetry, not a measured device bias. THD from H2–H5 is their root-sum-square divided by fundamental. Full-path THD is a finite-bin diagnostic; aliased harmonics and images can overlap bins and cannot be added as separate harmonic powers.

The generator exports all requested level, frequency, stage, clock, alias, transfer, feedback and performance CSVs plus README. Spectral sweeps use a 96kHz measurement host and coherent one-second windows after delay plus 100ms settling. This host rate resolves BBD images; it is not an oversampling processor inserted into production. Alias probes use coherent 250ms windows at 8kHz BBD clock. Paths are isolated element, transport/hold, and full Table1 async filtering. Noise is normally off; additional noisy full-path level rows are labeled.

Alias CSV distinguishes analytic generation (after predicted input-filter attenuation), folded frequency, lower/upper images, measured composite bins, hold sinc and output-filter response. Input-filter impulse invariance can differ slightly from its analog response at finite host rate. Coincident products, including folded DC, must be treated as composite measurements. The input anti-alias filter cannot eliminate harmonics generated downstream; the paper's p.7 optimistic aliasing statement is not a guarantee. No extra filter or nonlinear oversampling is inserted.

Weak-signal corrections vanish quadratically/cubically; gain tends to one, DC to zero and THD to zero. This intentionally shares the paper cubic's low-level limitation. A future compander must not be used to describe these coefficients as calibrated device distortion.

## Feedback and numerical acceptance

Feedback probes use 256 physical stages,16kHz clock,48kHz host,transport/hold and full Table1; gains 0,.5,.8,.9,.95, a captured positive impulse and 500Hz bursts of peak .1/.5, with nonlinear-only, nonlinear+noise and nonlinear+loss+noise. They export 10ms block peak,RMS,DC,H1–H5 and residual energy for two seconds,plus inter-block RMS ratio and decay dB/second (blank when current or previous RMS is zero). Residual energy includes transient/limit-cycle content and is not a calibrated noise floor. Tail and block trends support decay/cycle inspection, not global stability proofs. Positive impulses are aligned to a capture event in TransportOnly.

At strength one the negative-side gain can exceed one: `T(x)/x=1-x/8-x²/18`. This can sustain feedback below unity; no undocumented limiter is added. Noise and loss fixtures are the existing explicitly synthetic M2.2 values.

Always-run numerical tests check analytical harmonics/DC, monotonicity, finite-difference derivatives, ascending/descending/shuffled transfer identity, weak signal, zero-strength exact bypass for both filters/transport and all four character modes at constant/modulated clocks, allocation counts, finite extremes and output-event evaluation counts. Stress spans 256–4096 stages,1Hz through the maximum 128 physical events per host sample,NaN,Inf,float max and denormal-scale inputs. A separate frozen M2.3 core/character checks exact bypass; the existing M2.3 frozen M2.2 differential oracle and all qualifications remain intact.

Local ASan/UBSan compilation was attempted; this workspace's MinGW Clang lacks its ASan runtime libraries, so sanitizer execution is **unverified locally**. The existing Linux sanitizer CI compiles and executes the new always-run tests. Milestone sanitizer acceptance remains pending that CI result.

## Performance, production and deferred work

Timings are informational 48kHz stereo and eight independent cores,512–4096 stages,3/10/30ms; median of five 250ms trials. Evaluation/event rates are per audio second. Eight-core factor uses stereo linear baseline. NonlinearOnly omits linear loss/noise, so its comparison with FullLinearCharacter is not an isolated incremental overhead measure; FullCharacter versus FullLinearCharacter is. Hosted runner wall time never gates CI. See measured results below; hardware/load-dependent timing is not a universal guarantee.

Production remains `DigitalFractionalDelay`. No compressor,expander,detector,clock feedthrough,final gain staging,feedback limiter,UI,preset,stage selection or subjective tuning was introduced. Recommended M2.5: implement and qualify compander dynamics separately; retain zero default nonlinear strength until physical amplitude/gain calibration and device-specific spectra are available.

## Local measured results (Windows Release)

All 12 DSP CTests passed, including unchanged M2.1/M2.2/M2.3 qualifications. GCC Release and Clang Debug nonlinear tests passed. Windows VST3/Standalone rebuilt successfully; plugin state/routing passed. Frozen M2.3 comparisons are bit-identical for output,held value,scheduler phase and every logical bucket,with no callback allocations. Full-path weak-signal comparisons against the linear reference also pass. Long DC and maximum-event-rate stress passed. M2.3 retained numerical artifact reports filter max absolute error `4.44e-16`,core error `1.74e-14`.

At strength one,isolated 500Hz results:

| Peak level | THD H2–H5 | Fundamental gain |
|---|---:|---:|
| -90 dB | 0.0001976% | 0.99999999996 |
| -72 dB | 0.001570% | 0.99999999737 |
| -60 dB | 0.006250% | 0.99999995834 |
| -24 dB | 0.394453% | 0.9998341220 |
| -12 dB | 1.576518% | 0.9973710111 |
| -6 dB | 3.185124% | 0.9895338065 |
| 0 dB | 6.680829% | 0.9583333333 |

Nominal H2 peak is .0625,H3 .0138889; H4/H5 are numerical residuals inside the cubic domain and become nonzero in overload continuation. Maximum isolated analytical spectral absolute error over the exported nominal level/frequency sweeps was `1.823e-12` (acceptance `1e-10`). This is numerical reference error,**not measured-device fit error**. Against the inner published offset polynomial,the engineering variant differs by exactly 1/8 throughout the nominal domain; it is not an exact paper implementation.

At f_BBD=8kHz,f_input=.30*f_BBD=2400Hz,peak one,H3 generates at7200Hz and folds to800Hz. Transport/hold measured folded amplitude .0136631,lower image7200Hz .00153209,upper image8800Hz .00125929; predicted intrinsic H3 .0138889 and hold sinc .983632. With Table1 filters,the folded composite is .00958391; analytic input-attenuated H3 .0106847,output magnitude at800Hz .911887. Other ratios and amplitudes are in the alias CSV and include coincident-product warnings.

Feedback transport/hold at .95 after the .5 burst sustained a negative-side cycle through the two-second probe: terminal block peak1.21153,DC-.0757249 in nonlinear-only. Noise/loss cases also sustain this transport cycle. Gains through .9 decayed in this fixture. The full Table1 noise-free probes all decayed (largest terminal peak `1.93e-19`); noise-enabled terminal peaks were around `1.69e-4`. Every state stayed finite. This demonstrates strong placement/filter dependence and does not certify all feedback signals as stable.

One local timing run with the frozen M2.3 baseline measured median FullCharacter incremental overhead1.15% stereo and .76% for eight cores; individual cases ranged -2.25% to6.11% stereo and -.90% to6.32% eight cores (wall-time jitter). FullCharacter realtime factors were .00773–.05883 stereo and .03131–.22765 for eight independent cores. Repeat runs naturally differ; CSVs carry the exact latest run. The largest 4096-stage/3ms stereo case executes approximately1.365million nonlinear evaluations and2.731million physical edges per audio second. No wall-time acceptance threshold is imposed.

The generated artifact is `DriftBrigade-M2.4-BBD-Nonlinear-Qualification`. All numerical and production-routing checks passed locally; sanitizer acceptance remains pending Linux CI because the local ASan runtime cannot link. M2.4 is not claimed fully accepted until sanitizer CI passes.
