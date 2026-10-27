#!/usr/bin/env python3
"""Measure bounded native-source and QNX decoder-input intervals, separately."""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import re
import statistics

if __package__:
    from .extract_native_video import parse_records
else:
    from extract_native_video import parse_records


def intervals(times_ms: list[float]) -> dict:
    if any(not math.isfinite(value) for value in times_ms):
        raise ValueError("non-finite timestamp")
    gaps = [b-a for a, b in zip(times_ms, times_ms[1:])]
    if any(gap < 0 for gap in gaps):
        raise ValueError("timestamps moved backwards")
    ordered = sorted(gaps)
    def percentile(fraction):
        if not ordered:
            return None
        index = fraction*(len(ordered)-1)
        lo = int(index)
        return ordered[lo] + (ordered[min(lo+1, len(ordered)-1)]-ordered[lo])*(index-lo)
    duration = times_ms[-1]-times_ms[0] if len(times_ms) > 1 else 0
    return {"events": len(times_ms), "span_ms": duration,
            "observed_fps": len(gaps)*1000/duration if duration else None,
            "interval_ms": {"min": min(gaps) if gaps else None,
                "median": statistics.median(gaps) if gaps else None,
                "p95": percentile(.95), "p99": percentile(.99), "max": max(gaps) if gaps else None},
            "gaps_over_100ms": sum(gap > 100 for gap in gaps),
            "gaps_over_1000ms": sum(gap > 1000 for gap in gaps)}


def report(qnx_text: str, source_text: str | None = None) -> dict:
    records = parse_records(qnx_text)
    # SPS/PPS-only buffers configure the decoder; they are not pictures.
    pictures = [(data, stamp) for data, stamp in records
                if any(nal and 1 <= nal[0] & 31 <= 5
                       for nal in re.split(b"\x00\x00(?:\x00)?\x01", data)[1:])]
    stamps = [stamp for _, stamp in pictures]
    valid = bool(stamps) and all(stamp is not None and stamp > 0 for stamp in stamps)
    result = {"decoder_input_records": len(records), "picture_buffers": len(pictures),
        "avc_bytes": sum(len(data) for data, _ in records),
        "qnx_decoder_input": intervals([stamp/1e6 for stamp in stamps]) if valid else None,
        "qnx_timestamps_available": valid,
        "clock_alignment": "not established; source and receiver clocks are separate",
        "limitations": ["UART plaintext capture adds overhead to the decoder-input path",
            "picture buffers are not an independent decoded-frame count",
            "no end-to-end latency, packet loss or queue depth inferred from these intervals"]}
    if source_text is not None:
        source = re.findall(r"\[native-source-qemu\] frame=(\d+) elapsed_ms=(\d+(?:\.\d+)?) render_ms=(\d+(?:\.\d+)?)", source_text)
        if any(int(frame) != index+1 for index, (frame, _, _) in enumerate(source)):
            raise ValueError("native source frame sequence is incomplete or duplicated")
        times = [float(elapsed) for _, elapsed, _ in source]
        render = [float(cost) for _, _, cost in source]
        if any(not math.isfinite(cost) for cost in render):
            raise ValueError("non-finite render duration")
        result["native_source"] = intervals(times)
        result["native_source"]["render_ms"] = {"median": statistics.median(render) if render else None,
                                               "max": max(render) if render else None}
        finish = re.findall(r"\[mhi2-screen-framebuffer\] finished result=(\d+) submitted=(\d+)", source_text)
        result["native_source"]["completion"] = {"result": int(finish[-1][0]), "submitted": int(finish[-1][1])} if finish else None
        if finish and int(finish[-1][1]) != len(source):
            raise ValueError("source completion count disagrees with timing records")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("qnx_log", type=Path)
    parser.add_argument("--source-log", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--lab-dir", type=Path, default=Path(__file__).resolve().parents[3]/"build/ios-vm-lab")
    args = parser.parse_args()
    for path in (args.qnx_log, args.source_log):
        if path and path.stat().st_size > 67108864:
            parser.error("timing log exceeds 64 MiB")
    if args.output:
        build = Path(__file__).resolve().parents[3]/"build"
        lab = args.lab_dir.resolve()
        if not lab.is_relative_to(build.resolve()) or lab == build.resolve():
            parser.error("lab directory must be inside the owned build tree")
        if not args.output.resolve().is_relative_to(lab.resolve()) or args.output.suffix != ".json":
            parser.error("report must be JSON inside the owned lab")
    try:
        value = report(args.qnx_log.read_text(errors="replace"),
                       args.source_log.read_text(errors="replace") if args.source_log else None)
    except ValueError as error:
        if args.output:
            args.output.unlink(missing_ok=True)
        parser.error(str(error))
    rendered = json.dumps(value, indent=2)+"\n"
    if args.output:
        args.output.write_text(rendered)
    print(rendered, end="")


if __name__ == "__main__":
    main()
