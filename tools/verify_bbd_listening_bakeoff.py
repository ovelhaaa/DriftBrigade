"""Standard-library PCM/manifest/constant-gain verification; no subjective gates."""
import argparse
import csv
import hashlib
import json
import math
import pathlib
import struct
import wave

SIZES = {"A01": 2, "A02": 2, "A03": 2, "A04": 2, "B01": 2, "B02": 2,
         "C01": 2, "C02": 2, "D01": 3, "D02": 3, "E01": 2, "E02": 2}
NAMES = [f"SET_{group}_{label}.wav" for group, size in SIZES.items()
         for label in "XYZ"[:size]]
EXPECTED = {f"{version}/{name}" for version in ("raw", "level_matched") for name in NAMES}
EXPECTED |= {f"dry/DRY_{label}.wav" for label in "ABC"}
LSB = 1 / 8388608


def rows(path):
    with path.open(encoding="utf-8", newline="") as stream:
        data = list(csv.DictReader(stream))
    assert data and all(None not in row and None not in row.values() for row in data), path
    return data


def pcm(path):
    header = path.read_bytes()[:44]
    assert len(header) == 44 and header[:4] == b"RIFF" and header[8:16] == b"WAVEfmt "
    assert struct.unpack_from("<IHHIIHH", header, 16) == (16, 1, 2, 48000, 288000, 6, 24), path
    assert header[36:40] == b"data" and struct.unpack_from("<I", header, 40)[0] == 480000 * 6
    assert struct.unpack_from("<I", header, 4)[0] + 8 == path.stat().st_size == 44 + 480000 * 6
    with wave.open(str(path), "rb") as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()) == (2, 3, 48000, 480000)
        data = wav.readframes(wav.getnframes())
    samples = []
    for offset in range(0, len(data), 3):
        value = data[offset] | data[offset + 1] << 8 | data[offset + 2] << 16
        if value & 0x800000:
            value -= 1 << 24
        assert -8388608 < value < 8388607, f"Clipped PCM: {path}"
        samples.append(value * LSB)
    return samples


def db(x):
    return 20 * math.log10(max(x, 1e-300))


def rms(samples):
    active = samples[24000 * 2:360000 * 2]
    return math.sqrt(math.fsum(x * x for x in active) / len(active))


def verify(root, compare=None):
    manifest = rows(root / "listening_render_manifest.csv")
    indexed = {row["blind_filename"]: row for row in manifest}
    assert len(manifest) == len(indexed) == 55 and set(indexed) == EXPECTED
    assert {str(p.relative_to(root)).replace("\\", "/") for p in root.rglob("*.wav")} == EXPECTED
    key = rows(root / "ANSWER_KEY.csv")
    assert len(key) == 26 and {r["blind_filename"] for r in key} == set(NAMES)
    mapping = {r["blind_filename"]: r for r in key}
    score = rows(root / "LISTENING_SCORECARD.csv")
    assert len(score) == 12 and {r["pair_group"] for r in score} == set(SIZES)
    for row in score:
        assert set(row["blind_candidates"].split(";")) == {f"SET_{row['pair_group']}_{v}.wav" for v in "XYZ"[:SIZES[row["pair_group"]]]}
        for field in row:
            if field not in ("set", "pair_group", "source", "scene", "blind_candidates"):
                assert row[field] == "", "Subjective score was prefilled"

    digests, maximum_error, maximum_peak, constant_gain_error = {}, 0, 0, 0
    matched_by_group = {}
    for filename in sorted(EXPECTED):
        row, path = indexed[filename], root / filename
        for field, value in row.items():
            if value == "NA":
                continue
            try:
                parsed = float(value)
            except ValueError:
                continue
            assert math.isfinite(parsed), (filename, field, value)
        assert row["status"] == "PASS" and row["sample_rate"] == "48000" and row["frames"] == "480000"
        values = pcm(path)
        measured, peak = rms(values), max(map(abs, values))
        maximum_peak = max(maximum_peak, peak)
        # A rounded 24-bit sample differs by at most half an LSB from pre-PCM telemetry.
        assert abs(measured - float(row["output_rms"])) <= LSB, filename
        assert abs(peak - float(row["output_peak"])) <= LSB, filename
        measured_dc = math.fsum(values) / len(values)
        assert abs(measured_dc - float(row["output_dc"])) <= LSB, filename
        digests[filename] = hashlib.sha256(path.read_bytes()).hexdigest()
        if compare:
            assert digests[filename] == hashlib.sha256((compare / filename).read_bytes()).hexdigest(), f"Nondeterministic WAV {filename}"
        if row["version"] == "dry":
            assert row["backend"] == "NA"
            assert not any(values[:24000 * 2]) and not any(values[360000 * 2:])
            continue
        identity = mapping[path.name]
        for field in ("candidate_id", "backend", "stages", "gain_profile", "organic_variant", "bank_mode", "group"):
            assert row[field] == identity[field], (filename, field)
        assert row["callback_allocations"] == "0"
        assert row["deterministic_repeat"] in ("PASS", "SHORT_SANITY_PASS")
        assert float(row["requested_excursion_seconds"]) == float(row["actual_excursion_seconds"])
        if row["set"] != "D":
            assert float(row["trajectory_max_error_seconds"]) == 0
        if row["backend"] == "ExperimentalBBD":
            assert row["hidden_clamp_count"] == row["numerical_guard_count"] == "0"
            assert float(row["delay_min_seconds"]) >= float(row["product_delay_floor_seconds"]) - 1e-12
            assert float(row["bbd_clock_max_hz"]) <= float(row["product_clock_cap_hz"]) * (1 + 1e-12)
            assert float(row["maximum_events_per_sample_per_voice"]) <= (8 if row["stages"] == "512" else 16)
        else:
            for field in ("stages", "gain_profile", "bbd_clock_min_hz", "bbd_clock_max_hz", "internal_peak", "nominal_domain_occupancy", "total_events", "hidden_clamp_count", "numerical_guard_count"):
                assert row[field] == "NA", (filename, field)
        if row["version"] == "level_matched":
            assert peak <= .98 + LSB
            raw = pcm(root / "raw" / path.name)
            gain = 10 ** (float(row["level_match_gain_db"]) / 20)
            error = max(abs(y - x * gain) for x, y in zip(raw, values))
            assert error <= (1 + gain) * LSB * .501, f"Not constant-scalar matching: {filename}"
            constant_gain_error = max(constant_gain_error, error)
            matched_by_group.setdefault(row["group"], []).append(measured)
    for group, values in matched_by_group.items():
        assert len(values) == SIZES[group]
        error = db(max(values) / min(values))
        assert error <= .1, (group, error)
        maximum_error = max(maximum_error, error)
    # Each comparison changes exactly its intended principal factor.
    factor = {"A": "backend", "B": "stages", "C": "gain_profile", "D": "organic_variant", "E": "bank_mode"}
    for group in SIZES:
        members = [r for r in manifest if r["group"] == group and r["version"] == "raw"]
        assert len(members) == SIZES[group]
        varied = factor[group[0]]
        for field in ("backend", "stages", "gain_profile", "organic_variant", "bank_mode"):
            # Digital stages/gain have NA semantics, not a second architecture change.
            if group[0] == "A" and field in ("stages", "gain_profile"):
                continue
            assert (len({r[field] for r in members}) > 1) == (field == varied), (group, field)
        for field in ("source", "scene", "seed", "source_seed", "source_gain", "Motion", "Depth", "Center", "Chaos", "Coherence", "Dynamics", "Feedback", "Mix", "Width"):
            assert len({r[field] for r in members}) == 1, (group, field)
    sanity = rows(root / "objective" / "sanity_rates.csv")
    assert len(sanity) == 78 and {r["sample_rate"] for r in sanity} == {"44100", "48000", "88200"}
    assert all(r["deterministic"] == "PASS" and r["allocations"] == "0" for r in sanity)
    assert all(r["clamps"] in ("0", "NA") and r["guards"] in ("0", "NA") for r in sanity)
    assert all(float(r["trajectory_error_seconds"]) == 0 for r in sanity if not r["group"].startswith("D"))
    for name, count in (("backend", 8), ("stage", 4), ("gain", 4), ("modulation", 6), ("bank", 4)):
        assert len(rows(root / "objective" / f"{name}_comparison.csv")) == count
    assert len(rows(root / "scene_parameters.csv")) == 6
    assert (root / "LISTENING_GUIDE.md").is_file()
    with (root / "SHA256SUMS.txt").open("w", encoding="utf-8", newline="\n") as stream:
        for name, checksum in sorted(digests.items()):
            stream.write(f"{checksum}  {name}\n")
    result = dict(wavs=55, processed_candidates=26, groups=12, sample_rate=48000,
                  format="stereo PCM 24-bit", frames_per_wav=480000, blind_seed="0x4d333042",
                  maximum_pcm_matching_error_db=maximum_error, maximum_pcm_output_peak=maximum_peak,
                  maximum_constant_gain_quantization_error=constant_gain_error,
                  maximum_bbd_clock_hz=max(float(r["bbd_clock_max_hz"]) for r in manifest if r["backend"] == "ExperimentalBBD"),
                  maximum_events_per_sample_per_voice=max(float(r["maximum_events_per_sample_per_voice"]) for r in manifest if r["backend"] == "ExperimentalBBD"),
                  hidden_clamps=0, numerical_guards=0, callback_allocations=0,
                  common_trajectory="EXACT", digital_frozen_regression="PASS",
                  deterministic_wav_checksums="ALL_55_PASS" if compare else "NOT_INDEPENDENTLY_COMPARED",
                  subjective_winner="NONE", production_backend="DigitalFractional")
    (root / "VERIFICATION.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--compare", type=pathlib.Path, help="Independent rerender directory; compare every WAV SHA-256")
    args = parser.parse_args()
    verify(args.output, args.compare)
