#!/usr/bin/env python3
"""Read-only screen header timing from bounded Ethernet PCAP, without keys."""
from __future__ import annotations

import argparse
from array import array
import json
from pathlib import Path
import plistlib
import struct

if __package__:
    from .frame_pacing import intervals
else:
    from frame_pacing import intervals

LIMIT = 64*1024*1024


def packets(data: bytes):
    if len(data) < 24 or len(data) > LIMIT:
        raise ValueError("PCAP size outside bounds")
    formats = {b"\xd4\xc3\xb2\xa1": ("<", 1e6), b"\xa1\xb2\xc3\xd4": (">", 1e6),
               b"\x4d\x3c\xb2\xa1": ("<", 1e9), b"\xa1\xb2\x3c\x4d": (">", 1e9)}
    if data[:4] not in formats:
        raise ValueError("classic PCAP required")
    endian, scale = formats[data[:4]]
    major, minor, _, _, snaplen, network = struct.unpack_from(endian+"HHIIII", data, 4)
    if (major, minor) != (2, 4) or network != 1 or not 14 <= snaplen <= 65535:
        raise ValueError("bounded Ethernet PCAP required")
    offset = 24
    while offset < len(data):
        if len(data)-offset < 16:
            raise ValueError("truncated PCAP record")
        sec, fraction, captured, original = struct.unpack_from(endian+"IIII", data, offset)
        offset += 16
        if fraction >= scale or captured != original or captured > snaplen or captured > len(data)-offset:
            raise ValueError("invalid or truncated PCAP packet")
        yield (sec+fraction/scale)*1000, data[offset:offset+captured]
        offset += captured


def tcp(frame: bytes):
    if len(frame) < 14:
        return None
    kind = int.from_bytes(frame[12:14], "big")
    body = frame[14:]
    if kind == 0x86dd:
        if len(body) < 40 or body[0] >> 4 != 6:
            raise ValueError("truncated IPv6")
        length = int.from_bytes(body[4:6], "big")
        if len(body) < 40+length:
            raise ValueError("truncated IPv6 payload")
        if body[6] != 6:
            return None
        src, dst, segment = body[8:24], body[24:40], body[40:40+length]
    elif kind == 0x0800:
        if len(body) < 20 or body[0] >> 4 != 4:
            raise ValueError("truncated IPv4")
        header, length = (body[0] & 15)*4, int.from_bytes(body[2:4], "big")
        if header < 20 or length < header or len(body) < length:
            raise ValueError("invalid IPv4 layout")
        if body[9] != 6 or int.from_bytes(body[6:8], "big") & 0x3fff:
            return None
        src, dst, segment = body[12:16], body[16:20], body[header:length]
    else:
        return None
    if len(segment) < 20:
        raise ValueError("truncated TCP")
    header = (segment[12] >> 4)*4
    if header < 20 or header > len(segment):
        raise ValueError("invalid TCP header")
    sport, dport, seq = struct.unpack_from(">HHI", segment)
    return (src, dst, sport, dport), seq, segment[13], segment[header:]


def report(data: bytes, port: int = 6030) -> dict:
    if not 1 <= port <= 65535:
        raise ValueError("invalid receiver screen port")
    flows = {}
    for stamp, frame in packets(data):
        item = tcp(frame)
        if not item:
            continue
        flow, seq, flags, payload = item
        if flow[3] != port:
            continue
        if flow not in flows:
            if len(flows) >= 8:
                raise ValueError("too many screen TCP connections")
            flows[flow] = {"origin": None, "segments": [], "syn": False, "previous_stamp": None}
        state = flows[flow]
        # The bridge combines buffered BPF receive timestamps with immediate
        # transmit timestamps. Their global order can differ; only regressions
        # within the measured sender-to-screen flow invalidate its pacing.
        if state["previous_stamp"] is not None and stamp < state["previous_stamp"]:
            raise ValueError("screen TCP flow clock moved backwards")
        state["previous_stamp"] = stamp
        if flags & 2:
            if state["syn"] and state["origin"] != (seq+1) & 0xffffffff:
                raise ValueError("reused TCP tuple; split captures by connection")
            state["origin"], state["syn"] = (seq+1) & 0xffffffff, True
            seq = (seq+1) & 0xffffffff
        if payload:
            if len(state["segments"]) >= 65536:
                raise ValueError("too many screen TCP segments")
            state["segments"].append((seq, payload, stamp))
    results = []
    allocated = 0
    for state in flows.values():
        if not state["segments"]:
            continue
        if state["origin"] is None:
            raise ValueError("screen capture must include the TCP SYN")
        origin = state["origin"]
        maximum = max(((seq-origin) & 0xffffffff)+len(payload) for seq, payload, _ in state["segments"])
        allocated += maximum
        if allocated > LIMIT:
            raise ValueError("TCP sequence gap exceeds capture bound")
        stream, present = bytearray(maximum), bytearray(maximum)
        first_arrival = array("I", [0])*maximum
        arrival_times, repeated = [], 0
        for ordinal, (seq, payload, stamp) in enumerate(state["segments"], 1):
            arrival_times.append(stamp)
            start = (seq-origin) & 0xffffffff
            end = start+len(payload)
            for index, byte in enumerate(payload, start):
                if present[index]:
                    repeated += 1
                    if stream[index] != byte:
                        raise ValueError("conflicting TCP overlap")
                else:
                    stream[index], present[index] = byte, 1
                    first_arrival[index] = ordinal
        contiguous = present.find(0)
        if contiguous == -1:
            contiguous = maximum
        at, pictures, opcodes, telemetry, delivered = 0, [], {}, [], 0
        while contiguous-at >= 128:
            if sum(opcodes.values()) >= 8192:
                raise ValueError("too many screen headers")
            size = int.from_bytes(stream[at:at+4], "little")
            opcode = stream[at+4]
            if size > 16*1024*1024 or opcode > 6:
                raise ValueError("invalid screen header")
            end = at+128+size
            if end > contiguous:
                break
            # For each byte, use its first observed arrival; duplicates must
            # not make an already-complete frame look late. Arrival of a frame
            # includes earlier stream bytes which TCP ordering held back.
            completion = arrival_times[max(first_arrival[at:end])-1]
            opcodes[str(opcode)] = opcodes.get(str(opcode), 0)+1
            delivered = max(delivered, completion)
            if opcode == 0:
                pictures.append(delivered)
            if opcode == 5:
                try:
                    value = plistlib.loads(bytes(stream[at+128:end]))
                    if isinstance(value, dict):
                        telemetry.append({name: value[name] for name in
                            ("sentFramesAvg", "queuedFramesAvg", "encoderCurrentFPS")
                            if name in value and type(value[name]) in (int, float)})
                except (ValueError, plistlib.InvalidFileException):
                    pass
            at = end
        results.append({"screen_port": port, "opcodes": opcodes,
                        "screen_access_units": intervals(pictures),
                        "repeated_tcp_payload_bytes": repeated,
                        "unparsed_contiguous_bytes": contiguous-at,
                        "bytes_after_sequence_gap": maximum-contiguous,
                        "telemetry": telemetry})
    return {"connections": results, "clock": "companion PCAP capture clock",
            "limitations": ["no cryptographic keys or decrypted video used",
                "TCP repeats and capture gaps do not establish network packet loss",
                "NCM Ethernet is not a 2.4 GHz RF measurement",
                "IPv6 extension headers and fragmented IPv4 are not parsed"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcap", type=Path)
    parser.add_argument("--port", type=int, default=6030)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--lab-dir", type=Path, default=Path(__file__).resolve().parents[2]/"build/ios-vm-lab")
    args = parser.parse_args()
    if args.pcap.stat().st_size > LIMIT:
        parser.error("PCAP exceeds 64 MiB")
    if args.output:
        build = Path(__file__).resolve().parents[2]/"build"
        lab = args.lab_dir.resolve()
        if not lab.is_relative_to(build.resolve()) or lab == build.resolve():
            parser.error("lab directory must be inside the owned build tree")
        if not args.output.resolve().is_relative_to(lab.resolve()) or args.output.suffix != ".json":
            parser.error("report must be JSON inside the owned lab")
    try:
        rendered = json.dumps(report(args.pcap.read_bytes(), args.port), indent=2)+"\n"
    except ValueError as error:
        if args.output:
            args.output.unlink(missing_ok=True)
        parser.error(str(error))
    if args.output:
        args.output.write_text(rendered)
    print(rendered, end="")


if __name__ == "__main__":
    main()
