#!/usr/bin/env python3
"""Probe the synthetic iPhone from inside the ARM Linux QEMU companion."""
from __future__ import annotations

import argparse
import json
import hashlib
from pathlib import Path
import subprocess
import struct
import select
import sys
import time

SERIAL = "00008030-1122334455667788"
IAP2_DETECT = bytes.fromhex("ff550200ee10")
MAX_REPORT_BYTES = 1024
CERTIFICATE_SHA256 = "634a93dd6c4338080524025411752597ecb828591839cd062a8292fd7e9e844e"


def normalized_serial(serial: str) -> str:
    return serial.rstrip("\0").replace("-", "")


def output_reports(descriptor: bytes) -> list[tuple[int, int]]:
    """Read bounded byte-aligned Output reports from a short-item HID descriptor."""
    if not descriptor or len(descriptor) > 4096:
        raise ValueError("invalid descriptor size")
    pos = 0
    report_id = report_count = report_size = 0
    reports = {}
    while pos < len(descriptor):
        prefix = descriptor[pos]
        pos += 1
        if prefix == 0xFE:
            raise ValueError("long HID items are unsupported")
        size = (0, 1, 2, 4)[prefix & 3]
        if pos + size > len(descriptor):
            raise ValueError("truncated HID item")
        value = int.from_bytes(descriptor[pos:pos + size], "little")
        pos += size
        tag = prefix & 0xFC
        if tag == 0x74:
            report_size = value
        elif tag == 0x94:
            report_count = value
        elif tag == 0x84:
            if not 1 <= value <= 255:
                raise ValueError("invalid report ID")
            report_id = value
        elif tag in (0xA4, 0xB4):
            raise ValueError("HID global push/pop are unsupported")
        elif tag == 0x90:
            if report_size != 8 or not report_id or not 2 <= report_count <= MAX_REPORT_BYTES:
                raise ValueError("unsupported Output report layout")
            if report_id in reports:
                raise ValueError("split Output reports are unsupported")
            reports[report_id] = report_count
    if not reports:
        raise ValueError("descriptor has no Output reports")
    return sorted(reports.items(), key=lambda item: (item[1], item[0]))


def detect_report(reports: list[tuple[int, int]]) -> bytes:
    """Report ID + link-control byte + iAP2 detect marker + zero padding."""
    candidates = [(size, rid) for rid, size in reports
                  if 1 <= rid <= 255 and len(IAP2_DETECT) + 1 <= size <= MAX_REPORT_BYTES]
    if not candidates:
        raise ValueError("no report can hold the iAP2 detect marker")
    size, rid = min(candidates)
    return bytes((rid, 0)) + IAP2_DETECT + bytes(size - 1 - len(IAP2_DETECT))


def hid_reports(packet: bytes, reports: list[tuple[int, int]]) -> list[bytes]:
    if not packet or len(packet) > 8192:
        raise ValueError("invalid link packet size")
    sizes = sorted((size, rid) for rid, size in reports
                   if 1 <= rid <= 255 and 2 <= size <= MAX_REPORT_BYTES)
    if not sizes:
        raise ValueError("no supported Output report")
    result = []
    while packet:
        size, rid = next((item for item in sizes if item[0] - 1 >= len(packet)), sizes[-1])
        count = min(size - 1, len(packet))
        control = int(bool(result)) | (2 if count < len(packet) else 0)
        result.append(bytes((rid, control)) + packet[:count] + bytes(size - 1 - count))
        packet = packet[count:]
    return result


def link_frame(control: int, sequence: int, acknowledgement: int = 0,
               session: int = 0, payload: bytes = b"") -> bytes:
    if len(payload) > 4096:
        raise ValueError("link payload too large")
    header = struct.pack(">HHBBBB", 0xFF5A, len(payload) + (10 if payload else 9),
                         control, sequence, acknowledgement, session)
    frame = header + bytes((-sum(header) & 255,))
    return frame + payload + bytes((-sum(payload) & 255,)) if payload else frame


def parse_link_frame(data: bytes) -> dict:
    if len(data) < 9 or data[:2] != b"\xff\x5a" or sum(data[:9]) & 255:
        raise ValueError("invalid iAP2 link header")
    length = int.from_bytes(data[2:4], "big")
    if not 9 <= length <= 4106 or length > len(data):
        raise ValueError("invalid iAP2 link length")
    if length == 10 or (length > 9 and sum(data[9:length]) & 255):
        raise ValueError("invalid iAP2 payload checksum")
    return {"control": data[4], "sequence": data[5], "acknowledgement": data[6],
            "session": data[7], "payload": data[9:length - 1] if length > 9 else b""}


def control_message(message: int, value: bytes | None = None) -> bytes:
    if value is not None and len(value) > 4090:
        raise ValueError("control parameter too large")
    parameters = struct.pack(">HH", len(value) + 4, 0) + value if value is not None else b""
    return struct.pack(">HHH", 0x4040, len(parameters) + 6, message) + parameters


def parameter(kind: int, value: bytes) -> bytes:
    if not 0 <= kind <= 65535 or len(value) > 4090:
        raise ValueError("invalid identification parameter")
    return struct.pack(">HH", len(value) + 4, kind) + value


def identification_message(carplay_interface: int = 2) -> bytes:
    """HID transport plus the planned wired CarPlay NCM interface."""
    params = bytearray()
    if not 0 <= carplay_interface <= 255:
        raise ValueError("invalid CarPlay USB interface number")
    for kind, value in enumerate(("MHI2 QEMU Lab", "MHI2-Test", "MHI2 CarPlay Lab",
                                  "MHI2-IOS-VM-1122", "1.0", "1.0")):
        params += parameter(kind, value.encode() + b"\0")
    for kind, ids in ((6, (0x4301,)),
                      (7, (0x4300,))):
        params += parameter(kind, b"".join(struct.pack(">H", item) for item in ids))
    params += parameter(8, b"\0") + parameter(9, b"\0\0")
    params += parameter(12, b"en\0") + parameter(13, b"en\0")
    device = parameter(0, b"\0\0") + parameter(1, b"USBDeviceTransport\0") + parameter(2, b"")
    params += parameter(15, device)
    host = parameter(0, b"\0\x01") + parameter(1, b"USBHostTransport\0") + parameter(2, b"")
    host += parameter(3, bytes((carplay_interface,))) + parameter(4, b"")
    params += parameter(16, host)
    return struct.pack(">HHH", 0x4040, len(params) + 6, 0x1D01) + params


def carplay_start_message(public_key: bytes | None = None, receiver_ipv6: str = "fe80::2") -> bytes:
    import ipaddress
    if not ipaddress.IPv6Address(receiver_ipv6).is_link_local:
        raise ValueError("expected a link-local receiver IPv6 address")
    wired = parameter(0, receiver_ipv6.encode() + b"\0")
    params = parameter(0, wired) + parameter(2, struct.pack(">I", 5000))
    params += parameter(3, b"02:00:00:00:00:01\0") + parameter(5, b"210.81\0")
    if public_key is not None:
        if len(public_key) != 32:
            raise ValueError("expected a 32-byte receiver Ed25519 public key")
        params += parameter(4, public_key.hex().encode() + b"\0")
    return struct.pack(">HHH", 0x4040, len(params) + 6, 0x4301) + params


def parse_control_message(body: bytes) -> tuple[int, dict[int, bytes]]:
    if len(body) < 6 or body[:2] != b"\x40\x40" or int.from_bytes(body[2:4], "big") != len(body):
        raise ValueError("invalid control message length")
    parameters = {}
    pos = 6
    while pos < len(body):
        if pos + 4 > len(body):
            raise ValueError("truncated control parameter")
        size, kind = struct.unpack_from(">HH", body, pos)
        if size < 4 or pos + size > len(body) or kind in parameters:
            raise ValueError("invalid control parameter")
        parameters[kind] = body[pos + 4:pos + size]
        pos += size
    return int.from_bytes(body[4:6], "big"), parameters


def sign_via_stdio(digest: bytes) -> bytes:
    if len(digest) != 32:
        raise ValueError("public P-256 fixture requires a 32-byte challenge")
    print(json.dumps({"sign_digest": digest.hex()}), flush=True)
    if not select.select([sys.stdin], [], [], 5)[0]:
        raise RuntimeError("host fixture signer deadline expired")
    response = json.loads(sys.stdin.readline(1024))
    signature = bytes.fromhex(response["signature"])
    if len(signature) != 64:
        raise ValueError("invalid raw P-256 signature size")
    return signature


def handshake(phone, reports: list[tuple[int, int]], certificate: bytes | None = None,
              progress: dict | None = None, identify: bool = False, start_carplay: bool = False,
              hid_interface: int = 2, hid_endpoint: int = 0x83,
              hold_seconds: int = 2, public_key: bytes | None = None,
              receiver_ipv6: str = "fe80::2", carplay_interface: int = 2,
              signer=None) -> dict:
    def send(packet):
        for report in hid_reports(packet, reports):
            count = phone.ctrl_transfer(0x21, 9, 0x0200 | report[0], hid_interface, report, timeout=3000)
            if count != len(report):
                raise RuntimeError("short HID write")

    def receive(timeout):
        buf = bytearray()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            report = bytes(phone.read(hid_endpoint, MAX_REPORT_BYTES + 1,
                                      timeout=max(1, int((deadline - time.monotonic()) * 1000))))
            if len(report) < 3 or report[1] & ~3:
                raise ValueError("invalid HID link-control byte")
            if not report[1] & 1:
                buf.clear()
            buf.extend(report[2:])
            if len(buf) > 8192:
                raise ValueError("HID receive buffer too large")
            if not report[1] & 2:
                return parse_link_frame(bytes(buf))
        raise RuntimeError("iAP2 receive deadline expired")

    sync = struct.pack(">BBHHHBB", 1, 8, 4096, 4000, 500, 3, 3) + bytes((10, 0, 1))
    send(link_frame(0x80, 1, payload=sync))
    peer = receive(5)
    if peer["control"] & 0xC0 != 0xC0 or peer["acknowledgement"] != 1:
        raise ValueError("expected SYN/ACK acknowledging our SYN")
    payload = peer["payload"]
    if len(payload) < 13 or (len(payload) - 10) % 3 or payload[0] != 1:
        raise ValueError("invalid peer synchronization parameters")
    sessions = [list(payload[i:i + 3]) for i in range(10, len(payload), 3)]
    if [10, 0, 1] not in sessions:
        raise ValueError("peer did not negotiate our control session")
    send(link_frame(0x40, 1, peer["sequence"]))
    result = progress if progress is not None else {}
    result.update(link_synchronized=True, peer_synack=peer | {"payload": payload.hex()},
                  peer_sessions=sessions)
    control = receive(5)
    result["first_packet"] = control | {"payload": control["payload"].hex()}
    body = control["payload"]
    if len(body) >= 6 and body[:2] == b"\x40\x40":
        result["first_control_message"] = hex(int.from_bytes(body[4:6], "big"))
    send(link_frame(0x40, 1, control["sequence"]))
    if certificate is not None:
        if result.get("first_control_message") != "0xaa00":
            raise ValueError("expected authentication certificate request")
        send(link_frame(0x40, 2, control["sequence"], 10, control_message(0xAA01, certificate)))
        sequence = 2
        messages = ["0xaa00"]
        result["authentication_messages"] = messages
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            frame = receive(min(5, deadline - time.monotonic()))
            if frame["control"] & 0x10:
                raise ValueError("peer reset the authentication link")
            if not frame["payload"]:
                continue
            if frame["session"] != 10:
                raise ValueError("unexpected authentication session")
            message, parameters = parse_control_message(frame["payload"])
            messages.append(hex(message))
            send(link_frame(0x40, sequence, frame["sequence"]))
            if message == 0xAA02:
                digest = parameters.get(0, b"")
                result["challenge_bytes"] = len(digest)
                signature = (signer or sign_via_stdio)(digest)
                sequence += 1
                send(link_frame(0x40, sequence, frame["sequence"], 10,
                                control_message(0xAA03, signature)))
                result["signature_bytes"] = len(signature)
            elif message in (0xAA04, 0xAA05):
                result["authentication_succeeded"] = message == 0xAA05
                result["authentication_failed"] = message == 0xAA04
                if message == 0xAA05 and identify:
                    result["identification_messages"] = []
                    deadline = time.monotonic() + 10
                    while time.monotonic() < deadline:
                        packet = receive(min(5, deadline - time.monotonic()))
                        if not packet["payload"]:
                            continue
                        mid, params = parse_control_message(packet["payload"])
                        result["identification_messages"].append(hex(mid))
                        send(link_frame(0x40, sequence, packet["sequence"]))
                        if mid == 0x1D00:
                            sequence += 1
                            identity = identification_message(carplay_interface)
                            send(link_frame(0x40, sequence, packet["sequence"], 10, identity))
                            result["identification_sent"] = identity.hex()
                        elif mid == 0x1D03:
                            result["identification_rejected_parameters"] = sorted(params)
                            result["identification_accepted"] = False
                            return result
                        elif mid == 0x1D02:
                            result["identification_accepted"] = True
                            result["post_identification_packets"] = []
                            import usb.core
                            try:
                                packet = receive(5)
                                result["post_identification_packets"].append(packet | {"payload": packet["payload"].hex()})
                                send(link_frame(0x40, sequence, packet["sequence"]))
                                if start_carplay and packet["payload"]:
                                    mid, attributes = parse_control_message(packet["payload"])
                                    if mid != 0x4300:
                                        raise ValueError("expected CarPlayAvailability")
                                    sequence += 1
                                    request = carplay_start_message(public_key, receiver_ipv6)
                                    send(link_frame(0x40, sequence, packet["sequence"], 10, request))
                                    result["carplay_start_sent"] = request.hex()
                                    starts_sent = 1
                                    last_start = time.monotonic()
                                    result["session_packets"] = []
                                    hold_end = time.monotonic() + hold_seconds
                                    while time.monotonic() < hold_end:
                                        try:
                                            follow = receive(min(1, hold_end - time.monotonic()))
                                        except (usb.core.USBTimeoutError, TimeoutError):
                                            continue
                                        if follow["payload"]:
                                            send(link_frame(0x40, sequence, follow["sequence"]))
                                            message, _ = parse_control_message(follow["payload"])
                                            if message == 0x4300 and starts_sent < 3 and time.monotonic() - last_start >= 1:
                                                sequence += 1
                                                send(link_frame(0x40, sequence, follow["sequence"], 10, request))
                                                starts_sent += 1
                                                last_start = time.monotonic()
                                                result["carplay_start_count"] = starts_sent
                                        if len(result["session_packets"]) < 64:
                                            result["session_packets"].append(follow | {"payload": follow["payload"].hex()})
                            except usb.core.USBError as exc:
                                result["post_identification_read_error"] = str(exc)
                            return result
                        else:
                            result.setdefault("other_identification_packets", []).append(packet | {"payload": packet["payload"].hex()})
                    raise RuntimeError("identification deadline expired")
                return result
            else:
                raise ValueError(f"unexpected authentication message {message:#x}")
        raise RuntimeError("authentication deadline expired")
    return result


def require_companion() -> None:
    path = Path("/proc/device-tree/compatible")
    if not path.is_file() or b"linux,dummy-virt" not in path.read_bytes().split(b"\0"):
        raise RuntimeError("run only inside the ARM QEMU companion")


def probe(action: str, certificate: bytes | None = None, *, hold_seconds: int = 2,
          public_key: bytes | None = None, receiver_ipv6: str = "fe80::2") -> dict:
    require_companion()
    import usb.core
    import usb.util

    phones = list(usb.core.find(find_all=True, idVendor=0x05AC, idProduct=0x12A8))
    if len(phones) != 1:
        raise RuntimeError("expected exactly one synthetic iPhone")
    phone = phones[0]
    if normalized_serial(usb.util.get_string(phone, phone.iSerialNumber)) != normalized_serial(SERIAL):
        usb.util.dispose_resources(phone)
        raise RuntimeError("unexpected USB serial")
    result = {"synthetic_serial": SERIAL, "action": action}
    stopped = claimed = False
    previous = None
    hid_interface = 2
    switched = False
    try:
        if action == "ncm":
            result["selector"] = 3
            result["response"] = bytes(phone.ctrl_transfer(0xC0, 0x52, 0, 3, 1, timeout=3000)).hex()
            # This request can re-enumerate USB. Its byte is not proof of a network session.
            return result
        previous = phone.get_active_configuration()
        if not any(i.bInterfaceClass == 255 and i.bInterfaceSubClass == 254 for i in previous):
            raise RuntimeError("start from a configuration containing the USBMUX interface")
        result["configuration_before"] = previous.bConfigurationValue
        combined = any(i.bInterfaceClass == 3 for i in previous) and any(
            i.bInterfaceClass == 2 and i.bInterfaceSubClass == 13 for i in previous)
        if not combined:
            subprocess.run(["pkill", "-x", "usbmuxd"], check=True)
            stopped = True
            time.sleep(0.3)
            phone.set_configuration(2)
            switched = True
        result["configuration_active"] = phone.get_active_configuration().bConfigurationValue
        # Linux can bind usbhid/audio as soon as configuration 2 appears.
        for interface in phone.get_active_configuration():
            if not combined or interface.bInterfaceClass == 3:
                if phone.is_kernel_driver_active(interface.bInterfaceNumber):
                    phone.detach_kernel_driver(interface.bInterfaceNumber)
        hids = [i for i in phone.get_active_configuration() if i.bInterfaceClass == 3]
        if len(hids) != 1:
            raise RuntimeError("expected one active iAP HID interface")
        hid = hids[0]
        hid_interface = hid.bInterfaceNumber
        endpoints = [e.bEndpointAddress for e in hid if e.bEndpointAddress & 128]
        if len(endpoints) != 1:
            raise RuntimeError("expected one HID interrupt input endpoint")
        hid_endpoint = endpoints[0]
        result.update(hid_interface=hid_interface, hid_endpoint=hid_endpoint, combined_usb=combined)
        carplay_interface = 2
        if combined:
            data_interfaces = {i.bInterfaceNumber for i in phone.get_active_configuration()
                               if i.bInterfaceClass == 10}
            if action in ("identify", "start-carplay") and len(data_interfaces) != 1:
                raise RuntimeError("expected one NCM data interface for CarPlay identification")
            if data_interfaces:
                carplay_interface = next(iter(data_interfaces))
        result["carplay_interface"] = carplay_interface
        usb.util.claim_interface(phone, hid_interface)
        claimed = True
        descriptor = bytes(phone.ctrl_transfer(0x81, 6, 0x2200, hid_interface, 208, timeout=3000))
        reports = output_reports(descriptor)
        result.update(report_descriptor=descriptor.hex(), output_reports=reports)
        if action in ("detect", "handshake", "authenticate", "identify", "start-carplay"):
            report = detect_report(reports)
            result["detect_report"] = report.hex()
            result["detect_write"] = phone.ctrl_transfer(0x21, 9, 0x0200 | report[0], hid_interface,
                                                        report, timeout=3000)
            try:
                reply = bytes(phone.read(hid_endpoint, MAX_REPORT_BYTES + 1, timeout=5000))
                result["interrupt_input"] = reply.hex()
                result["iap2_detect_echo"] = len(reply) >= 8 and reply[1] == 0 and reply[2:8] == IAP2_DETECT
            except usb.core.USBError as exc:
                result["interrupt_error"] = str(exc)
                result["iap2_detect_echo"] = False
            if action in ("handshake", "authenticate", "identify", "start-carplay") and result["iap2_detect_echo"]:
                handshake(phone, reports, certificate, result, identify=action in ("identify", "start-carplay"),
                          start_carplay=action == "start-carplay", hid_interface=hid_interface,
                          hid_endpoint=hid_endpoint, hold_seconds=hold_seconds, public_key=public_key,
                          receiver_ipv6=receiver_ipv6, carplay_interface=carplay_interface)
    except (RuntimeError, ValueError, usb.core.USBError, subprocess.SubprocessError) as exc:
        result["error"] = str(exc)
    finally:
        if claimed:
            try:
                usb.util.release_interface(phone, hid_interface)
            except usb.core.USBError as exc:
                result["release_error"] = str(exc)
        if stopped:
            try:
                if switched:
                    phone.set_configuration(previous.bConfigurationValue)
                result["configuration_restored"] = phone.get_active_configuration().bConfigurationValue
            except usb.core.USBError as exc:
                result["restore_error"] = str(exc)
        usb.util.dispose_resources(phone)
        if stopped:
            with open("/tmp/usbmuxd.log", "ab") as log:
                subprocess.Popen(["usbmuxd", "-f", "-v"], stdin=subprocess.DEVNULL,
                                 stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            result["usbmuxd_restarted"] = True
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("descriptor", "detect", "handshake", "authenticate", "identify", "start-carplay", "ncm"))
    parser.add_argument("--certificate", type=Path)
    parser.add_argument("--corrupt-certificate-offset", type=int)
    parser.add_argument("--hold-seconds", type=int, default=2)
    parser.add_argument("--receiver-public-key")
    parser.add_argument("--receiver-ipv6", default="fe80::2")
    args = parser.parse_args()
    if not 1 <= args.hold_seconds <= 300:
        parser.error("hold seconds must be between 1 and 300")
    try:
        public_key = bytes.fromhex(args.receiver_public_key) if args.receiver_public_key else None
    except ValueError:
        parser.error("receiver public key must be hexadecimal")
    if public_key is not None and len(public_key) != 32:
        parser.error("receiver public key must contain 32 bytes")
    certificate = None
    if args.action in ("authenticate", "identify", "start-carplay"):
        if args.certificate is None:
            parser.error("authenticate requires --certificate")
        certificate = args.certificate.read_bytes()
        if hashlib.sha256(certificate).hexdigest() != CERTIFICATE_SHA256:
            parser.error("unexpected certificate fixture")
        if args.corrupt_certificate_offset is not None:
            offset = args.corrupt_certificate_offset
            if not 0 <= offset < len(certificate):
                parser.error("invalid certificate corruption offset")
            changed = bytearray(certificate)
            changed[offset] ^= 1
            certificate = bytes(changed)
    elif args.corrupt_certificate_offset is not None:
        parser.error("certificate corruption requires authenticate")
    try:
        result = probe(args.action, certificate, hold_seconds=args.hold_seconds, public_key=public_key,
                       receiver_ipv6=args.receiver_ipv6)
    except (RuntimeError, ImportError) as exc:
        result = {"action": args.action, "error": str(exc)}
    print(json.dumps(result, indent=2))
    return int(any(key in result for key in ("error", "release_error", "restore_error")) or
               (args.action in ("detect", "handshake", "authenticate", "identify", "start-carplay") and not result.get("iap2_detect_echo")) or
               (args.action in ("handshake", "authenticate", "identify", "start-carplay") and not result.get("link_synchronized")) or
               (args.action in ("authenticate", "identify", "start-carplay") and not result.get("authentication_succeeded")) or
               (args.action in ("identify", "start-carplay") and not result.get("identification_accepted")) or
               (args.action == "start-carplay" and not result.get("carplay_start_sent")))


if __name__ == "__main__":
    raise SystemExit(main())
