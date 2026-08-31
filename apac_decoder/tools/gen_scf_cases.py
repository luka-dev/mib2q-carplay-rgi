#!/usr/bin/env python3
"""Case list for `apac_fn_ref scf` - APACScaleFactorData::Deserialize.

Written to stdout.  Fixed seed: a fixture whose cases moved on every run would
make every regeneration an unreadable diff.

Six families, so that every branch of the original is reached:
  A  long blocks, 5-bit runs, random scalefactor payload
  B  short blocks, 3-bit runs, EXPLICITLY ENCODED book-0 deltas - the only way
     to keep a multi-group case in sync, since a random payload consumes an
     unpredictable number of bits and desynchronises the next group header
  C  deltas driven deliberately out of [-256, 255], both ways, to reach the two
     precomputed clamp constants
  D  pure random buffers: the reject paths (codebook > 11, run past maxSfb,
     zero run)
  E  escape-coded runs, the all-ones chain on both 3-bit and 5-bit fields
  F  edges: maxSfb 0, numWindowGroups 0, empty and truncated buffers
"""
import os, random, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import apac_codebooks as CB

rnd = random.Random(0x53434644)          # 'SCFD'
books, _ = CB.load(sys.argv[1] if len(sys.argv) > 1 else None)
BOOK0 = CB.codes(books[0])
# delta -> shortest codeword.  The payload word is (symbol << 6) | length and
# the symbol is signed, so the delta is (int16)word >> 6.
DELTA = {}
for word, bits in BOOK0.items():
    d = ((word - 0x10000) if word & 0x8000 else word) >> 6
    if d not in DELTA or len(bits) < len(DELTA[d]):
        DELTA[d] = bits
DMIN, DMAX = min(DELTA), max(DELTA)


class BW(object):
    def __init__(self):
        self.b = []

    def put(self, v, n):
        for i in range(n - 1, -1, -1):
            self.b.append((v >> i) & 1)

    def raw(self, s):
        self.b.extend(int(c) for c in s)

    def hex(self, minbytes=0):
        b = self.b + [rnd.randrange(2) for _ in range((-len(self.b)) % 8)]
        out = bytearray()
        for i in range(0, len(b), 8):
            v = 0
            for j in range(8):
                v = (v << 1) | b[i + j]
            out.append(v)
        while len(out) < minbytes:
            out.append(rnd.randrange(256))
        return out.hex() if out else '-'


def runfields(w, run, runbits):
    esc = (1 << runbits) - 1
    rem = run
    while rem >= esc:
        w.put(esc, runbits)
        rem -= esc
    w.put(rem, runbits)


cases = []


def emit(ws, maxsfb, groups, hexbuf):
    cases.append('SCF %d %d %d %d %s' % (len(cases), ws, maxsfb, groups, hexbuf))


def structured(ws, maxsfb, groups, deltas=None, zerochance=0.25, pad=1024):
    """One case: valid record headers, then either explicit deltas or noise."""
    runbits = 3 if ws == 2 else 5
    w = BW()
    w.put(rnd.randrange(256), 8)                       # start value
    for _ in range(groups):
        left = maxsfb
        while left:
            cb = 0 if rnd.random() < zerochance else 1 + rnd.randrange(11)
            run = 1 + rnd.randrange(left)
            w.put(cb, 4)
            runfields(w, run, runbits)
            left -= run
            if cb:
                if deltas is None:
                    for _ in range(run * 4 + 8):
                        w.put(rnd.randrange(2), 1)
                else:
                    for _ in range(run):
                        w.raw(DELTA[deltas()])
    emit(ws, maxsfb, groups, w.hex(pad))


# A - long blocks, random payload.  One group only: a random payload consumes
#     an unpredictable number of bits, so a second group would decode garbage.
for maxsfb in (1, 3, 7, 14, 20, 33, 49):
    for _ in range(14):
        structured(0, maxsfb, 1)

# B - short blocks, explicit deltas, up to eight groups and always in sync.
for groups in (1, 2, 4, 8):
    for maxsfb in (1, 4, 8, 14):
        for _ in range(7):
            structured(2, maxsfb, groups,
                       deltas=lambda: rnd.choice(list(DELTA)))
# ... and long blocks with explicit deltas, several groups.
for groups in (1, 2, 3):
    for maxsfb in (5, 20, 49):
        for _ in range(6):
            structured(0, maxsfb, groups,
                       deltas=lambda: rnd.choice(list(DELTA)))
# the 1-bit `0` code only, the fast path that never touches the codebook
for groups in (1, 8):
    for _ in range(10):
        structured(2, 14, groups, deltas=lambda: 0, zerochance=0.1)

# C - drive the running value out of range, both ways, and back.
for start, step, maxsfb in ((250, DMAX, 49), (5, DMIN, 49), (0, DMAX, 49),
                            (255, DMAX, 49), (0, DMIN, 49), (128, DMAX, 20),
                            (128, DMIN, 20)):
    for tail in (None, 0, DMIN, DMAX):
        w = BW()
        w.put(start, 8)
        w.put(1, 4)                          # codebook 1, one record
        runfields(w, maxsfb, 5)
        for i in range(maxsfb):
            d = step if (tail is None or i < maxsfb // 2) else tail
            w.raw(DELTA[d])
        emit(0, maxsfb, 1, w.hex(512))

# D - random buffers: rejections, and whatever the decoder makes of noise.
for ws in (0, 2):
    for maxsfb in (1, 8, 14, 49):
        for groups in (1, 3, 8):
            for _ in range(4):
                n = rnd.choice([1, 2, 4, 8, 16, 64, 256])
                emit(ws, maxsfb, groups,
                     bytes(rnd.randrange(256) for _ in range(n)).hex())

# E - escape-coded runs: the all-ones chain, and a run that lands exactly on
#     the escape value so one extra field is read for nothing.
for ws, runbits in ((0, 5), (2, 3)):
    esc = (1 << runbits) - 1
    for maxsfb in (esc, esc + 1, 2 * esc, 2 * esc + 1, 49 if ws == 0 else 14):
        if maxsfb > (49 if ws == 0 else 14):
            continue
        for cb in (0, 1, 5, 11):
            w = BW()
            w.put(rnd.randrange(256), 8)
            w.put(cb, 4)
            runfields(w, maxsfb, runbits)
            if cb:
                for _ in range(maxsfb):
                    w.raw(DELTA[rnd.choice(list(DELTA))])
            emit(ws, maxsfb, 1, w.hex(256))

# F - edges.
emit(0, 0, 1, bytes(rnd.randrange(256) for _ in range(16)).hex())
emit(2, 0, 8, bytes(rnd.randrange(256) for _ in range(16)).hex())
emit(0, 49, 0, bytes(rnd.randrange(256) for _ in range(16)).hex())
emit(0, 49, 1, '-')
for n in (1, 2, 3, 4, 5, 7, 8, 9):
    emit(0, 49, 1, bytes(rnd.randrange(256) for _ in range(n)).hex())
    emit(2, 14, 8, bytes(rnd.randrange(256) for _ in range(n)).hex())
# a full record set whose payload runs off the end of the buffer, so the
# reader's 0xFF tail padding decides the last few deltas
for n in (12, 20, 40):
    w = BW()
    w.put(0, 8)
    w.put(9, 4)
    runfields(w, 49, 5)
    emit(0, 49, 1, w.hex(n))

sys.stdout.write('\n'.join(cases) + '\n')
sys.stderr.write('%d SCF cases\n' % len(cases))
