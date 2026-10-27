#!/usr/bin/env python3
"""Build the one file the benchmark needs, out of the live capture.

    apac_decoder/bench/make_blob.py [CAPTURE_DIR] [OUT]

The blob carries the decoder cookie and every DECRYPTED packet plaintext - the
TLV prefix and the APAC packet behind it, exactly as spa_apap_open() would hand
them to spa_apap_parse_extension().  It deliberately carries NO session key and
no ciphertext:

  * the key never leaves the capture directory.  The benchmark drives the AEAD
    with an all-zero key over records of the real lengths, which costs exactly
    what the real one costs - ChaCha20 and Poly1305 are data independent, their
    work is a function of length alone - and the host run cross-checks that
    decomposition against the real key (see apac_bench.cpp, `--key`).
  * so the blob is safe to copy into a QEMU boot image, which the raw stream
    would not be.

Records are the plaintexts of `decrypted/NNNNNN_extension12.bin`, in capture
order, all 3980 of them including the three that carry TLV state and no audio -
dropping those would quietly change what the framer and the TLV parser are
measured on.

Layout, all little endian:

    "APACBENCH1\\0\\0"   12 bytes
    u32 cookieLength ; cookie bytes
    u32 recordCount
    recordCount x ( u32 plaintextLength ; plaintext bytes )
"""
import glob
import os
import struct
import sys

DEFAULT_CAPTURE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..",
    "captures", "apac_5_1_live_20260831_0205")


def main():
    capture = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_CAPTURE
    out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/apac_bench.blob"

    cookies = glob.glob(os.path.join(capture, "spatial_magiccookie_*.bin"))
    if len(cookies) != 1:
        sys.exit("expected exactly one magic cookie under %s, found %d"
                 % (capture, len(cookies)))
    cookie = open(cookies[0], "rb").read()

    plain = sorted(glob.glob(os.path.join(capture, "decrypted", "*_extension12.bin")))
    if not plain:
        sys.exit("no decrypted plaintexts under %s/decrypted" % capture)

    with open(out, "wb") as f:
        f.write(b"APACBENCH1\0\0")
        f.write(struct.pack("<I", len(cookie)))
        f.write(cookie)
        f.write(struct.pack("<I", len(plain)))
        total = 0
        for p in plain:
            b = open(p, "rb").read()
            f.write(struct.pack("<I", len(b)))
            f.write(b)
            total += len(b)

    print("%s: %d records, %d plaintext bytes, %d byte cookie, %d bytes total"
          % (out, len(plain), total, len(cookie), os.path.getsize(out)))


if __name__ == "__main__":
    main()
