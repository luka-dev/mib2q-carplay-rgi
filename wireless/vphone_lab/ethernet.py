"""Bridge a selected owned vphone USB NCM link to the owned QNX netdev."""
from __future__ import annotations

import argparse
import ctypes as c
import fcntl
import json
import os
import plistlib
import re
import select
import socket
import struct
import subprocess
import time

from . import lab
from wireless.lab_common import qnx_receiver

MAX_FRAME = 2048


def owned_interface(tree: list, udid: str, channel: str = "primary") -> dict:
    if channel not in ("primary", "auxiliary"):
        raise ValueError("expected an explicit owned NCM channel")
    if not re.fullmatch(r"0000FE01-[0-9A-F]{16}", udid):
        raise ValueError("expected research VM identity")
    matches = [d for d in tree if d.get("USB Serial Number", "").rstrip("\0") == udid.replace("-", "")]
    if len(matches) != 1 or matches[0].get("USB Product Name") != "iPhone Research Environment Virtual Machine" or \
            matches[0].get("idVendor") != 0x05AC or matches[0].get("idProduct") != 0x12A8:
        raise RuntimeError("expected exactly one owned virtual USB device")
    device = matches[0]
    data = [i for i in device.get("IORegistryEntryChildren", []) if i.get("IOObjectClass") == "IOUSBHostInterface"
            and i.get("IORegistryEntryName") == "NCM Data" and i.get("IORegistryEntryLocation") == ("2" if channel == "primary" else "4")]
    if len(data) != 1:
        raise RuntimeError("selected owned NCM interface is not available")
    def interfaces(node):
        found = [node] if node.get("IOObjectClass") == "IOEthernetInterface" else []
        for child in node.get("IORegistryEntryChildren", []):
            found.extend(interfaces(child))
        return found
    rows = interfaces(data[0])
    if len(rows) != 1 or not re.fullmatch(r"en[0-9]+", rows[0].get("IORegistryEntryName", "")):
        raise RuntimeError("selected owned NCM has no unique Ethernet interface")
    return {"udid": udid, "channel": channel, "interface": rows[0]["IORegistryEntryName"],
            "usb_registry_id": device["IORegistryEntryID"], "interface_registry_id": rows[0]["IORegistryEntryID"]}


def discovery(udid: str, channel: str = "primary") -> dict:
    tree = plistlib.loads(subprocess.run(["ioreg", "-a", "-r", "-c", "IOUSBHostDevice"],
                                        capture_output=True, check=True, timeout=10).stdout)
    return owned_interface(tree, udid, channel)


def frames(buffer: bytearray) -> list[bytes]:
    result = []
    while len(buffer) >= 4:
        size = struct.unpack_from(">I", buffer)[0]
        if not 14 <= size <= MAX_FRAME:
            raise ValueError("invalid QNX Ethernet record size")
        if len(buffer) < size + 4:
            break
        result.append(bytes(buffer[4:4 + size]))
        del buffer[:4 + size]
    return result


class Timeval(c.Structure):
    # Darwin has a 32-bit suseconds_t followed by alignment padding.
    _fields_ = [("seconds", c.c_long), ("microseconds", c.c_int32)]


class Header(c.Structure):
    _fields_ = [("timestamp", Timeval), ("captured", c.c_uint32), ("length", c.c_uint32)]


class Capture:
    def __init__(self, interface: str):
        self.lib = c.CDLL("/usr/lib/libpcap.A.dylib")
        signatures = {
            "pcap_open_live": (c.c_void_p, [c.c_char_p, c.c_int, c.c_int, c.c_int, c.c_char_p]),
            "pcap_setdirection": (c.c_int, [c.c_void_p, c.c_int]),
            "pcap_setnonblock": (c.c_int, [c.c_void_p, c.c_int, c.c_char_p]),
            "pcap_get_selectable_fd": (c.c_int, [c.c_void_p]),
            "pcap_datalink": (c.c_int, [c.c_void_p]),
            "pcap_next_ex": (c.c_int, [c.c_void_p, c.POINTER(c.POINTER(Header)), c.POINTER(c.POINTER(c.c_ubyte))]),
            "pcap_inject": (c.c_int, [c.c_void_p, c.c_void_p, c.c_size_t]),
            "pcap_geterr": (c.c_char_p, [c.c_void_p]),
            "pcap_close": (None, [c.c_void_p]),
        }
        for name, (returns, args) in signatures.items():
            function = getattr(self.lib, name); function.restype = returns; function.argtypes = args
        error = c.create_string_buffer(256)
        self.handle = self.lib.pcap_open_live(interface.encode(), MAX_FRAME, 0, 50, error)
        if not self.handle:
            raise RuntimeError(error.value.decode())
        try:
            if self.lib.pcap_datalink(self.handle) != 1:
                raise RuntimeError("owned NCM capture is not Ethernet")
            # Do not reflect frames injected from QNX back into its socket.
            if self.lib.pcap_setdirection(self.handle, 1) or self.lib.pcap_setnonblock(self.handle, 1, error):
                raise RuntimeError("cannot isolate incoming owned NCM frames")
            self.fd = self.lib.pcap_get_selectable_fd(self.handle)
            if self.fd < 0:
                raise RuntimeError("owned capture is not selectable")
        except Exception:
            self.close(); raise

    def receive(self):
        header = c.POINTER(Header)(); data = c.POINTER(c.c_ubyte)()
        status = self.lib.pcap_next_ex(self.handle, c.byref(header), c.byref(data))
        if status == 0:
            return None
        if status != 1:
            raise RuntimeError("owned NCM capture failed")
        h = header.contents
        if not 14 <= h.length <= MAX_FRAME or h.captured != h.length:
            raise ValueError("truncated or oversized NCM frame")
        if not 0 <= h.timestamp.microseconds < 1000000:
            raise ValueError("invalid host PCAP timestamp")
        return h.timestamp.seconds + h.timestamp.microseconds / 1e6, c.string_at(data, h.length)

    def send(self, frame):
        if self.lib.pcap_inject(self.handle, frame, len(frame)) != len(frame):
            raise RuntimeError("short NCM injection")

    def close(self):
        if self.handle:
            self.lib.pcap_close(self.handle); self.handle = None


def _run(seconds: int, channel: str = "primary") -> dict:
    if type(seconds) is not int or not 1 <= seconds <= 120 or channel not in ("primary", "auxiliary"):
        raise ValueError("bounded link duration is 1..120 seconds")
    # Root is needed for BPF only. No Launchpad command is run as root.
    recorded = json.loads((lab.OUT / "machine.json").read_text())
    if recorded.get("name") != lab.profile()["machine"] or recorded.get("bundle") != lab.profile()["bundle"]:
        raise RuntimeError("machine ledger does not match the laboratory profile")
    if os.geteuid() != 0:
        lab.require_machine(lab.profile(), "running")
    owned = discovery(recorded["udid"], channel)
    qnx_receiver.set_output(lab.OUT / "qnx-network")
    pid = qnx_receiver.running()
    if not pid or (qnx_receiver.OUT / "transport.txt").read_text().strip() != "socket":
        raise RuntimeError("owned QNX socket receiver must already be running")
    capture = Capture(owned["interface"])
    started = time.monotonic()
    result = {**owned, "qnx_pid": pid, "rx_frames": 0, "tx_frames": 0, "seconds": seconds,
              "complete": False, "pcap_clock": "host realtime"}
    buffer = bytearray()
    try:
        with socket.create_connection(("127.0.0.1", 32501), timeout=2) as peer, \
                open(os.open(lab.OUT / "ethernet.pcap", os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "wb") as pcap:
            os.chmod(lab.OUT / "ethernet.pcap", 0o600)
            pcap.write(struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, MAX_FRAME, 1))
            def record(frame, stamp, direction):
                pcap.write(struct.pack("<IIII", int(stamp), int((stamp % 1) * 1e6), len(frame), len(frame)) + frame)
                pcap.flush(); result[direction + "_frames"] += 1
            print(json.dumps({"ethernet_ready": True, **owned}), flush=True)
            end = time.monotonic() + seconds
            check = time.monotonic() + 5
            buffered_capture = False
            while time.monotonic() < end:
                if time.monotonic() >= check:
                    if discovery(recorded["udid"], channel) != owned or qnx_receiver.running() != pid:
                        raise RuntimeError("owned endpoints changed during capture")
                    print(json.dumps(result), flush=True); check = time.monotonic() + 5
                ready = select.select([capture.fd, peer], [], [], 0 if buffered_capture else .1)[0]
                if capture.fd in ready or buffered_capture:
                    buffered_capture = False
                    for _ in range(64):
                        item = capture.receive()
                        if item is None or time.monotonic() >= end:
                            break
                        stamp, frame = item
                        peer.sendall(struct.pack(">I", len(frame)) + frame); record(frame, stamp, "rx")
                    else:
                        # libpcap can have buffered packets after the BPF fd
                        # stops being readable. Drain them without a wait.
                        buffered_capture = True
                if peer in ready:
                    part = peer.recv(8192)
                    if not part:
                        raise EOFError("owned QNX Ethernet socket closed")
                    buffer.extend(part)
                    for frame in frames(buffer):
                        capture.send(frame); record(frame, time.time(), "tx")
            if buffer:
                raise ValueError("incomplete QNX frame at capture deadline")
            result["complete"] = True
    except BaseException as error:
        result["error"] = type(error).__name__ + ": " + str(error)
        raise
    finally:
        capture.close()
        result["elapsed_seconds"] = time.monotonic() - started
        lab.save("ethernet.json", result)
        # Leave artifacts usable by the invoking workspace owner after sudo.
        if os.geteuid() == 0 and os.environ.get("SUDO_UID") and os.environ.get("SUDO_GID"):
            for name in ("ethernet.json", "ethernet.pcap"):
                file = lab.OUT / name
                if file.exists():
                    os.chown(file, int(os.environ["SUDO_UID"]), int(os.environ["SUDO_GID"]))
    return result


def run(seconds: int, channel: str = "primary") -> dict:
    with open(os.open(lab.OUT / ".ethernet.lock", os.O_RDWR | os.O_CREAT, 0o600), "r+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return _run(seconds, channel)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--inspect", action="store_true")
    parser.add_argument("--channel", choices=("primary", "auxiliary"), default="primary")
    args = parser.parse_args()
    if args.inspect:
        machine = lab.require_machine(lab.profile(), "running")
        print(json.dumps(discovery(machine["udid"], args.channel), indent=2))
    else:
        print(json.dumps(run(args.seconds, args.channel), indent=2))
