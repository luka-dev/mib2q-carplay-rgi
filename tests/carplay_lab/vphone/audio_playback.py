"""Known waveform through real vphone audio routing, in a bounded owned job."""
import argparse
import base64
import hashlib
import io
import json
import math
import os
from pathlib import Path
import plistlib
import struct
import subprocess
import time
import wave
from . import lab,usb

OUT=lab.ROOT/'build/wireless-validation-20261022/vphone-audio'

def build():
    OUT.mkdir(parents=True,exist_ok=True)
    usb.build()
    for name in ('libSystem','objc','CoreFoundation'):
        text=(lab.OUT/'usb'/f'{name}.tbd').read_text()
        extra='_atoi, _strcmp, ' if name=='libSystem' else '_CFRunLoopRunInMode, _kCFRunLoopDefaultMode, ' if name=='CoreFoundation' else ''
        (OUT/f'{name}.tbd').write_text(text.replace('symbols: [ ','symbols: [ '+extra,1))
    env={k:v for k,v in os.environ.items() if k!='SDKROOT'}
    subprocess.run(['clang','-target','arm64-apple-ios18.0','-isysroot',str(lab.OUT/'usb/empty-sysroot'),
        '-O2','-Wall','-Wextra','-Werror','-c',str(Path(__file__).with_suffix('.c')),'-o',str(OUT/'player.o')],env=env,check=True)
    binary=OUT/'audio-playback'
    subprocess.run(['ld','-ld_classic','-arch','arm64','-platform_version','ios','18.0','18.0',
        '-o',str(binary),str(OUT/'player.o'),*[str(OUT/f'{name}.tbd') for name in ('libSystem','objc','CoreFoundation')]],check=True)
    subprocess.run(['codesign','-f','-s','-',str(binary)],check=True)
    return binary

FREQUENCIES=(997,701,503,73,1103,1301)

def waveform(channels=2,seconds=1):
    if channels not in (2,6):raise ValueError('source must be stereo or discrete 5.1')
    if type(seconds) is not int or not 1<=seconds<=10:raise ValueError('source duration must be 1..10 seconds')
    if channels==6:
        # WAVE_FORMAT_EXTENSIBLE: FL/FR/FC/LFE/SL/SR, PCM16, explicit mask.
        pcm=b''.join(struct.pack('<6h',*(int(12000*math.sin(2*math.pi*f*i/48000)) for f in FREQUENCIES)) for i in range(48000))
        pcm*=seconds
        fmt=struct.pack('<HHIIHHHHI',0xfffe,6,48000,48000*12,12,16,22,16,0x60f)+bytes.fromhex('0100000000001000800000aa00389b71')
        body=b'WAVEfmt '+struct.pack('<I',len(fmt))+fmt+b'data'+struct.pack('<I',len(pcm))+pcm
        return b'RIFF'+struct.pack('<I',len(body))+body
    stream=io.BytesIO()
    with wave.open(stream,'wb') as out:
        out.setnchannels(2);out.setsampwidth(2);out.setframerate(48000)
        out.writeframes(b''.join(struct.pack('<hh',int(12000*math.sin(2*math.pi*997*i/48000)),
                                                   int(12000*math.sin(2*math.pi*701*i/48000))) for i in range(48000))*seconds)
    return stream.getvalue()

def _run(seconds,channels=2,engine='avaudio'):
    if type(seconds) is not int or not 10<=seconds<=120:raise ValueError('bounded playback is 10..120s')
    settings=lab.profile();machine=lab.require_machine(settings,'running')
    info=lab.rpc(settings,'device.info')['result']
    if info.get('model')!='iPhone99,11' or info.get('ios_version')!='26.6.2':raise RuntimeError('owned vphone profile changed')
    binary=build();nonce=str(time.monotonic_ns());base=usb.GUEST_DIR+'/audio-'+nonce
    label='com.mhi2.vphone.audio-'+nonce
    remote=base+'.bin';audio=base+'.wav';job=base+'.plist';log=base+'.log'
    source=waveform(channels);source_path=OUT/('source-'+nonce+'.wav');source_path.write_bytes(source)
    loaded=False
    result={'udid':machine['udid'],'label':label,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
            'waveform':{'rate':48000,'channels':channels,'frequencies':FREQUENCIES[:channels],'engine':engine,'source_seconds':1,
                'channel_order':'FL FR FC LFE SL SR' if channels==6 else 'FL FR','path':str(source_path),'sha256':hashlib.sha256(source).hexdigest()},'started_ns':time.time_ns()}
    try:
        for path,data in ((remote,binary.read_bytes()),(audio,source)):
            lab.rpc(settings,'files.write',{'path':path,'content':base64.b64encode(data).decode(),'encoding':'base64'})
        lab.rpc(settings,'files.chmod',{'path':remote,'mode':'700'})
        spec={'Label':label,'ProgramArguments':[remote,audio,str(seconds),str(channels),engine],'RunAtLoad':True,'StandardOutPath':log,'StandardErrorPath':log}
        lab.rpc(settings,'files.write',{'path':job,'content':plistlib.dumps(spec).decode()})
        lab.rpc(settings,'services.load',{'paths':[job]});loaded=True
        deadline=time.monotonic()+seconds+30
        while time.monotonic()<deadline:
            status=lab.rpc(settings,'services.status',{'label':label})['result']
            if not status.get('running'):result['status']=status;break
            time.sleep(1)
        result['log']=lab.rpc(settings,'files.read',{'path':log,'limit':16000})
        (OUT/('playback-'+nonce+'.json')).write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2))
        if result.get('status',{}).get('last_exit_status')!=0:raise RuntimeError('native playback helper failed')
        return result
    finally:
        if loaded:lab.rpc(settings,'services.unload',{'paths':[job],'force':True})
        for path in (remote,audio,job):lab.rpc(settings,'files.remove',{'path':path})

def run(seconds=45,channels=2,engine='avaudio'):
    if type(seconds) is not int or not 10<=seconds<=120:raise ValueError('bounded playback is 10..120s')
    if channels not in (2,6):raise ValueError('source must be stereo or discrete 5.1')
    if engine not in ('avaudio','avplayer'):raise ValueError('unknown playback engine')
    with lab.rpc_batch(lab.profile()):return _run(seconds,channels,engine)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--seconds',type=int,default=45)
    parser.add_argument('--channels',type=int,choices=(2,6),default=2)
    parser.add_argument('--engine',choices=('avaudio','avplayer'),default='avaudio')
    args=parser.parse_args();run(args.seconds,args.channels,args.engine)
