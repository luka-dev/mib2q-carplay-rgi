#!/usr/bin/env python3
"""Emit src/bwe_tables.cpp - every constant stage e (the bandwidth extension)
reads.

Three tables, none of them retyped and none of them generated from a formula:

  0x0033b000  the FIRST LSF codebook, 512 x 16 floats.  Coarse LSFs in Hz for a
              24000 Hz half-band.  `apac::LSFTools::DecodeLSF` adds
              cb0[idx[0]] into a zeroed vector.
  0x00333000  the SECOND LSF codebook, 512 x 16 floats.  The refinement added
              on top.
  0x0030ec80  the 64-entry bandwidth-extension gain table, indexed by the 6-bit
              per-window-group scale field the BWE header carries.

The descriptor pair the library builds these from lives at 0x00425ad8 in
.data.rel.ro: two 16-byte records of {u16 numEntries, u16 dimension, u32 pad,
float* data}, whose data pointers are R_AARCH64_RELATIVE relocations.  This
script resolves them out of the relocation table rather than trusting the
addresses above, and fails if they move.

    apac_decoder/tools/gen_bwe_tables.py [libSPF_CP_PlugInWrapper.so]

The library ELF is REQUIRED: 0x0033b000 has no committed dump in
reference/decomp/rodata/ (only its 0x00333000 sibling does), and the descriptor
pointers are relocations.  The two tables that do have dumps are asserted equal
to the bytes still in the ELF.  The generated file is committed, so `make check`
needs neither the ELF nor Docker.
"""
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RODATA = os.path.join(ROOT, 'reference', 'decomp', 'rodata')
OUT = os.path.join(ROOT, 'src', 'bwe_tables.cpp')
DEFAULT_LIB = os.path.join(ROOT, 'reference', '.build', 'libs',
                           'libSPF_CP_PlugInWrapper.so')
OTA_LIB = ('/Users/luka/Desktop/AUDI_2/Firmwares/Other/Pioneer_SPHERA_OTA/'
           'extracted_01030010/rootfs/usr/local/lib/libSPF_CP_PlugInWrapper.so')

# apac::LSFTools::Initialize walks this pair, 16 bytes per record, pushing one
# apac::LPCodeBook per record.  Record 0 becomes codebooks[0] - the codebook
# DecodeLSF adds first, indexed by the FIRST 9-bit field.
DESCRIPTORS = 0x00425ad8
NUM_CODEBOOKS = 2
GAIN = 0x0030ec80
GAIN_N = 64


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
    if not os.path.exists(path):
        return None
    blob = open(path, 'rb').read()
    if len(blob) < n:
        sys.exit('%s: %d bytes, need %d' % (path, len(blob), n))
    return blob[:n]


def main():
    lib = None
    for cand in (sys.argv[1] if len(sys.argv) > 1 else None,
                 DEFAULT_LIB, OTA_LIB):
        if cand and os.path.exists(cand):
            lib = cand
            break
    if lib is None:
        sys.exit('the library ELF is required; pass its path')
    elf = Elf(lib)
    checked = 0

    def bytes_at(addr, n):
        """The library's bytes, asserted equal to the committed dump when there
        is one."""
        nonlocal checked
        blob = elf.read(addr, n)
        committed = dump(addr, n)
        if committed is not None:
            if blob != committed:
                sys.exit('0x%08x: the committed dump and %s disagree'
                         % (addr, lib))
            checked += n
        return blob

    # ---- the two codebook descriptors ------------------------------------
    books = []
    raw = elf.read(DESCRIPTORS, 16 * NUM_CODEBOOKS)
    for i in range(NUM_CODEBOOKS):
        n, dim, pad = struct.unpack_from('<HHI', raw, i * 16)
        addr = elf.relocs.get(DESCRIPTORS + i * 16 + 8)
        if addr is None:
            sys.exit('0x%08x + %d: no relocation for the data pointer'
                     % (DESCRIPTORS, i * 16 + 8))
        if pad != 0:
            sys.exit('descriptor %d: unexpected padding 0x%x' % (i, pad))
        books.append((n, dim, addr))
    if [d for _, d, _ in books] != [16] * NUM_CODEBOOKS:
        sys.exit('the codebooks are not all 16-dimensional: %r' % (books,))
    if [n for n, _, _ in books] != [512] * NUM_CODEBOOKS:
        sys.exit('the codebooks are not all 512 entries: %r' % (books,))

    out = []
    w = out.append
    w('// APACBWE2Decoder / BWELPC constant tables - COPIED byte-for-byte out')
    w('// of libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010, image base 0).')
    w('//')
    w('// GENERATED by tools/gen_bwe_tables.py.  Do not edit by hand; edit the')
    w('// generator and re-run it from apac_decoder/:')
    w('//')
    w('//     python3 tools/gen_bwe_tables.py [path/to/libSPF_CP_PlugInWrapper.so]')
    w('//')
    w('// Float32 BIT PATTERNS, not decimal literals.  The two LSF codebooks')
    w('// are trained vectors with no closed form, and the gain table is not')
    w('// analytic either - it is not a dB ladder, its ratios drift.')
    w('//')
    w('// The codebook addresses are NOT hardcoded in the generator: they are')
    w('// the relocated data pointers of the two apac::LPCodeBook descriptors')
    w('// at 0x%08x that apac::LSFTools::Initialize walks, in the order it' % DESCRIPTORS)
    w('// pushes them - which is the order apac::LSFTools::DecodeLSF then adds')
    w('// them in, and therefore the order the two 9-bit header fields index.')
    w('')
    w('#include "apac/bwe_lpc.h"')
    w('')
    w('namespace apac {')
    w('namespace tables {')
    w('')

    names = []
    for i, (n, dim, addr) in enumerate(books):
        blob = bytes_at(addr, n * dim * 4)
        words = struct.unpack('<%dI' % (n * dim), blob)
        name = 'kBWELsfCB%dBits' % i
        names.append((name, i, addr, n, dim))
        w('// 0x%08x, %d x %d floats - LSF codebook %d, LSF frequencies in Hz'
          % (addr, n, dim, i))
        if i == 0:
            w('// for a 24000 Hz half-band.  DecodeLSF adds this one into a')
            w('// zeroed vector; it holds the coarse, monotonically rising LSF')
            w('// sets (row 0 runs 776.7 .. 10988.4 Hz).')
        else:
            w('// for a 24000 Hz half-band.  The refinement added on top of')
            w('// codebook 0; its entries are small signed corrections.')
        w('const uint32_t %s[%d] = {' % (name, n * dim))
        for k in range(0, len(words), 6):
            w('    ' + ' '.join('0x%08xu,' % v for v in words[k:k + 6]))
        w('};')
        w('')

    gain = struct.unpack('<%dI' % GAIN_N, bytes_at(GAIN, GAIN_N * 4))
    w('// 0x%08x, %d floats - the bandwidth-extension gain table, indexed by'
      % (GAIN, GAIN_N))
    w('// the 6-bit per-window-group scale field.  Rises from 1.2e-04 to')
    w('// 1.647e+02 and is applied to the whole extension band.')
    w('const uint32_t kBWEGainBits[%d] = {' % GAIN_N)
    for k in range(0, GAIN_N, 6):
        w('    ' + ' '.join('0x%08xu,' % v for v in gain[k:k + 6]))
    w('};')
    w('')
    w('} // namespace tables')
    w('')
    for name, i, addr, n, dim in names:
        w('const float* const kBWELsfCB%d =' % i)
        w('    reinterpret_cast<const float*>(tables::%s);' % name)
    w('const float* const kBWEGain =')
    w('    reinterpret_cast<const float*>(tables::kBWEGainBits);')
    w('')
    w('} // namespace apac')
    w('')

    with open(OUT, 'w') as f:
        f.write('\n'.join(out))
    print('%s: %d bytes of table, %d of them also matched against the '
          'committed dumps in reference/decomp/rodata/'
          % (os.path.relpath(OUT, ROOT),
             sum(n * dim * 4 for n, dim, _ in books) + GAIN_N * 4, checked))


if __name__ == '__main__':
    main()
