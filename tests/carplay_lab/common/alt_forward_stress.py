"""Zooming AVC through the production AltAV queue/TCP path and FFmpeg.

Exercises valid predictive video, including queue overflow during a blocked
send. This does not emulate the HU's OMX decoder or physical display fences.
"""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'build/wireless-validation-20261022/alt-valid-video'

def command(args,**kwargs):
    return subprocess.run(args,check=True,timeout=60,**kwargs)

def units(data):
    markers=list(re.finditer(b'\x00\x00(?:\x00)?\x01',data))
    return [data[m.end():markers[i+1].start() if i+1<len(markers) else len(data)]
            for i,m in enumerate(markers)]

def frame_hashes(path):
    result=command(['ffmpeg','-hide_banner','-loglevel','error','-err_detect','explode','-xerror',
                    '-i',str(path),'-f','framemd5','-'],capture_output=True,text=True)
    return [line.rsplit(',',1)[1].strip() for line in result.stdout.splitlines() if line and not line.startswith('#')]

def run():
    OUT.mkdir(parents=True,exist_ok=True)
    receipt=OUT/'result.json';receipt.write_text('{"passed":false,"state":"running"}\n')
    source=OUT/'source.h264'
    command(['ffmpeg','-hide_banner','-loglevel','error','-f','lavfi','-i',
        "testsrc2=size=1440x542:rate=30,zoompan=z='1+0.8*abs(sin(on*0.4))':d=1:s=1440x542:fps=30",
        '-frames:v','90','-c:v','libx264','-preset','ultrafast','-crf','14','-pix_fmt','yuv420p',
        '-x264-params','keyint=15:min-keyint=15:scenecut=0:bframes=0:aud=1:repeat-headers=1:slices=1',
        '-f','h264','-y',str(source)])
    config=[];pictures=[];current=[]
    for nal in units(source.read_bytes()):
        kind=nal[0]&31
        if kind==9:
            if current:pictures.append(current);current=[]
        elif kind in (7,8):
            if nal not in config:config.append(nal)
        else:current.append(nal)
    if current:pictures.append(current)
    assert len(pictures)==90 and len(config)==2
    config=b''.join(b'\0\0\0\1'+v for v in config)
    # AVCC is network order regardless of host byte order.
    frames=[b''.join(struct.pack('>I',len(nal))+nal for nal in picture) for picture in pictures]
    vectors=OUT/'vectors.bin'
    vectors.write_bytes(struct.pack('<I',len(config))+config+struct.pack('<I',len(frames))+
                        b''.join(struct.pack('<I',len(f))+f for f in frames))
    binary=OUT/'stress'
    command(['clang','-O1','-g','-fsanitize=address,undefined','-pthread',
        str(Path(__file__).with_suffix('.c')),str(ROOT/'altscreen_render/frame_io.c'),'-o',str(binary)])
    expected=frame_hashes(source);scenarios={}
    for name,fault in [('continuous',0),('blocked-overflow-recovery',1)]:
        for stale in OUT.glob(name+'-*.h264'):stale.unlink()
        result=command([str(binary),str(vectors),str(OUT/name),str(fault)],capture_output=True,text=True)
        (OUT/(name+'.log')).write_text(result.stdout+result.stderr)
        captures=[];decoded=[]
        for path in sorted(OUT.glob(name+'-*.h264')):
            hashes=frame_hashes(path);decoded+=hashes
            captures.append({'file':path.name,'frames':len(hashes),'bytes':path.stat().st_size,
                             'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
        position=-1
        for value in decoded:
            position=expected.index(value,position+1)
        if not fault:assert decoded==expected
        assert decoded
        scenarios[name]={'passed':True,'frames':len(decoded),'captures':captures,
                         'decode':'FFmpeg explode/xerror; pixels match ordered source frames'}
    result={'passed':True,'source_frames':len(expected),'largest_au_bytes':max(map(len,frames)),
            'scope':'production host AltAV async queue + TCP + renderer framing; synthetic rapid zoom, no OMX/GPU',
            'scenarios':scenarios,'forwarder_sha256':hashlib.sha256((ROOT/'hook/altscreen/altscreen_av.c').read_bytes()).hexdigest()}
    receipt.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
    return result

if __name__=='__main__':run()
