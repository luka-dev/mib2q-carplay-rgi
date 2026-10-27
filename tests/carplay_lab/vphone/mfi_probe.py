"""Test the public MFi fixture against the owned guest's native iAP2 API endpoint."""
from __future__ import annotations

import argparse
import base64
import json
import ipaddress
import re
import secrets
import struct
import threading
import time

from tests.carplay_lab.legacy_ios import usb_probe as wire
from tests.carplay_lab.common.mfi_fixture import fixture_key, sign_digest
from . import lab, usb


class NativeAPIPeer:
    """Adapt the pure wire exchange to an owned RPC queue, without a USB device."""

    def __init__(self, settings: dict, incoming: str, log: str):
        self.settings, self.incoming, self.log = settings, incoming, log
        self.history = b""
        self.partial = bytearray()
        self.assembling = False
        self.out_index = 0
        self.synack = None
        self.discarded_synacks = 0

    def ctrl_transfer(self, request_type, request, value, index, report, timeout=0):
        if (request_type, request, value, index) != (0x21, 9, 0x0201, 2) or not 3 <= len(report) <= 1025 or report[0] != 1 or report[1] & ~3:
            raise ValueError("invalid API peer wire report")
        if not report[1] & 1:
            if self.assembling:
                raise ValueError("interrupted API peer packet")
            self.partial.clear()
        elif not self.assembling:
            raise ValueError("API peer continuation without a packet")
        self.partial.extend(report[2:])
        if len(self.partial) > 8192:
            raise ValueError("API peer packet exceeds its bound")
        self.assembling = bool(report[1] & 2)
        if not self.assembling:
            packet = bytes(self.partial)
            if len(packet) < 9 or packet[:2] != b"\xff\x5a":
                raise ValueError("unexpected API peer packet")
            length = int.from_bytes(packet[2:4], "big")
            if any(packet[length:]):
                raise ValueError("API peer packet has nonzero padding")
            packet = packet[:length]
            wire.parse_link_frame(packet)
            history = self.history + struct.pack(">I", len(packet)) + packet
            if len(history) > 65536:
                raise ValueError("API peer history exceeds its bound")
            lab.rpc(self.settings, "files.write", {"path": self.incoming,
                    "content": base64.b64encode(history).decode(), "encoding": "base64"})
            self.history = history
        return len(report)

    def read(self, endpoint, size, timeout):
        deadline = time.monotonic() + timeout / 1000
        while time.monotonic() < deadline:
            result = lab.rpc(self.settings, "files.read", {"path": self.log, "limit": 16000})["result"]
            if result.get("truncated"):
                raise RuntimeError("native peer output truncated")
            lines = [line for line in result["content"].splitlines() if line.startswith("native iAP2 outgoing ")]
            while self.out_index < len(lines):
                line = lines[self.out_index]
                # A read during printf can expose an incomplete final line.
                if not result["content"].endswith("\n") and line == result["content"].splitlines()[-1]:
                    break
                self.out_index += 1
                match = re.fullmatch(r"native iAP2 outgoing length=(\d+) hex=([0-9a-f]+)", line)
                if not match or int(match[1]) > 512:
                    raise ValueError("native peer packet truncated")
                packet = bytes.fromhex(match[2])
                if len(packet) != int(match[1]):
                    raise ValueError("native peer packet length differs from the capture")
                if packet == wire.IAP2_DETECT:
                    continue
                parsed = wire.parse_link_frame(packet)
                if int.from_bytes(packet[2:4], "big") != len(packet):
                    raise ValueError("native peer packet contains extra bytes")
                if parsed["control"] == 0xC0:
                    if self.synack is not None:
                        if packet != self.synack:
                            raise ValueError("native peer changed its SYN/ACK")
                        self.discarded_synacks += 1
                        continue
                    self.synack = packet
                return b"\x01\x00" + packet
            time.sleep(0.15)
        raise TimeoutError("native API peer receive deadline expired")


def run(*, negative: str | None = None, identify: bool = False, start_carplay: bool = False,
        public_key: bytes | None = None, receiver_ipv6: str = "fe80::4254:ff:fe12:3456", hold_seconds: int = 6,
        carplay_interface: int = 2) -> dict:
    if negative not in (None, "signature", "certificate"):
        raise ValueError("unknown native MFi negative control")
    if type(identify) is not bool or type(start_carplay) is not bool or (identify or start_carplay) and negative is not None:
        raise ValueError("identification requires the valid MFi control")
    if type(hold_seconds) is not int or not 1 <= hold_seconds <= 120:
        raise ValueError("native API peer hold must be between 1 and 120 seconds")
    if type(carplay_interface) is not int or carplay_interface not in (2, 4):
        raise ValueError("CarPlay interface must be an owned NCM data interface: 2 or 4")
    receiver = ipaddress.IPv6Address(receiver_ipv6)
    if not receiver.is_link_local or "%" in receiver_ipv6 or public_key is not None and (not isinstance(public_key, bytes) or len(public_key) != 32):
        raise ValueError("invalid native receiver address/public key")
    if start_carplay and public_key is None:
        raise ValueError("native CarPlay start requires the current receiver public key")
    key, certificate = fixture_key()
    if negative == "certificate":
        from cryptography.hazmat.primitives.serialization import pkcs7
        signature = pkcs7.load_der_pkcs7_certificates(certificate)[0].signature
        if certificate.count(signature) != 1:
            raise ValueError("ambiguous fixture certificate signature")
        offset = certificate.index(signature) + len(signature) - 1
        changed = bytearray(certificate)
        changed[offset] ^= 1
        certificate = bytes(changed)
    settings = lab.profile()
    lab.require_machine(settings, "running")
    info = lab.rpc(settings, "device.info")["result"]
    if info.get("model") != "iPhone99,11" or info.get("ios_version") != "26.6.2":
        raise RuntimeError("guest does not match the reviewed native iAP2 profile")
    nonce = secrets.token_hex(8)
    incoming = usb.GUEST_DIR + "/iap-peer-" + nonce + ".inbox"
    done = usb.GUEST_DIR + "/iap-peer-" + nonce + ".done"
    lab.rpc(settings, "files.mkdir", {"path": usb.GUEST_DIR})
    lab.rpc(settings, "files.write", {"path": incoming, "content": "", "encoding": "base64"})
    lab.rpc(settings, "files.chmod", {"path": incoming, "mode": "600"})
    context = {}
    ready = threading.Event()

    def launched(log):
        context["log"] = log
        ready.set()

    def worker():
        try:
            context["native"] = usb.run("iap-peer", peer_nonce=nonce,
                                        peer_seconds=max(60, hold_seconds + 30), on_launch=launched)
        except Exception as error:
            context["error"] = str(error)
            ready.set()

    def signer(digest):
        signature = sign_digest(key, digest)
        return signature[:-1] + bytes((signature[-1] ^ 1,)) if negative == "signature" else signature

    result = {"transport": "CoreAccessories API + bounded RPC queue", "negative_control": negative,
              "usb_forwarding_verified": False, "qnx_session_verified": False, "video_verified": False}
    thread = threading.Thread(target=worker)
    thread.start()
    try:
        if not ready.wait(timeout=25) or "error" in context:
            raise RuntimeError(context.get("error", "native peer did not start"))
        peer = NativeAPIPeer(settings, incoming, context["log"])
        wire.handshake(peer, [(1, 1024)], certificate, result, signer=signer, identify=identify or start_carplay,
                       start_carplay=start_carplay, hold_seconds=hold_seconds, public_key=public_key, receiver_ipv6=receiver.compressed,
                       carplay_interface=carplay_interface)
        result["discarded_retransmitted_synacks"] = peer.discarded_synacks
        expected = "0xaa04" if negative else "0xaa05"
        if not result.get("link_synchronized") or result.get("first_control_message") != "0xaa00" or result.get("authentication_messages", [])[-1:] != [expected]:
            raise RuntimeError("native MFi result does not match the requested control")
        if negative != "certificate" and (result.get("challenge_bytes") != 32 or result.get("signature_bytes") != 64):
            raise RuntimeError("native MFi challenge/signature size differs from the fixture")
        if negative == "certificate" and "challenge_bytes" in result:
            raise RuntimeError("corrupted certificate was not rejected before the challenge")
        if (identify or start_carplay) and result.get("identification_accepted") is not True:
            raise RuntimeError("native identification was not accepted")
        if start_carplay and "carplay_start_sent" not in result:
            raise RuntimeError("native CarPlay start request was not sent")
        if identify and not start_carplay:
            # Keep the native connection alive for its first-pairing UI.
            result["identification_hold_seconds"] = hold_seconds
            time.sleep(hold_seconds)
    except Exception as error:
        result["error"] = str(error)
    finally:
        try:
            lab.rpc(settings, "files.write", {"path": done, "content": "done"})
        finally:
            thread.join(timeout=50)
        if thread.is_alive():
            raise RuntimeError("bounded native peer helper remains active")
        result["native"] = context.get("native", {"error": context.get("error")})
        if context.get("error"):
            result["error"] = context["error"]
        case = "carplay" if start_carplay else "identify" if identify else negative or "valid"
        lab.save("mfi-api-" + case + ".json", result)
        for path in (incoming, done):
            lab.rpc(settings, "files.remove", {"path": path})
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--experimental", action="store_true")
    parser.add_argument("--negative", choices=("signature", "certificate"))
    parser.add_argument("--identify", action="store_true")
    parser.add_argument("--start-carplay", action="store_true")
    parser.add_argument("--receiver-public-key")
    parser.add_argument("--receiver-ipv6", default="fe80::4254:ff:fe12:3456")
    parser.add_argument("--hold-seconds", type=int, default=6)
    parser.add_argument("--carplay-interface", type=int, choices=(2, 4), default=2)
    args = parser.parse_args()
    if not args.experimental:
        parser.error("a transient native accessory connection requires --experimental")
    public_key = bytes.fromhex(args.receiver_public_key) if args.receiver_public_key else None
    result = run(negative=args.negative, identify=args.identify, start_carplay=args.start_carplay,
                 public_key=public_key, receiver_ipv6=args.receiver_ipv6, hold_seconds=args.hold_seconds,
                 carplay_interface=args.carplay_interface)
    print(json.dumps(result, indent=2))
    raise SystemExit(int("error" in result))
