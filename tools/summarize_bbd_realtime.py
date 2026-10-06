"""Validate completed M2.8 artifacts and summarize observations, never gate CPU."""
import csv
import json
import math
import sys
from collections import Counter, defaultdict
from pathlib import Path

REQUIRED = (
    "callback_distribution", "max_clock", "deadline_margin", "long_run",
    "feedback_longrun", "startup", "hostile_input", "nonfinite_recovery",
    "sample_rate_matrix", "stage_matrix", "automation", "denormals",
    "reset_recovery", "prepare_failures",
)


def rows(root, suffix):
    with (root / f"bbd_realtime_{suffix}.csv").open(encoding="utf-8", newline="") as f:
        result = list(csv.DictReader(f))
    if not result or any(None in r or None in r.values() for r in result):
        raise ValueError(f"Incomplete or malformed {suffix} report")
    return result


def main(root):
    data = {name: rows(root, name) for name in REQUIRED}
    if not (root / "README.txt").is_file():
        raise ValueError("Missing README")
    # Source-specific exception: internal fault rows report AFTER explicit
    # documented recovery. CPU miss counts are deliberately never acceptance.
    for name, table in data.items():
        for r in table:
            for key in ("finite", "finite_after_recovery", "output_finite",
                        "reset_bit_identity", "bit_identity", "safe_reprepare_identity"):
                if key in r and r[key] != "1":
                    raise ValueError(f"{name}: {key} acceptance failed")
            for key in ("allocations", "callback_allocations", "hidden_clamps", "clamps"):
                if key in r and float(r[key]) != 0:
                    raise ValueError(f"{name}: {key} acceptance failed")
            for key, value in r.items():
                if key == "worst_margin" and value.lower() in {"inf", "infinity"}:
                    continue  # Zero-duration timer quantization: informational.
                if name == "prepare_failures" and key == "requested_value":
                    continue  # These are intentional invalid configuration labels.
                try:
                    number = float(value)
                except ValueError:
                    continue
                if not math.isfinite(number):
                    raise ValueError(f"{name}: nonfinite measurement {key}")

    callback = [r for r in data["callback_distribution"] if r["backend"] == "1"]
    representative = [r for r in callback if r["representative"] == "1"]
    margin = [r for r in data["deadline_margin"] if r["backend"] == "1"]
    worst = max(margin, key=lambda r: float(r["max_utilization"]))
    feedback = data["feedback_longrun"]
    voices = rows(root, "feedback_voices")
    groups = defaultdict(list)
    for r in voices:
        key = tuple(r[k] for k in ("noise_enabled", "nonlinearity_enabled",
                                   "feedback_requested", "stimulus", "voice"))
        groups[key].append(r)
    # Last eight windows after excitation: report trends without inventing an
    # absolute DC threshold. Gain growth during silence can be ordinary release.
    trends = []
    for key, table in sorted(groups.items()):
        tail = sorted(table, key=lambda r: float(r["time_seconds"]))[-8:]
        enough = len(tail) == 8 and float(tail[0]["time_seconds"]) > 1
        dc = [abs(float(r["window_return_dc"])) for r in tail]
        rms = [float(r["window_return_rms"]) for r in tail]
        growing_dc = enough and all(b > a for a, b in zip(dc, dc[1:]))
        growing_rms = enough and all(b > a for a, b in zip(rms, rms[1:]))
        trends.append(dict(zip(("noise_enabled", "nonlinearity_enabled",
                                "feedback_requested", "stimulus", "voice"), key),
                           seconds=float(tail[-1]["time_seconds"]),
                           last_window_dc=dc[-1], last_window_rms=rms[-1],
                           eight_late_windows=int(enough),
                           strictly_growing_abs_dc=int(growing_dc),
                           strictly_growing_rms=int(growing_rms)))
    with (root / "bbd_realtime_feedback_tail_trends.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(trends[0]))
        writer.writeheader()
        writer.writerows(trends)

    # Same noise-off fixture, nonlinear strength1 versus strength0. Positive and
    # negative bursts expose the even/asymmetric contribution before DC blocking.
    first = {key: min(table, key=lambda r: float(r["time_seconds"]))
             for key, table in groups.items()}
    asymmetry = []
    for fb in sorted({r["feedback_requested"] for r in voices}, key=float):
        for voice in range(8):
            keys = [("0", nonlinear, fb, stimulus, str(voice))
                    for nonlinear in ("1", "0") for stimulus in ("1", "2")]
            if not all(k in first for k in keys):
                continue
            nonlinear_pos, nonlinear_neg, linear_pos, linear_neg = [
                float(first[k]["window_return_dc"]) for k in keys]
            asymmetry.append(dict(feedback=float(fb), voice=voice,
                                  window_seconds=float(first[keys[0]]["time_seconds"]),
                                  nonlinear_positive_dc=nonlinear_pos,
                                  nonlinear_negative_dc=nonlinear_neg,
                                  linear_positive_dc=linear_pos,
                                  linear_negative_dc=linear_neg,
                                  nonlinear_even_component=(nonlinear_pos+nonlinear_neg)/2,
                                  linear_even_component=(linear_pos+linear_neg)/2,
                                  asymmetry_contribution=(nonlinear_pos+nonlinear_neg-linear_pos-linear_neg)/2))
    with (root / "bbd_realtime_feedback_asymmetry.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(asymmetry[0]))
        writer.writeheader()
        writer.writerows(asymmetry)

    startup = max(data["startup"], key=lambda r: float(r["post_mix_peak"]))
    denormals = data["denormals"]
    summary = dict(
        representative_callbacks=[int(r["callbacks"]) for r in representative],
        maximum_clock_hz=max(float(r["clock_hz"]) for r in data["max_clock"]),
        maximum_edges_per_sample=max(int(r["max_edges_sample"]) for r in data["max_clock"]),
        classifications=dict(Counter(r["classification"] for r in margin)),
        worst_deadline_case=worst,
        sustained_cells=len(data["long_run"]),
        sustained_seconds=sorted({float(r["duration_seconds"]) for r in data["long_run"]}),
        maximum_effective_engine_feedback=max(float(r["feedback_effective_max"]) for r in feedback if r["backend"] == "eight_voice_engine"),
        feedback_observation_seconds=sorted({float(r["seconds"]) for r in feedback}),
        feedback_monotonic_dc_flags=sum(r["strictly_growing_abs_dc"] for r in trends),
        feedback_monotonic_rms_flags=sum(r["strictly_growing_rms"] for r in trends),
        maximum_late_window_return_dc=max(r["last_window_dc"] for r in trends),
        maximum_asymmetry_contribution=max(abs(r["asymmetry_contribution"]) for r in asymmetry),
        worst_startup_case=startup,
        maximum_startup_to_single_path_ratio=max(float(r["post_mix_peak"])/float(r["single_voice_m26_path_peak"]) for r in data["startup"] if float(r["single_voice_m26_path_peak"])>0),
        maximum_hostile_detector_release_seconds=max(int(r["detector_recovery_frames"]) for r in data["hostile_input"])/48000,
        internal_fault_classifications=dict(Counter(r["recovery_classification"] for r in data["nonfinite_recovery"] if r["source"].startswith("internal"))),
        denormal_observation_seconds=max(float(r["time_s"]) for r in denormals),
        maximum_subnormal_observations=max(sum(int(r[k]) for k in r if k.endswith("subnormals")) for r in denormals),
        allocation_gates="PASS", numerical_gates="PASS", cpu_acceptance="INFORMATIONAL_ONLY",
    )
    high = root / "bbd_realtime_denormals_max_clock.csv"
    if high.exists():
        table = rows(root, "denormals_max_clock")
        summary["high_clock_denormal_seconds"] = max(float(r["time_s"]) for r in table)
        summary["high_clock_maximum_subnormal_observations"] = max(sum(int(r[k]) for k in r if k.endswith("subnormals")) for r in table)
    (root / "SUMMARY.json").write_text(json.dumps(summary, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    try:
        main(Path(sys.argv[1]))
    except (OSError, ValueError, KeyError, IndexError) as error:
        sys.exit(f"M2.8 artifact validation failed: {error}")
