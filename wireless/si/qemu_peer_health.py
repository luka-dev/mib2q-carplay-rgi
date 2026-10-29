"""Independent paired timing peer for native SI/DIO health acceptance only."""
import hashlib
import shlex
import subprocess
from pathlib import Path


def stage(work: Path, repo: Path, integrated: bool = False) -> tuple[str, dict]:
    guest = '/src/' + str(work.relative_to(repo))
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
    commands = [
        [cc, '-shared', '-fPIC', '-fvisibility=hidden', '-Wl,--gc-sections',
         '-o', guest+'/libphone_crypto.so', guest+'/phone_crypto.c',
         '/src/build/wireless-sdk/libwireless_pairing.a', '-lsocket', '-lm', '-lbacktrace'],
        [cc, '-std=gnu99', '-O2', '-Wall', '-Wextra', '-Werror',
         '-DMHI2Q_PAIRING_TEST_PORT=5000', '-DMHI2Q_PAIRING_HOOK_QEMU',
         '-DMHI2Q_INDEPENDENT_CONTROL_SETUP', '-DMHI2Q_INDEPENDENT_SCREEN_SETUP',
         '-DMHI2Q_SCREEN_DATA_QEMU', '-DMHI2Q_PEER_HEALTH_QEMU',
         *(['-DMHI2Q_INTEGRATED_MEDIA_QEMU','-DMHI2Q_SCREEN_AUDIO_QEMU','-DMHI2Q_SCREEN_FRAME_QEMU'] if integrated else []),
         '-DMHI2Q_PAIRING_TEST_CRYPTO="/proc/boot/libphone_crypto.so"',
         '-o', guest+'/independent-health-phone',
         '/src/tests/carplay_lab/qnx/independent_srp_qemu.c',
         *(['/src/altscreen_render/frame_io.c'] if integrated else []), '-lcrypto', '-lsocket'],
    ]
    for command in commands:
        subprocess.run(['docker', 'run', '--rm', '--platform=linux/amd64',
                        '-v', f'{repo}:/src', 'qnx65-armv7-toolchain:latest',
                        'bash', '-c', shlex.join(command)], check=True)
    entries = ''.join(f'/proc/boot/{name}={guest}/{name}\n' for name in
                      ['libphone_crypto.so', 'independent-health-phone'])
    entries += '/proc/boot/libcrypto.so.1=/opt/qnx650/target/qnx6/armle-v7/usr/lib/libcrypto.so.1\n'
    sources = [repo/'tests/carplay_lab/qnx/independent_srp_qemu.c',
               repo/'tests/carplay_lab/qnx/stock210_peer_health_qemu.h',
               work/'phone_crypto.c', work/'libphone_crypto.so', work/'independent-health-phone',
               repo/'build/wireless-sdk/libwireless_pairing.a']
    return entries, {str(p.relative_to(repo)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
