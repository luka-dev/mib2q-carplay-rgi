"""Resolve immediate CFString addresses and ADRP/ADD objc selector stubs.

Read-only annotation of ipsw disassembly for this cache. This is not data-flow
analysis: only adjacent ADRP/ADD pairs are resolved. Branch targets are decoded
from instruction bytes; ipsw's a2s names are deliberately not trusted.
"""
import json
import re
import struct
import sys
from pathlib import Path
from read_cache import Cache

cache = Cache()

def cfstring(address):
    # Reject block descriptors that happen to resemble the four-word layout.
    if struct.unpack('<Q', cache.read(address + 8, 8))[0] != 0x7c8:
        raise ValueError('Not a UTF-8 constant CFString in this cache')
    return cache.cfstring(address)

def selector(target):
    adrp, add, branch = struct.unpack('<III', cache.read(target, 12))
    if adrp & 0x9f00001f != 0x90000001:
        return None
    if add & 0xffc003ff != 0x91000021 or branch & 0xfc000000 != 0x14000000:
        return None
    imm = ((adrp >> 5 & 0x7ffff) << 2) | (adrp >> 29 & 3)
    if imm & (1 << 20):
        imm -= 1 << 21
    address = (target & ~0xfff) + (imm << 12) + (add >> 10 & 0xfff)
    return {'address': hex(address), 'selector': cache.read(address, 256).split(b'\0')[0].decode()}

def annotate(path):
    lines = path.read_text().splitlines()
    result = []
    prev = None
    prevprev = None
    for line in lines:
        match = re.match(r'(0x[0-9a-f]+):\s+((?:[0-9a-f]{2} ){3}[0-9a-f]{2})\s+(\w+)\s+(.*)', line)
        if not match:
            continue
        address, raw, op, args = match.groups()
        if op == 'add' and prev and prev[0] == 'adrp':
            p = re.match(r'(x\d+), (0x[0-9a-f]+)', prev[1])
            a = re.match(r'(x\d+), (x\d+), #(0x[0-9a-f]+|\d+)', args)
            if p and a and p[1] == a[2]:
                target = int(p[2], 0) + int(a[3], 0)
                try:
                    value = cfstring(target)
                    if value['value']:
                        result.append({'instruction': address, 'register': a[1], 'cfstring': value})
                except (ValueError, UnicodeError):
                    pass
        if op == 'ldr' and prev and prevprev and prev[0] == 'ldr' and prevprev[0] == 'adrp':
            p = re.match(r'(x\d+), (0x[0-9a-f]+)', prevprev[1])
            a = re.match(r'(x\d+), \[(x\d+), #(0x[0-9a-f]+|\d+)\]', prev[1])
            b = re.match(r'(x\d+), \[(x\d+)\]', args)
            if p and a and b and p[1] == a[2] and a[1] == b[2]:
                got = int(p[2], 0) + int(a[3], 0)
                try:
                    target = cache.pointer(struct.unpack('<Q', cache.read(got, 8))[0])
                    target = cache.pointer(struct.unpack('<Q', cache.read(target, 8))[0])
                    value = cfstring(target)
                    if value['value']:
                        result.append({'instruction': address, 'register': b[1], 'got': hex(got), 'cfstring': value})
                except (ValueError, UnicodeError):
                    pass
        if op in ('bl', 'b'):
            insn = int.from_bytes(bytes.fromhex(raw), 'little')
            displacement = insn & 0x3ffffff
            if displacement & (1 << 25):
                displacement -= 1 << 26
            target = int(address, 0) + (displacement << 2)
            try:
                value = selector(target)
                if value:
                    result.append({'instruction': address, 'stub': hex(target), **value})
            except (ValueError, UnicodeError):
                pass
        prevprev, prev = prev, (op, args)
    return result

if __name__ == '__main__':
    print(json.dumps({p: annotate(Path(p)) for p in sys.argv[1:]}, indent=2, ensure_ascii=False))
