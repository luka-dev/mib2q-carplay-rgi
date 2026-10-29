#!/usr/bin/env python3
"""Repeatable offline acceptance: actual ARM hooks/helpers and sanitized C.
No SSH, QEMU boot, production configuration changes or automatic rollback.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from test_si_trampolines import run_arm_tests
from test_si_runtime_guard import run_guard_tests

ROOT=Path(__file__).resolve().parents[2]
SI=ROOT/'wireless/si'
OUT=ROOT/'build/si-wireless-stage2'

def run(*args): subprocess.run(list(map(str,args)),cwd=ROOT,check=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='si-stage2-') as temp:
        work=Path(temp); out=work/'cand'
        run(sys.executable,SI/'build_candidate.py','--stage2','--apply','--outdir',out)
        candidate=out/'smartphone_integrator.si_wireless_stage1'
        run(sys.executable,SI/'build_candidate.py','--stage2','--verify',candidate)
        manifest=json.loads((out/'manifest.json').read_text())
        assert manifest['cave']['abi_version']==2 and len(manifest['hooks'])==5
        arm=run_arm_tests(candidate.read_bytes(),manifest);assert arm==320
        guard=run_guard_tests(SI,candidate.read_bytes(),manifest,work)
        for name,sources in (
            ('controller',['si_wire.c','si_controller.c','si_envv.c','test_si_controller.c']),
            ('receiver_guard',['test_si_receiver_guard.c']),
            ('receiver_listener',['../receiver_bridge/test_airplay_listener.c']),
            ('state_publisher',['../receiver_bridge/test_si_state_publisher.c']),
        ):
            executable=work/name
            run('cc','-std=c99','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-g','-I',SI,*[SI/s for s in sources],'-pthread','-o',executable)
            run(executable)
        executable=work/'dio_transport_guard'
        run('cc','-std=c99','-DSI_TRANSPORT_GUARD_TEST','-Wall','-Wextra','-Werror',
            '-fsanitize=address,undefined','-g',SI/'test_si_dio_transport_guard.c',
            ROOT/'wireless/receiver_bridge/si_transport.c','-o',executable)
        run(executable)
        run(sys.executable,SI/'test_si_native_arm.py')
        run(sys.executable,SI/'test_si_payload.py')
        run(sys.executable,SI/'test_si_startup.py')
        run(sys.executable,ROOT/'wireless/graft/test_rfcomm_peer.py')
        run(sys.executable,ROOT/'wireless/si/test_si_media_watch.py')
    run('cargo','test','--manifest-path',ROOT/'wireless/daemon/Cargo.toml')
    for test in ('test_wireless_runtime.py','test_dio_owner_exec.py','test_launch_candidate.py','test_supervisor_env.py'):
        run(sys.executable,ROOT/'wireless/integration'/test)
    report={'status':'OFFLINE_PASS','scope':'host/Unicorn; separate QEMU native receipt required',
            'arm_trampoline_cases':arm,'native_guard_cases':guard,
            'sanitizers':['ASan','UBSan'],'parser_mutations':20000,
            'native_arm_helpers':'record construct/copy, present insert/remove/reuse, completion 17/11/20/19',
            'models':['UnicodeString boundary','ASI delivery','host native-op callbacks','host DIO process'],
            'not_proven':['native QNX acceptance by this host-only suite',
                          'native C++ discovery publication on HU',
                          'physical wireless video/input/audio/AltScreen and vehicle data',
                          'physical Bluetooth peer metadata'],
            'hu_deployed':False}
    (OUT/'offline-tests.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
