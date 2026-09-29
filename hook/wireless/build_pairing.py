#!/usr/bin/env python3
"""Build private 280 pairing primitives into an archive for libcarplay_hook.

Reads the owner's SDK in place; never builds/replaces libairplay. Every SDK
global is renamed cp280_* (including references between object files), so
stock 210 and our private CFLite cannot preempt one another. No HU access.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import os
import fcntl

REPO = Path(__file__).resolve().parents[2]
AUDI = REPO.parents[1]
SDK = AUDI / 'Refferences/Carplay/AccessorySDK'
OUT = REPO / 'build/wireless-sdk'
SUPPORT = '''AESUtils ArrayUtils AtomicUtils CFCompat CFLite CFLiteBinaryPlist
CFLitePreferencesFile CFLiteRunLoopSelect CFPrefUtils CFUtils ChaCha20Poly1305
ChecksumUtils CryptoHashUtils DataBufferUtils DebugServices DispatchLite
LogUtils MathUtils MD5Utils MiscUtils NetTransportChaCha20Poly1305 NetUtils
PairingUtils PrintFUtils RandomNumberUtils SHAUtils SRPUtils StringUtils
SystemUtils ThreadUtils TickUtils TimeUtils TLVUtils URLUtils utfconv UUIDUtils'''.split()

def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, **kwargs)

def main():
    if '--inside' not in sys.argv:
        OUT.mkdir(parents=True, exist_ok=True)
        build_lock = (OUT / '.build-lock').open('w')
        fcntl.flock(build_lock, fcntl.LOCK_EX)
        # Hash sources AND headers; never reuse a stale archive after a private
        # wrapper, SDK header or build-flag change. Pin the actual Docker image.
        image = 'qnx65-armv7-toolchain:latest'
        image_id = run('docker', 'image', 'inspect', '--format={{.Id}}', image,
                       capture_output=True).stdout.strip()
        inputs = sorted(p for root in (SDK,
                            AUDI / 'Patches/dio_asm_to_c/_legacy_attempt/libairplay-mhi2q/vendor/shims')
                        for p in root.rglob('*') if p.suffix in ('.c', '.h'))
        inputs += [REPO / 'hook/wireless' / name for name in
                   ('build_pairing.py', 'pairing_sdk.c', 'pairing.h', 'mfi_stock.c')]
        def source_key():
            digest = hashlib.sha256(image_id.encode())
            for file in inputs:
                digest.update(str(file.relative_to(AUDI)).encode())
                digest.update(file.read_bytes())
            return digest.hexdigest()
        key = source_key()
        stamp = OUT / 'build-key'
        archive = OUT / 'libwireless_pairing.a'
        expected = key + ':' + (hashlib.sha256(archive.read_bytes()).hexdigest() if archive.exists() else '')
        if not os.getenv('WIRELESS_SDK_REBUILD') and stamp.exists() and stamp.read_text() == expected:
            print('Private pairing SDK unchanged; reusing verified source build')
            return
        run('docker', 'run', '--rm', '--platform=linux/amd64',
            '-v', f'{AUDI}:/src:ro', '-v', f'{OUT}:/out',
            image, 'python3',
            '/src/Patches/mhi2-carplay/hook/wireless/build_pairing.py', '--inside')
        if source_key() != key:
            raise SystemExit('Pairing build inputs changed during compilation; rebuild required')
        stamp.write_text(key + ':' + hashlib.sha256(archive.read_bytes()).hexdigest())
        return
    sdk = Path('/src/Refferences/Carplay/AccessorySDK')
    source = Path('/src/Patches/mhi2-carplay/hook/wireless')
    out = Path('/out')
    obj = out / 'obj'; obj.mkdir(exist_ok=True)
    files = [sdk / 'Support' / (n + '.c') for n in SUPPORT]
    files += [sdk / 'External/Curve25519/curve25519-donna.c',
              sdk / 'External/Ed25519/ed25519.c']
    files += sorted((sdk / 'External/LibTomMath').glob('*.c'))
    files += [sdk / 'External/SRP' / n for n in
              ['cstr.c', 'srp.c', 'srp6_client.c', 'srp6_server.c', 't_math.c']]
    files += [sdk / 'External/GladmanAES' / n for n in
              ['aes_modes.c', 'aescrypt.c', 'aeskey.c', 'aestab.c']]
    files += [source / 'pairing_sdk.c', source / 'mfi_stock.c']
    flags = ['-O2', '-fPIC', '-fvisibility=hidden', '-ffunction-sections', '-fdata-sections',
             '-include', 'stddef.h',
             '-std=gnu99', '-D_GNU_SOURCE', '-D__STDC_LIMIT_MACROS=1',
             '-DCFLITE_ENABLED=1', '-DCFCOMPAT_NOTIFICATIONS_ENABLED=0',
             '-DCFL_BINARY_PLISTS=1', '-DXPC_LITE_ENABLED=0', '-DXPC_ENABLED=0',
             '-DPAIRING_KEYCHAIN=0', '-DPAIRING_MFI_CLIENT=0',
             '-DPAIRING_MFI_SERVER=1', '-DEXCLUDE_UNIT_TESTS=1', '-DDEBUG=0',
             '-DLOGUTILS_ENABLED=1', '-DTARGET_HAS_MD5_UTILS=1', '-DTARGET_HAS_SHA_UTILS=1',
             '-DTARGET_NO_OPENSSL=1', '-DAES_UTILS_HAS_GLADMAN_GCM=0',
             '-DAES_UTILS_USE_GLADMAN_AES=1', '-DUSE_VIA_ACE_IF_PRESENT=0',
             '-DTOMMATH=1', '-DAtomicUtils_USE_BUILTINS=1']
    for inc in ['', 'Support', 'External/Curve25519', 'External/Ed25519',
                'External/GladmanAES', 'External/Small25519', 'External/SRP',
                'External/LibTomMath', 'CoreUtils']:
        flags += ['-I' + str(sdk / inc)]
    flags += ['-I' + str(source)]
    flags += ['-I/src/Patches/dio_asm_to_c/_legacy_attempt/libairplay-mhi2q/vendor/shims']
    prefix = 'arm-unknown-nto-qnx6.5.0eabi-'
    objects = []
    with (out / 'compile.log').open('w') as log:
        for index, file in enumerate(files):
            dest = obj / f'{index:03d}_{file.stem}.o'
            run(prefix + 'gcc', *flags, '-c', str(file), '-o', str(dest), stdout=log, stderr=log)
            objects.append(dest)
    symbols = set()
    for file in objects:
        for line in run(prefix + 'nm', '-g', '--defined-only', str(file), capture_output=True).stdout.splitlines():
            fields = line.split()
            if len(fields) == 3 and not fields[2].startswith('cp_pair_'):
                symbols.add(fields[2])
    mapping = out / 'namespace.map'
    mapping.write_text(''.join(f'{s} cp280_{s}\n' for s in sorted(symbols)))
    for file in objects:
        run(prefix + 'objcopy', '--redefine-syms=' + str(mapping), str(file))
    archive = out / 'libwireless_pairing.a'
    archive.unlink(missing_ok=True)
    run(prefix + 'ar', 'rcs', str(archive), *map(str, objects))
    manifest = {str(f.relative_to('/src')): hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    (out / 'sources.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Built {archive.name}: {len(objects)} objects; {len(symbols)} private SDK symbols')

if __name__ == '__main__':
    main()
