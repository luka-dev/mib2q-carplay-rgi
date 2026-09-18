#!/usr/bin/env python3
"""Build a local QNX installer payload; never connects to or modifies a HU."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('vc_verify', ROOT / 'native/capture_clock/verify_firmware.py')
firmware = importlib.util.module_from_spec(spec)
spec.loader.exec_module(firmware)


def verify_library(path):
    data = Path(path).read_bytes()
    if data[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<HH', data, 16) != (3, 40):
        raise ValueError('capture library must be ELF32 ARM little-endian ET_DYN')
    shoff = struct.unpack_from('<I', data, 32)[0]
    shsize, count = struct.unpack_from('<HH', data, 46)
    sections = [struct.unpack_from('<10I', data, shoff + i * shsize) for i in range(count)]
    exports = {}
    for sec in sections:
        if sec[1] != 11:  # SHT_DYNSYM
            continue
        strings = sections[sec[6]]
        names = data[strings[4]:strings[4] + strings[5]]
        for pos in range(sec[4], sec[4] + sec[5], sec[9]):
            name, value, size, info, other, index = struct.unpack_from('<IIIBBH', data, pos)
            end = names.find(b'\0', name)
            text = names[name:end].decode('ascii')
            if 'emutls' in text:
                raise ValueError('QNX 6.5 cannot load an emutls capture library')
            if index and info >> 4 in (1, 2) and other & 3 == 0 and text:
                exports[text] = (value, size)
    wanted = {'_ZN4util10TimerGroup10startTimerEj', 'timer_settime'}
    if exports.keys() != wanted:
        raise ValueError('unexpected capture exports: ' + repr(sorted(exports)))
    entry, size = exports['_ZN4util10TimerGroup10startTimerEj']
    for sec in sections:
        if sec[3] <= entry and entry + size <= sec[3] + sec[5] and sec[1] == 1:
            off = sec[4] + entry - sec[3]
            words = struct.unpack_from('<5I', data, off)
            if words[:3] != (0xe92d4010, 0xe1a0200e, 0xe1a03006) or words[4] != 0xe8bd8010 or words[3] >> 24 != 0xeb:
                raise ValueError('capture ARM entry ABI mismatch')
            break
    else:
        raise ValueError('capture ARM entry missing')
    return {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data), 'exports': sorted(exports)}


def package(encoder, library, output):
    fw = firmware.verify(encoder)
    lib = verify_library(library)
    output = Path(output)
    if output.exists():
        raise ValueError('output already exists; choose a new directory')
    output.mkdir(parents=True)
    (output / 'bin').mkdir()
    (output / 'reference').mkdir()
    shutil.copy2(encoder, output / 'reference/videoencoderservice')
    shutil.copy2(library, output / 'bin/libvc_capture_clock.so')
    shutil.copy2(ROOT / 'deploy/capture_clock/vc_capture_startup.sh', output / 'bin/vc_capture_startup.sh')
    shutil.copy2(ROOT / 'deploy/capture_clock/install.sh', output / 'install.sh')
    shutil.copy2(ROOT / 'deploy/capture_clock/README.md', output / 'README.md')
    sources = ['native/capture_clock/capture_clock.c', 'native/capture_clock/entry_arm.S',
               'native/capture_clock/exports.map', 'deploy/capture_clock/install.sh',
               'deploy/capture_clock/vc_capture_startup.sh', 'scripts/package_capture_clock.py']
    manifest = {'status': 'local candidate; no HU actions', 'firmware': fw, 'library': lib,
                'source_sha256': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in sources},
                'files': {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted(output.rglob('*')) if p.is_file()}}
    (output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware', required=True, type=Path)
    parser.add_argument('--library', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    print(package(args.firmware, args.library, args.output))
