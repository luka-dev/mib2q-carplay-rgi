"""Byte-verified guest USB transport experiments through VZ's own GDB stub."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import socket
import struct
import subprocess
from pathlib import Path

from . import lab

PORT = 60321
IMAGE_SHA256 = "86895a2eebeb7fcfb68bcd60f2d7e685687faea07ee31166fb58c872b38e0d92"
ADDRESS = 0xFFFFFE00080EDE14
REGION = bytes.fromhex(
    "970080524000a05293430094f50300aa801e42f9100040f9f10300aa31b4f9f2"
    "301ac1da080e4af8e10315aa0200805230fff2f210093fd7350100b4b00240f9"
    "f10315aa31b4f9f2301ac1da088e42f8e00315aaf050e7f210093fd7f7060071"
    "21fdff54801a42f9100040f9f10300aa31b4f9f2301ac1da11b980d21002118b"
    "080240f981008052b0edeaf210093fd7c0010036")
VENDOR_BULK = (
    (0xFFFFFE000859148C, bytes.fromhex("61008052"), bytes.fromhex("e11f8052")),
    (0xFFFFFE0008591490, bytes.fromhex("02008052"), bytes.fromhex("e21f8052")),
    (0xFFFFFE0008591494, bytes.fromhex("03008052"), bytes.fromhex("e31f8052")),
    (0xFFFFFE000859151C, bytes.fromhex("61008052"), bytes.fromhex("41008052")),
)


def relocated(word: int, old: int, new: int, start: int, end: int) -> int:
    """Relocate the selected block's ARM64 PC-relative calls and branches."""
    if word & 0x7C000000 == 0x14000000:
        bits, shift = 26, 0
    elif word & 0x7E000000 == 0x34000000 or word & 0xFF000010 == 0x54000000:
        bits, shift = 19, 5
    elif word & 0x7E000000 == 0x36000000:
        bits, shift = 14, 5
    else:
        return word
    mask = (1 << bits) - 1
    immediate = (word >> shift) & mask
    signed = immediate - (1 << bits) if immediate & (1 << (bits - 1)) else immediate
    target = old + signed * 4
    if start <= target < end:
        return word  # All internal targets move with their block.
    delta = target - new
    if delta % 4 or not -(1 << (bits - 1)) <= delta // 4 < (1 << (bits - 1)):
        raise ValueError("relocated branch is out of range")
    return (word & ~(mask << shift)) | ((delta // 4 & mask) << shift)


def patch(region: bytes) -> bytes:
    if region != REGION:
        raise ValueError("USB initialization bytes differ from the reviewed research kernel")
    result = bytearray()
    # Original A: four buffers; B: DRIVER_OK and its original failure branch.
    # Both queues and EP0 already exist. Reorder B then A, keeping every check.
    for offset, size, new_offset in ((100, 48, 0), (0, 100, 48)):
        for at in range(offset, offset + size, 4):
            word = struct.unpack_from("<I", region, at)[0]
            result += struct.pack("<I", relocated(word, ADDRESS + at, ADDRESS + new_offset + at - offset,
                                                   ADDRESS + offset, ADDRESS + offset + size))
    return bytes(result)


class Remote:
    def __init__(self):
        self.socket = socket.create_connection(("127.0.0.1", PORT), timeout=5)
        self.socket.settimeout(5)

    def exact(self, size: int) -> bytes:
        result = bytearray()
        while len(result) < size:
            chunk = self.socket.recv(size - len(result))
            if not chunk:
                raise EOFError("truncated guest GDB response")
            result.extend(chunk)
        return bytes(result)

    def receive(self) -> bytes:
        while True:
            ch = self.socket.recv(1)
            if not ch:
                raise EOFError("guest GDB connection closed")
            if ch == b"$":
                break
        wire = bytearray()
        while True:
            ch = self.socket.recv(1)
            if not ch:
                raise EOFError("truncated guest GDB packet")
            if ch == b"#":
                break
            wire.extend(ch)
            if len(wire) > 65536:
                raise ValueError("guest GDB reply too large")
        checksum = self.exact(2)
        if len(checksum) != 2 or int(checksum, 16) != sum(wire) % 256:
            raise ValueError("guest GDB checksum mismatch")
        self.socket.sendall(b"+")
        decoded = bytearray(); index = 0
        while index < len(wire):
            if wire[index] == 125:
                index += 1
                if index >= len(wire):
                    raise ValueError("truncated GDB escape")
                decoded.append(wire[index] ^ 32)
            else:
                decoded.append(wire[index])
            index += 1
        return bytes(decoded)

    def command(self, payload: bytes) -> bytes:
        wire = bytearray()
        for byte in payload:
            if byte in b"$#}*":
                wire.extend((125, byte ^ 32))
            else:
                wire.append(byte)
        self.socket.sendall(b"$" + wire + b"#" + f"{sum(wire) % 256:02x}".encode())
        return self.receive()

    def read(self, address: int, size: int) -> bytes:
        value = self.command(f"x{address:x},{size:x}".encode())
        if len(value) != size:
            raise RuntimeError("guest GDB returned no complete memory read")
        return value

    def write(self, address: int, data: bytes) -> bytes:
        reply = self.command(f"X{address:x},{len(data):x}:".encode() + data)
        if reply == b"":  # VZ can expose only the classic hex write packet.
            reply = self.command(f"M{address:x},{len(data):x}:".encode() + data.hex().encode())
        return reply


def endpoint_owner(machine: dict) -> int:
    output = subprocess.run(["lsof", "-nP", f"-iTCP:{PORT}", "-sTCP:LISTEN", "-Fpn"],
                            capture_output=True, text=True, check=True).stdout.splitlines()
    pids = [int(line[1:]) for line in output if line.startswith("p")]
    names = [line[1:] for line in output if line.startswith("n")]
    if len(pids) != 1 or names != [f"127.0.0.1:{PORT}"]:
        raise RuntimeError("guest debug listener must be unique and loopback only")
    disk = Path(machine["libraryRoot"]) / machine["name"] / "Disk.img"
    holders = subprocess.run(["lsof", "-a", "-p", str(pids[0]), "-Fp", str(disk)],
                             capture_output=True, text=True, check=True).stdout
    command = subprocess.run(["ps", "-p", str(pids[0]), "-o", "command="],
                             capture_output=True, text=True, check=True).stdout
    if f"p{pids[0]}\n" not in holders or "com.apple.Virtualization.VirtualMachine" not in command:
        raise RuntimeError("debug listener does not belong to this guest's VZ service")
    return pids[0]


def reference(machine: dict):
    base = lab.OUT / "usb-analysis"
    base.mkdir(parents=True, exist_ok=True)
    raw_path = base / "kernelcache.raw"
    if not raw_path.exists():
        source = (Path(machine['libraryRoot']) / machine['name'] / 'FirmwareOriginals' /
                  'iPhone17,3_26.6.2_23G90_Restore' / 'kernelcache.research.vphone600')
        lab.client('exec', '--bundle', machine['bundle'], 'fw', 'im4p-extract', str(source),
                   '--output', str(raw_path), timeout=120)
    raw = raw_path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != IMAGE_SHA256:
        raise ValueError("pristine research kernel fingerprint differs")
    sections = subprocess.run(['r2', '-q', '-c', 'iSj;q', str(raw_path)],
                              capture_output=True, text=True, check=True, timeout=30)
    mappings = json.loads(sections.stdout)
    return raw, mappings


def verify_sites(remote: Remote, sites, slide: int) -> bool:
    current = [remote.read(address + slide, len(original)) for address, original, _ in sites]
    originals = [original for _, original, _ in sites]
    replacements = [replacement for _, _, replacement in sites]
    if current not in (originals, replacements):
        raise RuntimeError("live USB instructions differ or have a partial patch")
    return current == replacements


def run(apply: bool = False, experiment: str = "vendor-bulk"):
    if experiment not in ("vendor-bulk", "ready-order"):
        raise ValueError("unknown guest USB experiment")
    settings = lab.profile(); machine = lab.require_machine(settings, "running")
    pid = endpoint_owner(machine)
    raw, sections = reference(machine)
    remote = Remote()
    result = {"udid": machine["udid"], "debug_pid": pid, "applied": False, "experiment": experiment}
    written = []
    try:
        remote.socket.sendall(b"\x03")
        stop = remote.receive()
        match = re.search(rb"20:([0-9a-f]{16})", stop)
        if not match:
            raise RuntimeError("guest debug stop has no ARM64 PC")
        pc = int.from_bytes(bytes.fromhex(match[1].decode()), "little")
        code = remote.read(pc, 64)
        at = raw.find(code)
        if at < 0 or raw.find(code, at + 1) >= 0:
            raise RuntimeError("live PC cannot identify a unique pristine kernel location")
        mapping = [s for s in sections if s["perm"] == "-r-x" and s["paddr"] <= at < s["paddr"] + s["size"]]
        if len(mapping) != 1:
            raise RuntimeError("live code has no unique executable mapping")
        original_pc = mapping[0]["vaddr"] + at - mapping[0]["paddr"]
        slide = pc - original_pc
        sites = VENDOR_BULK if experiment == "vendor-bulk" else ((ADDRESS, REGION, patch(REGION)),)
        already_applied = verify_sites(remote, sites, slide)
        result['already_applied'] = already_applied
        result.update(slide=hex(slide), sites=[{"address": hex(a + slide), "original": o.hex(),
                                             "replacement": n.hex()} for a, o, n in sites])
        if apply and not already_applied:
            # Recheck process ownership immediately before a write while all
            # guest CPUs are stopped. Never change the host or disk image.
            if endpoint_owner(machine) != pid:
                raise RuntimeError("debug endpoint ownership changed")
            for address, original, replacement in sites:
                address += slide
                written.append((address, original))
                reply = remote.write(address, replacement)
                if reply != b"OK":
                    raise RuntimeError("guest GDB refused the code write: " + repr(reply))
                if remote.read(address, len(replacement)) != replacement:
                    raise RuntimeError("guest patch readback differed")
            result["applied"] = True
        lab.save("usb-kernel.json", result)
        return result
    except BaseException:
        for address, original in reversed(written):
            remote.write(address, original)
        raise
    finally:
        try:
            remote.command(b"D")
        finally:
            remote.socket.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="apply the reviewed RAM-only guest experiment")
    parser.add_argument("--experiment", choices=("vendor-bulk", "ready-order"), default="vendor-bulk")
    args = parser.parse_args()
    print(json.dumps(run(args.apply, args.experiment), indent=2))
