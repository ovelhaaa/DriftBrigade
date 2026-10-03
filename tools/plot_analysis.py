"""Offline plots only; requires matplotlib. No plugin/runtime dependency."""
import argparse
import csv
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument("csv", type=Path)
parser.add_argument("--output", type=Path)
args = parser.parse_args()
with args.csv.open(newline="", encoding="utf-8") as stream:
    rows = list(csv.DictReader(stream))
if not rows:
    raise SystemExit("Empty telemetry")
data = {key: [float(row[key]) for row in rows] for key in rows[0]}
fig, axes = plt.subplots(4, 1, figsize=(11, 10), sharex=True, constrained_layout=True)
t = data["time_s"]
axes[0].plot(t, data["organic"], label="Chaos morph")
axes[0].plot(t, data["random_control"], alpha=0.65, label="Raised-cosine random")
axes[0].legend(loc="upper right"); axes[0].set_ylabel("Control")
for band in range(4):
    axes[1].plot(t, data[f"mod_0_{band}"], label=f"Band {band}")
    axes[2].plot(t, [1000*x for x in data[f"delay_s_0_{band}"]], label=f"Band {band}")
axes[1].legend(ncol=4, loc="upper right"); axes[1].set_ylabel("Band motion")
axes[2].set_ylabel("Delay (ms)")
for key in ["envelope", "effective_chaos", "effective_feedback", "wet_prominence"]:
    axes[3].plot(t, data[key], label=key.replace("_", " "))
axes[3].set_ylabel("Dynamics"); axes[3].set_xlabel("Time (s)")
axes[3].legend(ncol=2, loc="upper right")
for ax in axes:
    ax.grid(alpha=0.2)
    ax.axvline(2, color="gray", linestyle=":", alpha=0.6)
    ax.axvline(5, color="gray", linestyle=":", alpha=0.6)
fig.suptitle(f"DRIFT BRIGADE — {args.csv.stem} (fixed seed, quiet/loud/quiet input)")
destination = args.output or args.csv.with_suffix(".png")
fig.savefig(destination, dpi=150)
print(destination)
