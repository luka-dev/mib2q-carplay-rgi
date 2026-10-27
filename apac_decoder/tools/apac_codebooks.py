"""Read the APAC Huffman codebooks out of the reference material.

Shared by tools/gen_huff_tables.py (which emits our tables) and
tools/gen_fixtures.sh (which needs a book-0 ENCODER to build test bitstreams
that reach a chosen scalefactor value).

Structure comes from reference/decomp/RODATA.md, "Full entry lists", and is
cross-checked field by field against the library ELF; payload bytes come from
the ELF, and are checked against the committed dumps in
reference/decomp/rodata/ wherever one exists.  See
reference/decomp/LRVQ_SEMANTICS.md section 1.1 for what the fields mean.
"""
import os, re, struct

HERE = os.path.dirname(os.path.abspath(__file__))
DEC  = os.path.dirname(HERE)
RO   = os.path.join(DEC, 'reference', 'decomp', 'rodata')
MD   = os.path.join(DEC, 'reference', 'decomp', 'RODATA.md')

import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '../..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
DEFAULT_LIB = (_WS + '/Firmwares/Other/Pioneer_SPHERA_OTA/'
               'extracted_01030010/rootfs/usr/local/lib/libSPF_CP_PlugInWrapper.so')


def dump(addr):
    """The committed .rodata dump for an address, or None if none was cut."""
    path = os.path.join(RO, 'apac_rodata_%08x.bin' % addr)
    if not os.path.exists(path):
        return None
    with open(path, 'rb') as fh:
        return fh.read()


class Elf(object):
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        phoff, = struct.unpack_from('<Q', self.d, 0x20)
        phentsize, phnum = struct.unpack_from('<HH', self.d, 0x36)
        self.segs = []
        for i in range(phnum):
            o = phoff + i * phentsize
            t, _, off, va, _, filesz, _, _ = struct.unpack_from('<IIQQQQQQ', self.d, o)
            if t == 1:
                self.segs.append((va, off, filesz))

    def read(self, vaddr, n):
        for va, off, sz in self.segs:
            if va <= vaddr < va + sz:
                fo = off + (vaddr - va)
                return self.d[fo:fo + n]
        raise KeyError('vaddr 0x%x not mapped' % vaddr)


def load(libpath=None):
    """[{n, desc, entries, pairs:[(shift, addr)], payload:[[u16]]}] x 12."""
    elf = Elf(libpath or DEFAULT_LIB)
    text = open(MD).read().replace('—', '---')
    books = {}
    for m in re.finditer(r'\* codebook (\d+) --- descriptor `0x([0-9a-f]+)`, '
                         r'(\d+) entries at `0x([0-9a-f]+)`:\n\s*(`[^\n]*`)', text):
        idx, desc, nent, entaddr, pairs = m.groups()
        got = [(int(a), int(p, 16)) for a, p in
               re.findall(r'\((\d+), 0x([0-9a-f]+)\)', pairs)]
        assert len(got) == int(nent), (idx, len(got), nent)
        books[int(idx)] = dict(desc=int(desc, 16), entries=int(entaddr, 16), pairs=got)
    assert sorted(books) == list(range(12)), sorted(books)

    missing = []
    for idx, b in sorted(books.items()):
        entptr, n = struct.unpack('<QQ', elf.read(b['desc'], 16))
        assert entptr == b['entries'] and n == len(b['pairs']), idx
        b['n'] = n
        raw = elf.read(b['entries'], 16 * (n + 1))
        for k in range(n + 1):
            a, p = struct.unpack_from('<QQ', raw, 16 * k)
            if k < n:
                assert (a, p) == b['pairs'][k], (idx, k)
            else:
                b['pairs'].append((a, p))     # the guard, not listed in RODATA.md
        b['payload'] = []
        for k, (a, p) in enumerate(b['pairs']):
            cnt = 2 if k == n else 1 << (31 - a)
            bs = elf.read(p, 2 * cnt)
            d = dump(p)
            if d is None:
                missing.append(p)
            else:
                assert d[:2 * cnt] == bs, (idx, k, hex(p))
            b['payload'].append(list(struct.unpack('<%dH' % cnt, bs)))
    return [books[i] for i in range(12)], missing


def codes(book):
    """symbol-payload-word -> bit string, for every codeword in one book.

    entries[k] owns prefix length k: the codeword is k ones, a terminating
    zero, and then the leading bits of the payload slot index.  The guard
    record entries[n] is n ones and nothing else.
    """
    out = {}
    n = book['n']
    for k, (a, _) in enumerate(book['pairs']):
        payload = book['payload'][k]
        if k == n:
            out.setdefault(payload[0], '1' * n)
            continue
        idxbits = 31 - a
        for s, word in enumerate(payload):
            length = word & 0x3f
            tail = format(s, '0%db' % idxbits)[:length - k - 1] if length > k + 1 else ''
            out.setdefault(word, '1' * k + '0' + tail)
    return out
