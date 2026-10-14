"""Verify the native iAP2 parser through a bounded CoreAccessories connection."""
from __future__ import annotations

import argparse
import json
import re

# These are pure wire codecs; importing them does not operate the legacy VM.
from tests.carplay_lab.legacy_ios.usb_probe import IAP2_DETECT, parse_control_message, parse_link_frame
from . import lab, usb


def verify(log: str) -> dict:
    frames = []
    detect = False
    for line in log.splitlines():
        if not line.startswith("native iAP2 outgoing "):
            continue
        match = re.fullmatch(r"native iAP2 outgoing length=(\d+) hex=([0-9a-f]+)", line)
        if not match or int(match[1]) > 512:
            raise ValueError("incomplete native iAP2 packet")
        data = bytes.fromhex(match[2])
        if len(data) != int(match[1]):
            raise ValueError("native iAP2 packet length differs from the capture")
        if data == IAP2_DETECT:
            detect = True
        else:
            if len(data) < 9 or int.from_bytes(data[2:4], "big") != len(data):
                raise ValueError("native iAP2 frame contains extra or missing bytes")
            frames.append(parse_link_frame(data))
    synack = next((p for p in frames if p["control"] == 0xC0 and p["acknowledgement"] == 1), None)
    if not detect or synack is None:
        raise ValueError("native detect and SYN/ACK were not both captured")
    payload = synack["payload"]
    if len(payload) < 13 or (len(payload) - 10) % 3 or payload[0] != 1:
        raise ValueError("invalid native synchronization parameters")
    sessions = [list(payload[i:i + 3]) for i in range(10, len(payload), 3)]
    if [10, 0, 1] not in sessions:
        raise ValueError("native peer did not negotiate the control session")
    if f"native SYNACK acknowledged={synack['sequence']}" not in log.splitlines():
        raise ValueError("native SYN/ACK was not acknowledged")
    certificate = None
    for packet in frames:
        if packet["session"] != 10 or not packet["payload"] or packet["control"] != 0x40 or packet["acknowledgement"] != 1:
            continue
        message, parameters = parse_control_message(packet["payload"])
        if message == 0xAA00 and not parameters and packet["sequence"] == (synack["sequence"] + 1) % 256:
            certificate = packet
            break
    if certificate is None or "native smoke connection cleanup complete" not in log.splitlines():
        raise ValueError("native certificate request or connection cleanup was not captured")
    return {"native_detect_packet": True, "link_synchronized": True,
            "control_sessions": sessions, "certificate_request": "0xAA00",
            "synack": synack | {"payload": payload.hex()},
            "request": certificate | {"payload": certificate["payload"].hex()},
            "transport": "CoreAccessories API", "connection_type": 6,
            "endpoint_transport": 0, "endpoint_protocol": 4,
            "usb_forwarding_verified": False, "mfi_acceptance_verified": False,
            "video_verified": False}


def run() -> dict:
    native = usb.run("iap-smoke")
    result = {"udid": native["udid"], "verification": verify(native["log"]["result"]["content"]),
              "native": native}
    lab.save("iap-smoke.json", result)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--experimental", action="store_true")
    args = parser.parse_args()
    if not args.experimental:
        parser.error("a transient native accessory connection requires --experimental")
    print(json.dumps(run(), indent=2))
