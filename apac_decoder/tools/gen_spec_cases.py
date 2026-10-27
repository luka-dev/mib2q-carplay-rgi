#!/usr/bin/env python3
"""Case list for `apac_fn_ref spec` - APACSpectralData::Deserialize.

Written to stdout.  Fixed seed, for the same reason as the other generators.

The scalefactor object is handed over whole rather than decoded from the
stream, so a case can aim a chosen codebook at a chosen band without having to
find a bitstream that produces that record set.

Families:
  A  one codebook at a time, 16 bands x 8 coefficients, random bitstream -
     the broad sweep; every book's decode path, sign handling and refill
  B  bitstreams built from EXPLICIT codewords chosen to force the extreme
     field values, so the NEGATIVE WING of the i^(4/3) table (books 1..6 index
     it at -4 .. -1) is reached deliberately and not by luck
  C  codebook 11's escape, every prefix length k = 0..8, including magnitudes
     above 1023 where the library leaves the table for pow(v, 4/3)
  D  mixed record sets, including codebook-0 runs (whole bands zeroed) and a
     maxSfb below numBands, which leaves a tail to zero
  E  the real 48 kHz LONG layout, 49 bands, 1024 coefficients
  F  the real 48 kHz SHORT layout, 14 bands, every window grouping from eight
     groups of one to one group of eight - the band-interleaved staging and
     the de-interleave that unpacks it
  G  edges: maxSfb 0 (whole spectrum zeroed, no bits read), numWindowGroups 0,
     an empty buffer, and buffers too short to finish (0xFF tail padding)
"""
import os, random, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import apac_codebooks as CB

DEC = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rnd = random.Random(0x53504543)          # 'SPEC'
books, _ = CB.load(sys.argv[1] if len(sys.argv) > 1 else None)
CODES = [CB.codes(b) for b in books]


def sfb48():
    """The 48 kHz band offsets, out of the committed library-produced fixture."""
    lng = shrt = None
    path = os.path.join(DEC, 'tests', 'fixtures', 'sfb_offsets.fixture')
    want = False
    for line in open(path):
        if line.startswith('SFB 48000 1024'):
            want = True
        elif want and line.startswith('LONG'):
            lng = [int(x) for x in line.split()[1:]]
        elif want and line.startswith('SHORT'):
            shrt = [int(x) for x in line.split()[1:]]
            break
    assert lng and shrt, 'no 48 kHz record in sfb_offsets.fixture'
    return lng, shrt


OFF_LONG, OFF_SHORT = sfb48()
OFF_15x8 = [8 * i for i in range(16)]


class BW(object):
    def __init__(self):
        self.b = []

    def put(self, v, n):
        for i in range(n - 1, -1, -1):
            self.b.append((v >> i) & 1)

    def raw(self, s):
        self.b.extend(int(c) for c in s)

    def hexpad(self, minbytes):
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


def scfbits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


cases = []


def emit(ws, maxsfb, groups, groupword, off, nrec, rec, scf, bufhex):
    numbands = len(off) - 1
    if groups:
        numout = off[numbands] * sum(((groupword >> (4 * g)) & 0xF)
                                     for g in range(min(groups, 8)))
    else:
        numout = off[numbands]
    # Both arrays have a stride of 15 per window group, but a single-group long
    # block legitimately runs past 15: 49 bands and up to 49 records all live in
    # group 0's slice because there is no group 1.  Carry exactly what the case
    # uses; apac_fn_ref zero-pads the rest of the 120-entry struct.
    used = max(15 * max(1, min(groups, 8)), maxsfb, len(rec), len(scf))
    assert used <= 120
    recb = struct.pack('<%dH' % used, *(rec + [0] * used)[:used])
    scfb = struct.pack('<%df' % used, *(scf + [0.0] * used)[:used])
    nb = bytearray(8)
    for i, v in enumerate(nrec[:8]):
        nb[i] = v
    offb = struct.pack('<%dH' % len(off), *off)
    cases.append('SPEC %d %d %d %d %d %d %d\nOFF %s\nNREC %s\nREC %s\nSCF %s\nBUF %s'
                 % (len(cases), ws, maxsfb, groups, groupword, numbands, numout,
                    offb.hex(), nb.hex(), recb.hex(), scfb.hex(), bufhex))


def randscf(n):
    out = []
    for _ in range(n):
        k = rnd.randrange(8)
        if k == 0:
            out.append(0.0)
        elif k == 1:
            out.append(-1.0 * (1 << rnd.randrange(6)))     # negative: catches
        elif k == 2:                                        # the -0.0 lanes
            out.append(-0.5)
        else:
            out.append(2.0 ** ((rnd.randrange(200) - 100) / 4.0))
    return out


def randbuf(n):
    return bytes(rnd.randrange(256) for _ in range(n)).hex()


def mkrecords(maxsfb, maxrec, zerop=0.25):
    """Partition maxsfb bands into at most maxrec (codebook, run) records.

    A run of ZERO would be fatal: the library zeroes a codebook-0 band with a
    do/while that decrements by four and stops only on exactly zero, so an
    empty run runs away and writes over everything.  APACScaleFactorData
    rejects a zero run, and so must every case here.
    """
    k = 1 + rnd.randrange(min(maxsfb, maxrec))
    cuts = sorted(rnd.sample(range(1, maxsfb), k - 1)) if k > 1 else []
    runs = [b - a for a, b in zip([0] + cuts, cuts + [maxsfb])]
    rec = [(0 if rnd.random() < zerop else 1 + rnd.randrange(11)) | (r << 8)
           for r in runs]
    return len(rec), rec


# --- A: one codebook at a time, 16 bands of 8 -------------------------------
for cb in range(1, 12):
    for _ in range(16):
        nb = 15
        rec = [cb | (1 << 8)] * nb
        emit(0, nb, 1, 1, OFF_15x8, [nb], rec, randscf(nb), randbuf(384))

# --- B: explicit codewords, the extreme field values ------------------------
def sext(v, bits):
    return v - (1 << bits) if v & (1 << (bits - 1)) else v

def fields(cb, word):
    if cb <= 2:
        return [sext((word >> s) & 3, 2) for s in (14, 12, 10, 8)]
    if cb <= 4:
        return [(word >> s) & 3 for s in (14, 12, 10, 8)]
    if cb <= 6:
        return [sext((word >> 11) & 0xF, 4), sext((word >> 6) & 0xF, 4)]
    if cb <= 10:
        return [(word >> 11) & 0xF, (word >> 6) & 0xF]
    return [word >> 11, (word >> 6) & 0x1F]

for cb in range(1, 7):
    # every word whose first field is the most negative this book offers, and
    # every word whose second field is - both index X below zero.
    words = list(CODES[cb])
    lo = min(min(fields(cb, w)) for w in words)
    picked = [w for w in words if min(fields(cb, w)) == lo]
    picked += [w for w in words if max(fields(cb, w)) == max(max(fields(cb, x))
                                                            for x in words)]
    step = 4 if cb <= 4 else 2
    for trial in range(6):
        w = BW()
        for _ in range(120 // step + 8):
            word = picked[rnd.randrange(len(picked))]
            w.raw(CODES[cb][word])
            if 3 <= cb <= 4:
                for v in fields(cb, word):
                    if v:
                        w.put(rnd.randrange(2), 1)
        nb = 15
        emit(0, nb, 1, 1, OFF_15x8, [nb], [cb | (1 << 8)] * nb,
             randscf(nb), w.hexpad(384))

# --- C: codebook 11's escape, every prefix length ---------------------------
ESC_WORDS = [w for w in CODES[11] if (w >> 11) == 16 or ((w >> 6) & 0x1F) == 16]
assert ESC_WORDS
for k in range(9):
    for trial in range(3):
        w = BW()
        for _ in range(80):
            word = ESC_WORDS[rnd.randrange(len(ESC_WORDS))]
            m0, m1 = word >> 11, (word >> 6) & 0x1F
            w.raw(CODES[11][word])
            if m0:
                w.put(rnd.randrange(2), 1)
            if m1:
                w.put(rnd.randrange(2), 1)
            for m in (m0, m1):
                if m == 16:
                    nb = k + 4
                    w.put((1 << k) - 1, k)
                    w.put(0, 1)
                    w.put(rnd.randrange(1 << nb), nb)
        nb = 15
        emit(0, nb, 1, 1, OFF_15x8, [nb], [11 | (1 << 8)] * nb,
             randscf(nb), w.hexpad(768))

# --- D: mixed record sets ---------------------------------------------------
for _ in range(60):
    nb = 15
    maxsfb = rnd.choice([4, 8, 12, 15])
    n, rec = mkrecords(maxsfb, 15, 0.3)
    emit(0, maxsfb, 1, 1, OFF_15x8, [n], rec, randscf(nb), randbuf(384))

# --- E: the real 48 kHz long layout ----------------------------------------
for _ in range(12):
    nb = len(OFF_LONG) - 1
    maxsfb = rnd.choice([nb, nb, nb - 1, 30, 12])
    n, rec = mkrecords(maxsfb, 40)
    emit(0, maxsfb, 1, 1, OFF_LONG, [n], rec, randscf(nb), randbuf(2560))

# --- F: the real 48 kHz short layout, every grouping ------------------------
GROUPINGS = [
    (8, 0x11111111), (1, 0x00000008), (2, 0x00000044), (2, 0x00000062),
    (3, 0x00000332), (3, 0x00000116), (4, 0x00001421), (4, 0x00002222),
    (5, 0x00022211), (6, 0x00311111), (7, 0x02111111), (2, 0x00000017),
]
for groups, word in GROUPINGS:
    assert sum((word >> (4 * g)) & 0xF for g in range(groups)) == 8, hex(word)
    for _ in range(3):
        nb = len(OFF_SHORT) - 1
        maxsfb = rnd.choice([nb, nb, nb - 2, 6])
        nrec, rec = [], [0] * (15 * groups)
        for g in range(groups):
            n, rr = mkrecords(maxsfb, 15)
            for i, v in enumerate(rr):
                rec[15 * g + i] = v
            nrec.append(n)
        scf = [0.0] * (15 * groups)
        for g in range(groups):
            for i, v in enumerate(randscf(nb)):
                scf[15 * g + i] = v
        emit(2, maxsfb, groups, word, OFF_SHORT, nrec, rec, scf, randbuf(2560))

# --- G: edges ---------------------------------------------------------------
emit(0, 0, 1, 1, OFF_15x8, [15], [1 | (1 << 8)] * 15, randscf(15), randbuf(64))
emit(2, 0, 8, 0x11111111, OFF_SHORT, [1] * 8, [1 | (14 << 8)] * 15 * 8,
     randscf(120), randbuf(64))
emit(0, 15, 0, 1, OFF_15x8, [15], [1 | (1 << 8)] * 15, randscf(15), randbuf(64))
emit(0, 15, 1, 1, OFF_15x8, [15], [1 | (1 << 8)] * 15, randscf(15), '-')
for n in (1, 2, 3, 4, 7, 8, 16, 32, 64):
    emit(0, 15, 1, 1, OFF_15x8, [15], [11 | (1 << 8)] * 15, randscf(15), randbuf(n))
    emit(2, 14, 1, 0x00000008, OFF_SHORT, [14], [9 | (1 << 8)] * 14,
         randscf(14), randbuf(n))
# every band coded with codebook 0: the whole spectrum zeroed record by record
emit(0, 15, 1, 1, OFF_15x8, [1], [0 | (15 << 8)], randscf(15), randbuf(64))
emit(2, 14, 1, 0x00000008, OFF_SHORT, [1], [0 | (14 << 8)], randscf(14), randbuf(64))
# maxSfb below numBands: a tail of bands the records never reach, which the
# library zeroes with the trailing memset
for maxsfb in (2, 5, 9, 13):
    n, rec = mkrecords(maxsfb, 15)
    emit(0, maxsfb, 1, 1, OFF_15x8, [n], rec, randscf(15), randbuf(384))
    n, rec = mkrecords(maxsfb, 15)
    emit(2, maxsfb, 1, 0x00000008, OFF_SHORT, [n], rec, randscf(14),
         randbuf(2560))

sys.stdout.write('\n'.join(cases) + '\n')
sys.stderr.write('%d SPEC cases\n' % len(cases))
