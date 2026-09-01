#!/usr/bin/env python3
"""Carve ring interpretations or csd_write submission order from serial."""

import argparse
import array
import base64
import hashlib
import math
from pathlib import Path
import sys
import wave


STREAMS = {
    "A_HIGH16_CORRECT": "dsp_render_a_high16.wav",
    "B_LOW24_DSP": "dsp_render_b_low24.wav",
}
SUBMISSION_NAME = "SUBMIT_HIGH16"
SUBMISSION_FILE = "dsp_submission_order.wav"


def carve(log: bytes, name: str) -> bytes:
    begin = f"=== DSP_RENDER_{name}_S16LE_BASE64_BEGIN ".encode()
    end = f"=== DSP_RENDER_{name}_S16LE_BASE64_END ===".encode()
    active = False
    chunks = []
    for raw_line in log.splitlines():
        line = raw_line.strip().replace(b"\r", b"")
        if line.startswith(begin):
            if active:
                raise RuntimeError(f"duplicate BEGIN marker for {name}")
            active = True
            continue
        if line == end:
            if not active:
                raise RuntimeError(f"END marker without BEGIN for {name}")
            return base64.b64decode(b"".join(chunks), validate=True)
        if active and line.startswith(b"DSP64 "):
            chunks.append(line[6:])
    raise RuntimeError(f"complete marked stream not found: {name}")


def carve_submission(log: bytes) -> bytes:
    begin = b"=== DSP_SUBMIT_HIGH16_S16LE_BASE64_BEGIN "
    end = b"=== DSP_SUBMIT_HIGH16_S16LE_BASE64_END ==="
    active = False
    chunks = []
    for raw_line in log.splitlines():
        line = raw_line.strip().replace(b"\r", b"")
        if line.startswith(begin):
            if active:
                raise RuntimeError("duplicate submission BEGIN marker")
            active = True
            continue
        if line == end:
            if not active:
                raise RuntimeError("submission END without BEGIN")
            return base64.b64decode(b"".join(chunks), validate=True)
        if active and line.startswith(b"DSP64 "):
            chunks.append(line[6:])
    raise RuntimeError("complete csd_write submission stream not found")


def samples_s16le(raw: bytes) -> array.array:
    samples = array.array("h")
    samples.frombytes(raw)
    if sys.byteorder != "little":
        samples.byteswap()
    return samples


def rms(samples: array.array) -> float:
    if not samples:
        return 0.0
    return math.sqrt(sum(int(v) * int(v) for v in samples) / len(samples))


def write_wav(path: Path, raw: bytes) -> None:
    with wave.open(str(path), "wb") as out:
        out.setnchannels(6)
        out.setsampwidth(2)
        out.setframerate(48000)
        out.writeframes(raw)


def read_reference(path: Path, frames: int) -> bytes:
    with wave.open(str(path), "rb") as src:
        geometry = (src.getnchannels(), src.getframerate(), src.getsampwidth())
        if geometry != (6, 48000, 2):
            raise RuntimeError(f"unexpected reference geometry: {geometry}")
        return src.readframes(frames)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("serial_log", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument(
        "--reference",
        type=Path,
        default=Path(
            "captures/broken_5_1_20260901_drive/"
            "session6_5_1_declared_order.wav"
        ),
    )
    args = parser.parse_args()

    log = args.serial_log.read_bytes()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    if b"=== DSP_SUBMIT_HIGH16_S16LE_BASE64_BEGIN " in log:
        submitted = carve_submission(log)
        frame_bytes = 6 * 2
        if len(submitted) % frame_bytes:
            raise RuntimeError(
                f"submission is not whole 6-channel frames: {len(submitted)}"
            )
        frames = len(submitted) // frame_bytes
        output_path = args.output_dir / SUBMISSION_FILE
        write_wav(output_path, submitted)
        expected = read_reference(args.reference, frames)
        submitted_samples = samples_s16le(submitted)
        expected_samples = samples_s16le(expected)
        mismatches = sum(
            actual != reference
            for actual, reference in zip(submitted_samples, expected_samples)
        )
        if len(submitted) != len(expected):
            mismatches += abs(len(submitted) - len(expected)) // 2
        print(
            f"frames={frames} duration={frames / 48000:.6f}s "
            f"samples={len(submitted_samples)}"
        )
        print(
            f"{SUBMISSION_NAME} bit_identical={submitted == expected} "
            f"mismatches={mismatches} "
            f"peak={max((abs(int(v)) for v in submitted_samples), default=0)} "
            f"rms={rms(submitted_samples):.3f} "
            f"pcm_sha256={hashlib.sha256(submitted).hexdigest()}"
        )
        print(output_path)
        return 0 if submitted == expected else 2

    rendered = {name: carve(log, name) for name in STREAMS}
    sizes = {len(raw) for raw in rendered.values()}
    if len(sizes) != 1:
        raise RuntimeError(f"stream sizes differ: {sorted(sizes)}")
    byte_count = sizes.pop()
    frame_bytes = 6 * 2
    if byte_count % frame_bytes:
        raise RuntimeError(f"stream is not whole 6-channel frames: {byte_count}")
    frames = byte_count // frame_bytes

    for name, filename in STREAMS.items():
        write_wav(args.output_dir / filename, rendered[name])

    expected = read_reference(args.reference, frames)
    a_raw = rendered["A_HIGH16_CORRECT"]
    a_samples = samples_s16le(a_raw)
    expected_samples = samples_s16le(expected)
    a_mismatches = sum(a != b for a, b in zip(a_samples, expected_samples))
    if len(a_raw) != len(expected):
        a_mismatches += abs(len(a_raw) - len(expected)) // 2

    b_samples = samples_s16le(rendered["B_LOW24_DSP"])
    b_peak = max((abs(int(v)) for v in b_samples), default=0)
    b_saturated = sum(v == -32768 or v == 32767 for v in b_samples)
    b_sat_percent = 100.0 * b_saturated / len(b_samples) if b_samples else 0.0
    b_different = sum(a != b for a, b in zip(b_samples, expected_samples))

    print(f"frames={frames} duration={frames / 48000:.6f}s samples={len(a_samples)}")
    print(
        "A_HIGH16_CORRECT "
        f"bit_identical={a_raw == expected} mismatches={a_mismatches} "
        f"peak={max((abs(int(v)) for v in a_samples), default=0)} "
        f"rms={rms(a_samples):.3f}"
    )
    print(
        "B_LOW24_DSP "
        f"peak={b_peak} saturated={b_saturated}/{len(b_samples)} "
        f"saturated_percent={b_sat_percent:.6f} "
        f"different_from_input={b_different}/{len(b_samples)} "
        f"rms={rms(b_samples):.3f}"
    )
    for filename in STREAMS.values():
        print(args.output_dir / filename)
    return 0 if a_raw == expected else 2


if __name__ == "__main__":
    raise SystemExit(main())
