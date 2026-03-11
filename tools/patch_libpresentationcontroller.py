#!/usr/bin/env python3
"""
Patch libPresentationController.so for CarPlay KOMO widget rendering.

Applies 3 patches to remove native-side blockers that prevent the
GuidanceView widget from rendering when driven by Java (CarPlay) instead
of native navigation guidance.

Patch 1: NOP StopDSIs — keep DSI services alive after guidance stops
Patch 2: Force StartDrawing — bypass activeMode check
Patch 3: Redirect frame rate reads — Java-controlled fps (0x68->0x54)

Architecture: ARM32, ARM mode, little-endian. File offset = VA (base 0).
Finds patch targets by byte signature search (firmware-version independent).
"""

import argparse
import sys

# Patch definitions using signature-based search.
# Each entry: (signature, patch_offset, original_bytes, patched_bytes, description)
#   signature    — unique byte sequence surrounding the patch target
#   patch_offset — offset within signature where the actual patch bytes start
#   original     — the bytes to replace (at patch_offset within signature)
#   patched      — replacement bytes
PATCHES = [
    # Patch 1: NOP StopDSIs (MOV R0,#0; BX LR)
    # ManeuvreViewManager_StopDSIs unregisters DSIKOMOView and
    # DSIMapViewerManeuverView services, then clears offset 104 and 116.
    # NOP it to keep DSI interfaces alive so Java can still call through.
    # Signature: 4 bytes before (end of preceding reloc table) + 16 bytes of function prologue
    (bytes([0x6C, 0xD7, 0xF4, 0xFF,
            0xF0, 0x40, 0x2D, 0xE9, 0xB0, 0x30, 0x9F, 0xE5,
            0x1C, 0xD0, 0x4D, 0xE2, 0x00, 0x50, 0xA0, 0xE1]),
     4,  # patch starts at byte 4 within signature
     bytes([0xF0, 0x40, 0x2D, 0xE9, 0xB0, 0x30, 0x9F, 0xE5]),
     bytes([0x00, 0x00, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1]),
     "NOP StopDSIs (keep DSI interfaces alive)"),

    # Patch 2: BNE->B in StartDrawing (force mode check pass)
    # GuidanceView_StartDrawing checks activeMode via
    # ManeuvreViewControllerRequestInterface. During CarPlay the mode is
    # wrong. Change conditional BNE to unconditional B.
    # Signature: CMP R0,#0 + ADR R4 + BNE + LDR R6
    (bytes([0x00, 0x00, 0x50, 0xE3, 0x04, 0x40, 0x8F, 0xE0,
            0x0A, 0x00, 0x00, 0x1A, 0xA8, 0x60, 0x9F, 0xE5]),
     8,  # BNE instruction is at byte 8
     bytes([0x0A, 0x00, 0x00, 0x1A]),
     bytes([0x0A, 0x00, 0x00, 0xEA]),
     "Force StartDrawing mode check (BNE->B)"),

    # Patch 3a-c: Redirect StartNewFrame fps reads from 0x68 to 0x54
    # SetFrameRateMode (called from Java setKOMODataRate) writes fps to
    # offset 0x54, but StartNewFrame reads from 0x68 (double-buffered
    # "active" field). The 0x54->0x64->0x68 propagation chain breaks
    # without native guidance. Redirect all 3 reads to 0x54 directly.

    # 3a: Initial fps check (LDR R6,[R0,#0x68] -> [R0,#0x54])
    # Signature: STMFD SP! + LDR R4,[PC] + LDR R6,[R0,#0x68]
    (bytes([0xF0, 0x47, 0x2D, 0xE9, 0x54, 0x43, 0x9F, 0xE5,
            0x68, 0x60, 0x90, 0xE5]),
     8,  # LDR is at byte 8
     bytes([0x68, 0x60, 0x90, 0xE5]),
     bytes([0x54, 0x60, 0x90, 0xE5]),
     "Redirect fps read 1 (LDR R6,[R0,#0x68]->[R0,#0x54])"),

    # 3b: Frame time calc, non-trace path (LDR R1,[R5,#0x68] -> [R5,#0x54])
    # Signature: CMP R0,#0 + BNE (large offset) + LDR R1,[R5,#0x68]
    (bytes([0x00, 0x00, 0x50, 0xE3, 0x68, 0x00, 0x00, 0x1A,
            0x68, 0x10, 0x95, 0xE5]),
     8,
     bytes([0x68, 0x10, 0x95, 0xE5]),
     bytes([0x54, 0x10, 0x95, 0xE5]),
     "Redirect fps read 2 (LDR R1,[R5,#0x68]->[R5,#0x54])"),

    # 3c: Frame time calc, trace path (LDR R1,[R5,#0x68] -> [R5,#0x54])
    # Signature: two BL instructions + LDR R1,[R5,#0x68]
    (bytes([0x3F, 0x1C, 0xEA, 0xEB, 0x0D, 0x1D, 0xEA, 0xEB,
            0x68, 0x10, 0x95, 0xE5]),
     8,
     bytes([0x68, 0x10, 0x95, 0xE5]),
     bytes([0x54, 0x10, 0x95, 0xE5]),
     "Redirect fps read 3 (LDR R1,[R5,#0x68]->[R5,#0x54])"),
]

LIB_PATH_ON_HU = "/apps/PresentationController/lib/libPresentationController.so"


def find_signature(data, signature, patched_signature):
    """Find unique signature in data. Returns offset or -1.
    Checks both original and patched forms (for already-patched detection)."""
    idx = data.find(signature)
    if idx != -1:
        # Verify unique
        if data.find(signature, idx + 1) != -1:
            return -2  # multiple matches
        return idx

    # Try patched form
    idx = data.find(patched_signature)
    if idx != -1:
        if data.find(patched_signature, idx + 1) != -1:
            return -2
        return idx

    return -1


def apply_patch(data, signature, patch_offset, original, patched, description, revert=False):
    """Find signature, verify bytes, apply patch. Returns True on success."""
    # Build both signature forms: with original bytes and with patched bytes
    orig_sig = bytearray(signature)  # signature as defined (contains original bytes)
    patched_sig = bytearray(signature)
    patched_sig[patch_offset:patch_offset + len(patched)] = patched

    # Search for either form
    idx = find_signature(data, bytes(orig_sig), bytes(patched_sig))

    if idx == -2:
        print(f"  [FAIL] {description}")
        print(f"         Multiple signature matches — cannot determine target")
        return False
    if idx == -1:
        print(f"  [FAIL] {description}")
        print(f"         Signature not found in binary")
        return False

    target = idx + patch_offset
    actual = data[target:target + len(original)]

    # Determine expected source and target based on mode
    expect = patched if revert else original
    apply = original if revert else patched

    if actual == apply:
        action = "already reverted" if revert else "already patched"
        print(f"  [SKIP] 0x{target:06X}: {description} ({action})")
        return True
    if actual != expect:
        print(f"  [FAIL] 0x{target:06X}: {description}")
        print(f"         Expected: {' '.join(f'{b:02X}' for b in expect)}")
        print(f"         Found:    {' '.join(f'{b:02X}' for b in actual)}")
        return False

    data[target:target + len(apply)] = apply
    print(f"  [ OK ] 0x{target:06X}: {description}")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Patch libPresentationController.so for CarPlay widget rendering")
    parser.add_argument("input", help="Input libPresentationController.so")
    parser.add_argument("output", nargs="?", default=None,
                        help="Output patched file (required unless --verify-only)")
    parser.add_argument("--verify-only", action="store_true",
                        help="Only verify bytes, don't write output")
    parser.add_argument("--revert", action="store_true",
                        help="Revert patches (swap original/patched)")
    args = parser.parse_args()

    if not args.verify_only and args.output is None:
        parser.error("output file is required (unless --verify-only)")

    with open(args.input, 'rb') as f:
        data = bytearray(f.read())

    mode_str = 'VERIFY' if args.verify_only else 'REVERT' if args.revert else 'PATCH'
    print(f"File: {args.input} ({len(data)} bytes)")
    print(f"Mode: {mode_str}")
    print()

    all_ok = True
    for sig, patch_off, original, patched, desc in PATCHES:
        if not apply_patch(data, sig, patch_off, original, patched, desc, args.revert):
            all_ok = False

    if not all_ok:
        print("\nSignature search FAILED. Wrong firmware version?")
        sys.exit(1)

    if args.verify_only:
        print("\nAll patches verified OK.")
        return

    with open(args.output, 'wb') as f:
        f.write(data)

    action = "Reverted" if args.revert else "Patched"
    out_name = args.output.split('/')[-1]
    print(f"\n{action} file written to: {args.output}")

    print(f"""
Deployment:
  scp {args.output} root@<HU>:/tmp/
  ssh root@<HU>
  mount -o remount,rw /apps
  cp {LIB_PATH_ON_HU} {LIB_PATH_ON_HU}.bak
  cp /tmp/{out_name} {LIB_PATH_ON_HU}
  reboot""")


if __name__ == '__main__':
    main()
