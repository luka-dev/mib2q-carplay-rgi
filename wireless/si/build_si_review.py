#!/usr/bin/env python3
"""Build an SI-owned wireless review kit. No network, install or auto rollback.

The tar is deliberately not an installer. Live SI/wrapper/firewall/AP baselines
must be reconciled before deployment. Existing wired libraries are preserved.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/si-wireless-stage2'
def run(*argv):subprocess.run(list(map(str,argv)),cwd=ROOT,check=True)
def digest(b):return hashlib.sha256(b).hexdigest()
def delivery_mode(name):
    executables={'hooks/mhi2q-wireless-daemon','hooks/dio_owner_exec',
                 'hooks/wireless_runtime','hooks/mhi2q_mdns_probe'}
    return 0o755 if name in executables or name.endswith('.sh') or (
        name.startswith(('bin/','si/')) and not name.endswith('.so')) else 0o644
def code_sources():
    files=[p for directory in ('wireless/si','wireless/daemon/src','wireless/daemon/probes','wireless/graft',
        'wireless/integration','wireless/receiver_bridge','hook','scripts','common','altscreen_render',
        'java_patch','java_patch_cn','tools/java')
        for p in (ROOT/directory).rglob('*') if p.is_file() and p.suffix in ('.c','.h','.rs','.py','.sh','.map','.java','.patch')
        and not any(part in ('out','__pycache__') for part in p.relative_to(ROOT).parts)]
    files += [ROOT/'wireless/daemon'/n for n in ('Cargo.toml','Cargo.lock','build.rs')]
    files += [ROOT/'tests/carplay_lab/qnx'/n for n in ('independent_srp_qemu.c','stock210_peer_health_qemu.h',
        'paired_alt_fixture.h','paired_alt_forward_qemu.h')]
    files += [ROOT/'tests/CoverArtClearTest.java']
    files += [ROOT/'tests'/n for n in ('wireless_iap2_semantic_test.c','coverart_pipeline_test.c',
        'test_wireless_hook_contract.py','wireless_media_contract_test.c')]
    return {str(p.relative_to(ROOT)):digest(p.read_bytes()) for p in files}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--full-cycle',action='store_true',help='validate and include the native integrated fault/stress matrix')
    args=ap.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    # Never leave an older successful tar at the expected delivery path if a
    # newer build fails part way through.
    (OUT/'mhi2q-si-owned-review.tar').unlink(missing_ok=True)
    initial_sources=code_sources()
    run('bash','wireless/si/build_si_adapter.sh','--stage2')
    run('python3','wireless/si/test_si_stage2.py')
    run('python3','wireless/si/qemu_stage1.py','--stage2','--timeout','100',
        '--workdir',OUT/'qemu-acceptance')
    run('bash','scripts/build_hook_wireless.sh')
    run('bash','scripts/build_java.sh')
    run('python3','wireless/graft/build_offline.py')
    run('python3','wireless/receiver_bridge/build_offline.py','--receiver','stock210','--property-handoff')
    run('python3','wireless/integration/build_owner_offline.py','--stock210','--property-handoff')
    run('python3','wireless/integration/build_launch_candidate.py','--stock210','--property-handoff','--si-owned')
    run('docker','run','--rm','--platform=linux/amd64','-v',f'{ROOT.parents[1]}:/src',
        '-w','/src/Patches/mhi2-carplay',os.environ.get('QNX_RUST_IMAGE','qnx65-armv7-toolchain:8.5-rust'),
        '/opt/rust/qnx-cargo','build','--manifest-path','/src/Patches/mhi2-carplay/wireless/daemon/Cargo.toml',
        '--bin','mhi2q-wireless-daemon')
    run('python3','wireless/si/test_qemu_vehicle_providers.py')
    run('python3','wireless/si/qemu_stage1.py','--stage2','--vehicle-providers',
        '--carplay-coded','--rcc-fec-fixture','--timeout','210',
        '--workdir',OUT/'qemu-positive')
    integrated=[]
    if args.full_cycle:
        for fault in ('blackhole','microphone','partial-record','rfcomm-gap','dio-crash','daemon-crash','si-crash','icmp','features'):
            work=OUT/('integrated-'+fault)
            print(f'INTEGRATED {fault}: starting',flush=True)
            run('python3','wireless/si/qemu_full_cycle.py','--fault',fault,'--cycles',
                '10' if fault=='blackhole' else '2','--timeout',
                '850' if fault=='blackhole' else '300','--workdir',work)
            record=json.loads((work/'integrated-receipt.json').read_text())
            assert record['status']=='PASS' and all(record['checks'].values())
            assert record['log_sha256']==digest((work/'qemu_stage1.log').read_bytes())
            assert record['source_snapshot_sha256']==digest((work/'source-snapshot.tar').read_bytes())
            for name,expected in record['source_hashes'].items():
                assert digest((ROOT/name).read_bytes())==expected, f'stale integrated source {name}'
            for key,path in (
                ('si',OUT/'smartphone_integrator.si_wireless_stage1'),
                ('adapter',OUT/'libsi_adapter.so'),
                ('dio_libcarplay_hook.so',ROOT/'build/wireless/libcarplay_hook.so'),
                ('integrated_mhi2q-wireless-daemon',ROOT/'wireless/daemon/target/armv7-unknown-nto-qnx650/release/mhi2q-wireless-daemon'),
            ):
                assert record['build']['payloads'][key]['sha256']==digest(path.read_bytes()), f'integrated payload differs: {key}'
            integrated.append((fault,work))
    run('docker','run','--rm','--platform=linux/amd64','-v',f'{ROOT}:/src',
        '-w','/src','qnx65-armv7-toolchain:latest','arm-unknown-nto-qnx6.5.0eabi-gcc',
        '-std=c99','-Wall','-Wextra','-Werror','wireless/si/si_control_client.c','wireless/si/si_wire.c',
        '-lsocket','-o','build/si-wireless-stage2/si_control_client')
    paths={
        'si/smartphone_integrator':OUT/'smartphone_integrator.si_wireless_stage1',
        'si/libsi_adapter.so':OUT/'libsi_adapter.so',
        'si/si_control_client':OUT/'si_control_client',
        'reference/smartphone_integrator.rearm':ROOT/'deploy/smartphone_integrator/smartphone_integrator',
        'bin/btstack':ROOT/'wireless/graft/out/btstack.graft.offline',
        'hooks/mhi2q-wireless-daemon':ROOT/'wireless/daemon/target/armv7-unknown-nto-qnx650/release/mhi2q-wireless-daemon',
        'hooks/libcarplay_wireless_stock210.so':ROOT/'build/wireless/libcarplay_hook.so',
        'hooks/libcarplay_wireless_apac.so':ROOT/'build/wireless/libcarplay_apac.so',
        'hooks/libdio_receiver_bridge.so':ROOT/'wireless/receiver_bridge/out/libdio_receiver_bridge.stock210.offline.so',
        'hooks/dio_owner_exec':ROOT/'wireless/integration/out/dio_owner_exec.handoff.offline',
        'hooks/wireless_runtime':ROOT/'wireless/integration/out/wireless_runtime',
        'hooks/mhi2q_mdns_probe':ROOT/'wireless/integration/out/mhi2q_mdns_probe',
        'hooks/carplay_startup.sh':ROOT/'wireless/integration/out/carplay_startup.stock210.offline.sh',
        'hooks/wireless_si_runtime.sh':ROOT/'wireless/integration/wireless_si_runtime.sh',
        'java/carplay_hook.jar':ROOT/'build/carplay_hook.jar',
        'java/carplay_hook_cn.jar':ROOT/'build/carplay_hook_cn.jar',
        'evidence/java-us.json':ROOT/'build/carplay_hook-build.json',
        'evidence/java-cn.json':ROOT/'build/carplay_hook_cn-build.json',
        'config/wireless_carplay.env.example':ROOT/'wireless/integration/out/wireless_carplay.si_owned.env.example',
        'config/rc.local.review.sh':ROOT/'wireless/integration/out/rc.local.si_owned.offline.sh',
        'evidence/candidate.json':OUT/'manifest.json',
        'evidence/adapter.json':OUT/'libsi_adapter.so.receipt.json',
        'evidence/offline-tests.json':OUT/'offline-tests.json',
        'evidence/qemu-native.json':OUT/'qemu-acceptance/run-receipt.json',
        'evidence/qemu-native.log':OUT/'qemu-acceptance/qemu_stage1.log',
        'evidence/qemu-positive.json':OUT/'qemu-positive/run-receipt.json',
        'evidence/qemu-positive.log':OUT/'qemu-positive/qemu_stage1.log',
        'evidence/dio-inputs.json':OUT/'qemu-positive/dio-inputs.json',
        'evidence/README.md':ROOT/'docs/wireless-carplay/SI_STAGE2_OFFLINE.md',
        'evidence/WIRELESS_PRODUCTION_CHANNELS.md':ROOT/'docs/wireless-carplay/WIRELESS_PRODUCTION_CHANNELS.md',
    }
    for fault,work in integrated:
        for name in ('integrated-receipt.json','qemu_stage1.log','source-snapshot.tar'):
            paths[f'evidence/integrated/{fault}/{name}']=work/name
        if fault=='features':
            for path in sorted(work.glob('forwarded-alt-*.h264')):
                paths[f'evidence/integrated/{fault}/{path.name}']=path
    receipt=json.loads((OUT/'libsi_adapter.so.receipt.json').read_text())
    assert receipt['sha256']==digest((OUT/'libsi_adapter.so').read_bytes())
    for name,expected in receipt['sources'].items():
        assert digest((ROOT/'wireless/si'/name).read_bytes())==expected, f'stale adapter source {name}'
    native=json.loads((OUT/'qemu-acceptance/run-receipt.json').read_text())
    assert native['status']=='PASS' and native['abi']==2 and not native['no_adapter']
    assert native['payloads']['adapter']['sha256']==digest((OUT/'libsi_adapter.so').read_bytes())
    assert native['payloads']['si']['sha256']==digest((OUT/'smartphone_integrator.si_wireless_stage1').read_bytes())
    assert native['log_sha256']==digest((OUT/'qemu-acceptance/qemu_stage1.log').read_bytes())
    for name,expected in native['sources'].items():
        assert digest((ROOT/name).read_bytes())==expected, f'stale native QEMU source {name}'
    positive=json.loads((OUT/'qemu-positive/run-receipt.json').read_text())
    assert positive['status']=='PASS' and positive['rcc_fec_fixture']
    assert positive['payloads']['adapter']['sha256']==digest((OUT/'libsi_adapter.so').read_bytes())
    assert positive['payloads']['si']['sha256']==digest((OUT/'smartphone_integrator.si_wireless_stage1').read_bytes())
    assert positive['payloads']['dio_libcarplay_hook.so']['sha256']==digest((ROOT/'build/wireless/libcarplay_hook.so').read_bytes())
    assert positive['log_sha256']==digest((OUT/'qemu-positive/qemu_stage1.log').read_bytes())
    for name,expected in positive['sources'].items():
        assert digest((ROOT/name).read_bytes())==expected, f'stale positive QEMU source {name}'
    sources=code_sources()
    assert sources==initial_sources, 'source changed during build; rebuild before packaging'
    payload={name:path.read_bytes() for name,path in paths.items()}
    payload['config/si_startup.review.sh']=b'''#!/bin/sh
# Review layout: /mnt/app/root/si-wireless-stage2/{smartphone_integrator,libsi_adapter.so}
D=/mnt/app/root/si-wireless-stage2
export MHI2Q_SI_HOOK=1 MHI2Q_SI_HOOK_WIRELESS=1
export LD_PRELOAD=$D/libsi_adapter.so
exec "$D/smartphone_integrator" "$@"
'''
    payload['config/wireless_carplay.env.example']+=b'''
# Preserve the existing wired hook/APAC. Wireless uses its own freshly built pair.
export MHI2Q_CP_WIRELESS_HOOK=/mnt/app/root/hooks/libcarplay_wireless_stock210.so
export MHI2Q_CP_WIRELESS_APAC=/mnt/app/root/hooks/libcarplay_wireless_apac.so
export MHI2Q_CP_IE_MONITOR_SECONDS=0
'''
    manifest={'profile':'si-owned-stock210-review','hu_deployed':False,'installable_without_live_review':False,
              'integrated_faults':[fault for fault,_ in integrated],
              'automatic_rollback':False,'standalone_dio_supervisor_included':False,
              'git_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              'files':{n:{'sha256':digest(b),'size':len(b),'mode':delivery_mode(n)} for n,b in payload.items()},'sources':sources}
    payload['manifest.json']=(json.dumps(manifest,indent=2,sort_keys=True)+'\n').encode()
    archive=OUT/'mhi2q-si-owned-review.tar'
    with tarfile.open(archive,'w') as tar:
        for name,b in sorted(payload.items()):
            info=tarfile.TarInfo(name);info.size=len(b);info.mtime=0
            info.mode=delivery_mode(name)
            tar.addfile(info,io.BytesIO(b))
    with tarfile.open(archive) as tar:
        for name,record in manifest['files'].items():
            assert digest(tar.extractfile(name).read())==record['sha256']
            assert tar.getmember(name).mode==record['mode']
    (OUT/'review-manifest.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    print(f'{archive} SHA256 {digest(archive.read_bytes())}; review only, no HU operations')
if __name__=='__main__':main()
