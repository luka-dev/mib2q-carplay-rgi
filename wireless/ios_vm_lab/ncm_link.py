#!/usr/bin/env python3
"""Bounded userspace CDC-NCM/TAP link for the synthetic phone in ARM QEMU."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import select
import socket
import struct
import subprocess
import threading
import time

SERIAL = "000080301122334455667788"


def receive_ethernet(sock) -> bytes:
    def exact(count):
        data = bytearray()
        while len(data) < count:
            part = sock.recv(count - len(data))
            if not part:
                raise EOFError("QNX Ethernet socket closed")
            data.extend(part)
        return bytes(data)
    size = struct.unpack(">I", exact(4))[0]
    if not 14 <= size <= 1514:
        raise ValueError("invalid QEMU Ethernet frame length")
    return exact(size)


def encode_ntb(frame: bytes, sequence: int, divisor: int = 4,
               remainder: int = 0, alignment: int = 4) -> bytes:
    if (not 14 <= len(frame) <= 1514 or not 1 <= divisor <= 1024 or
            not 0 <= remainder < divisor or not 1 <= alignment <= 1024):
        raise ValueError("invalid NCM frame/alignment")
    offset = 12 + ((remainder - 14 - 12) % divisor)
    ndp = ((offset + len(frame) + alignment - 1) // alignment) * alignment
    length = ndp + 16
    header = struct.pack("<4sHHHH", b"NCMH", 12, sequence & 65535, length, ndp)
    table = struct.pack("<4sHHHHHH", b"NCM0", 16, 0, offset, len(frame), 0, 0)
    return header + bytes(offset - 12) + frame + bytes(ndp - offset - len(frame)) + table


def decode_ntb(data: bytes) -> list[bytes]:
    if len(data) < 12:
        raise ValueError("truncated NTB header")
    magic, header, _, length, ndp = struct.unpack_from("<4sHHHH", data)
    if magic != b"NCMH" or header != 12 or not 12 <= length <= len(data):
        raise ValueError("invalid NTB header")
    tables = []
    datagrams = []
    seen = set()
    while ndp:
        if ndp in seen or len(seen) >= 8 or ndp < 12 or ndp + 8 > length:
            raise ValueError("invalid NDP chain")
        seen.add(ndp)
        signature, size, following = struct.unpack_from("<4sHH", data, ndp)
        if signature != b"NCM0" or size < 12 or size % 4 or ndp + size > length:
            raise ValueError("invalid NDP table")
        tables.append((ndp, ndp + size))
        ended = False
        for pos in range(ndp + 8, ndp + size, 4):
            offset, count = struct.unpack_from("<HH", data, pos)
            if offset == count == 0:
                ended = True
                break
            if offset < 12 or not 14 <= count <= 1514 or offset + count > length or len(datagrams) >= 64:
                raise ValueError("invalid NCM datagram")
            datagrams.append((offset, offset + count))
        if not ended:
            raise ValueError("NDP terminator missing")
        ndp = following
    ranges = sorted(tables + datagrams)
    if any(a[1] > b[0] for a, b in zip(ranges, ranges[1:])):
        raise ValueError("overlapping NCM objects")
    return [data[start:end] for start, end in datagrams]


def synthetic_phone():
    import usb.core
    import usb.util
    if b"linux,dummy-virt" not in Path("/proc/device-tree/compatible").read_bytes().split(b"\0"):
        raise RuntimeError("ARM QEMU companion required")
    devices = list(usb.core.find(find_all=True, idVendor=0x05AC, idProduct=0x12A8))
    if len(devices) != 1:
        raise RuntimeError("expected one synthetic phone")
    phone = devices[0]
    if usb.util.get_string(phone, phone.iSerialNumber).rstrip("\0").replace("-", "") != SERIAL:
        raise RuntimeError("unexpected phone identity")
    return phone


def run(seconds: int, prepare: bool, qnx_socket: bool = False) -> dict:
    import fcntl
    import usb.core
    import usb.util
    phone = synthetic_phone()
    if prepare:
        raise RuntimeError("use the explicit combined USB guest profile; mode switching is unsupported")
    configuration = phone.get_active_configuration()
    controls = [i for i in configuration if i.bInterfaceClass == 2 and i.bInterfaceSubClass == 13]
    if len(controls) != 1:
        raise RuntimeError("active USB configuration has no unique NCM control interface")
    control = controls[0]
    data = next(i for i in configuration if i.bInterfaceClass == 10 and i.bAlternateSetting == 1)
    incoming = next(e.bEndpointAddress for e in data if e.bEndpointAddress & 128)
    outgoing = next(e.bEndpointAddress for e in data if not e.bEndpointAddress & 128)
    notification = next(e.bEndpointAddress for e in control if e.bEndpointAddress & 128)
    claimed = []
    fd = None
    reader = None
    peer = None
    stop = threading.Event()
    result = {"configuration": configuration.bConfigurationValue, "rx_frames": 0, "tx_frames": 0,
              "errors": [], "sample_headers": []}
    pcap = None
    lock = threading.Lock()
    def capture(frame, direction):
        now = time.time()
        with lock:
            pcap.write(struct.pack("<IIII", int(now), int((now % 1) * 1000000), len(frame), len(frame)) + frame)
            pcap.flush()
            result[f"{direction}_frames"] += 1
            if len(result["sample_headers"]) < 16:
                result["sample_headers"].append({"direction": direction, "ethernet": frame[:14].hex(), "bytes": len(frame)})
    try:
        for number in (control.bInterfaceNumber, data.bInterfaceNumber):
            if phone.is_kernel_driver_active(number):
                phone.detach_kernel_driver(number)
            usb.util.claim_interface(phone, number)
            claimed.append(number)
        # A rejected concurrent probe must not truncate the active capture.
        pcap = open("/tmp/mhi2-ncm.pcap", "wb")
        pcap.write(struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))
        params = bytes(phone.ctrl_transfer(0xA1, 0x80, 0, control.bInterfaceNumber, 28, timeout=3000))
        if len(params) != 28:
            raise ValueError("invalid NTB parameters")
        values = struct.unpack("<HHIHHHHIHHHH", params)
        if values[0] != 28 or not values[1] & 1 or values[2] < 2048 or values[7] < 2048:
            raise ValueError("NTB16 is unavailable")
        result["ntb_parameters"] = list(values)
        print(json.dumps({"ntb_parameters": list(values)}), flush=True)
        phone.ctrl_transfer(0x21, 0x84, 0, control.bInterfaceNumber, b"", timeout=3000)
        input_size = min(16384, values[2])
        phone.ctrl_transfer(0x21, 0x86, 0, control.bInterfaceNumber, struct.pack("<I", input_size), timeout=3000)
        phone.ctrl_transfer(0x21, 0x8A, 0, control.bInterfaceNumber, b"", timeout=3000)
        phone.set_interface_altsetting(interface=data.bInterfaceNumber, alternate_setting=1)
        phone.ctrl_transfer(0x21, 0x43, 15, control.bInterfaceNumber, b"", timeout=3000)
        result["notifications"] = []
        for _ in range(4):
            try:
                notice = bytes(phone.read(notification, 16, timeout=250))
                result["notifications"].append(notice.hex())
                print(json.dumps({"ncm_notification": notice.hex()}), flush=True)
            except usb.core.USBTimeoutError:
                break
        if qnx_socket:
            # Fixed companion slirp host alias and an explicitly local QNX netdev.
            peer = socket.create_connection(("10.0.2.2", 32501), timeout=3)
            fd = peer.fileno()
            result["transport"] = "qnx-ethernet"
            print(json.dumps({"ncm_ready": True, "transport": "qnx-ethernet",
                              "receiver_ipv6": "fe80::4254:ff:fe12:3456"}), flush=True)
        else:
            fd = os.open("/dev/net/tun", os.O_RDWR | os.O_NONBLOCK)
            fcntl.ioctl(fd, 0x400454CA, struct.pack("16sH", b"mhi2ncm0", 0x1002) + bytes(22))
            subprocess.run(["ip", "link", "set", "mhi2ncm0", "address", "02:00:00:00:00:01"], check=True)
            subprocess.run(["ip", "link", "set", "mhi2ncm0", "up"], check=True)
            subprocess.run(["ip", "-6", "address", "add", "fe80::2/64", "dev", "mhi2ncm0"], check=True)
            result["transport"] = "tap"
            print(json.dumps({"ncm_ready": True, "interface": "mhi2ncm0", "ipv6": "fe80::2"}), flush=True)
        def receive():
            while not stop.is_set():
                try:
                    block = bytes(phone.read(incoming, input_size, timeout=100))
                    for frame in decode_ntb(block):
                        capture(frame, "rx")
                        if peer is not None:
                            peer.sendall(struct.pack(">I", len(frame)) + frame)
                        else:
                            os.write(fd, frame)
                except usb.core.USBTimeoutError:
                    pass
                except Exception as exc:
                    result["errors"].append(str(exc))
                    stop.set()
        reader = threading.Thread(target=receive)
        reader.start()
        end = time.monotonic() + seconds
        sequence = 0
        while time.monotonic() < end and not stop.is_set():
            if not select.select([fd], [], [], .1)[0]:
                continue
            frame = receive_ethernet(peer) if peer is not None else os.read(fd, 1514)
            block = encode_ntb(frame, sequence, values[8], values[9], values[10])
            result["last_tx_ntb"] = block.hex()
            if len(block) > values[7] or phone.write(outgoing, block, timeout=3000) != len(block):
                raise RuntimeError("short/oversized NCM write")
            capture(frame, "tx")
            sequence += 1
        stop.set()
        reader.join(timeout=1)
        if peer is None:
            result["addresses"] = subprocess.check_output(["ip", "-6", "address", "show", "dev", "mhi2ncm0"], text=True)
    except Exception as exc:
        result["errors"].append(f"{type(exc).__name__}: {exc}")
    finally:
        stop.set()
        if reader is not None:
            reader.join(timeout=5)
            if reader.is_alive():
                raise RuntimeError("NCM reader failed to stop; resources retained")
        if peer is not None:
            peer.close()
        elif fd is not None:
            os.close(fd)
        if data.bInterfaceNumber in claimed:
            try:
                phone.set_interface_altsetting(interface=data.bInterfaceNumber, alternate_setting=0)
            except usb.core.USBError as exc:
                result["errors"].append(f"disable NCM data: {exc}")
        for number in reversed(claimed):
            try:
                usb.util.release_interface(phone, number)
            except usb.core.USBError as exc:
                result["errors"].append(f"release {number}: {exc}")
        usb.util.dispose_resources(phone)
        if pcap is not None:
            pcap.close()
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=int, default=20)
    parser.add_argument("--prepare", action="store_true")
    parser.add_argument("--qnx-socket", action="store_true", help="forward raw Ethernet to the local QNX socket netdev")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error("seconds must be between 1 and 300")
    result = run(args.seconds, args.prepare, args.qnx_socket)
    print(json.dumps(result, indent=2))
    raise SystemExit(bool(result["errors"]))
