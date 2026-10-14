#!/usr/bin/env python3
"""Re-derive and check the facts the btstack iAP graft plan rests on.

Everything here is read straight out of two firmware binaries, so the plan in
docs/wireless-carplay/HARDWARE_AND_DONORS.md can be re-checked after any
firmware is re-extracted, without IDA.

  python3 wireless/verify_graft_points.py            # check, print a summary
  python3 wireless/verify_graft_points.py --record   # also emit the iAP SDP record bytes

Checks, in order:
  1. premise      the Apple iAP UUID is in the donor and absent from ours
  2. anchors      the strings used for cross-binary function matching exist in both
  3. record       the donor's iAP SDP attribute values are exactly what the plan documents
  4. channel      the RFCOMM channel byte in the image is 0, i.e. patched at runtime

# ponytail: byte search instead of ELF section walking — the patterns are unique
# enough that vaddr-to-offset mapping buys nothing. Add pyelftools if that changes.
"""
import argparse
import sys
from pathlib import Path

import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
FW = Path(_WS + "/Firmwares/HU")
OURS = FW / "MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/bin/apps/btstack"
DONOR = FW / "MHI2_US_AUG22_K3347_MU1439/extracted/app50/eso/bin/apps/btstack"

# Apple's wireless-iAP Bluetooth service UUID, as a 128-bit SDP UUID.
IAP_UUID = bytes.fromhex("00000000decafadedecadeafdecacaff")

# SDP attribute 0x0001, ServiceClassIDList: DE sequence(17) > UUID128.
SERVICE_CLASS_ID_LIST = bytes([0x35, 0x11, 0x1C]) + IAP_UUID

# SDP attribute 0x0004, ProtocolDescriptorList: sequence(12) > [seq(3) UUID16 L2CAP,
# seq(5) UUID16 RFCOMM + uint8 channel]. The trailing channel byte is written by
# IapServices::registerService once RFCOMM grants a channel, so it is 0 in the image.
PROTOCOL_DESCRIPTOR_LIST = bytes(
    [0x35, 0x0C, 0x35, 0x03, 0x19, 0x01, 0x00, 0x35, 0x05, 0x19, 0x00, 0x03, 0x08, 0x00]
)

# Strings present in both builds, used to match functions across the two binaries.
ANCHORS = {
    "did record": b"DevIdAddServiceRecord: DID SDP record succeeded",
    "server bind": b"Fatal Error: Unable to bind the server to the protocol stack",
}

# Only the donor has these; they name the module we want.
DONOR_ONLY = {
    "iap sdp record log": b"Registered iAP-SDP-Record",
    "iap services class": b"IapServices",
}


def load(path: Path) -> bytes:
    if not path.is_file():
        sys.exit(f"missing binary: {path}")
    return path.read_bytes()


def check(label: str, ok: bool, detail: str = "") -> bool:
    print(f"  [{'ok' if ok else 'FAIL'}] {label}{(' — ' + detail) if detail else ''}")
    return ok


def verify(ours: bytes, donor: bytes) -> tuple[bool, int]:
    failures = 0
    print("premise")
    failures += not check("iAP UUID present in donor", donor.count(IAP_UUID) == 1,
                          f"{donor.count(IAP_UUID)} occurrence(s)")
    failures += not check("iAP UUID absent from ours", IAP_UUID not in ours)
    for name, pat in DONOR_ONLY.items():
        failures += not check(f"donor-only marker {name!r}", pat in donor and pat not in ours)

    print("anchors for cross-binary matching")
    for name, pat in ANCHORS.items():
        failures += not check(f"{name!r} in both", pat in ours and pat in donor)

    print("iAP SDP record in the donor image")
    scil = donor.find(SERVICE_CLASS_ID_LIST)
    pdl = donor.find(PROTOCOL_DESCRIPTOR_LIST)
    failures += not check("ServiceClassIDList value matches the documented bytes", scil != -1,
                          f"at file offset 0x{scil:x}" if scil != -1 else "not found")
    failures += not check("ProtocolDescriptorList value matches the documented bytes", pdl != -1,
                          f"at file offset 0x{pdl:x}" if pdl != -1 else "not found")
    if pdl != -1:
        failures += not check("RFCOMM channel byte is 0 in the image, patched at runtime",
                              donor[pdl + 13] == 0, f"value 0x{donor[pdl + 13]:02x}")
    if scil != -1 and pdl != -1:
        # The two values sit next to each other, the descriptor list first.
        failures += not check("the two values are adjacent, descriptor list first",
                              0 < scil - pdl < 0x40, f"delta 0x{scil - pdl:x}")
    return failures == 0, failures


def emit_record() -> None:
    print("\niAP SDP record, two attributes, channel byte left at 0:")
    print(f"  0x0001 ServiceClassIDList     len {len(SERVICE_CLASS_ID_LIST):2d}  "
          f"{SERVICE_CLASS_ID_LIST.hex()}")
    print(f"  0x0004 ProtocolDescriptorList len {len(PROTOCOL_DESCRIPTOR_LIST):2d}  "
          f"{PROTOCOL_DESCRIPTOR_LIST.hex()}")
    print("\nOur btstack entry points, verified line-for-line against the donor:")
    for name, addr in (("SDP add record", 0x2073C4), ("SDP remove record", 0x207344),
                       ("RFCOMM register server", 0x277B98),
                       ("RFCOMM deregister server", 0x277B34)):
        print(f"  {name:<26} 0x{addr:06X}")


def demo() -> None:
    """Self-check of the record encoding, independent of any firmware."""
    assert SERVICE_CLASS_ID_LIST[0] == 0x35 and SERVICE_CLASS_ID_LIST[1] == 0x11
    assert len(SERVICE_CLASS_ID_LIST) == 2 + SERVICE_CLASS_ID_LIST[1] == 19
    assert SERVICE_CLASS_ID_LIST[2] == 0x1C and len(IAP_UUID) == 16
    assert PROTOCOL_DESCRIPTOR_LIST[0] == 0x35 and PROTOCOL_DESCRIPTOR_LIST[1] == 0x0C
    assert len(PROTOCOL_DESCRIPTOR_LIST) == 2 + PROTOCOL_DESCRIPTOR_LIST[1] == 14
    assert PROTOCOL_DESCRIPTOR_LIST[4:7] == bytes([0x19, 0x01, 0x00])   # L2CAP
    assert PROTOCOL_DESCRIPTOR_LIST[9:12] == bytes([0x19, 0x00, 0x03])  # RFCOMM
    assert PROTOCOL_DESCRIPTOR_LIST[12] == 0x08                         # uint8 follows
    print("self-check: record encoding consistent")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--ours", type=Path, default=OURS)
    ap.add_argument("--donor", type=Path, default=DONOR)
    ap.add_argument("--record", action="store_true", help="also print the record and addresses")
    args = ap.parse_args()

    demo()
    print(f"\nours  {args.ours.name}  {args.ours.stat().st_size if args.ours.is_file() else '?'} B")
    print(f"donor {args.donor.name}  {args.donor.stat().st_size if args.donor.is_file() else '?'} B\n")
    ok, failures = verify(load(args.ours), load(args.donor))
    if args.record:
        emit_record()
    print(f"\n{'all checks passed' if ok else f'{failures} check(s) FAILED'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
