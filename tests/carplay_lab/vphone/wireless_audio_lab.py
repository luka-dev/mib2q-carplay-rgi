"""Isolated native wireless audio profile; preserves the original QNX VM."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import time
from . import lab,wireless_native as native,wireless_link as link
from tests.carplay_lab.common import qnx_receiver

OUT=lab.ROOT/'build/wireless-validation-20261022/native-audio'
HOOK=OUT.parent/'hook-audio/libcarplay_hook.so'

def configure(spatial=False):
    global OUT,HOOK
    OUT=lab.ROOT/'build/wireless-validation-20261022'/('native-spatial' if spatial else 'native-audio')
    HOOK=OUT.parent/('hook-spatial-lab' if spatial else 'hook-audio')/'libcarplay_hook.so'
    native.OUT=OUT
    qnx_receiver.set_output(OUT)
    prefix='wireless-spatial' if spatial else 'wireless-audio'
    link.CONFIG=prefix+'-link-config.json'
    link.STATE=prefix+'-link-state.json'

def build():
    native.build(hook_file=HOOK,audio=True,audio_peripheral=True,spatial=OUT.name=='native-spatial',ip_address='192.168.64.201')
    p=OUT/'build-record.json';r=json.loads(p.read_text())
    r.update(relay_host_port=32514,input_host_port=32513,qnx_mac='40:54:00:12:34:66')
    r['audio_asset_delivery']=verify_audio_delivery()
    p.write_text(json.dumps(r,indent=2)+'\n')

def verify_audio_delivery():
    from tools.qnx_ifs_extract import find_header
    raw=(OUT/'qemu_start_debug.bin').read_bytes();start=find_header(raw)
    _,length,offset=struct.unpack_from('<III',raw,start+8);position=start+offset;end=position+length
    delivery={}
    while position<end:
        size,_,inode=struct.unpack_from('<HHI',raw,position)
        if size<24 or not inode:break
        mode=struct.unpack_from('<I',raw,position+8)[0]&0o170000
        if mode==0o100000:
            name=raw[position+32:position+size].split(b'\0')[0].decode()
            if name.startswith('mnt/app/armle/usr/lib/cinemo/'):
                file_offset,file_size=struct.unpack_from('<II',raw,position+24)
                if file_offset+file_size>64*1024*1024:
                    raise RuntimeError('audio runtime asset placed beyond the verified IFS front region')
                data=raw[start+file_offset:start+file_offset+file_size]
                delivery[name]={'ifs_offset':file_offset,'size':file_size,'ifs_sha256':hashlib.sha256(data).hexdigest()}
        position+=size
    if len(delivery)!=3:raise RuntimeError('missing native audio runtime assets')
    return delivery

def start():
    if os.geteuid()!=0:raise PermissionError('vmnet join requires sudo')
    if qnx_receiver.running():raise RuntimeError('isolated audio QNX already runs')
    if link.state().get('status')!='prepared':raise RuntimeError('prepare-link as owner first')
    record=json.loads((OUT/'build-record.json').read_text())
    verify_audio_delivery()
    if not record.get('wireless_audio') or record.get('pairing_hook_sha256')!=hashlib.sha256(HOOK.read_bytes()).hexdigest():
        raise RuntimeError('rebuild the isolated audio fixture for the current hook')
    for port in (32513,32514):
        with socket.socket() as s:s.bind(('127.0.0.1',port))
    before=link.snapshot()
    with (OUT/'serial.log').open('w') as serial,(OUT/'qemu-host.log').open('w') as host:
        process=subprocess.Popen(qnx_receiver.command('vmnet'),stdin=subprocess.DEVNULL,
            stdout=serial,stderr=host,start_new_session=True)
    (OUT/'qnx.pid').write_text(str(process.pid)+'\n');(OUT/'transport.txt').write_text('vmnet\n')
    try:return link.join(before)
    except BaseException:
        link.leave()
        if process.poll() is None:process.terminate();process.wait(timeout=10)
        raise

def stop():
    if os.geteuid()!=0:raise PermissionError('restoring vmnet ownership requires sudo')
    pid=qnx_receiver.running()
    link.leave()
    if pid:
        os.kill(pid,15)
        deadline=time.monotonic()+10
        while qnx_receiver.running()==pid and time.monotonic()<deadline:time.sleep(.1)
        if qnx_receiver.running()==pid:raise RuntimeError('owned audio QNX did not stop after SIGTERM')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('build','prepare-link','start','status','run','stop'))
    parser.add_argument('--seconds',type=int,default=90)
    parser.add_argument('--spatial',action='store_true',help='separate wired-parity buffered103/APAC profile')
    args=parser.parse_args();configure(args.spatial)
    if args.action=='build':build()
    elif args.action=='prepare-link':print(json.dumps(link.prepare(),indent=2))
    elif args.action=='start':print(json.dumps(start(),indent=2))
    elif args.action=='stop':stop()
    elif args.action=='status':print(json.dumps({'pid':qnx_receiver.running(),'link':link.state()},indent=2))
    else:
        result=native.run(args.seconds,guest_logs=True,report_name='wireless-audio-native-daemon.json')
        (OUT/'native-run.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({k:v for k,v in result.items() if k not in ('packets','guest_logs','native')},indent=2))

if __name__=='__main__':main()
