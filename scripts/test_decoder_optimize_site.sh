#!/bin/bash
# hook/decoder_optimize/decoder_optimize_hook.c patch sites against the real stock libairplay of all
# three trains: the 12-byte signatures must match at 0x8c694 (present queue) and
# 0x8c6f4 (decode-feed queue) on P5087 and P5092 (byte-identical libs), a simulated
# imm8 swap must leave the rest of both signature words intact, and the CN K1004 lib
# (different build) must NOT match - the hook's fail-inert path.  Host-side static
# check; the runtime patcher verifies the same bytes in memory before writing.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
U=$ROOT/../../Tools/mhi2q-qemu/out/unpacked
python3 - "$U" <<'EOF'
import sys, os, tempfile

SIG_B = bytes.fromhex("04 00 a0 e3 3a 38 fe eb 00 00 50 e3".replace(" ", ""))
SIG_A = bytes.fromhex("04 30 a0 e3 01 10 84 e0 bc 36 fe eb".replace(" ", ""))
SITE_B, SITE_A = 0x8c694, 0x8c6f4

def lib(fw):
    root = os.path.join(sys.argv[1], fw)
    for dirpath, _, files in os.walk(root):
        if "libairplay.so" in files:
            return os.path.join(dirpath, "libairplay.so")
    return None

def check(fw, expect):
    p = lib(fw)
    if p is None:
        print(f"SKIP ({fw}: no unpacked libairplay)"); return
    with open(p, "rb") as f:
        data = f.read()
    got_b, got_a = data[SITE_B:SITE_B+12], data[SITE_A:SITE_A+12]
    if not expect:
        assert got_b != SIG_B and got_a != SIG_A, \
            f"{fw}: sites match but this train is expected to differ"
        print(f"decoder_optimize: {fw} (different build) refuses as expected  PASS")
        return
    assert got_b == SIG_B, f"{fw}: present-queue signature mismatch: {got_b.hex()}"
    assert got_a == SIG_A, f"{fw}: feed-queue signature mismatch: {got_a.hex()}"
    # imm8 swap simulation: depth 2 must touch exactly byte 0 of each mov word.
    depth = 2
    patched = bytearray(data)
    patched[SITE_B] = depth
    patched[SITE_A] = depth
    assert patched[SITE_B:SITE_B+12] == bytes([depth]) + SIG_B[1:], "present site patch not imm8-only"
    assert patched[SITE_A:SITE_A+12] == bytes([depth]) + SIG_A[1:], "feed site patch not imm8-only"
    assert patched[SITE_B+4:SITE_B+12] == SIG_B[4:] and patched[SITE_A+4:SITE_A+12] == SIG_A[4:]
    print(f"decoder_optimize: {fw} both sites match, imm8 swap to {depth} is byte-exact  PASS")

check("MHI2Q_US_AUG22_P5087_MU1316", True)
check("MHI2Q_ER_AUG22_P5092_MU1329", True)
check("MHI2Q_CN_AUG22_K1004_MU1003_4M0906961EG", False)
EOF
echo "decoder_optimize site test PASS"
