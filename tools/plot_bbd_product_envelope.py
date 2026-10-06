"""Optional local research plot (requires matplotlib); CSVs remain authoritative."""
import csv
import pathlib
import sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

root = pathlib.Path(sys.argv[1])
with (root / "bbd_product_event_budget.csv").open(newline="", encoding="utf-8") as stream:
    rows = list(csv.DictReader(stream))
fig, axes = plt.subplots(1, 2, figsize=(12, 5))
colors = plt.get_cmap("tab10").colors
for index, rate in enumerate(sorted({float(r["sample_rate"]) for r in rows})):
    data = sorted((r for r in rows if float(r["sample_rate"]) == rate), key=lambda r: float(r["target_edges_per_sample"]))
    x = [float(r["actual_edges_per_sample"]) for r in data]
    axes[0].plot(x, [float(r["p99"])*1000 for r in data], "o-", lw=1.5, ms=3, color=colors[index], label=f"{rate/1000:g} kHz")
    axes[1].plot(x, [float(r["p99_utilization"])*100 for r in data], "o-", lw=1.5, ms=3, color=colors[index])
for axis in axes:
    axis.set_xscale("log", base=2)
    axis.set_xticks([4, 8, 12, 16, 24, 32, 48, 64, 96, 128], labels=[4, 8, 12, 16, 24, 32, 48, 64, 96, 128])
    axis.tick_params(axis="x", labelsize=8)
    axis.set_xlabel("Physical edges / host sample / voice")
    axis.grid(alpha=.18)
    axis.spines[["top", "right"]].set_visible(False)
axes[0].set_ylabel("p99 callback (ms)")
axes[1].set_ylabel("p99 / callback deadline (%)")
axes[0].legend(frameon=False, ncol=2, fontsize=9)
axes[1].axhline(50, color="#444444", ls="--", lw=1, label="50% target")
axes[1].axhline(70, color="#a23a25", ls=":", lw=1.3, label="70% risk threshold")
axes[1].legend(frameon=False, fontsize=9)
fig.suptitle("BBD event-budget sweep — complete eight-voice engine", fontsize=15)
callback_counts = "/".join(sorted({r["callbacks"] for r in rows}))
fig.text(.5, .015, f"1024 stages · block128 · {callback_counts} callbacks/point · static clocks · see artifact build/machine metadata\nExploratory timing; product admission also uses five independent 10,000-callback modulation trials.", ha="center", fontsize=8, color="#555555")
fig.tight_layout(rect=(0, .08, 1, .94))
fig.savefig(root / "bbd_product_event_curve.png", dpi=200)
