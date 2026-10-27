#!/usr/bin/env python3
"""Emit src/lrvq_tables.cpp - every constant APACLRVQDecoder reads.

Nothing here is retyped or recomputed.  Each table is taken from the committed
byte-for-byte dump in reference/decomp/rodata/ and, when the library ELF is
available, asserted equal to the bytes still in it.  The VQ mode table at
0x42b1f8 lives in .data.rel.ro and its two codebook pointers are relocations,
so those are resolved out of the ELF's relocation table and turned into offsets
into the one contiguous codebook block.

    apac_decoder/tools/gen_lrvq_tables.py [libSPF_CP_PlugInWrapper.so]
"""
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RODATA = os.path.join(ROOT, 'reference', 'decomp', 'rodata')
OUT = os.path.join(ROOT, 'src', 'lrvq_tables.cpp')
DEFAULT_LIB = os.path.join(ROOT, 'reference', '.build', 'libs',
                           'libSPF_CP_PlugInWrapper.so')

# The VQ codebook block, 0x310290 .. 0x317e90.  Five tables, one after the
# other, 992 vectors of 8 floats.  They are emitted as ONE array so that the
# out-of-range reads the library's own bound checks let through - index == n
# (the check is `n < idx`, not `n <= idx`) and the -1 a reuse flag could feed
# in - land on the same bytes they land on in the library.
VQ_BLOCK = [
    (0x310290, 16384, 'cb512', '512 vectors, modes 3, 9, 10 stage 0'),
    (0x314290,  8192, 'cb256', '256 vectors, modes 2, 7, 8 stage 0; mode 10 stage 1'),
    (0x316290,  4096, 'cb128', '128 vectors, modes 1, 5, 6 stage 0; modes 8, 9 stage 1'),
    (0x317290,  2048, 'cb64',  ' 64 vectors, modes 0, 4 stage 0; modes 6, 7 stage 1'),
    (0x317a90,  1024, 'cb32',  ' 32 vectors, modes 4, 5 stage 1'),
]
VQ_BASE = 0x310290
VQ_END = 0x317e90

LSF_CODEBOOKS = [
    ('kLrvqLsfCB11',      0x32be90, 16384,
     'kCodebook_Lrvq_Lsf_CB11, float[512][8], stage 1 for lsf[0..7]'),
    ('kLrvqLsfCB11Short', 0x323e90, 16384,
     'kCodebook_Lrvq_Lsf_CB11_SHORT, the shortLsfBooks variant of CB11'),
    ('kLrvqLsfCB12',      0x327e90, 16384,
     'kCodebook_Lrvq_Lsf_CB12, float[512][8], stage 1 for lsf[8..15]'),
    ('kLrvqLsfCB12Short', 0x31fe90, 16384,
     'kCodebook_Lrvq_Lsf_CB12_SHORT, the shortLsfBooks variant of CB12'),
    ('kLrvqLsfCB21',      0x31de90,  8192,
     'kCodebook_Lrvq_Lsf_CB21, float[512][4], stage 2 refinement of lsf[0..3]'),
    ('kLrvqLsfCB22',      0x31be90,  8192,
     'kCodebook_Lrvq_Lsf_CB22, float[512][4], stage 2 refinement of lsf[4..7]'),
    ('kLrvqLsfCB23',      0x317e90, 16384,
     'kCodebook_Lrvq_Lsf_CB23, float[512][8], stage 2 refinement of lsf[8..15]'),
]

MODE_TABLE = 0x42b1f8
RAMP32 = 0x32ff00
HALF = 0x32fef8
ZERO = 0x32fefc


class Elf(object):
    """Just enough of an ELF reader to fetch bytes at a virtual address and to
    resolve R_AARCH64_RELATIVE relocations."""

    def __init__(self, path):
        self.ok = os.path.exists(path)
        if not self.ok:
            return
        self.data = open(path, 'rb').read()
        self.secs = []
        out = subprocess.check_output(['gobjdump', '-h', path]).decode()
        for line in out.splitlines():
            m = re.match(r'\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+'
                         r'([0-9a-f]+)\s+([0-9a-f]+)', line)
            if m:
                self.secs.append((int(m.group(2), 16), int(m.group(3), 16),
                                  int(m.group(5), 16)))
        self.relocs = {}
        out = subprocess.check_output(['gobjdump', '-R', path]).decode()
        for line in out.splitlines():
            m = re.match(r'^([0-9a-f]+)\s+\S+\s+\*ABS\*\+0x([0-9a-f]+)', line)
            if m:
                self.relocs[int(m.group(1), 16)] = int(m.group(2), 16)

    def read(self, addr, n):
        for size, vma, off in self.secs:
            if vma <= addr < vma + size:
                return self.data[off + addr - vma:off + addr - vma + n]
        raise KeyError(hex(addr))


def dump(addr, n):
    path = os.path.join(RODATA, 'apac_rodata_%08x.bin' % addr)
    blob = open(path, 'rb').read()
    if len(blob) < n:
        sys.exit('%s: %d bytes, need %d' % (path, len(blob), n))
    return blob[:n]


def main():
    lib = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_LIB
    elf = Elf(lib)
    checked = 0

    def bytes_at(addr, n):
        """The committed dump, asserted equal to the library."""
        nonlocal checked
        blob = dump(addr, n)
        if elf.ok:
            if blob != elf.read(addr, n):
                sys.exit('0x%08x: the committed dump and %s disagree'
                         % (addr, lib))
            checked += n
        return blob

    if not elf.ok:
        sys.stderr.write('warning: %s is missing - the dumps are used '
                         'unverified\n' % lib)

    out = []
    w = out.append
    w('// APACLRVQDecoder constant tables - COPIED byte-for-byte out of')
    w('// libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010, image base 0).')
    w('//')
    w('// GENERATED by tools/gen_lrvq_tables.py from the dumps in')
    w('// reference/decomp/rodata/, each asserted equal to the bytes still in')
    w('// the library ELF.  Do not edit by hand; edit the generator and re-run')
    w('// it from apac_decoder/:')
    w('//')
    w('//     python3 tools/gen_lrvq_tables.py')
    w('//')
    w('// Float32 BIT PATTERNS, not decimal literals.  The VQ codebooks are')
    w('// dense trained vectors with no closed form at all, and even ramp32 -')
    w('// which looks like 1 + i/31 - stores four-decimal roundings of that,')
    w('// not the quotients.')
    w('')
    w('#include "apac/lrvq.h"')
    w('')
    w('namespace apac {')
    w('namespace tables {')
    w('')

    # ---- the VQ codebook block ------------------------------------------
    blob = b''
    for addr, n, name, what in VQ_BLOCK:
        if VQ_BASE + len(blob) != addr:
            sys.exit('VQ block is not contiguous at 0x%08x' % addr)
        blob += bytes_at(addr, n)
    if VQ_BASE + len(blob) != VQ_END:
        sys.exit('VQ block ends at 0x%08x, expected 0x%08x'
                 % (VQ_BASE + len(blob), VQ_END))
    words = struct.unpack('<%dI' % (len(blob) // 4), blob)
    w('// The whole VQ codebook block, 0x%08x .. 0x%08x - %d vectors of 8'
      % (VQ_BASE, VQ_END, len(words) // 8))
    w('// floats, five tables laid end to end:')
    for addr, n, name, what in VQ_BLOCK:
        w('//   +%6d floats  0x%08x  %s' % ((addr - VQ_BASE) // 4, addr, what))
    w('// Kept as ONE array on purpose, in the order the library has them, so')
    w('// that a stage-1 codebook that a two-stage mode reaches from a')
    w('// stage-0 one is at the same relative offset here as there.  Two reads')
    w('// the library does not prevent - idx == n, because its bound check is')
    w('// `n < idx` and not `n <= idx`, and the -1 stage-1 sentinel - would')
    w('// leave even this array at its two ends; both are unreachable from any')
    w('// bitstream and tests/test_lrvq.cpp is where that stops being true.')
    w('const uint32_t kLrvqVQBits[%d] = {' % len(words))
    for i in range(0, len(words), 6):
        w('    ' + ' '.join('0x%08xu,' % v for v in words[i:i + 6]))
    w('};')
    w('')

    # ---- ramp32, 0.5f, 0.0f ---------------------------------------------
    ramp = struct.unpack('<32I', bytes_at(RAMP32, 128))
    w('// 0x%08x, 32 floats - the divisor the shortLsfBooks output layout' % RAMP32)
    w('// applies to each window\'s first 32 bins.  A ramp from 1.0 to 2.0, but')
    w('// stored as four-decimal roundings of 1 + i/31 (1.0323, 1.0645, ...),')
    w('// so it is NOT reproducible from the formula.')
    w('const uint32_t kLrvqRamp32Bits[32] = {')
    for i in range(0, 32, 6):
        w('    ' + ' '.join('0x%08xu,' % v for v in ramp[i:i + 6]))
    w('};')
    w('')
    half = struct.unpack('<I', bytes_at(HALF, 4))[0]
    zero = struct.unpack('<I', bytes_at(ZERO, 4))[0]
    w('// 0x%08x and 0x%08x - the two vDSP scalars in the VQ loop: the 0.5'
      % (HALF, ZERO))
    w('// a two-stage mode averages with, and the 0.0 a single-stage mode adds.')
    w('const uint32_t kLrvqHalfBits = 0x%08xu;   // %g' % (half, struct.unpack('<f', struct.pack('<I', half))[0]))
    w('const uint32_t kLrvqZeroBits = 0x%08xu;   // %g' % (zero, struct.unpack('<f', struct.pack('<I', zero))[0]))
    w('')

    # ---- the LSF codebooks ----------------------------------------------
    for name, addr, n, what in LSF_CODEBOOKS:
        blob = bytes_at(addr, n)
        words = struct.unpack('<%dI' % (n // 4), blob)
        w('// 0x%08x, %d floats - %s.' % (addr, len(words), what))
        w('// LSF frequencies in Hz for a 24000 Hz rate.')
        w('const uint32_t %sBits[%d] = {' % (name, len(words)))
        for i in range(0, len(words), 6):
            w('    ' + ' '.join('0x%08xu,' % v for v in words[i:i + 6]))
        w('};')
        w('')

    w('} // namespace tables')
    w('')

    # ---- the mode table --------------------------------------------------
    if not elf.ok:
        sys.exit('the VQ mode table at 0x%08x lives in .data.rel.ro and its '
                 'codebook pointers are relocations, so the library is '
                 'required to emit it' % MODE_TABLE)
    raw = elf.read(MODE_TABLE, 16 * 32)
    w('// The VQ mode table at 0x%08x: 16 records of 32 bytes.  `lo`/`hi` are'
      % MODE_TABLE)
    w('// a bit-budget range the DECODER never reads - they are how the encoder')
    w('// picks a mode - and are carried only so the record matches the binary.')
    w('// Rows 11..15 are the invalid ones: both codebook pointers NULL, both')
    w('// bit counts zero, lo = hi = 999, totalBits = 99.  The codebook')
    w('// pointers are R_AARCH64_RELATIVE relocations in the library; here they')
    w('// are offsets into kLrvqVQCodebook, resolved from the relocation table.')
    w('const LrvqMode kLrvqModes[16] = {')
    for i in range(16):
        rec = raw[i * 32:(i + 1) * 32]
        idx, lo, hi, tot = struct.unpack_from('<4H', rec, 0)
        b0, n0, b1, n1 = struct.unpack_from('<4h', rec, 24)
        p0 = elf.relocs.get(MODE_TABLE + i * 32 + 8, 0)
        p1 = elf.relocs.get(MODE_TABLE + i * 32 + 16, 0)
        if idx != i:
            sys.exit('mode row %d says index %d' % (i, idx))
        if p0 and not (VQ_BASE <= p0 < VQ_END):
            sys.exit('row %d cb0 0x%x is outside the codebook block' % (i, p0))
        if p1 and not (VQ_BASE <= p1 < VQ_END):
            sys.exit('row %d cb1 0x%x is outside the codebook block' % (i, p1))
        if p0 and n0 != 1 << b0:
            sys.exit('row %d: n0 %d != 1 << %d' % (i, n0, b0))
        if p1 and n1 != 1 << b1:
            sys.exit('row %d: n1 %d != 1 << %d' % (i, n1, b1))
        if b0 + b1 != tot and p0:
            sys.exit('row %d: bits0 + bits1 %d != totalBits %d'
                     % (i, b0 + b1, tot))
        o0 = (p0 - VQ_BASE) // 4 if p0 else -1
        o1 = (p1 - VQ_BASE) // 4 if p1 else -1
        w('    { %2d, %4d, %4d, %2d, %6d, %6d, %d, %3d, %d, %3d },   // %s'
          % (idx, lo, hi, tot, o0, o1, b0, n0, b1, n1,
             ('invalid' if not p0 else
              ('%d bits, 0x%08x' % (b0, p0) if not p1 else
               '%d + %d bits, 0x%08x + 0x%08x' % (b0, b1, p0, p1)))))
    w('};')
    w('')
    w('const float* const kLrvqVQCodebook =')
    w('    reinterpret_cast<const float*>(tables::kLrvqVQBits);')
    w('const float* const kLrvqRamp32 =')
    w('    reinterpret_cast<const float*>(tables::kLrvqRamp32Bits);')
    for name, addr, n, what in LSF_CODEBOOKS:
        w('const float* const %s =' % name)
        w('    reinterpret_cast<const float*>(tables::%sBits);' % name)
    w('')
    w('} // namespace apac')

    with open(OUT, 'w') as fh:
        fh.write('\n'.join(out) + '\n')
    sys.stderr.write('%s: %d bytes of table, %d of them re-checked against %s\n'
                     % (OUT, 31744 + 128 + 8 + sum(n for _, _, n, _ in LSF_CODEBOOKS),
                        checked, os.path.basename(lib)))


if __name__ == '__main__':
    main()
