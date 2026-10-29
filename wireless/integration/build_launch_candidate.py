#!/usr/bin/env python3
"""Prepare review-only rc.local and per-unit environment templates.

Reads the existing local autostart entry. Does not install or contact the HU.
"""

from __future__ import annotations

from hashlib import sha256
from pathlib import Path
import argparse
import subprocess


HERE = Path(__file__).resolve().parent
RC_LOCAL = HERE.parents[2] / "wifi_fix/rc.local"
OUT = HERE / "out"
EXPECTED_RC_SHA256 = "588e18e429605f122daece60c3bb2b2be5c9dba55cf6969e474ba1841ee6e0be"
ANCHOR = 'echo "rc.local: done" >> "$LOG"\n'
LAUNCH = (
    "# Wireless CarPlay; supervisor waits for dio's bridge, daemon retries AP IE setup.\n"
    "respawn /bin/sh /mnt/app/root/hooks/wireless_dio_supervisor.sh\n\n"
)
REQUIRED = (
    "MHI2Q_CP_IE_INDEX", "MHI2Q_CP_BT_MAC", "MHI2Q_CP_DEVICE_ID",
    "MHI2Q_CP_MODEL", "MHI2Q_CP_SERIAL", "MHI2Q_CP_HARDWARE",
    "MHI2Q_CP_SSID", "MHI2Q_CP_PASSPHRASE", "MHI2Q_CP_CHANNEL",
    "MHI2Q_CP_IP_ADDRESS", "MHI2Q_CP_AIRPLAY_PORT",
)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--property-handoff", action="store_true")
    parser.add_argument("--stock210", action="store_true")
    parser.add_argument("--si-owned", action="store_true", help="bootstrap/mDNS runtime only; SI owns DIO")
    args = parser.parse_args()
    if args.stock210 and not args.property_handoff:
        parser.error("--stock210 requires --property-handoff")
    if args.si_owned and not args.stock210:
        parser.error("--si-owned requires --stock210 --property-handoff")
    source = RC_LOCAL.read_bytes()
    if sha256(source).hexdigest() != EXPECTED_RC_SHA256:
        raise RuntimeError("local rc.local differs from reviewed autostart source")
    text = source.decode()
    if text.count(ANCHOR) != 1 or "wireless_dio_supervisor.sh" in text:
        raise RuntimeError("autostart insertion point is not unique")
    launch_text = LAUNCH if not args.si_owned else (
        "# Wireless bootstrap and mDNS; smartphone_integrator owns DIO.\n"
        "respawn /bin/sh /mnt/app/root/hooks/wireless_si_runtime.sh\n\n")
    candidate = text.replace(ANCHOR, launch_text + ANCHOR)
    if candidate.replace(launch_text, "") != text:
        raise RuntimeError("candidate changes outside wireless launch slot")
    OUT.mkdir(exist_ok=True)
    launch = OUT / ("rc.local.stock210.offline.sh" if args.stock210 else "rc.local.wireless.offline.sh")
    if args.si_owned: launch = OUT / "rc.local.si_owned.offline.sh"
    launch.write_text(candidate)
    subprocess.run(["sh", "-n", str(launch)], check=True)
    example = OUT / ("wireless_carplay.stock210.env.example" if args.stock210 else
                     "wireless_carplay.handoff.env.example" if args.property_handoff
                     else "wireless_carplay.env.example")
    if args.si_owned: example = OUT / "wireless_carplay.si_owned.env.example"
    example.write_text(
        "# Per-unit values only. Copy to wireless_carplay.env with mode 0600 after\n"
        "# verifying AP, Bluetooth, MFi and AirPlay identity on the unit.\n"
        "# DEVICE_ID must match the receiver's _airplay._tcp deviceid and /info deviceID.\n"
        "# The bundled AirPlay 280 receiver uses port 7000.\n"
        "# Empty values intentionally make the daemon refuse startup.\n"
        "# Name, manufacturer and firmware come from iap2.accessoryIdentification\n"
        "# in $IPL_CONFIG_DIR_DIO_MANAGER/dio_manager.json (IPL_CONFIG_DIR fallback,\n"
        "# default /etc/eso/production). This must be the active stock DIO config.\n"
        "# MHI2Q_CP_NAME/MANUFACTURER/FIRMWARE are obsolete and ignored.\n"
        + "".join(f"export {name}=''\n" for name in REQUIRED)
        + ("export MHI2Q_REQUIRE_BRIDGE_QUIESCE=1\n" if args.property_handoff else "")
        + "# The bundled AirPlay receiver reports sourceVersion 280.33.8.\n"
        + "# Optional MHI2Q_CP_PAIRING_ID pins the receiver pi (not its Ed25519 pk).\n"
    )
    if args.stock210:
        text=example.read_text().replace("# The bundled AirPlay 280 receiver uses port 7000.",
            "# Original AirPlay 210.81 + hook; port 5000. No libairplay replacement.")
        text=text.replace("export MHI2Q_CP_AIRPLAY_PORT=''", "export MHI2Q_CP_AIRPLAY_PORT=5000")
        text=text.replace("# The bundled AirPlay receiver reports sourceVersion 280.33.8.",
            "export MHI2Q_CP_RECEIVER=stock210\nexport MHI2Q_CP_SOURCE_VERSION=210.81\n"
            "export MHI2Q_CP_WIRELESS_PAIRING=1\nexport MHI2Q_CP_WIRELESS_MEDIA=1\n"
            "export MHI2Q_CP_PAIRING_DIR=/mnt/persist/mhi2q-carplay/pairing\n"
            "export OWNER_FILE=/mnt/persist/.mhi2q_dio_generation")
        example.write_text(text)
    else:
        with example.open("a") as stream:
            stream.write("export MHI2Q_CP_RECEIVER=experimental280\n")
    if args.si_owned:
        with example.open("a") as stream:
            stream.write("export MHI2Q_CP_SI_CONTROL=/tmp/mhi2q_si_control.sock\n")
            stream.write("export MHI2Q_CP_WIRELESS_ALT=1\nexport IAP2_VEHICLE_STATUS=1\nexport IAP2_VEHICLE_SPEED=1\n")
    subprocess.run(["sh", "-n", str(example)], check=True)
    print("review-only rc.local and per-unit environment candidates generated")


if __name__ == "__main__":
    main()
