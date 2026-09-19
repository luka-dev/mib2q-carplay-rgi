#!/usr/bin/env python3
"""protect.py - post-build protector. Layers on top of a compiled artifact and
binds it to ONE VIN. The developer's source stays plain C; this runs in the
build/provision pipeline, once per flashed VIN.

  compile once (generic)  ->  protect.py --vin <VIN>  ->  image for that car

What it does, matching prot_rt.c exactly:
  1. locate the .prot_slot marker in the binary
  2. CRC32 the WHOLE file with the slot's mutable fields (CHK/LEN/ENC) zeroed
  3. stamp CHK = that CRC, LEN, and ENC = payload ^ keystream(KDF(secret,VIN,CHK))

Ceiling: the secret is baked into the runtime and passed here; a rooted attacker
who runs the image once can extract the derived key. Anti-clone / anti-casual-
patch grade, not cryptographic. Keep the secret identical to the runtime build
(-DPROT_SECRET) and out of the source tree (env / CI secret).
"""
import argparse, hashlib, hmac, os, sys

MAGIC   = b"PROT\x00SLT"           # 8 bytes; must equal the magic in prot_rt.h
MAGLEN  = 8
TAG     = bytes([0xA5, 0x5A, 0xC3, 0x3C])   # must equal TAG in prot_rt.c
TAGLEN  = len(TAG)
CAP     = 64                       # PROT_SLOT_CAP (max payload)
SLOTSZ  = MAGLEN + 4 + 2 + TAGLEN + CAP

def _ks(key, n):                   # sha256 counter-mode; matches prot_keystream()
    out, c = b"", 0
    while len(out) < n:
        out += hashlib.sha256(key + c.to_bytes(4, "big")).digest()
        c += 1
    return out[:n]

def _kdf(secret, vin, chk):        # matches HMAC(secret, vin || chk_le32) in rt
    return hmac.new(secret, vin + chk.to_bytes(4, "little"), hashlib.sha256).digest()

def _crc32(buf):
    import zlib
    return zlib.crc32(buf) & 0xffffffff

def stamp(path, secret, vin, payload, mode="token"):
    """mode='token': body = TAG||payload, runtime prot_unlock() rejects wrong VIN.
       mode='blind': body = raw constants, runtime prot_unblind() folds them into
                     your math; wrong VIN -> wrong constants -> wrong output, no tag."""
    if len(vin) != 17:
        sys.exit("VIN must be 17 chars, got %d" % len(vin))
    if len(payload) > CAP:
        sys.exit("payload %d > cap %d" % (len(payload), CAP))
    b = bytearray(open(path, "rb").read())
    # The magic can appear more than once (compiler may echo the constant into
    # .rodata). The real, unprovisioned slot is the one whose mutable tail
    # (CHK/LEN/ENC) is still all zero. Pick that one.
    cands, off = [], b.find(MAGIC)
    while off != -1:
        if off + SLOTSZ <= len(b):
            cands.append(off)
        off = b.find(MAGIC, off + 1)
    fresh = [c for c in cands if b[c+8:c+SLOTSZ] == bytes(SLOTSZ-8)]
    if not cands:
        sys.exit("no PROT slot in %s (did you PROT_DEFINE_SLOT?)" % path)
    if not fresh:
        sys.exit("no unprovisioned PROT slot (already stamped? rebuild first)")
    if len(fresh) > 1:
        sys.exit("multiple unprovisioned slots - define exactly one PROT_DEFINE_SLOT")
    i = fresh[0]

    # zero the mutable fields, CRC the whole file, then stamp them
    b[i+8 : i+SLOTSZ] = bytes(SLOTSZ - 8)
    chk  = _crc32(b)
    K    = _kdf(secret, vin.encode(), chk)
    body = (TAG + payload) if mode == "token" else payload   # blind: raw constants, no tag
    enc  = bytes(a ^ c for a, c in zip(body, _ks(K, len(body))))

    b[i+8 : i+12]  = chk.to_bytes(4, "little")           # CHK
    b[i+12: i+14]  = len(body).to_bytes(2, "little")     # LEN = full body length
    b[i+14: i+14+len(enc)] = enc                         # ENC = keystream ^ body
    open(path, "wb").write(b)
    return chk

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Bind a compiled build to one VIN.")
    ap.add_argument("binary", help="compiled artifact to stamp in place")
    ap.add_argument("--vin", required=True)
    ap.add_argument("--mode", choices=("token", "blind"), default="token",
                    help="token: rt prot_unlock() rejects wrong VIN; "
                         "blind: rt prot_unblind() folds constants into the math (wrong VIN=wrong output)")
    ap.add_argument("--payload", default="", help="string payload (token mode) or raw constants")
    ap.add_argument("--payload-file", help="binary payload/constants file (overrides --payload)")
    ap.add_argument("--secret", help="build secret; default $PROT_SECRET")
    ap.add_argument("--out", help="write stamped copy here instead of in place")
    a = ap.parse_args()

    secret = (a.secret or os.environ.get("PROT_SECRET", "")).encode()
    if not secret:
        sys.exit("no secret: pass --secret or set PROT_SECRET (must match the rt build)")
    payload = open(a.payload_file, "rb").read() if a.payload_file else a.payload.encode()

    path = a.binary
    if a.out:
        import shutil; shutil.copy(a.binary, a.out); path = a.out
    chk = stamp(path, secret, a.vin, payload, a.mode)
    print("stamped %s: VIN=%s CHK=%08x %s=%dB" %
          (path, a.vin, chk, "constants" if a.mode == "blind" else "payload", len(payload)))
