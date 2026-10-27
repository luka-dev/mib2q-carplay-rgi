#!/usr/bin/env python3
"""Dump the APAC Huffman codebooks and the LRVQ mode table out of the library.

    python3 dump_codebooks.py            # human readable
    python3 dump_codebooks.py --codes    # every codeword, per book

Reads the ELF directly (lib.py applies R_AARCH64_RELATIVE relocations), so it
needs no container and no running decoder.
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib import rd, u64, ptr

ARR = 0x424d98          # apac::GetHuffmanTable's 12-entry pointer array


def book(i):
    d = ptr(ARR + 8 * i)
    ents, n = ptr(d), u64(d + 8)
    rows = []
    for k in range(n + 1):                       # n usable prefixes + the guard
        a = u64(ents + 16 * k)
        p = ptr(ents + 16 * k + 8)
        cnt = 1 << (31 - a)
        rows.append((a, p, [struct.unpack('<H', rd(p + 2 * j, 2))[0] for j in range(cnt)]))
    return n, rows


def codes(i):
    """(codeword string, raw uint16) for every distinct codeword in book i."""
    n, rows = book(i)
    out = {}
    for k, (a, _p, vals) in enumerate(rows):
        m = 31 - a
        for j, r in enumerate(vals):
            L = r & 0x3f
            if k == n:                            # guard: the all-ones code
                out['1' * n] = r
                continue
            e = L - k - 1
            suf = (j >> (m - e)) if e else 0
            out[('1' * k) + '0' + (format(suf, '0%db' % e) if e else '')] = r
    return out


def main():
    want_codes = '--codes' in sys.argv
    for i in range(12):
        n, rows = book(i)
        print('=== codebook %d   n=%d  (%s)' % (i, n, 'scalefactors' if i == 0 else 'spectrum'))
        for k, (a, p, vals) in enumerate(rows):
            print('   prefix %2d ones: a=%d -> %2d entries @%08x%s'
                  % (k, a, len(vals), p, '   [guard, code 1^n]' if k == n else ''))
        if want_codes:
            for c, r in sorted(codes(i).items(), key=lambda kv: (len(kv[0]), kv[0])):
                L = r & 0x3f
                if i == 0:
                    sym = struct.unpack('<h', struct.pack('<H', r))[0] >> 6
                    print('     %-20s len=%2d  delta=%d' % (c, L, sym))
                else:
                    print('     %-20s len=%2d  raw=%04x  f15:11=%2d f10:6=%2d'
                          % (c, L, r, (r >> 11) & 0x1f, (r >> 6) & 0x1f))

    print()
    print('=== LRVQ mode table @0x42b1f8, 16 x 32 bytes')
    for i in range(16):
        b = 0x42b1f8 + 32 * i
        idx, lo, hi, bits = struct.unpack('<4H', rd(b, 8))
        t0, t1 = ptr(b + 8), ptr(b + 16)
        b0, n0, b1, n1 = struct.unpack('<4h', rd(b + 24, 8))
        print('  %2d lo=%-5d hi=%-5d totalBits=%-3d stage0(%d bits, %d entries @%08x) stage1(%d bits, %d entries @%08x)'
              % (idx, lo, hi, bits, b0, n0, t0, b1, n1, t1))


main()
