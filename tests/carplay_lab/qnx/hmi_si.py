"""Build isolated real J9/LSD + native SI using the supported KGSL launcher.

No substitute SI/CarPlay Java providers. app.img/system.img stay unchanged.
This first profile proves boot/NOOP and leaves wireless session admission idle.
"""
from pathlib import Path
import argparse
import fcntl
import hashlib
import json
import os
import shutil
import socket
import subprocess
import time
import zlib
from . import qemu_capture as capture

ROOT=capture.REPO
HMI=capture.TOOLS/'mhi2q-qemu'
OUT=ROOT/'build/wireless-validation-20261022/hmi-si'

def build():
    meta=dict(line.split('=',1) for line in (HMI/'out/boot.ifs.meta').read_text().splitlines() if '=' in line)
    overlay=OUT/'overlay';overlay.mkdir(parents=True,exist_ok=True)
    def write(path,text):
        target=overlay/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text)
    def copy(source,path):
        target=overlay/path;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    assets=ROOT/'build/si-wireless-stage2'
    subprocess.run(['bash',str(ROOT/'wireless/si/build_si_adapter.sh'),'--stage2'],check=True)
    args=[];packed={}
    for name,source in [('si',assets/'smartphone_integrator.si_wireless_stage1'),('adapter',assets/'libsi_adapter.so')]:
        raw=source.read_bytes();blob=zlib.compress(raw,9)
        (overlay/'lab').mkdir(exist_ok=True)
        (overlay/f'lab/{name}.zlib').write_bytes(blob)
        destination='/mnt/persist/'+('smartphone_integrator' if name=='si' else 'libsi_adapter.so')
        args += [f'/lab/{name}.zlib',destination,str(len(raw)),f'{zlib.crc32(raw):08x}']
        packed[name]={'sha256':hashlib.sha256(raw).hexdigest(),'size':len(raw)}
    for source,name,libs in [('si_payload.c','si_payload',['-lz']),('si_stage1_client.c','si_stage1_client',['-lsocket'])]:
        capture.docker(OUT,'arm-unknown-nto-qnx6.5.0eabi-gcc','-O2','-mno-unaligned-access',
            '/src/Patches/mhi2-carplay/wireless/si/'+source,'-o','/work/'+name,*libs)
        copy(OUT/name,'lab/'+name)
    sdk=capture.SDP/'sdp/target/qnx6/armle-v7'
    copy(sdk/'usr/sbin/pps','proc/boot/pps')
    copy(sdk/'usr/sbin/random','proc/boot/random')
    copy(sdk/'usr/lib/libz.so.2','proc/boot/libz.so.2')
    copy(sdk/'bin/mkdir','proc/boot/mkdir')
    copy(Path(meta['CORE_SH']),'proc/boot/sh')
    # The baseline's qnx4 DLL closure rejects this ramdisk mount with EINVAL
    # ("Program version wrong"). Use one matching SDP closure in its own
    # process search path; the release-image devb-loopback stays untouched.
    for source,name in [('sbin/devb-ram','devb-ram'),('lib/dll/io-blk.so','io-blk.so'),
                        ('lib/dll/cam-disk.so','cam-disk.so'),('lib/dll/fs-qnx4.so','fs-qnx4.so'),
                        ('lib/libcam.so.2','libcam.so.2')]:
        copy(sdk/source,'lab/ramdisk/'+name)
    copy(HMI/'guest/blobs/proc/boot/pidin','proc/boot/pidin')
    copy(HMI/'guest/blobs/proc/boot/ls','proc/boot/ls')
    config=HMI/'guest/blobs/config'
    framework=(config/'framework.json').read_text()
    anchor='"exec":"./bin/apps/smartphone_integrator"'
    if framework.count(anchor)!=1:raise RuntimeError('native SI framework entry changed')
    write('config/framework.json',framework.replace(anchor,'"exec":"/lab/si_wrapper.sh"').replace('/ramdisk/var/run','/mnt/persist/var/run'))
    manager=json.loads((config/'servicemgr.json').read_text())
    manager['startup']=[
        {'cmd':'init','app':'smartphone_integrator','apprunmode':0,'ps_access':'PS_ACC_READ','timeout':20000,'blocking':True},
        {'cmd':'run','app':'smartphone_integrator','timeout':20000,'blocking':True}]
    write('config/servicemgr.json',json.dumps(manager,indent=2)+'\n')
    for source in config.glob('*smartphone*'):copy(source,'config/'+source.name)
    write('lab/si_wrapper.sh','''#!/proc/boot/sh
export LD_LIBRARY_PATH=/proc/boot:/eso/lib:/mnt/app/eso/lib:/mnt/app/armle/lib
export IPL_CONFIG_DIR_SMARTPHONE_INTEGRATOR=/config
export USB_HOST_LIB_PATH=/proc/boot/libusbdi.so.2
export USB_DEVICE_LIB_PATH=/mnt/app/armle/lib/libusbdci.so
export LD_PRELOAD=/mnt/persist/libsi_adapter.so
export MHI2Q_SI_HOOK=1 MHI2Q_SI_HOOK_VERBOSE=1 MHI2Q_SI_HOOK_TRACK_JOBS=1 MHI2Q_SI_HOOK_WIRELESS=1
echo $$ > /mnt/persist/var/run/smartphone_integrator.pid
exec /mnt/persist/smartphone_integrator "$@"
''')
    # The hyphenated key is already provided by native framework spawn envv.
    # Keep that native value; the stock production JSON is also mapped below.
    for source in config.glob('*smartphone*'):copy(source,'etc/eso/production/'+source.name)
    original=(HMI/'guest/images/boot.script').read_text()
    ramdisk='/proc/boot/devb-ram ram capacity=8192 blk cache=512k,noatime &'
    if original.count(ramdisk)!=1:raise RuntimeError('baseline ramdisk startup changed')
    original=original.replace(ramdisk,
        'LD_LIBRARY_PATH=/lab/ramdisk:/proc/boot:/mnt/app/eso/lib\n'
        '/lab/ramdisk/devb-ram ram capacity=32768 blk cache=512k,noatime &\n'
        'LD_LIBRARY_PATH=/proc/boot:/mnt/app/eso/lib:/mnt/app/eso/lib/factories')
    anchor='display_msg "--- starting servicemgr (-> persistence) ---"\n'
    if original.count(anchor)!=1:raise RuntimeError('native SM startup anchor changed')
    extra=('display_msg "--- isolated native SI payload ---"\n'
           '/proc/boot/mkdir -p /mnt/persist/var/run\n'
           '/lab/si_payload '+' '.join(args)+'\n'
           '/proc/boot/pps &\nwaitfor /pps 5\n'
           '/proc/boot/random -t -p &\nwaitfor /dev/urandom 8\n')
    original=original.replace(anchor,extra+anchor)
    anchor='display_msg "=== launching HMI (J9 -> LSD -> ATIP -> HMI -> EAL) ==="\n'
    original=original.replace(anchor,'/lab/si_stage1_client --require-delete &\n'+anchor)
    boot=OUT/'boot.script';boot.write_text(original)
    for name in ('app.img','hmi.img','system.img'):
        target=OUT/name
        if not target.exists():target.symlink_to(HMI/'out'/name)
    subprocess.run(['sh',str(HMI/'scripts/firmware/build_boot_ifs.sh'),'--gpu-tree',meta['GPU_TREE'],'--out',str(OUT/'boot.ifs'),
                    '--boot-script',str(boot),'--overlay',str(overlay)],cwd=HMI,check=True)
    record={'scope':'real J9/LSD + native SI startup/NOOP; wireless media not yet connected',
            'payloads':packed,'launcher':str(HMI/'scripts/run/run_hmi.sh'),
            'runtime_dir':str(OUT),'boot_sha256':hashlib.sha256((OUT/'boot.ifs').read_bytes()).hexdigest()}
    (OUT/'receipt.json').write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(record,indent=2))

def run(seconds=240):
    """Bounded acceptance through the supported launcher; owns only this VM."""
    if type(seconds) is not int or not 60<=seconds<=600:
        raise ValueError('HMI boot deadline must be 60..600 seconds')
    ram=Path('/tmp/mhi2q-wireless-si.ram')
    with (OUT/'run.lock').open('w') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        if ram.exists() or ram.is_symlink():raise RuntimeError('HMI runtime RAM already exists; inspect its owner first')
        record=json.loads((OUT/'receipt.json').read_text())
        if hashlib.sha256((OUT/'boot.ifs').read_bytes()).hexdigest()!=record['boot_sha256']:
            raise RuntimeError('HMI image differs from build receipt')
        for port in (7777,2225):
            with socket.socket() as probe:probe.bind(('127.0.0.1',port))
        acceptance={'passed':False,'checks':{},'boot_sha256':record['boot_sha256'],
                    'scope':record['scope'],'owned_qemu_stopped':False}
        result=OUT/'acceptance.json';result.write_text(json.dumps(acceptance,indent=2)+'\n')
        env={**os.environ,'MHI2Q_RUNTIME_DIR':str(OUT),'QNX_RAM_FILE':str(ram),'RENDER_WORK':str(OUT/'render')}
        markers={'payload':'SI-PAYLOAD: verified size=',
                 'publish':'slot table published after image and controller checks',
                 'noop':'CLIENT: ENQ CONTOUR OK job_noop=1',
                 'deletion':'CLIENT: NATIVE DELETE OK job_deleted=1',
                 'completion':'=== CLIENT DONE OK ===','render':'[FRAME] swap #20'}
        process=None
        try:
            with (OUT/'boot.log').open('w') as log:
                process=subprocess.Popen(['sh',str(HMI/'scripts/run/run_hmi.sh')],cwd=HMI,env=env,
                                         stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                acceptance['launcher_pid']=process.pid;deadline=time.monotonic()+seconds
                while time.monotonic()<deadline:
                    text=(OUT/'boot.log').read_text(errors='replace')
                    acceptance['checks']={name:marker in text for name,marker in markers.items()}
                    acceptance['checks']['payload']=text.count(markers['payload'])==2
                    if all(acceptance['checks'].values()):break
                    if process.poll() is not None:raise RuntimeError('supported HMI launcher exited during acceptance')
                    time.sleep(1)
                if not all(acceptance['checks'].values()):raise TimeoutError('native HMI/SI boot acceptance timed out')
                snapshot=subprocess.run(['python3',str(HMI/'host/shmdbg/shmdbg.py'),'/proc/boot/pidin','ar'],
                                        env=env,capture_output=True,text=True,timeout=40,check=True)
                (OUT/'processes.txt').write_text(snapshot.stdout+snapshot.stderr)
                acceptance['checks'].update(si_process='smartphone_integrator' in snapshot.stdout,
                                             java_process='j9' in snapshot.stdout)
                acceptance['passed']=all(acceptance['checks'].values())
        except Exception as error:
            acceptance['error']=str(error)
        finally:
            if process is not None:
                if process.poll() is None:process.terminate()
                try:process.wait(timeout=20)
                except subprocess.TimeoutExpired:
                    # Do not report acceptance or delete RAM while a VM may
                    # still map it. Preserve the owned launcher PID for repair.
                    acceptance['error']='owned HMI launcher did not stop cleanly'
                    acceptance['passed']=False
            acceptance['owned_qemu_stopped']=not ram.exists() and process is not None and process.poll() is not None
            acceptance['passed'] &= acceptance['owned_qemu_stopped']
            acceptance['log_sha256']=hashlib.sha256((OUT/'boot.log').read_bytes()).hexdigest()
            result.write_text(json.dumps(acceptance,indent=2)+'\n')
        print(json.dumps(acceptance,indent=2))
        if not acceptance['passed']:raise RuntimeError('native HMI/SI acceptance failed; see acceptance.json')
        return acceptance

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('build','run'),nargs='?',default='build')
    parser.add_argument('--seconds',type=int,default=240)
    args=parser.parse_args()
    if args.action=='build':build()
    else:run(args.seconds)
