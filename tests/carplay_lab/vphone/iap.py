"""Verify the native iAP2 parser through a bounded CoreAccessories connection."""
from __future__ import annotations

import argparse
import json
import re

# These are pure wire codecs; importing them does not operate the legacy VM.
from tests.carplay_lab.legacy_ios.usb_probe import IAP2_DETECT, parse_control_message, parse_link_frame
from . import lab, usb


def verify_transport(log: str, transport: str, *, require_published: bool = True) -> dict:
    if transport not in ("usb", "bluetooth"):
        raise ValueError("unsupported native iAP2 transport")
    connection, endpoint = (6, 0) if transport == "usb" else (2, 2)
    expected = f"connection_type={connection} endpoint_transport={endpoint} endpoint_protocol=4"
    profiles = re.findall(r"^native iAP2 profile (.*)$", log, re.M)
    readbacks = re.findall(r"^native iAP2 readback (.*)$", log, re.M)
    published=readbacks==[expected]
    absent=["connection_type=11 endpoint_transport=18 endpoint_protocol=0"]
    if profiles != [expected] or transport == "bluetooth" and (
            not published and (require_published or readbacks!=absent)):
        raise ValueError("native transport profile/readback does not match the requested transport")
    return {"connection_type": connection, "endpoint_transport": endpoint,
            "endpoint_protocol": 4, "non_usb_endpoint_verified": transport == "bluetooth" and published,
            "connection_info_published": published,
            "wireless_carplay_verified": False, "physical_bluetooth_verified": False}


def verify(log: str, transport: str = "usb") -> dict:
    profile = verify_transport(log, transport, require_published=False)
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
            "transport": "CoreAccessories API", **profile,
            "usb_forwarding_verified": False, "mfi_acceptance_verified": False,
            "video_verified": False}


def run(transport: str = "usb") -> dict:
    if transport not in ("usb", "bluetooth"):
        raise ValueError("unsupported native iAP2 transport")
    native = usb.run("iap-smoke-bluetooth" if transport == "bluetooth" else "iap-smoke")
    result = {"udid": native["udid"], "verification": verify(native["log"]["result"]["content"], transport),
              "native": native}
    lab.save("iap-smoke-bluetooth.json" if transport == "bluetooth" else "iap-smoke.json", result)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--experimental", action="store_true")
    parser.add_argument("--transport", choices=("usb", "bluetooth"), default="usb")
    args = parser.parse_args()
    if not args.experimental:
        parser.error("a transient native accessory connection requires --experimental")
    print(json.dumps(run(args.transport), indent=2))
