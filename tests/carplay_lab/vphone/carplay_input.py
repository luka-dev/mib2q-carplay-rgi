"""Tap the native CarPlay screen only in the explicitly enabled owned QNX lab."""
import argparse
import json
import socket
import struct
import hashlib
import time
import tempfile
from pathlib import Path

from tests.carplay_lab.common import qnx_receiver
from . import lab
from tests.carplay_lab.common.frame_pacing import intervals


def request(x: int, y: int) -> bytes:
    if type(x) is not int or type(y) is not int or not 0 <= x < 800 or not 0 <= y < 480:
        raise ValueError("CarPlay coordinates must be integer pixels inside 800x480")
    return b"MHT" + struct.pack(">HH", x, y)


def receiver(capability: str = "input_control") -> int:
    lab.require_machine(lab.profile(), "running")
    qnx_receiver.set_output(lab.OUT / "qnx-network")
    pid = qnx_receiver.running()
    record = json.loads((qnx_receiver.OUT / "build-record.json").read_text())
    if not pid or record.get(capability) is not True or record.get("input_control") is not True or record.get("software_mfi") is not True:
        raise RuntimeError("owned QNX input-control receiver is not running")
    return pid


def receive(peer, length: int) -> bytes:
    response = bytearray()
    while len(response) < length:
        part = peer.recv(length-len(response))
        if not part:
            raise RuntimeError("incomplete QNX response")
        response.extend(part)
    return bytes(response)


def snapshot(channel: str, output: Path | None = None, fresh: bool = False) -> dict:
    if channel not in ("primary", "cluster"):
        raise ValueError("unknown CarPlay screen")
    target = (output or lab.OUT / ("memory-"+channel+".h264")).resolve()
    if not target.is_relative_to(lab.OUT.resolve()) or target.suffix != ".h264":
        raise ValueError("AVC output must remain inside the owned vphone lab")
    pid = receiver("capture_memory")
    request_ns = time.time_ns()
    with socket.create_connection(("127.0.0.1", 32503), timeout=3) as peer:
        peer.settimeout(10 if fresh else 3)
        command = (b"MFC" if channel == "cluster" else b"MFM") if fresh else (b"MHC" if channel == "cluster" else b"MHM")
        peer.sendall(command + bytes(4))
        length, = struct.unpack(">I", receive(peer, 4))
        if not 1 <= length <= 2*1024*1024:
            raise ValueError("no current bounded IDR capture available; try --fresh (requests a native keyframe)")
        data = receive(peer, length)
    if not data.startswith((b"\0\0\1", b"\0\0\0\1")):
        raise ValueError("native capture is not Annex B")
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=target.parent, prefix=".avc-", delete=False) as file:
        temporary = Path(file.name)
        try:
            file.write(data)
            file.close()
            temporary.replace(target)
        finally:
            temporary.unlink(missing_ok=True)
    result = {"channel": channel, "bytes": length, "sha256": hashlib.sha256(data).hexdigest(),
              "qnx_pid": pid, "output": str(target), "native_keyframe_requested": fresh,
              "host_request_ns": request_ns, "host_complete_ns": time.time_ns(), "independent_decode_verified": False}
    lab.save("memory-"+channel+".json", result)
    return result


def timing(channel: str) -> dict:
    if channel not in ("primary", "cluster"):
        raise ValueError("unknown CarPlay screen")
    pid = receiver("capture_memory")
    with socket.create_connection(("127.0.0.1", 32503), timeout=3) as peer:
        peer.settimeout(3)
        peer.sendall((b"MDC" if channel == "cluster" else b"MDM") + bytes(4))
        count, = struct.unpack(">I", receive(peer, 4))
        if not 0 <= count <= 8192:
            raise ValueError("decoder timing count outside bounds")
        data = receive(peer, count*8)
    stamps = [v[0] for v in struct.iter_unpack(">Q", data)]
    result = {"channel": channel, "qnx_pid": pid,
              "picture_input": intervals([(v-stamps[0])/1e6 for v in stamps]),
              "timestamps_ns": stamps, "capture_limit": 8192, "capture_limit_reached": count == 8192,
              "clock": "QNX CLOCK_MONOTONIC; rate not aligned to host",
              "limitations": ["decoder input only; no physical decode or presentation measured",
                              "first 8192 picture buffers; configuration buffers excluded"]}
    lab.save("memory-"+channel+"-timing.json", result)
    return result


def clock(samples: int = 6, interval: float = 1.0) -> dict:
    if type(samples) is not int or not 2 <= samples <= 20 or not .1 <= interval <= 2:
        raise ValueError("clock sample bounds: 2..20 samples, 0.1..2 seconds apart")
    pid = receiver()
    result = {"qnx_pid": pid, "samples": []}
    for i in range(samples):
        with socket.create_connection(("127.0.0.1",32503),timeout=3) as peer:
            peer.settimeout(3)
            before = time.monotonic_ns()
            peer.sendall(b"MCL"+bytes(4))
            values = struct.unpack(">QQQQQqQ", receive(peer,56))
            after = time.monotonic_ns()
        result["samples"].append(dict(zip(
            ("mono_ns","real_ns","cycles","declared_cycles_hz","timer_rate","timer_scale","nsec_inc"),values),
            host_before_ns=before, host_after_ns=after))
        if i+1 < samples: time.sleep(interval)
    a,b=result["samples"][0],result["samples"][-1]
    host = ((b["host_before_ns"]+b["host_after_ns"])-(a["host_before_ns"]+a["host_after_ns"]))/2
    if host <= 0 or b["mono_ns"] <= a["mono_ns"]:
        raise ValueError("clock sampling did not advance")
    result["guest_to_host_rate"] = (b["mono_ns"]-a["mono_ns"])/host
    minimum = b["host_before_ns"]-a["host_after_ns"]
    maximum = b["host_after_ns"]-a["host_before_ns"]
    elapsed = b["mono_ns"]-a["mono_ns"]
    lower, upper = elapsed/maximum, elapsed/minimum if minimum > 0 else None
    result["rate_bounds"] = {"lower":lower,"upper":upper}
    result["rate_estimate_reliable"] = upper is not None and (upper-lower)/result["guest_to_host_rate"] <= .1
    result["cycles_hz_observed"] = (b["cycles"]-a["cycles"])*1e9/host
    result["max_roundtrip_ms"] = max((s["host_after_ns"]-s["host_before_ns"])/1e6 for s in result["samples"])
    result["limitations"] = ["rate measured at bracket midpoints; asymmetric RPC delay affects estimate",
                             "clock offsets are bounded by host request/response, not exactly synchronized"]
    lab.save("qnx-clock.json",result)
    return result


def tap(x: int, y: int) -> dict:
    packet = request(x, y)
    pid = receiver()
    with socket.create_connection(("127.0.0.1", 32503), timeout=3) as peer:
        peer.settimeout(3)
        peer.sendall(packet)
        response = receive(peer, 4)
    status, = struct.unpack(">i", response)
    result = {"x": x, "y": y, "qnx_pid": pid, "native_hid_status": status,
              "visible_effect_verified": False}
    lab.save("carplay-input.json", result)
    if status:
        raise RuntimeError("native HID rejected the tap; inspect carplay-input.json")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    touch = sub.add_parser("tap")
    touch.add_argument("x", type=int)
    touch.add_argument("y", type=int)
    memory = sub.add_parser("snapshot")
    memory.add_argument("channel", choices=["primary", "cluster"])
    memory.add_argument("--output", type=Path)
    memory.add_argument("--fresh", action="store_true")
    timing_parser = sub.add_parser("timing")
    timing_parser.add_argument("channel", choices=["primary", "cluster"])
    clock_parser = sub.add_parser("clock")
    clock_parser.add_argument("--samples", type=int, default=6)
    clock_parser.add_argument("--interval", type=float, default=1)
    args = parser.parse_args()
    print(json.dumps(clock(args.samples,args.interval) if args.action == "clock" else tap(args.x, args.y) if args.action == "tap" else timing(args.channel) if args.action == "timing" else snapshot(args.channel, args.output, args.fresh)))
