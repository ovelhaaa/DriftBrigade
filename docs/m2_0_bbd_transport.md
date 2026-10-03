# M2.0 clocked BBD transport (historical milestone)

M2.0 established an internal, preallocated O(1) transport, nominal
`D=N/(2*f_BBD)` timing, clock-phase accumulation, delay limits, telemetry,
clock-change history preservation, block invariance, allocation tests and
fail-safe artifact handling. Production continued to use DigitalFractionalDelay.

The original M2.0 implementation stored N values and inserted a new signal
sample at each half-clock transfer edge. Its abstract signal rate was 2*f_BBD.
That abstraction reproduced nominal delay timing but **did not reproduce physical
BBD signal sampling**. It must not be used to infer the signal Nyquist or filter
requirements of a BBD.

M2.1 supersedes that representation: physical transfer edges run at 2*f_BBD,
but captures occur only on alternating edges at f_BBD, with N/2 occupied signal
buckets. Output updates on the opposite alternating phase and holds for one
full BBD period. Signal Nyquist is f_BBD/2. Holters/Parker Eq.1 distinguishes the
N-1-edge pulse onset from the N-edge nominal centre delay. For exact conventions,
equivalence, analytical filters, measurements and current tests, see
[m2_1_bbd_linear_resampling.md](m2_1_bbd_linear_resampling.md).

The M2.0 historical artifact consisted of constant-clock CPU/event metrics,
variable-clock sine telemetry, host scheduler timing, a stage-memory snapshot
and README. Its ambiguous event-rate column has been replaced in M2.1 by
`per_channel_transfer_edge_rate_hz`, alongside separately labeled stereo
measured throughput. The current M2.1 artifact includes corrected phase,
hold, asynchronous response, alias/image and historical marker qualifications.

M2.0 made no subjective sound selection or final anti-alias/reconstruction claim.
M2.1 likewise remains internal/headless and adds only linear filtering and
resampling. Nonlinear coloration, noise, companding, clock bleed, production BBD
routing, controls, UI, presets and all M1.1 listening decisions remain deferred.
