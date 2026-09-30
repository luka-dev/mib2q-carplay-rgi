"""Offline stock-210 fixture. No 280 receiver is loaded into dio_manager.

Only the test phone loads raw Ed/Curve/ChaCha primitives. Its SRP, HKDF,
TLV exchange and HTTP framing are implemented independently in the probe.
"""
from hashlib import sha256
import shutil
import re
from qemu_capture import AUDI, REPO, docker


def prepare(work, media=False, lifecycle=False):
    stock = AUDI / ('Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/'
                    'advanced/MU1316-appimg/eso/lib/libairplay.so')
    if sha256(stock.read_bytes()).hexdigest() != '01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2':
        raise RuntimeError('stock 210 changed')
    shutil.copyfile(REPO / 'build/wireless/libcarplay_hook.so', work / 'libcarplay_hook.so')
    # Export raw primitives for the independent client only; never preload it.
    prototypes = [
        ('void', 'ed25519_make_key_pair_ref', 'unsigned char *p, unsigned char *s', 'p,s'),
        ('void', 'ed25519_sign_ref', 'unsigned char *s, const void *m, size_t n, const unsigned char *p, const unsigned char *k', 's,m,n,p,k'),
        ('int', 'ed25519_verify_ref', 'const void *m, size_t n, const unsigned char *s, const unsigned char *p', 'm,n,s,p'),
        ('void', 'curve25519_donna', 'unsigned char *o, const unsigned char *s, const unsigned char *p', 'o,s,p'),
        ('void', 'chacha20_poly1305_encrypt_all_64x64', 'const unsigned char *k, const unsigned char *n, const void *a, size_t al, const void *p, size_t pl, void *o, unsigned char *t', 'k,n,a,al,p,pl,o,t'),
        ('int', 'chacha20_poly1305_decrypt_all_64x64', 'const unsigned char *k, const unsigned char *n, const void *a, size_t al, const void *p, size_t pl, void *o, const unsigned char *t', 'k,n,a,al,p,pl,o,t'),
    ]
    source = '#include <stddef.h>\n'
    for result, name, params, args in prototypes:
        source += f'extern {result} cp280_{name}({params});\n'
        source += (f'__attribute__((visibility("default"))) {result} {name}({params})'
                   '{' + ('return ' if result != 'void' else '') + f'cp280_{name}({args});' + '}\n')
    (work / 'phone_crypto.c').write_text(source)
    cc = 'arm-unknown-nto-qnx6.5.0eabi-gcc'
    docker(work, cc, '-shared', '-fPIC', '-fvisibility=hidden', '-Wl,--gc-sections',
           '-o', '/work/libphone_crypto.so', '/work/phone_crypto.c',
           '/src/Patches/mhi2-carplay/build/wireless-sdk/libwireless_pairing.a',
           '-lsocket', '-lm', '-lbacktrace')
    docker(work, cc, '-std=gnu99', '-O2', '-Wall', '-Wextra', '-Werror',
           '-DMHI2Q_PAIRING_TEST_PORT=5000',
           '-DMHI2Q_PAIRING_HOOK_QEMU',
           *(['-DMHI2Q_STOCK210_LIFECYCLE_QEMU'] if lifecycle else []),
           *(['-DMHI2Q_WIRELESS_MEDIA_QEMU', '-DMHI2Q_INDEPENDENT_CONTROL_SETUP',
              '-DMHI2Q_INDEPENDENT_SCREEN_SETUP', '-DMHI2Q_SCREEN_DATA_QEMU',
              '-DMHI2Q_SCREEN_FRAME_QEMU', '-DMHI2Q_SCREEN_AUDIO_QEMU'] if media else []),
           '-DMHI2Q_PAIRING_TEST_CRYPTO="/proc/boot/libphone_crypto.so"',
           '-o', '/work/independent-srp-qemu',
           '/src/Patches/mhi2-carplay/wireless/receiver_bridge/independent_srp_qemu.c',
           '-lcrypto', '-lsocket')
    (work / 'load_hook.c').write_text('#include <stdio.h>\n#include <dlfcn.h>\n'
        'int main(void) { void *p=dlopen("/proc/boot/libcarplay_hook.so",RTLD_NOW|RTLD_LOCAL);'
        'printf("[hook-loader] %s\\n",p?"OK":dlerror()); return p?0:1;}\n')
    docker(work, cc, '-o', '/work/load-hook', '/work/load_hook.c')
    docker(work, cc, '-std=gnu99', '-O2', '-Wall', '-Wextra', '-Werror',
           '-Wl,--gc-sections', '-o', '/work/pairing-store-qemu',
           '/src/Patches/mhi2-carplay/wireless/receiver_bridge/pairing_store_qemu.c',
           '/src/Patches/mhi2-carplay/build/wireless-sdk/libwireless_pairing.a',
           '-lsocket', '-lm', '-lbacktrace')
    manifest = work / 'capture.build'
    source = manifest.read_text()
    def replace_once(old, new):
        nonlocal source
        if source.count(old) != 1:
            raise RuntimeError(f'non-unique fixture insertion: {old!r}')
        source = source.replace(old, new)
    replace_once('    reopen /dev/ser1\n',
                 '    reopen /dev/ser1\n'
                 '    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=8192 blk cache=256k\n'
                 '    waitfor /dev/hd0t77 8\n'
                 '    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/pairing\n')
    replace_once('    LD_PRELOAD=/proc/boot/libdio_capture_probe.so:/proc/boot/libdioprobe.so\n',
                 '    MHI2Q_DIO_OWNER_MODE=wireless\n'
                 '    MHI2Q_CP_WIRELESS_PAIRING=1\n'
                 '    MHI2Q_CP_PAIRING_DIR=/mnt/pairing/store\n'
                 '    LD_PRELOAD=/proc/boot/libcarplay_hook.so:/proc/boot/libdio_capture_probe.so:/proc/boot/libdioprobe.so\n')
    replace_once('    /proc/boot/ifconfig lo0 alias 10.0.0.16\n',
                 '    /proc/boot/ifconfig lo0 alias 10.0.0.16\n'
                 '    /proc/boot/load-hook\n'
                 '    /proc/boot/independent-srp-qemu &\n')
    replace_once('    waitfor /dev/i2c8 4\n',
                 '    waitfor /dev/i2c8 4\n    /proc/boot/pairing-store-qemu\n')
    # Exact stock library, not an inherited experimental image.
    lines = source.splitlines()
    lines = [f'/proc/boot/libairplay.so=/src/{stock.relative_to(AUDI)}'
             if line.startswith('/proc/boot/libairplay.so=') else line for line in lines]
    assets = '\n[type=dir] /mnt/pairing\n[perms=+r,+x]\n'
    for name in ('libcarplay_hook.so', 'libphone_crypto.so', 'independent-srp-qemu',
                 'load-hook', 'pairing-store-qemu'):
        assets += f'/proc/boot/{name}=/work/{name}\n'
    sdk = '/opt/qnx650/target/qnx6/armle-v7'
    for name, path in [('devb-ram', 'sbin'), ('mount', 'bin'), ('io-blk.so', 'lib/dll'),
                       ('cam-disk.so', 'lib/dll'), ('fs-qnx4.so', 'lib/dll'),
                       ('libcam.so.2', 'lib'), ('libz.so.2', 'usr/lib'),
                       ('libcrypto.so.1', 'usr/lib'), ('libbacktrace.so.1', 'usr/lib')]:
        if not any(line.startswith(f'/proc/boot/{name}=') for line in lines):
            assets += f'/proc/boot/{name}={sdk}/{path}/{name}\n'
    # Keep executable ELF objects alongside the other boot programs. The mesh
    # IFS has a long tail of factory plugins; late ELF entries fail to map on
    # this QNX/QEMU bootstrap (ENOEXEC), even though dumpifs lists them.
    insertion = next(i for i, line in enumerate(lines) if line.startswith('/proc/boot/ifconfig='))
    lines.insert(insertion, assets)
    source = '\n'.join(lines) + '\n'
    if media:
        source = source.replace('    MHI2Q_CP_WIRELESS_PAIRING=1\n',
                                '    MHI2Q_CP_WIRELESS_PAIRING=1\n    MHI2Q_CP_WIRELESS_MEDIA=1\n'
                                '    ALTSCREEN_HANDLE_ALT=1\n    ALTSCREEN_ADVERT_DISPLAYS=1\n')
    manifest.write_text(source)
    if lifecycle:
        from stock210_lifecycle_fixture import prepare_lifecycle
        prepare_lifecycle(work)


def validate(output, log, media=False, lifecycle=False):
    required = ['[independent-srp-qemu] ' + step for step in (
        'M1-M4 proof verified', 'M1-M6 identity verified',
        'pair-verify M1-M4 verified', 'pipelined body and bad-tag rejection verified',
        'bad-signature rejection verified', 'reconnect with stored peer verified',
        'encrypted /info HTTP 200')]
    if media:
        required.remove('[independent-srp-qemu] pipelined body and bad-tag rejection verified')
        required += ['[independent-srp-qemu] control SETUP status=200',
                     '[independent-srp-qemu] screen AVCC config+encrypted frame sent',
                     '[stock210-phone] microphone encryption, splitting, sequence and timestamp verified',
                     '[stock210-phone] encrypted events request/response and bad tag sent',
                     '[stock210-media] events completion status=0',
                     '[stock210-media] events completion status=-6754',
                     '[stock210-media] authenticated PCM reached stock jitter buffer',
                     '[screen-data-qemu] ProcessFrames result=-6754',
                     '[independent-srp-qemu] screen TEARDOWN status=200',
                     '[dio-capture] manager released']
    missing = [m for m in required if m not in output]
    if '[pairing-store-qemu] reopen, process lock, permissions, corruption, symlink passed' not in output:
        missing.append('persistent-store regression')
    if media:
        import plistlib
        bodies = re.findall(r'\[independent-srp-qemu\] screen-setup-body-hex=([0-9a-f]+)', output)
        if len(bodies) != (2 if lifecycle else 1):
            missing.append('paired SETUP response count')
        for body in bodies:
            if 'altScreen' in plistlib.loads(bytes.fromhex(body)).get('enabledFeatures', []):
                missing.append('unsupported paired altScreen negotiated')
        if len(re.findall(r'\[fakeomx\] EmptyThisBuffer #\d+ nFilledLen=8 flags=0x10', output)) != (2 if lifecycle else 1):
            missing.append('authenticated video frame count')
        if output.count('[stock210-media] authenticated PCM reached stock jitter buffer') != (160 if lifecycle else 80):
            missing.append('authenticated PCM packet count')
        if 'unexpected PCM admitted' in output:
            missing.append('PCM integrity')
        if output.count('[independent-srp-qemu] screen TEARDOWN status=200') != 1:
            missing.append('media session tore down')
        if lifecycle and output.count('[stock210-lifecycle] media ended by wired drain') != 1:
            missing.append('second media session ended by wired drain')
    if missing or '(dio_manager) terminated SIG' in output or 'pairing failed' in output:
        raise SystemExit(f'Stock210 hook pairing failed: missing={missing}; log: {log}')
    if lifecycle:
        from stock210_lifecycle_fixture import validate_lifecycle
        validate_lifecycle(output)
    print(f'Stock210 + hook: independent pairing/control'
          f'{" + events/screen/PCM/microphone/teardown" if media else ""} passed; {log}')
