"""Read CFStrings in the local 24A5418b dyld cache; no writes to the cache.

This cache's v5 rebases use a 34-bit runtime offset from the shared-region
base. The decoded pointer must resolve to an actual cache mapping.
"""
from pathlib import Path
import argparse
import json
import struct

ROOT = Path('/Users/luka/Desktop/AUDI_2/Refferences/ios27_dsc/24A5418b__iPhone18,1')

class Cache:
    def __init__(self):
        self.maps = []
        for path in ROOT.glob('dyld_shared_cache_arm64e*'):
            if not path.is_file():
                continue
            with path.open('rb') as file:
                header = file.read(32)
                if not header.startswith(b'dyld_'):
                    continue
                offset, count = struct.unpack_from('<II', header, 16)
                file.seek(offset)
                for _ in range(count):
                    va, size, pos, _, _ = struct.unpack('<QQQII', file.read(32))
                    self.maps.append((va, size, pos, path))
        self.base = min(x[0] for x in self.maps)

    def read(self, address, size):
        for start, length, offset, path in self.maps:
            if start <= address and address + size <= start + length:
                with path.open('rb') as file:
                    file.seek(offset + address - start)
                    return file.read(size)
        raise ValueError(f'Unmapped range: {address:#x}+{size}')

    def pointer(self, value):
        decoded = self.base + (value & ((1 << 34) - 1))
        self.read(decoded, 1)
        return decoded

    def cfstring(self, address):
        fields = struct.unpack('<QQQQ', self.read(address, 32))
        if fields[3] > 4096:
            raise ValueError('Not a short CFString')
        target = self.pointer(fields[2])
        return {'address': hex(address), 'raw_pointer': hex(fields[2]),
                'string_address': hex(target),
                'value': self.read(target, fields[3]).decode('utf-8')}

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('addresses', nargs='+')
    parser.add_argument('--indirect', type=int, default=0)
    args = parser.parse_args()
    cache = Cache()
    result = []
    for text in args.addresses:
        address = int(text, 0)
        for _ in range(args.indirect):
            address = cache.pointer(struct.unpack('<Q', cache.read(address, 8))[0])
        result.append(cache.cfstring(address))
    print(json.dumps(result, indent=2, ensure_ascii=False))
