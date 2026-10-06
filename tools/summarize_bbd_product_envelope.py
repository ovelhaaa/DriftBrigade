"""Finalize inspectable policy from generated evidence; never a timing CI gate."""
import csv
import json
import math
import pathlib
import sys

root = pathlib.Path(sys.argv[1])


def rows(name):
    with (root / name).open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def write(name, data):
    with (root / name).open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(data[0]))
        writer.writeheader()
        writer.writerows(data)


# Independent operating measurements may run concurrently in separate folders;
# no callback timing is performed in those passes. Timed passes stay serial.
measurement_root = root / "measurements"
if measurement_root.exists():
    for filename in ("bbd_product_stage_tradeoff.csv", "bbd_product_startup_headroom.csv",
                     "bbd_product_gain_profile.csv", "bbd_product_feedback_dc.csv",
                     "bbd_product_quality_cpu_frontier.csv", "bbd_product_feedback_dc_experiment.csv"):
        combined = []
        for candidate in ("Economy", "Balanced", "Extended", "LargeStageResearch"):
            with (measurement_root / candidate / filename).open(newline="", encoding="utf-8") as stream:
                combined.extend(csv.DictReader(stream))
        write(filename, combined)

timing = rows("bbd_product_repeated_timing.csv")
floors = rows("bbd_product_delay_floor.csv")
startup = rows("bbd_product_startup_headroom.csv")
feedback = rows("bbd_product_feedback_dc.csv")
quality = rows("bbd_product_quality_cpu_frontier.csv")
modulation_name = "bbd_product_production_scheduler_counters_modulation.csv"
modulation = rows(modulation_name) if (root / modulation_name).exists() else []
profiles = rows("bbd_product_candidate_profiles.csv")
rates = {44100, 48000, 88200, 96000, 176400, 192000}
assert {int(float(r["sample_rate"])) for r in floors} == rates
assert {int(p["stages"]) for p in profiles} == {512, 1024, 2048, 4096}
assert len(quality) == 24
assert len(startup) == 648
assert len(feedback) == 960
for r in startup + feedback:
    assert r["finite"] == "1" and r["clamps"] == "0" and r["allocations"] == "0"
for r in quality:
    assert all(math.isfinite(float(r[key])) for key in ("gain500_db", "snr_db", "thd_h2_h5", "startup_peak", "feedback_return_dc"))
for r in floors:
    sr, stages, clock = (float(r[k]) for k in ("sample_rate", "stages", "product_max_clock_hz"))
    assert math.isclose(float(r["product_min_delay_seconds"]), stages/(2*clock), rel_tol=1e-12)
    assert math.isclose(float(r["max_edges_per_sample"]), 2*clock/sr, rel_tol=1e-12)
    assert clock <= 64*sr
for r in rows("bbd_product_event_budget.csv"):
    assert abs(float(r["actual_edges_per_sample"])-float(r["target_edges_per_sample"])) < .01

support, recommendation = [], []
for floor in floors:
    name, sr = floor["candidate"], floor["sample_rate"]
    selected = next(r for r in timing if r["candidate"] == name and r["sample_rate"] == sr and r["block"] == "64")
    mod_options = [r for r in modulation if r["candidate"] == name and r["sample_rate"] == sr]
    acceptable = [r for r in mod_options if float(r["worst_p99_utilization"]) <= .7 and int(r["runs_with_misses"]) < 2]
    mod = min(acceptable, key=lambda r: int(r["block"])) if acceptable else min(mod_options, key=lambda r: float(r["worst_p99_utilization"]), default=None)
    static_util = float(selected["worst_p99_utilization"])
    mod_util = float(mod["worst_p99_utilization"]) if mod else None
    long_run = int(selected["runs"]) >= 5 and int(selected["callbacks_per_run"]) >= 10000
    static_median_util = float(selected["median_p99"]) / (64/float(sr))
    if (mod_util is not None and mod_util > .7) or (mod_util is None and static_median_util > .7):
        classification, reason = "UNSUPPORTED_FOR_BBD", "p99_exceeds_engineering_70_percent_budget_on_this_machine"
    elif not long_run or mod is None or float(sr) > 96000:
        classification, reason = "EXPERIMENTAL", "no_complete_repeated_production_modulation_evidence_or_high_rate_policy"
    elif int(mod["runs_with_misses"]) >= 2:
        classification, reason = "EXPERIMENTAL", "repeated_runs_with_misses_require_scheduler_vs_load_review"
    else:
        classification, reason = "SUPPORTED_WITH_REDUCED_BBD_RANGE", "repeated_production_modulated_budget_pass_with_static_floor_and_tail_evidence"
    support.append(dict(candidate=name, sample_rate=sr, classification=classification,
                        static_worst_p99_utilization=static_util,
                        static_median_p99_utilization=static_median_util,
                        production_modulated_worst_p99_utilization=mod_util,
                        minimum_qualified_block=int(mod["block"]) if mod else 64, reason=reason))
    recommendation.append(dict(candidate=name, sample_rate=sr,
                               status="TIMING_ELIGIBLE_PRODUCTIZATION_CANDIDATE" if classification == "SUPPORTED_WITH_REDUCED_BBD_RANGE" else "REVIEW_REQUIRED",
                               max_clock_hz=floor["product_max_clock_hz"],
                               min_delay_seconds=floor["product_min_delay_seconds"],
                               reason=reason, shipping_default="NO"))
write("bbd_product_sample_rate_support.csv", support)
write("bbd_product_recommendation.csv", recommendation)

# Clock ceilings are admitted hypotheses only where actual repeated evidence passes.
ceilings = rows("bbd_product_clock_ceiling.csv")
for r in ceilings:
    s = next(s for s in support if s["candidate"] == r["candidate"] and s["sample_rate"] == r["sample_rate"])
    r["status"] = "QUALIFIED_CANDIDATE_CAP" if s["classification"] == "SUPPORTED_WITH_REDUCED_BBD_RANGE" else "NOT_QUALIFIED_FOR_PRODUCT_SUPPORT"
write("bbd_product_clock_ceiling.csv", ceilings)

for r in quality:
    t = next(t for t in timing if t["candidate"] == r["candidate"] and t["sample_rate"] == r["sample_rate"] and t["block"] == "64")
    r["repeated_static_worst_p99_utilization_64"] = t["worst_p99_utilization"]
    representative = next(t for t in timing if t["candidate"] == r["candidate"] and t["sample_rate"] == r["sample_rate"] and t["block"] == "128")
    r["p99_utilization_128"] = representative["worst_p99_utilization"]
    s = next(s for s in support if s["candidate"] == r["candidate"] and s["sample_rate"] == r["sample_rate"])
    r["support_classification"] = s["classification"]
write("bbd_product_quality_cpu_frontier.csv", quality)

eligible = [r for r in support if r["candidate"] == "Balanced" and r["classification"] == "SUPPORTED_WITH_REDUCED_BBD_RANGE"]
summary = dict(
    candidate="Balanced" if eligible else "NO_SUPPORTED_BALANCED_RECOMMENDATION",
    eligible_sample_rates=[float(r["sample_rate"]) for r in eligible],
    stage_count=1024, minimum_callback_frames=max((r["minimum_qualified_block"] for r in eligible), default=None),
    gain_profile="Nominal_for_listening_pending_startup_and_noise_review",
    startup_max_nominal_peak=max(float(r["post_mix_peak"]) for r in startup if r["gain_profile"] == "Nominal"),
    startup_max_nominal_internal_peak=max(float(r["per_voice_internal_peak"]) for r in startup if r["gain_profile"] == "Nominal"),
    feedback_max_abs_return_dc=max(abs(float(r["loop_return_dc"])) for r in feedback),
    feedback_max_abs_output_dc=max(abs(float(r["output_dc"])) for r in feedback),
    feedback_worst_late_decay_rms=max(float(r["late_decay_rms"]) for r in feedback),
    model_ceiling_edges_per_sample=128,
    product_cap_is_candidate_specific=True,
    final_shipping_default=False,
    blockers=["repeat timing on additional machines", "listening and SNR/THD comparison",
              "16/32-frame host policy", "startup operating level and feedback topology decision",
              "ASan availability on local Windows toolchain"],
)
with (root / "SUMMARY.json").open("w", encoding="utf-8") as stream:
    json.dump(summary, stream, indent=2)
print(json.dumps(summary, indent=2))
