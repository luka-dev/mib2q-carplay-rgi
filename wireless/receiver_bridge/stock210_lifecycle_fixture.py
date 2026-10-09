"""Disposable QNX integration: real supervisor/owner/daemon/stock210/hook.

Peripheral MFi, Wi-Fi IE command and phone are synthetic. No radio/HU access.
"""
import plistlib
import re
import shutil
import subprocess
from qemu_capture import AUDI, REPO, IMAGE, docker


def prepare_lifecycle(work):
    subprocess.run(['docker','run','--rm','--platform=linux/amd64',
        '-v',f'{AUDI}:/src','-w','/src/Patches/mhi2-carplay',IMAGE,
        '/opt/rust/qnx-cargo','build','-q','--manifest-path',
        '/src/Patches/mhi2-carplay/wireless/daemon/Cargo.toml',
        '--features','qemu-probe','--bin','phone-qemu-probe'],check=True)
    # Stage the same default-feature daemon that the review builder packages.
    # Probe features belong to the synthetic phone, not the production daemon.
    subprocess.run(['docker','run','--rm','--platform=linux/amd64',
        '-v',f'{AUDI}:/src','-w','/src/Patches/mhi2-carplay',IMAGE,
        '/opt/rust/qnx-cargo','build','-q','--manifest-path',
        '/src/Patches/mhi2-carplay/wireless/daemon/Cargo.toml',
        '--bin','mhi2q-wireless-daemon'],check=True)
    cc='arm-unknown-nto-qnx6.5.0eabi-gcc'
    root='/src/Patches/mhi2-carplay/wireless/'
    assets=[]
    for source,name,flags in [
        ('integration/dio_owner_exec.c','dio_owner_exec',['-DMHI2Q_REQUIRE_QUIESCE_ALWAYS']),
        ('integration/wireless_dio_supervisor.c','wireless_dio_supervisor_bin',['-DMHI2Q_REQUIRE_QUIESCE_ALWAYS']),
        ('integration/owner_stock_dio_child.c','carplay_startup.sh',['-DMHI2Q_STOCK210_LIFECYCLE_QEMU']),
        ('integration/wired_confirm_qemu.c','wired-confirm-qemu',['-DMHI2Q_QEMU_WIRED_HOLD']),
        ('daemon/probes/mock_mfi_qemu.c','libmock_mfi_qemu.so',['-shared','-fPIC']),
        ('daemon/probes/mock_dns_sd_qemu.c','libqemu_dns_sd.so',['-shared','-fPIC']),
        ('daemon/probes/fake_uaputl.c','fake_uaputl',[]),
    ]:
        docker(work,cc,'-std=gnu99','-Os','-Wall','-Wextra','-Werror',*flags,
               '-o',f'/work/{name}',root+source,'-lsocket')
        assets.append(name)
    for name in ('mhi2q-wireless-daemon','phone-qemu-probe'):
        shutil.copyfile(REPO/'wireless/daemon/target/armv7-unknown-nto-qnx650/release'/name,work/name)
        assets.append(name)
    (work/'supervisor_start.c').write_text('''#include <stdio.h>
#include <unistd.h>
int main(void){FILE *f=fopen("/dev/shmem/stock210_supervisor_pid","w");
if(!f)return 1;fprintf(f,"%ld\\n",(long)getpid());fclose(f);
execl("/mnt/pairing/bin/wireless_dio_supervisor_bin","wireless_dio_supervisor_bin",(char*)0);return 127;}
''')
    docker(work,cc,'-o','/work/stock210-supervisor-start','/work/supervisor_start.c')
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
    MHI2Q_OWNER_DIO_PRELOAD=/mnt/pairing/bin/libcarplay_hook.so:/mnt/pairing/bin/libdio_capture_probe.so
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
    MHI2Q_CP_NAME=Audi
    MHI2Q_CP_MANUFACTURER=Audi
    MHI2Q_CP_MODEL=MHI2Q
    MHI2Q_CP_SERIAL=QEMU
    MHI2Q_CP_FIRMWARE=MU1316
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
    # pipe is needed by Command::output when the real daemon executes uaputl.
    if '/proc/boot/pipe=' not in text:
        entries+='/proc/boot/pipe=/opt/qnx650/target/qnx6/armle-v7/sbin/pipe\n'
    text=text.replace('    /proc/boot/load-hook\n','    /proc/boot/pipe &\n    /proc/boot/load-hook\n')
    pos=text.index('/proc/boot/ifconfig=')
    text=text[:pos]+entries+text[pos:]
    manifest.write_text(text)


def validate_lifecycle(output):
    required=['[stock210-lifecycle] iAP2 pi matches signed PairSetup identity',
        '[stock210-lifecycle] active QUIESCE=3 unowned DRAIN=1',
        '[stock210-lifecycle] wired takeover and release verified',
        '[stock210-lifecycle] two media generations, USB priority, wireless resume passed']
    for marker in required:
        if marker not in output:raise RuntimeError(f'missing {marker}')
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
