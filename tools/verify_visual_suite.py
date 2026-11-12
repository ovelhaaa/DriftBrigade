#!/usr/bin/env python3
"""Verify that the DriftBrigade --visual-suite generated all expected PNG captures with valid headers and dimensions."""

from __future__ import annotations

import pathlib
import struct
import sys

EXPECTED_CAPTURES: dict[str, tuple[int, int]] = {
    "01_default_960x600.png": (960, 600),
    "02_minimum_760x520.png": (760, 520),
    "03_expanded_1280x800.png": (1280, 800),
    "04_scale_125pct.png": (1200, 750),
    "05_scale_150pct.png": (1440, 900),
    "06_scale_200pct.png": (1920, 1200),
    "07_controls_minimum.png": (960, 600),
    "08_controls_maximum.png": (960, 600),
    "09_quickstart_subtle.png": (960, 600),
    "10_quickstart_wide.png": (960, 600),
    "11_quickstart_drift.png": (960, 600),
    "12_hold_modified_controls.png": (960, 600),
}

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def read_png_dimensions(path: pathlib.Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < 24 or data[:8] != PNG_SIGNATURE:
        raise ValueError(f"Invalid PNG signature in {path}")
    chunk_len, chunk_type = struct.unpack(">I4s", data[8:16])
    if chunk_type != b"IHDR" or chunk_len < 8:
        raise ValueError(f"Missing IHDR chunk in {path}")
    width, height = struct.unpack(">II", data[16:24])
    if width <= 0 or height <= 0:
        raise ValueError(f"Non-positive PNG dimensions ({width}x{height}) in {path}")
    return width, height


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: verify_visual_suite.py <visual-suite-dir>", file=sys.stderr)
        return 2

    suite_dir = pathlib.Path(sys.argv[1])
    if not suite_dir.is_dir():
        print(f"FAIL: visual suite directory does not exist: {suite_dir}", file=sys.stderr)
        return 1

    for filename, (exp_w, exp_h) in EXPECTED_CAPTURES.items():
        file_path = suite_dir / filename
        if not file_path.is_file():
            print(f"FAIL: missing expected visual capture: {file_path}", file=sys.stderr)
            return 1
        size_bytes = file_path.stat().st_size
        if size_bytes < 4096:
            print(f"FAIL: capture file suspiciously small ({size_bytes} bytes): {file_path}", file=sys.stderr)
            return 1
        try:
            w, h = read_png_dimensions(file_path)
        except Exception as exc:
            print(f"FAIL: {exc}", file=sys.stderr)
            return 1
        if (w, h) != (exp_w, exp_h):
            print(
                f"FAIL: {filename} has dimensions {w}x{h}, expected {exp_w}x{exp_h}",
                file=sys.stderr,
            )
            return 1
        print(f"OK: {filename} ({w}x{h}, {size_bytes} bytes)")

    print(f"PASS: verified {len(EXPECTED_CAPTURES)} real PNG captures in {suite_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
