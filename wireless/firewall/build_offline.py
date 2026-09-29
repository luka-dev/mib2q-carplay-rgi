#!/usr/bin/env python3
"""Build separate MU1316 PF profiles that admit wireless CarPlay on uap0.

The existing wired CarPlay 6030:6032 profiles are inputs. Firmware and
deployment sources are never modified by this script.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
INPUT = ROOT / "deploy" / "pf" / "patched"
OUTPUT = Path(__file__).resolve().parent / "out"
HASHES = {
    "pf.conf": "93fd8be49f643d393b106d97b21e35a00cba99a54001c489d5819808d74d4ee9",
    "pf.ecm0.conf": "60a52e7477c08b0de1d8f304cfa70525e3614c142b3e205b1122369a0ca48048",
    "pf.mlan0.conf": "ec8f29b865fc0b95622f7deb5a519b60cf52d4114d00870921ff3bd1a9d37654",
    "pf.ppp0.conf": "167b9be7e5371a300f331011517992ae3fb67cee77565888f85f4888a3dd7c8f",
}
ANCHOR = "# WLAN to WLAN traffic\nblock in quick on $wlan_if from any to ($wlan_if)\n"
RULES = """# Wireless CarPlay on the local access point (adapted from MH2p PF)
pass in quick on $wlan_if inet proto udp from any to 224.0.0.251 port 5353
pass in quick on $wlan_if inet6 proto udp from any to FF02::FB port 5353
pass in quick on $wlan_if proto tcp from any to ($wlan_if) port { 5000:5001, 5010, 6000:6001, 6030:6032, 6100, 6200, 7000:7001, 7100 } keep state allow-opts queue(med_out, med_ack)
pass in quick on $wlan_if proto udp from any to ($wlan_if) port { 5001, 5020, 5353, 6000:6003, 6010:6011, 6020:6021, 7010:7011, 7070:7071 } keep state allow-opts queue(med_out, med_ack)

"""


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    report: dict[str, dict[str, str]] = {}
    OUTPUT.mkdir(exist_ok=True)
    for name, expected in HASHES.items():
        source = (INPUT / name).read_bytes()
        if sha256(source) != expected:
            raise ValueError(f"{name}: source hash mismatch")
        text = source.decode("utf-8")
        if text.count(ANCHOR) != 1:
            raise ValueError(f"{name}: expected exactly one WLAN block anchor")
        if text.count(" 6030:6032,") != 1:
            raise ValueError(f"{name}: wired CarPlay 6030:6032 rule missing")
        if text.count('wlan_if    = "uap0"') != 1:
            raise ValueError(f"{name}: expected uap0 WLAN macro")
        patched = text.replace(ANCHOR, RULES + ANCHOR).encode("utf-8")
        if patched.replace(RULES.encode("utf-8"), b"", 1) != source:
            raise ValueError(f"{name}: unexpected change outside added rules")
        output = OUTPUT / name
        output.write_bytes(patched)
        report[name] = {
            "input_sha256": expected,
            "output_sha256": sha256(patched),
            "added_before": "block in quick on $wlan_if from any to ($wlan_if)",
        }
    (OUTPUT / "build_report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
