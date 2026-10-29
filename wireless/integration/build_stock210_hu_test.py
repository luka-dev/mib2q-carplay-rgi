#!/usr/bin/env python3
"""Rebuild a stock210 HU test kit. No SSH, installation or radio access.

Without --baseline/--config the kit is intentionally incomplete. A baseline is
a private directory mirroring absolute HU paths, captured without modification.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile
from build_wrapper_candidate import REVIEWED_SHAS

REPO = Path(__file__).resolve().parents[2]
AUDI = REPO.parents[1]
FW = AUDI / 'Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg'
OUT = REPO / 'wireless/out/mhi2q-stock210-hu-test.tar'
RUST_IMAGE = os.environ.get('QNX_RUST_IMAGE', 'qnx65-armv7-toolchain:8.5-rust')


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def run(*args: str) -> None:
    subprocess.run(args, cwd=REPO, check=True)


def assemble(baseline: Path | None, config: Path | None, output: Path = OUT) -> None:
    replaced = {
        '/mnt/app/eso/bin/apps/btstack': FW / 'eso/bin/apps/btstack',
        '/mnt/app/root/hooks/carplay_startup.sh': REPO / 'deploy/smartphone_integrator/carplay_startup.sh',
        **{f'/mnt/system/etc/{name}': REPO / 'deploy/pf/patched' / name
           for name in ('pf.conf', 'pf.ecm0.conf', 'pf.mlan0.conf', 'pf.ppp0.conf')},
    }
    untouched = {
        '/mnt/app/eso/lib/libairplay.so': FW / 'eso/lib/libairplay.so',
        '/mnt/app/eso/bin/apps/dio_manager': FW / 'eso/bin/apps/dio_manager',
        '/mnt/app/armle/usr/sbin/mdnsd': FW / 'armle/usr/sbin/mdnsd',
        '/mnt/app/armle/usr/lib/libdns_sd.so.1': FW / 'armle/usr/lib/libdns_sd.so.1',
    }
    if digest(untouched['/mnt/app/eso/lib/libairplay.so'].read_bytes()) != '01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2':
        raise ValueError('not the reviewed stock210 receiver')
    if baseline:
        for path, source in {**replaced, **untouched}.items():
            captured = baseline / path.lstrip('/')
            if path.endswith('/carplay_startup.sh') and digest(captured.read_bytes()) in REVIEWED_SHAS:
                replaced[path] = captured
                run('python3', 'wireless/integration/build_wrapper_candidate.py',
                    '--stock210', '--property-handoff', '--source', str(captured.resolve()))
            elif captured.read_bytes() != source.read_bytes():
                raise ValueError(f'captured HU differs from reviewed baseline: {path}')
        for name in ('libcarplay_hook.so', 'carplay_monitor.sh', 'carplay_processes.sh'):
            path = f'/mnt/app/root/hooks/{name}'
            untouched[path] = baseline / path.lstrip('/')
        apac = baseline / 'mnt/app/root/hooks/libcarplay_apac.so'
        if apac.is_file():
            untouched['/mnt/app/root/hooks/libcarplay_apac.so'] = apac
    files = {
        'bin/btstack': REPO / 'wireless/graft/out/btstack.graft.offline',
        'bin/mhi2q-wireless-daemon': REPO / 'wireless/daemon/target/armv7-unknown-nto-qnx650/release/mhi2q-wireless-daemon',
        'bin/dio_owner_exec': REPO / 'wireless/integration/out/dio_owner_exec.handoff.offline',
        'bin/wireless_dio_supervisor_bin': REPO / 'wireless/integration/out/wireless_dio_supervisor_bin.handoff.offline',
        'bin/wireless_runtime': REPO / 'wireless/integration/out/wireless_runtime',
        'bin/mhi2q_mdns_probe': REPO / 'wireless/integration/out/mhi2q_mdns_probe',
        'lib/libcarplay_wireless_stock210.so': REPO / 'build/wireless/libcarplay_hook.so',
        'lib/libcarplay_wireless_apac.so': REPO / 'build/wireless/libcarplay_apac.so',
        'lib/libdio_receiver_bridge.so': REPO / 'wireless/receiver_bridge/out/libdio_receiver_bridge.stock210.offline.so',
        'integration/carplay_startup.sh': REPO / 'wireless/integration/out/carplay_startup.stock210.offline.sh',
        'integration/wireless_dio_supervisor.sh': REPO / 'wireless/integration/wireless_dio_supervisor.sh',
        'integration/install_stock210_test.sh': REPO / 'wireless/integration/install_stock210_test.sh',
        **{f'pf/{Path(path).name}': REPO / 'wireless/firewall/out' / Path(path).name
           for path in replaced if '/etc/pf' in path},
        **{f'reference/{Path(path).name}': source for path, source in replaced.items()},
    }
    payload = {name: path.read_bytes() for name, path in files.items()}
    template = (REPO / 'wireless/integration/out/wireless_carplay.stock210.env.example').read_text()
    first_session = '''
# First physical session: no periodic radio queries; separate wireless libraries.
export MHI2Q_CP_IE_MONITOR_SECONDS=0
export MHI2Q_CP_WIRELESS_HOOK=/mnt/app/root/hooks/libcarplay_wireless_stock210.so
export MHI2Q_CP_WIRELESS_APAC=/mnt/app/root/hooks/libcarplay_wireless_apac.so
'''
    payload['integration/wireless_carplay.env'] = (config.read_text() if config else template).encode() + first_session.encode()
    payload['baseline-status.txt'] = b'verified\n' if baseline else b'pending\n'
    payload['untouched.txt'] = ''.join(f'{digest(path.read_bytes())} {name}\n' for name, path in untouched.items()).encode()
    source_files = subprocess.check_output(['git', 'ls-files', '-z'], cwd=REPO).decode().split('\0')
    # Include newly added implementation in source provenance before it is committed.
    source_files += subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard', '-z'], cwd=REPO).decode().split('\0')
    sources = {name: digest((REPO / name).read_bytes()) for name in sorted(set(source_files))
               if name and (REPO / name).is_file() and name.startswith(('wireless/', 'hook/', 'audio/', 'maneuver_render/', 'scripts/'))}
    manifest = {
        'profile': 'stock210-hu-test-main110-pcm', 'receiver': '210.81', 'port': 5000,
        'baseline_verified': bool(baseline), 'unit_config_supplied': bool(config),
        'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
        'source_sha256': sources,
        'toolchain_images': {tag: subprocess.check_output(['docker', 'image', 'inspect', '--format', '{{.Id}}', tag], text=True).strip()
                             for tag in ('qnx65-armv7-toolchain:latest', RUST_IMAGE)},
        'files': {name: {'bytes': len(data), 'sha256': digest(data)} for name, data in payload.items()},
    }
    payload['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    payload['README.txt'] = b'''Stock210 HU test candidate. Verify the archive sidecar before extraction.
No replacement libairplay.so/dio_manager, rc.local, radio firmware or autostart.
No physical wireless session has been established by this build.
An incomplete baseline/config is rejected before installation.
Use docs/wireless-carplay/STOCK210_HU_TEST.md in the repository.
Keep this kit private (unit AP credentials may be present); retain it for rollback.
'''
    payload['checksums.txt'] = ''.join(f'{digest(data)} {name}\n' for name, data in sorted(payload.items())).encode()
    output.parent.mkdir(exist_ok=True, parents=True)
    # Atomic publication: never expose a partly written candidate as a usable kit.
    temporary = output.with_suffix('.tar.new')
    fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    os.fchmod(fd, 0o600)
    with os.fdopen(fd, 'wb') as stream, tarfile.open(fileobj=stream, mode='w') as archive:
        for name, data in sorted(payload.items()):
            entry = tarfile.TarInfo(name)
            entry.size, entry.mtime = len(data), 0
            entry.mode = 0o755 if name.startswith(('bin/', 'lib/')) or name.endswith('.sh') else 0o600
            archive.addfile(entry, io.BytesIO(data))
    temporary.replace(output)
    sidecar = output.with_suffix('.tar.sha256')
    sidecar.write_text(f'{digest(output.read_bytes())}  {output.name}\n')
    sidecar.chmod(0o600)
    print(f'{output}: {output.stat().st_size} bytes; baseline={bool(baseline)} config={bool(config)}')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--config', type=Path)
    parser.add_argument('--output', type=Path, default=OUT)
    args = parser.parse_args()
    # Fix the production profile explicitly; do not inherit experimental build flags.
    env = dict(os.environ, HOOK_AUDIO='1', AUDIO_STATE_TRACE='0', ALTSCREEN_PACE_TRACE='0',
               LOG_RGD_PACKET_RAW='0', WIRELESS_ALT_QEMU='0', PROT_BUILD='0', QNX_TOOLCHAIN_VARIANT='latest')
    subprocess.run(['bash', 'scripts/build_hook_wireless.sh'], cwd=REPO, env=env, check=True)
    run('python3', 'wireless/graft/build_offline.py')
    run('python3', 'wireless/firewall/build_offline.py')
    run('python3', 'wireless/integration/build_stock210_review.py')
    assemble(args.baseline, args.config, args.output)


if __name__ == '__main__':
    main()
