"""Compare primary and AltScreen in the same explicit PCAP time window."""
import argparse
import json
import math
from pathlib import Path
import statistics

from .screen_wire_pacing import TELEMETRY_FIELDS, packets, report


def counters(samples):
    result = {}
    for key in TELEMETRY_FIELDS:
        values = [sample["metrics"][key] for sample in samples if key in sample["metrics"]]
        result[key] = ({"samples": len(values), "mean": statistics.mean(values),
                        "max": max(values)} if values else None)
    return result


def compare(data, start_ms, end_ms, bin_ms=5000, primary_port=6030, alt_port=6031):
    if (any(not math.isfinite(v) for v in (start_ms, end_ms, bin_ms)) or
            start_ms >= end_ms or bin_ms <= 0 or end_ms-start_ms > 300000 or
            math.ceil((end_ms-start_ms)/bin_ms) > 300):
        raise ValueError("require a finite window up to 300 seconds and at most 300 positive bins")
    if primary_port == alt_port:
        raise ValueError("primary and AltScreen must use distinct ports")
    stamps = [stamp for stamp, _ in packets(data)]
    if not stamps or min(stamps) > start_ms or max(stamps) < end_ms:
        raise ValueError("PCAP does not cover the complete comparison window")
    result = {"window_ms": [start_ms, end_ms], "bin_ms": bin_ms, "screens": {}}
    for name, port in (("primary", primary_port), ("alt", alt_port)):
        connections = report(data, port, (start_ms, end_ms), include_samples=True)["connections"]
        if len(connections) > 1:
            raise ValueError("split sessions before comparing screens")
        if not connections:
            result["screens"][name] = {"port": port, "flow_available": False}
            continue
        connection = connections[0]
        # Half-open bins count every frame once; both screens use the full
        # chosen duration, including any idle part or interruption in that flow.
        pictures = [v for v in connection["access_unit_capture_ms"] if v < end_ms]
        samples = [s for s in connection["telemetry_samples"] if s["capture_ms"] < end_ms]
        bins = []
        for i in range(math.ceil((end_ms-start_ms)/bin_ms)):
            left, right = start_ms+i*bin_ms, min(end_ms, start_ms+(i+1)*bin_ms)
            count = sum(left <= stamp < right for stamp in pictures)
            bins.append({"start_ms": left, "end_ms": right, "video_aus": count,
                         "window_au_per_second": count*1000/(right-left),
                         "sender_counters": counters([s for s in samples if left <= s["capture_ms"] < right])})
        result["screens"][name] = {
            "port": port, "flow_available": True, "video_aus": len(pictures),
            "window_au_per_second": len(pictures)*1000/(end_ms-start_ms),
            "bins_without_video": sum(b["video_aus"] == 0 for b in bins),
            "wire_interval_cadence": connection["screen_access_units"],
            "sender_target_cadence": connection["sender_timestamp_intervals"],
            "sender_counters": counters(samples), "bins": bins,
            "tcp": {key: connection[key] for key in (
                "bytes_after_sequence_gap", "unparsed_contiguous_bytes",
                "repeated_tcp_payload_bytes", "receiver_advertised_window")},
        }
    result["limitations"] = [
        "AUs are not independently decoded, unique or physically presented pictures",
        "both streams must depict continuous motion to test sustained animation",
        "window rates include idle time; interval cadence inside a short burst is different",
        "missing sender counters are null, not zero; reporting windows can cross bin edges",
        "a lower sender target cadence can result from receiver backpressure",
        "a smooth primary screen does not rule out a per-stream receive/sink bottleneck",
        "no automatic phone, USB or renderer root-cause classification is made",
    ]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcap", type=Path)
    parser.add_argument("--window-start-ms", type=float, required=True)
    parser.add_argument("--window-end-ms", type=float, required=True)
    parser.add_argument("--bin-ms", type=float, default=5000)
    parser.add_argument("--primary-port", type=int, default=6030)
    parser.add_argument("--alt-port", type=int, default=6031)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.pcap.stat().st_size > 64*1024*1024:
        parser.error("PCAP exceeds the 64 MiB bound")
    if args.output and (args.output.suffix != ".json" or
                       not args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3] / "build")):
        parser.error("output must be JSON inside this repository's build directory")
    try:
        result = compare(args.pcap.read_bytes(), args.window_start_ms, args.window_end_ms,
                         args.bin_ms, args.primary_port, args.alt_port)
    except ValueError as error:
        if args.output:
            args.output.unlink(missing_ok=True)
        parser.error(str(error))
    rendered = json.dumps(result, indent=2)+"\n"
    if args.output:
        args.output.write_text(rendered)
    else:
        print(rendered, end="")


if __name__ == "__main__":
    main()
