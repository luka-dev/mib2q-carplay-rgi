"""Install/restore the owned VM's opt-in airplayd network association backend."""
from __future__ import annotations
import argparse
import base64
import hashlib
import json
from pathlib import Path
import plistlib
import struct
import subprocess
import uuid
from . import lab,usb

OUT=lab.OUT/'wireless-adapter'
BUNDLE=Path('/Library/Application Support/vphone-launchpad/Bundles/2.9.2/VPhone.bundle/Contents/Resources/guest-resources')
SYSTEM='SystemHook-vphone.dylib'
GUEST_DIR='/var/mobile/Library/Application Support/MHI2CarPlayLab'
LOADER=GUEST_DIR+'/wireless-loader.dylib'
ADAPTER=GUEST_DIR+'/wireless-network.dylib'
CONFIG=GUEST_DIR+'/wireless-network.plist'
RECORD='wireless-adapter-install.json'
ORIGINAL_SHA256='3457d5e8f681bca2603c3359ce77e5c70804f7ef3f6da16f2bb0080b41711e0a'


def weak_dependency(data:bytes,path:str)->bytes:
    """Add one weak load command only inside existing zero header padding."""
    if path != LOADER:raise ValueError('owned dependency path required')
    if len(data)<32 or struct.unpack_from('<I',data)[0]!=0xFEEDFACF:raise ValueError('expected thin Mach-O64')
    if struct.unpack_from('<III',data,4)!=(0x100000C,0x80000002,6):raise ValueError('expected arm64e dylib')
    count,size=struct.unpack_from('<II',data,16);end=32+size;at=32;first=len(data)
    if end>len(data) or count>size//8:raise ValueError('load commands exceed image')
    for _ in range(count):
        if at+8>end:raise ValueError('truncated load command')
        command,length=struct.unpack_from('<II',data,at)
        if length<8 or length%8 or at+length>end:raise ValueError('invalid load command')
        if command==0x19:
            if length<72:raise ValueError('truncated segment')
            sections=struct.unpack_from('<I',data,at+64)[0]
            if 72+sections*80>length:raise ValueError('invalid segment sections')
            for i in range(sections):
                offset=struct.unpack_from('<I',data,at+72+i*80+48)[0]
                if offset:first=min(first,offset)
        if command in (0xC,0x80000018):
            if length<24:raise ValueError('truncated dylib command')
            offset=struct.unpack_from('<I',data,at+8)[0]
            if not 24<=offset<length:raise ValueError('invalid dylib path offset')
            if data[at+offset:at+length].split(b'\0',1)[0]==path.encode():raise ValueError('dependency already installed')
        at+=length
    if at!=end:raise ValueError('load command sizes differ')
    name=path.encode()+b'\0';length=(24+len(name)+7)&~7
    if end+length>first or any(data[end:end+length]):raise ValueError('no zero header padding for dependency')
    result=bytearray(data);struct.pack_into('<II',result,16,count+1,size+length)
    command=struct.pack('<IIIIII',0x80000018,length,24,0,0,0)+name
    result[end:end+length]=command+bytes(length-len(command));return bytes(result)


def build()->dict:
    OUT.mkdir(parents=True,exist_ok=True)
    libraries={
        'System':('/usr/lib/libSystem.B.dylib','dlopen dlsym getprogname strcmp memcmp memcpy malloc free fopen fread fclose fprintf getpid usleep mmap munmap vm_protect mach_task_self_ sys_icache_invalidate dispatch_queue_create dispatch_time dispatch_after_f os_unfair_lock_lock os_unfair_lock_unlock socket bind close inet_pton __stack_chk_guard __stack_chk_fail __chkstk_darwin'.split()),
        'CF':('/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation','kCFBooleanTrue kCFTypeDictionaryKeyCallBacks kCFTypeDictionaryValueCallBacks kCFTypeArrayCallBacks CFStringCreateWithCString CFStringGetCString CFDataCreate CFDataGetBytePtr CFDataGetLength CFPropertyListCreateWithData CFGetTypeID CFDictionaryGetTypeID CFStringGetTypeID CFDataGetTypeID CFDictionaryCreateMutable CFDictionaryGetValue CFDictionarySetValue CFRelease CFRetain CFArrayCreate'.split()),
        'IO':('/System/Library/Frameworks/IOKit.framework/IOKit','IOServiceGetMatchingService IORegistryEntryFromPath IOServiceMatching IORegistryEntryCreateCFProperty IOObjectRelease'.split()),
        'SC':('/System/Library/Frameworks/SystemConfiguration.framework/SystemConfiguration','SCDynamicStoreCreate SCDynamicStoreNotifyValue'.split()),
    }
    for key,(install,symbols) in libraries.items():
        (OUT/f'{key}.tbd').write_text('--- !tapi-tbd-v3\narchs: [ arm64e ]\nplatform: ios\ninstall-name: '+install+'\nexports:\n  - archs: [ arm64e ]\n    symbols: [ '+', '.join('_'+s for s in symbols)+' ]\n...\n')
    sysroot=OUT/'empty-sysroot';sysroot.mkdir(exist_ok=True)
    for source,name,target,libs in [('native_network_loader.c','wireless-loader',LOADER,['System']),('native_network_adapter.c','wireless-network',ADAPTER,['System','CF','IO','SC'])]:
        obj=OUT/(name+'.o');binary=OUT/(name+'.dylib')
        subprocess.run(['clang','-target','arm64e-apple-ios18.0','-isysroot',str(sysroot),'-O2','-Wall','-Wextra','-Werror','-c',str(Path(__file__).with_name(source)),'-o',str(obj)],check=True)
        subprocess.run(['ld','-ld_classic','-arch','arm64e','-platform_version','ios','18.0','18.0','-dylib','-install_name',target,'-o',str(binary),str(obj),*[str(OUT/(k+'.tbd')) for k in libs]],check=True)
        subprocess.run(['codesign','-f','-s','-',str(binary)],check=True)
        subprocess.run(['codesign','--verify','--strict',str(binary)],check=True)
    original=(BUNDLE/SYSTEM).read_bytes()
    if hashlib.sha256(original).hexdigest()!=ORIGINAL_SHA256:raise ValueError('pinned vphone SystemHook differs')
    (OUT/('original-'+SYSTEM)).write_bytes(original)
    modified=OUT/SYSTEM;modified.write_bytes(weak_dependency(original,LOADER))
    subprocess.run(['codesign','-f','-s','-',str(modified)],check=True)
    subprocess.run(['codesign','--verify','--strict',str(modified)],check=True)
    record={'original_sha256':hashlib.sha256(original).hexdigest(),'installed_sha256':hashlib.sha256(modified.read_bytes()).hexdigest()}
    record['files']={name:hashlib.sha256((OUT/name).read_bytes()).hexdigest() for name in
                     ('wireless-loader.dylib','wireless-network.dylib',SYSTEM,'original-'+SYSTEM)}
    record['sources']={name:hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest() for name in
                       ('native_network_adapter.c','native_network_loader.c','network_adapter.py')}
    lab.save('wireless-adapter-build.json',record);return record


def upload(settings,path,data,mode):
    lab.rpc(settings,'files.write',{'path':path,'encoding':'base64','content':base64.b64encode(data).decode()})
    lab.rpc(settings,'files.chmod',{'path':path,'mode':mode})


def install_environment(settings,binary,expected):
    status=lab.rpc(settings,'environment.status')['result']
    matches=[r for r in status['libraries'] if r['name']==SYSTEM]
    if len(matches)!=1 or matches[0]['sha256']!=expected:raise RuntimeError('guest SystemHook differs from expected owned revision')
    staging=status['staging'];lab.rpc(settings,'files.mkdir',{'path':staging})
    data=binary.read_bytes();upload(settings,staging+'/'+SYSTEM,data,'755')
    return lab.rpc(settings,'environment.install',{'libraries':[{'name':SYSTEM,'sha256':hashlib.sha256(data).hexdigest()}]})


def restart_airplayd(settings):
    value=lab.rpc(settings,'processes.list',{'filter':'airplayd'})
    lab.save('wireless-adapter-airplayd-before.json',value)
    result=value.get('result',{});rows=result if isinstance(result,list) else result.get('processes',[])
    for row in rows:
        if row.get('name')=='airplayd' and row.get('path',row.get('executable',''))=='/usr/libexec/airplayd':
            lab.rpc(settings,'processes.kill',{'pid':row['pid'],'signal':15,'force':True})
    # This Apple job is demand-launched; service-configure is intentionally
    # unavailable to the guest RPC. Its native CarKit client activates it.
    return lab.rpc(settings,'services.status',{'label':'com.apple.airplayd'})


def stage():
    settings=lab.profile();machine=lab.require_machine(settings,'running')
    previous=lab.OUT/RECORD
    if previous.exists() and json.loads(previous.read_text()).get('status') in ('installed','restoring'):raise RuntimeError('restore the previous adapter first')
    built=build()
    info=lab.rpc(settings,'device.info')['result']
    if info.get('model')!='iPhone99,11' or info.get('ios_version')!='26.6.2':raise ValueError('owned native VM differs')
    addresses=[x.split()[1] for x in info['network']['addresses'] if x.startswith('en0 192.168.64.')]
    if len(addresses)!=1:raise ValueError('owned NAT interface is missing')
    preferences=lab.rpc(settings,'files.plist',{'path':'/var/mobile/Library/Preferences/com.apple.carplay.plist'})['result']['plist']
    vehicles=[v for v in preferences.get('pairings',{}).values() if v.get('bluetoothAddress')=='02:00:00:00:00:01']
    if len(vehicles)!=1:raise ValueError('run native bootstrap first to create one real CarKit vehicle')
    network_uuid=str(uuid.UUID(vehicles[0]['carplayWiFiUUID'])).upper()
    config={'Enabled':True,'UDID':machine['udid'],'LocalIP':addresses[0],'CarPlayUUID':network_uuid}
    lab.rpc(settings,'files.mkdir',{'path':GUEST_DIR});lab.rpc(settings,'files.chmod',{'path':GUEST_DIR,'mode':'755'})
    upload(settings,LOADER,(OUT/'wireless-loader.dylib').read_bytes(),'755')
    upload(settings,ADAPTER,(OUT/'wireless-network.dylib').read_bytes(),'755')
    upload(settings,CONFIG,plistlib.dumps(config),'644')
    record={**built,'udid':machine['udid'],'status':'prepared','config':config,'config_path':CONFIG};lab.save(RECORD,record)
    return record


def install():
    settings=lab.profile();machine=lab.require_machine(settings,'running')
    record=json.loads((lab.OUT/RECORD).read_text())
    if record['status'] not in ('prepared','installed') or record['udid']!=machine['udid']:raise ValueError('stage the owned adapter first')
    if not record.get('files') or any(hashlib.sha256((OUT/name).read_bytes()).hexdigest()!=digest
                                     for name,digest in record['files'].items()):raise ValueError('staged adapter build changed')
    if record['status']=='installed':
        # Launchpad synchronizes bundled libraries after each cold boot. Its
        # official sync restores SystemHook, so reapply our opt-in loader only
        # after that sync; never modify the host's signed bundle.
        status=lab.rpc(settings,'environment.status')['result']
        current=next(r['sha256'] for r in status['libraries'] if r['name']==SYSTEM)
        if current==record['installed_sha256']:return record
        if current!=record['original_sha256']:raise RuntimeError('SystemHook changed outside this experiment')
        record['status']='prepared';lab.save(RECORD,record)
    probe=usb.run('wireless-adapter-probe')
    record['probe_status']=probe['status']['last_exit_status']
    lab.rpc(settings,'files.chown',{'path':'/var/mobile/Library/Caches/mhi2-wireless-adapter.log','owner':'mobile:mobile'})
    record['environment_install']=install_environment(settings,OUT/SYSTEM,record['original_sha256'])
    record['status']='installed';lab.save(RECORD,record)
    record['restart']=restart_airplayd(settings);lab.save(RECORD,record)
    return record


def restore():
    settings=lab.profile();machine=lab.require_machine(settings,'running');record=json.loads((lab.OUT/RECORD).read_text())
    if record['udid']!=machine['udid'] or record['status'] not in ('installed','prepared','restoring'):raise ValueError('no matching installed adapter')
    # Disable association before replacing its loader. Existing mappings live
    # until the owned airplayd process exits; no shared-cache file was changed.
    config_path=record.get('config_path',usb.GUEST_DIR+'/wireless-network.plist')
    if config_path not in (CONFIG,usb.GUEST_DIR+'/wireless-network.plist'):raise ValueError('unexpected association config path')
    status=lab.rpc(settings,'environment.status')['result']
    current=next(r['sha256'] for r in status['libraries'] if r['name']==SYSTEM)
    original=OUT/('original-'+SYSTEM)
    if current==record['installed_sha256']:
        if hashlib.sha256(original.read_bytes()).hexdigest()!=record['original_sha256']:raise ValueError('original SystemHook backup changed')
    elif current!=record['original_sha256']:raise RuntimeError('SystemHook changed outside this experiment')
    record['status']='restoring';lab.save(RECORD,record)
    upload(settings,config_path,plistlib.dumps({**record['config'],'Enabled':False}),'644')
    if current==record['installed_sha256']:
        record['restore']=install_environment(settings,original,record['installed_sha256'])
    record['restart']=restart_airplayd(settings);record['status']='restored';lab.save(RECORD,record)
    return record


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=('build','stage','install','restore'))
    args=parser.parse_args();print(json.dumps({'build':build,'stage':stage,'install':install,'restore':restore}[args.action](),indent=2))
