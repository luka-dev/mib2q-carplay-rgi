"""Disposable QNX integration: real supervisor/owner/daemon/stock210/hook.

Peripheral MFi, Wi-Fi IE command and phone are synthetic. No radio/HU access.
"""
import plistlib
import re
import shutil
import subprocess
from tests.carplay_lab.qnx.qemu_capture import AUDI, REPO, RUST_IMAGE, docker


def prepare_lifecycle(work, active_stop=False):
    subprocess.run(['docker','run','--rm','--platform=linux/amd64',
        '-v',f'{AUDI}:/src','-w','/src/Patches/mhi2-carplay',RUST_IMAGE,
        '/opt/rust/qnx-cargo','build','-q','--manifest-path',
        '/src/Patches/mhi2-carplay/wireless/daemon/Cargo.toml',
        '--features','qemu-probe','--bin','phone-qemu-probe'],check=True)
    # Stage the same default-feature daemon that the review builder packages.
    # Probe features belong to the synthetic phone, not the production daemon.
    subprocess.run(['docker','run','--rm','--platform=linux/amd64',
        '-v',f'{AUDI}:/src','-w','/src/Patches/mhi2-carplay',RUST_IMAGE,
        '/opt/rust/qnx-cargo','build','-q','--manifest-path',
        '/src/Patches/mhi2-carplay/wireless/daemon/Cargo.toml',
        '--bin','mhi2q-wireless-daemon'],check=True)
    cc='arm-unknown-nto-qnx6.5.0eabi-gcc'
    root='/src/Patches/mhi2-carplay/wireless/'
    assets=[]
    for source,name,flags in [
        ('integration/dio_owner_exec.c','dio_owner_exec',['-DMHI2Q_REQUIRE_QUIESCE_ALWAYS']),
        ('integration/wireless_dio_supervisor.c','wireless_dio_supervisor_bin',['-DMHI2Q_REQUIRE_QUIESCE_ALWAYS']),
        ('integration/wireless_runtime.c','wireless_runtime',[]),
        ('integration/mdns_probe.c','mhi2q_mdns_probe',[]),
        ('daemon/probes/mock_mfi_qemu.c','libmock_mfi_qemu.so',['-shared','-fPIC']),
        ('daemon/probes/mock_dns_sd_qemu.c','libqemu_dns_sd.so',['-shared','-fPIC']),
        ('daemon/probes/fake_uaputl.c','fake_uaputl',[]),
    ]:
        docker(work,cc,'-std=gnu99','-Os','-Wall','-Wextra','-Werror',*flags,
               '-o',f'/work/{name}',root+source,'-lsocket')
        assets.append(name)
    docker(work,cc,'-std=gnu99','-Os','-Wall','-Wextra','-Werror',
           '-o','/work/stock210-wired-owner-qemu',
           '/src/Patches/mhi2-carplay/tests/carplay_lab/qnx/stock210_wired_owner_qemu.c','-lsocket')
    assets.append('stock210-wired-owner-qemu')
    docker(work,cc,'-std=gnu99','-Os','-Wall','-Wextra','-Werror',
           '-o','/work/runtime-fault-qemu',
           '/src/Patches/mhi2-carplay/tests/carplay_lab/qnx/runtime_fault_qemu.c','-lsocket')
    assets.append('runtime-fault-qemu')
    shutil.copyfile(REPO/'build/unprotected/libcarplay_hook.so', work/'wired-carplay-hook.so')
    if (work/'wired-carplay-hook.so').read_bytes()[:256] == (work/'libcarplay_hook.so').read_bytes()[:256]:
        raise RuntimeError('wired/wireless ELF headers must differ for mapped-image assertion')
    assets.append('wired-carplay-hook.so')
    for name in ('mhi2q-wireless-daemon','phone-qemu-probe'):
        shutil.copyfile(REPO/'wireless/daemon/target/armv7-unknown-nto-qnx650/release'/name,work/name)
        assets.append(name)
    # Execute the exact generated wrapper and production supervisor shell.
    subprocess.run(['python3', str(REPO/'wireless/integration/build_wrapper_candidate.py'),
                    '--stock210', '--property-handoff'], check=True)
    for source,name in [
        (REPO/'wireless/integration/out/carplay_startup.stock210.offline.sh','carplay_startup.sh'),
        (REPO/'wireless/integration/wireless_dio_supervisor.sh','wireless_dio_supervisor.sh'),
        (REPO/'deploy/smartphone_integrator/carplay_monitor.sh','carplay_monitor.sh'),
        (REPO/'deploy/smartphone_integrator/carplay_processes.sh','carplay_processes.sh'),
    ]:
        shutil.copyfile(source, work/name); assets.append(name)
    fw=AUDI/'Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/armle'
    sdk=AUDI/'Tools/qnx-65-sdp-docker/sdp/target/qnx6/armle-v7'
    for source,name in [(fw/'bin/sh','sh'), (fw/'usr/sbin/mdnsd','mdnsd'),
                        (sdk/'usr/bin/sleep','sleep'), (sdk/'usr/bin/expr','expr'),
                        (sdk/'bin/mv','mv'), (sdk/'bin/rm','rm'), (sdk/'bin/pidin','pidin')]:
        shutil.copyfile(source,work/name); assets.append(name)
    # Capture contains the real bridge and test-only peripheral interposers;
    # its ELF dependencies retain the mesh without changing the shell wrapper.
    shutil.copyfile(work/'libdio_capture_probe.so', work/'libdio_receiver_bridge.so')
    assets.append('libdio_receiver_bridge.so')
    (work/'wireless_carplay.env').write_text("""export MHI2Q_CP_RECEIVER=stock210
export MHI2Q_CP_WIRELESS_PAIRING=1
export MHI2Q_CP_WIRELESS_MEDIA=1
export MHI2Q_CP_PAIRING_DIR=/mnt/pairing/store
export MHI2Q_CP_MDNS_BINARY=/mnt/pairing/bin/mdnsd
export MHI2Q_CP_MDNS_LOCK=/mnt/pairing/mdns.lock
export MHI2Q_CP_MDNS_PRELOAD=/mnt/pairing/bin/libdio_capture_probe.so
# TCG crypto can starve lower-priority IPC clients; fault cases use 500 ms.
export MHI2Q_CP_MDNS_PROBE_MS=3000
""")
    assets.append('wireless_carplay.env')
    (work/'supervisor_start.c').write_text(r'''#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
int main(void){
unsetenv("LD_PRELOAD"); unsetenv("LD_LIBRARY_PATH");
unsetenv("IPL_CONFIG_DIR_DIO_MANAGER"); unsetenv("IPL_CONFIG_DIR_FRAMEWORK");
setenv("IPL_CONFIG_DIR","/missing-factory-config",1);
pid_t check=fork(); if(!check){execl("/mnt/pairing/bin/sh","sh",
"/mnt/pairing/bin/wireless_dio_supervisor.sh",(char*)0);_exit(127);}
int status=0;if(check<0||waitpid(check,&status,0)!=check||!WIFEXITED(status)||WEXITSTATUS(status)!=78)return 1;
puts("[production-launch] missing config refused=78");fflush(stdout);
unsetenv("IPL_CONFIG_DIR");
puts("[production-launch] clean environment; exact supervisor and wrapper");fflush(stdout);
pid_t runtime=fork();if(!runtime){execl("/mnt/pairing/bin/sh","sh","/mnt/pairing/bin/wireless_dio_supervisor.sh",(char*)0);_exit(127);}
FILE *f=fopen("/dev/shmem/stock210_supervisor_pid","w");
if(!f)return 1;fprintf(f,"%ld\n",(long)runtime);fclose(f);
if(runtime<0||waitpid(runtime,&status,0)!=runtime||!WIFEXITED(status))return 1;
#ifdef MHI2Q_STOCK210_ACTIVE_STOP_QEMU
f=fopen("/mnt/pairing/runtime-exit","w");if(!f)return 1;
fprintf(f,"%ld %d\n",(long)runtime,WEXITSTATUS(status));fclose(f);return WEXITSTATUS(status)==1?0:1;
#else
if(WEXITSTATUS(status))return 1;
execl("/mnt/pairing/bin/runtime-fault-qemu","runtime-fault-qemu",(char*)0);return 127;
#endif
}
''')
    docker(work,cc,*(['-DMHI2Q_STOCK210_ACTIVE_STOP_QEMU'] if active_stop else []),
           '-o','/work/stock210-supervisor-start','/work/supervisor_start.c')
    assets.append('stock210-supervisor-start')
    staged=assets+['dio_manager','libairplay.so','libcarplay_hook.so','libdio_capture_probe.so',
                   'libdioprobe.so','libphone_crypto.so']
    names=','.join('"'+name+'"' for name in staged)
    (work/'stage.c').write_text(r'''#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
int main(void){const char *names[]={'''+names+r'''};unsigned i;char from[200],to[200],data[8192];
mkdir("/mnt/pairing/bin",0700);
for(i=0;i<sizeof(names)/sizeof(names[0]);++i){int a,b;ssize_t n;
snprintf(from,sizeof(from),"/proc/boot/%s",names[i]);snprintf(to,sizeof(to),"/mnt/pairing/bin/%s",names[i]);
a=open(from,O_RDONLY);b=open(to,O_WRONLY|O_CREAT|O_TRUNC,0755);if(a<0||b<0){perror(from);return 1;}
while((n=read(a,data,sizeof(data)))>0){if(write(b,data,n)!=n)return 2;}close(a);close(b);if(n<0)return 3;}
mkdir("/mnt/pairing/wired-hooks",0700);
{const char *wired[]={"carplay_startup.sh","carplay_monitor.sh","carplay_processes.sh","dio_owner_exec","libcarplay_hook.so","libdio_receiver_bridge.so"};
for(i=0;i<sizeof(wired)/sizeof(wired[0]);++i){int a,b;ssize_t n;
snprintf(from,sizeof(from),"/mnt/pairing/bin/%s",i==4?"wired-carplay-hook.so":wired[i]);
snprintf(to,sizeof(to),"/mnt/pairing/wired-hooks/%s",wired[i]);
a=open(from,O_RDONLY);b=open(to,O_WRONLY|O_CREAT|O_TRUNC,0755);if(a<0||b<0)return 4;
while((n=read(a,data,sizeof(data)))>0){if(write(b,data,n)!=n)return 5;}close(a);close(b);if(n<0)return 6;}}
puts("[stock210-lifecycle] repeated-exec files staged on QNX4");return 0;}
''')
    docker(work,cc,'-o','/work/stock210-stage','/work/stage.c')
    assets.append('stock210-stage')
    manifest=work/'capture.build'
    text=manifest.read_text()
    launch='    /proc/boot/dio_manager &\n'
    if text.count(launch)!=1:raise RuntimeError('stock dio launch not unique')
    text=text.replace('ram capacity=8192','ram capacity=65536')
    text=text.replace('    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/pairing\n',
                      '    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/pairing\n    /proc/boot/stock210-stage\n')
    text=text.replace(launch,'''    H=/mnt/pairing/bin
    LD_LIBRARY_PATH=/mnt/pairing/bin:/proc/boot:/eso/lib
    OWNER_FILE=/mnt/pairing/supervisor.owner
    DIODIR=/mnt/pairing/bin
    PATH=/mnt/pairing/bin:/proc/boot
    MHI2Q_CP_DAEMON_PRELOAD=/mnt/pairing/bin/libmock_mfi_qemu.so
    MHI2Q_DIO_OWNER_LOCK=/mnt/pairing/owner.lock
    MHI2Q_DIO_WIRED_PRIORITY_LOCK=/mnt/pairing/wired.lock
    MHI2Q_REQUIRE_BRIDGE_QUIESCE=1
    MHI2Q_OWNER_READY_MS=30000
    MHI2Q_OWNER_RETRY_MS=1000
    MHI2Q_CP_IE_INDEX=2
    MHI2Q_CP_UAPUTL=/mnt/pairing/bin/fake_uaputl
    MHI2Q_CP_MFI_PATH=/dev/shmem/qemu_mfi
    MHI2Q_CP_BT_MAC=00:01:02:03:04:05
    MHI2Q_CP_MODEL=MHI2Q
    MHI2Q_CP_SERIAL=QEMU
    MHI2Q_CP_HARDWARE=MHI2Q
    MHI2Q_CP_SSID=QemuCarPlay
    MHI2Q_CP_PASSPHRASE=testpassword
    MHI2Q_CP_CHANNEL=11
    MHI2Q_CP_IP_ADDRESS=127.0.0.1
    MHI2Q_CP_AIRPLAY_PORT=5000
    MHI2Q_CP_DEVICE_ID=02:00:00:00:00:01
    MHI2Q_CP_RECEIVER=stock210
    MHI2Q_CP_SOURCE_VERSION=210.81
    MHI2Q_PHONE_QEMU_CONTROL=1
    LD_PRELOAD=
    /proc/boot/stock210-supervisor-start &
''')
    entries='\n[perms=+r,+x]\n'+''.join(f'/proc/boot/{name}=/work/{name}\n' for name in assets)
    entries += '\n[type=link] /bin/sh=/mnt/pairing/bin/sh\n'
    entries += '[type=link] /etc/eso/production=/config\n'
    entries += '[type=link] /mnt/app/eso/lib/libairplay.so=/mnt/pairing/bin/libairplay.so\n'
    # The extracted peripheral shim carries this absolute ELF SONAME.
    entries += '[type=link] /work/extracted/proc/boot/libdioprobe.so=/mnt/pairing/bin/libdioprobe.so\n'
    # Keep the capture probe's IP_PKTINFO shim for the synthetic io-pkt only.
    # pipe is needed by Command::output when the real daemon executes uaputl.
    if '/proc/boot/pipe=' not in text:
        entries+='/proc/boot/pipe=/opt/qnx650/target/qnx6/armle-v7/sbin/pipe\n'
    text=text.replace('    /proc/boot/load-hook\n','    /proc/boot/pipe &\n    /proc/boot/load-hook\n')
    pos=text.index('/proc/boot/ifconfig=')
    text=text[:pos]+entries+text[pos:]
    manifest.write_text(text)


def validate_lifecycle(output, active_stop=False):
    if active_stop:
        for marker in ('[active-stop-qemu] real media BUSY; TERM bounded; same owner, daemon and mDNS preserved',
                       '[active-stop-qemu] active media refusal and explicit recovery passed',
                       '[wireless-dio] bridge quiesce not confirmed; owner remains running',
                       '[wireless-runtime] owner remains; mDNS preserved for recovery',
                       '[wireless-runtime] exit=1'):
            if marker not in output:raise RuntimeError(f'missing {marker}')
        if len(re.findall(r'generation owner=(\d+) daemon=(\d+)',output))!=1:
            raise RuntimeError('active-stop spawned a duplicate generation')
        print('Stock210 active media: bounded TERM/QUIESCE refusal, preserved owner/discovery and explicit recovery passed')
        return
    if '[runtime-fault-qemu] all native runtime fault cases passed' not in output:
        raise RuntimeError('native runtime fault matrix incomplete')
    output=output.split('[runtime-fault-qemu] begin',1)[0]
    required=['[production-launch] missing config refused=78',
        '[production-launch] clean environment; exact supervisor and wrapper',
        '[wireless-runtime] mDNS ready via IPC', '[wireless-runtime] exit=0',
        '[wired-stock-qemu] real wired owner released; mDNS recovered',
        '[wireless-runtime] wired owner active; wireless supervisor suspended',
        '[wireless-runtime] wired release; restoring mDNS and supervisor',
        '[stock210-lifecycle] iAP2 pi matches signed PairSetup identity',
        '[stock210-lifecycle] active QUIESCE=3 unowned DRAIN=1',
        '[stock210-lifecycle] wired takeover and release verified',
        '[stock210-lifecycle] two media generations, USB priority, wireless resume passed']
    for marker in required:
        if marker not in output:raise RuntimeError(f'missing {marker}')
    if output.index('[wireless-runtime] mDNS ready via IPC') > output.index('generation owner='):
        raise RuntimeError('DIO launched before mDNS readiness')
    responders=re.findall(r'\[wireless-runtime\] mDNS child=(\d+) starting',output)
    if len(responders)!=2:raise RuntimeError('expected mDNS restart after factory wired teardown')
    if output.count('runtime-managed mDNS stop delegated') < 3:
        raise RuntimeError('factory DIO did not delegate responder shutdown')
    if output.count('[mdns-ownership-qemu] unrelated process still terminated')!=3:
        raise RuntimeError('managed mDNS guard affected unrelated process shutdown')
    responder_exit=f'Process {responders[-1]} (mdnsd) exited'
    if responder_exit not in output or output.index(responder_exit) < output.rindex('[wireless-dio] generation stopped'):
        raise RuntimeError('owned mDNS was not stopped after generation cleanup')
    if output.count('[phone-qemu] authentication before identification=1') != 1:
        raise RuntimeError('second phone must authenticate before identification')
    for marker in ('[phone-qemu] wifi=1','[phone-qemu] CarPlayStartSession=1',
                   '[phone-qemu] full daemon path=1','[dio-property] handoff installed'):
        if output.count(marker)!=3:raise RuntimeError(f'expected 3: {marker}')
    ids=re.findall(r'\[stock210-lifecycle\] iAP2 StartSession pi=(\S+) cycle=\d+',output)
    if len(ids)!=3 or len(set(ids))!=1:raise RuntimeError('identity changed across DIO restarts')
    snapshots=re.findall(r'\[stock210-info\] ([0-9a-f]+)',output)
    if len(snapshots)!=4:raise RuntimeError(f'expected 4 info snapshots, got {len(snapshots)}')
    for snapshot in snapshots:
        info=plistlib.loads(bytes.fromhex(snapshot))
        if info.get('pi')!=ids[0] or info.get('sourceVersion')!='210.81':
            raise RuntimeError('wrong info identity/version')
        features=info['features']
        if 'altScreen' in info.get('extendedFeatures', []):
            raise RuntimeError('unsupported wireless altScreen advertised')
        if len(info.get('displays', [])) > 1 or len(info.get('displayPanels', [])) > 1:
            raise RuntimeError('unsupported second wireless display advertised')
        if features & ((1<<37)|(1<<38)) != ((1<<37)|(1<<38)) or features & ((1<<20)|(1<<22)):
            raise RuntimeError('incorrect wireless features')
        for audio in info['audioFormats']:
            if audio['type'] not in (100,101):raise RuntimeError('unsupported audio stream advertised')
            for key in ('audioInputFormats','audioOutputFormats'):
                if audio.get(key,0) & ~(((1<<12)-(1<<2))|(1<<14)|(1<<15)):
                    raise RuntimeError('unsupported PCM format advertised')
    # Concurrent QNX serial writers can split the logging prefix. The unique
    # generation body still identifies the supervisor's owner/daemon record.
    owners=re.findall(r'generation owner=(\d+) daemon=(\d+)',output)
    if len(owners)!=3 or len({o for o,_ in owners})!=3:raise RuntimeError('expected three distinct owner PIDs')
    if output.count('[wireless-dio] generation stopped')!=3:raise RuntimeError('supervisor cleanup incomplete')
    print('Stock210 lifecycle: real daemon iAP2/MFi(mock)/WiFi/StartSession, stable pi, two media generations, USB priority and wireless resume passed')
