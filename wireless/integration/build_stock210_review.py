#!/usr/bin/env python3
"""Assemble the stock210 hook mod for offline review. Never install or SSH.

Deliberately separate from the historical 280 receiver replacement installer.
Original firmware libraries/binaries are inputs, never outputs of this builder.
"""
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile

REPO=Path(__file__).resolve().parents[2]
AUDI=REPO.parents[1]
OUT=REPO/'wireless/out'
DOCS=REPO/'docs/wireless-carplay'


def run(*args):
    subprocess.run(args,cwd=REPO,check=True)


def main():
    run('python3','wireless/receiver_bridge/build_offline.py','--receiver','stock210','--property-handoff')
    run('python3','wireless/integration/build_owner_offline.py','--stock210','--property-handoff')
    run('python3','wireless/integration/build_launch_candidate.py','--stock210','--property-handoff')
    run('docker','run','--rm','--platform=linux/amd64','-v',f'{AUDI}:/src',
        '-w','/src/Patches/mhi2-carplay',os.environ.get('QNX_RUST_IMAGE','qnx65-armv7-toolchain:8.5-rust'),
        '/opt/rust/qnx-cargo','build','-q','--manifest-path','wireless/daemon/Cargo.toml',
        '--bin','mhi2q-wireless-daemon')
    files={
        'hooks/libcarplay_hook.so':'build/wireless/libcarplay_hook.so',
        'hooks/libcarplay_apac.so':'build/wireless/libcarplay_apac.so',
        'hooks/libdio_receiver_bridge.so':'wireless/receiver_bridge/out/libdio_receiver_bridge.stock210.offline.so',
        'hooks/dio_owner_exec':'wireless/integration/out/dio_owner_exec.handoff.offline',
        'hooks/wireless_dio_supervisor_bin':'wireless/integration/out/wireless_dio_supervisor_bin.handoff.offline',
        'hooks/wireless_runtime':'wireless/integration/out/wireless_runtime',
        'hooks/mhi2q_mdns_probe':'wireless/integration/out/mhi2q_mdns_probe',
        'hooks/wireless_dio_supervisor.sh':'wireless/integration/wireless_dio_supervisor.sh',
        'hooks/carplay_startup.sh':'wireless/integration/out/carplay_startup.stock210.offline.sh',
        'hooks/mhi2q-wireless-daemon':'wireless/daemon/target/armv7-unknown-nto-qnx650/release/mhi2q-wireless-daemon',
        'config/wireless_carplay.env.example':'wireless/integration/out/wireless_carplay.stock210.env.example',
        'config/rc.local.review.sh':'wireless/integration/out/rc.local.stock210.offline.sh',
        'wireless/receiver_bridge/out/qemu_stock210_lifecycle.log':'wireless/receiver_bridge/out/qemu_stock210_lifecycle.log',
        'wireless/daemon/out/qemu_daemon_phone_control.log':'wireless/daemon/out/qemu_daemon_phone_control.log',
    }
    active_log = 'wireless/receiver_bridge/out/qemu_stock210_active_stop.log'
    if (REPO / active_log).is_file():
        files[active_log] = active_log
    # Preserve canonical relative paths so links between the migrated notes and
    # the included logs also work inside the review archive.
    if not (DOCS/'README.md').is_file():
        raise RuntimeError('stock210 documentation index is missing')
    for path in sorted(DOCS.rglob('*.md')):
        relative=path.relative_to(REPO).as_posix()
        files[relative]=relative
    manifest={'receiver':'stock210','sourceVersion':'210.81','port':5000,
              'stock_libairplay_sha256':'01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2',
              'files':{}}
    OUT.mkdir(exist_ok=True)
    archive=OUT/'mhi2q-stock210-hook-review.tar'
    with tarfile.open(archive,'w') as tar:
        def add(name,data,mode=0o644):
            entry=tarfile.TarInfo(name);entry.size=len(data);entry.mode=mode;entry.mtime=0
            tar.addfile(entry,io.BytesIO(data))
        for name,path in files.items():
            data=(REPO/path).read_bytes()
            manifest['files'][name]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
            add(name,data,0o755 if name.startswith('hooks/') else 0o644)
        add('manifest.json',(json.dumps(manifest,indent=2)+'\n').encode())
        add('README.txt',b'''Offline stock210 hook mod candidate; no installer and no head-unit operations.
Keep factory libairplay.so (210.81). This archive contains no replacement receiver.
Do not use install_wireless_candidate.sh from the historical 280 archive.
The existing MU1316 btstack graft and wireless firewall/AP prerequisites are
separate components; this archive does not alter or inject btstack.
Use the per-unit configuration template; empty AP/device values intentionally
prevent daemon startup. Pairing keys and owner locks belong on /mnt/persist.
The generated wrapper gives SI's wired child priority and the supervisor owns
wireless generations. Existing wired monitor/helper files remain dependencies.
See docs/wireless-carplay/README.md and STOCK210_RECEIVER_LIFECYCLE.md in that
directory for evidence and hardware limits. Links to other project domains
and source files refer to the repository checkout, outside this review archive.
''')
    digest=hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix('.tar.sha256').write_text(f'{digest}  {archive.name}\n')
    print(f'{archive} ({archive.stat().st_size} bytes) SHA-256 {digest}')


if __name__=='__main__':main()
