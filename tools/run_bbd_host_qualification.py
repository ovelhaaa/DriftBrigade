"""Run the M3.1D timing matrix and record machine and build provenance."""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--compile-commands", type=Path, required=True)
    parser.add_argument("--environment", choices=("ci", "workstation", "dedicated"), required=True)
    parser.add_argument("--callbacks", type=int, default=2000)
    parser.add_argument("--conditions", default="")
    args = parser.parse_args()

    commands = json.loads(args.compile_commands.read_text(encoding="utf-8"))
    relevant = [row for row in commands if Path(row["file"]).name in {
        "BBDHostQualification.cpp", "DriftEngine.cpp", "ClockedBBDCore.cpp", "OrganicModulator.cpp"
    } and ("drift_dsp.dir" in str(row) or "drift_bbd_host_qualification.dir" in str(row))]
    if {Path(row["file"]).name for row in relevant} != {
        "BBDHostQualification.cpp", "DriftEngine.cpp", "ClockedBBDCore.cpp", "OrganicModulator.cpp"
    }:
        parser.error("compile database must contain the production DSP and host timing targets")

    cpu = platform.processor()
    if platform.system() == "Linux":
        cpu = next((line.partition(":")[2].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                    if line.startswith("model name")), cpu)
    elif platform.system() == "Darwin":
        cpu = subprocess.check_output(["sysctl", "-n", "machdep.cpu.brand_string"], text=True).strip()
    elif platform.system() == "Windows":
        cpu = subprocess.check_output([
            "powershell", "-NoProfile", "-Command", "(Get-CimInstance Win32_Processor).Name"
        ], text=True).strip()
    build = args.compile_commands.resolve().parent
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    compiler = next(line.partition("=")[2] for line in cache.splitlines()
                    if line.startswith("CMAKE_CXX_COMPILER:"))
    if Path(compiler).name.lower() == "cl.exe":
        command = subprocess.run([compiler], capture_output=True, text=True)
        version = command.stdout + command.stderr
    else:
        version = subprocess.check_output([compiler, "--version"], text=True)
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    dirty = bool(subprocess.check_output(["git", "status", "--porcelain"], text=True).strip())
    subprocess.run([str(args.executable.resolve()), str(args.output.resolve()), str(args.callbacks)], check=True)
    metadata = {
        "environment": args.environment,
        "conditions": args.conditions,
        "os": platform.platform(),
        "cpu": cpu,
        "logical_cpus": os.cpu_count(),
        "git_revision": revision,
        "dirty_worktree": dirty,
        "compiler": version.strip(),
        "compile_commands": relevant,
        "scene": "Balanced1024 Wander Gentle Nominal, same automation and inputs for Digital",
        "warmup_callbacks_per_case": 128,
        "callbacks_per_case": args.callbacks,
        "dsp_operating_instrumentation": False,
        "allocation_tracking": "ordinary C++ new/delete on callback thread",
    }
    (args.output / "host_metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
