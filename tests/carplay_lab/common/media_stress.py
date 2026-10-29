"""Concurrent 30+30-fps AVC, modeled APAC reserve and real Opus over bounded TCP.

Capacity/latency model on loopback, not physical Wi-Fi or full CarPlay admission.
111 uses production AltAV + renderer framing. 110 uses a declared frame model.
"""
import argparse
import asyncio
import ctypes
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import struct
import subprocess
import time

from . import alt_forward_stress as video

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'build/wireless-validation-20261022/media-stress'
FRAME=struct.Struct('!QQI')

def percentile(values,p):
    if not values:return None
    values=sorted(values);return values[min(len(values)-1,math.ceil(p*len(values))-1)]

def pacing(rows,expected,seconds):
    ordered=sorted(rows,key=lambda r:r[0]);ids=[r[0] for r in ordered]
    if len(set(ids))!=len(ids) or any(i<1 or i>expected for i in ids):raise ValueError('duplicate/out-of-range sequence')
    lag=[(r[2]-r[1])/1e6 for r in ordered]
    if any(v<0 for v in lag):raise ValueError('clock domain mismatch')
    gaps=[(b[2]-a[2])/1e6 for a,b in zip(ordered,ordered[1:])]
    epoch=min((r[1]-(r[0]-1)*seconds/expected*1e9 for r in ordered),default=0)
    in_window=sum(epoch<=r[2]<epoch+seconds*1e9 for r in ordered)
    return {'expected_frames':expected,'received_frames':len(rows),'missing_frames':expected-len(rows),
        'source_window_fps':in_window/seconds,'late_to_100ms_buffer':sum(v>100 for v in lag),
        'modeled_playout_buffer_ms':100,'lag_ms':{'p50':percentile(lag,.5),'p95':percentile(lag,.95),'p99':percentile(lag,.99),'max':max(lag,default=None)},
        'gap_ms':{'p95':percentile(gaps,.95),'max':max(gaps,default=None)},
        'gaps_over_66ms':sum(v>1000/15 for v in gaps),
        # Playout has its own fixed source timeline. An arrival gap can exceed
        # the buffer duration and still be harmless when both adjacent AUs
        # meet their individual deadlines. Report gaps, never invent a miss.
        'pacing_pass':len(rows)==expected and max(lag,default=float('inf'))<=100}

class SharedLink:
    """One FIFO byte server for every modeled direction, with 10-ms burst credit.

    Absolute virtual finish times prevent sleep granularity accumulating into
    an accidental lower cap. No socket or application queue is unbounded here.
    """
    def __init__(self,mbps,epoch,stall=0):
        if not math.isfinite(mbps) or not 1<=mbps<=100:raise ValueError('capacity must be 1..100 Mbit/s')
        self.rate=mbps*1e6/8;self.epoch=epoch;self.finish=epoch;self.stall=stall
        self.lock=asyncio.Lock();self.bins={};self.bytes={}

    def reserve(self,now,length):
        start=max(self.finish,now-.010,self.epoch)
        # One shared outage, centered in the source window, in both directions.
        if self.stall and start<self.epoch+2+self.stall and start+length/self.rate>self.epoch+2:
            start=self.epoch+2+self.stall
        self.finish=start+length/self.rate
        return max(0,self.finish-now)

    async def send(self,writer,data,flow):
        for pos in range(0,len(data),16384):
            chunk=data[pos:pos+16384]
            async with self.lock:
                delay=self.reserve(time.monotonic(),len(chunk))
                if delay:await asyncio.sleep(delay)
                writer.write(chunk);await writer.drain()
                stamp=time.monotonic()-self.epoch
                self.bins[int(stamp*10)]=self.bins.get(int(stamp*10),0)+len(chunk)
                self.bytes[flow]=self.bytes.get(flow,0)+len(chunk)

async def at(due):
    delay=due-time.monotonic()
    if delay>0:await asyncio.sleep(delay)

async def calibrate(mbps):
    class Writer:
        def write(self,data):pass
        async def drain(self):pass
    start=time.monotonic();link=SharedLink(mbps,start)
    size=int(mbps*1e6/32) # one second total, not a short timer-granularity test
    await asyncio.gather(*(link.send(Writer(),bytes(size),str(i)) for i in range(4)))
    elapsed=time.monotonic()-start;measured=4*size*8/elapsed/1e6
    return {'configured_mbps':mbps,'measured_scheduler_mbps':measured,'seconds':elapsed,'burst_credit_ms':10,
            'passed':.9*mbps<=measured<=1.1*mbps,'scope':'saturated four-flow scheduler; no TCP or radio'}

def sources(seconds):
    result={}
    for name,size,mbps in [('110','1024x480',10),('111','1440x542',8)]:
        source=OUT/(name+'.h264')
        video.command(['ffmpeg','-hide_banner','-loglevel','error','-f','lavfi','-i',
            f"testsrc2=size={size}:rate=30,zoompan=z='1+0.8*abs(sin(on*0.4))':d=1:s={size}:fps=30",
            '-frames:v',str(seconds*30),'-c:v','libx264','-threads','2','-preset','ultrafast','-b:v',f'{mbps}M',
            '-minrate',f'{mbps}M','-maxrate',f'{mbps}M','-bufsize',f'{mbps}M','-pix_fmt','yuv420p',
            '-x264-params','nal-hrd=cbr:force-cfr=1:keyint=30:min-keyint=30:scenecut=0:bframes=0:aud=1:repeat-headers=1:slices=1',
            '-f','h264','-y',str(source)])
        config=[];pictures=[];current=[]
        for nal in video.units(source.read_bytes()):
            kind=nal[0]&31
            if kind==9:
                if current:pictures.append(current);current=[]
            elif kind in (7,8):
                if nal not in config:config.append(nal)
            else:current.append(nal)
        if current:pictures.append(current)
        if len(pictures)!=seconds*30 or len(config)!=2:raise ValueError('unexpected AVC vector geometry')
        config=b''.join(b'\0\0\0\1'+v for v in config)
        frames=[b''.join(struct.pack('>I',len(nal))+nal for nal in picture) for picture in pictures]
        vectors=OUT/(name+'.bin');vectors.write_bytes(struct.pack('<I',len(config))+config+struct.pack('<I',len(frames))+
            b''.join(struct.pack('<I',len(f))+f for f in frames))
        result[name]={'path':source,'config':config,'frames':frames,'vectors':vectors,'hashes':video.frame_hashes(source),
                      'payload_mbps':sum(map(len,frames))*8/seconds/1e6}
    return result

def opus_vector():
    library=OUT/'opus.dylib'
    video.command(['clang','-O1','-g','-dynamiclib','-fsanitize=undefined','-I/opt/homebrew/include/opus','-L/opt/homebrew/lib',
        str(ROOT/'hook/wireless/audio_codec.c'),str(ROOT/'hook/audio/aac_bridge.c'),'-lopus','-lpthread','-lm','-o',str(library)])
    lib=ctypes.CDLL(str(library));lib.cp_audio_codec_create.argtypes=[ctypes.c_uint64,ctypes.c_int];lib.cp_audio_codec_create.restype=ctypes.c_void_p
    lib.cp_audio_codec_encode.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_uint,ctypes.c_void_p,ctypes.c_size_t]
    lib.cp_audio_codec_decode.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_uint)]
    lib.cp_audio_codec_destroy.argtypes=[ctypes.c_void_p]
    return lib

class Microphone:
    def __init__(self,lib):
        self.lib=lib;self.enc=lib.cp_audio_codec_create(1<<28,1);self.dec=lib.cp_audio_codec_create(1<<28,0)
        self.decoded=0
        if not self.enc or not self.dec:
            self.close();raise RuntimeError('production Opus codec creation failed')

    def encode(self,index):
        samples=(ctypes.c_int16*320)(*(int(6000*math.sin(2*math.pi*853*(index*320+j)/16000)) for j in range(320)))
        packet=(ctypes.c_ubyte*1416)();n=self.lib.cp_audio_codec_encode(self.enc,samples,320,packet,len(packet))
        if not 0<n<=len(packet):raise RuntimeError('Opus encode failed')
        return bytes(packet[:n])

    def decode(self,body):
        pcm=(ctypes.c_int16*320)();frames=ctypes.c_uint()
        packet=ctypes.create_string_buffer(body)
        if self.lib.cp_audio_codec_decode(self.dec,packet,len(body),pcm,len(pcm),ctypes.byref(frames)) or frames.value!=320 or (self.decoded>640 and not any(pcm)):
            raise RuntimeError('real Opus decode control failed')
        self.decoded+=frames.value

    def close(self):
        if self.enc:self.lib.cp_audio_codec_destroy(self.enc);self.enc=None
        if self.dec:self.lib.cp_audio_codec_destroy(self.dec);self.dec=None

def annexb(payload):
    out=bytearray();pos=0
    while pos<len(payload):
        if pos+4>len(payload):raise ValueError('truncated AVCC length')
        n=struct.unpack_from('>I',payload,pos)[0];pos+=4
        if not n or n>len(payload)-pos:raise ValueError('truncated AVCC payload')
        out+=b'\0\0\0\1'+payload[pos:pos+n];pos+=n
    return out

async def scenario(name,mbps,seconds,vectors,opus,stall=0):
    path=OUT/name;path.mkdir(exist_ok=True)
    for stale in path.glob('*.h264'):stale.unlink()
    proc=await asyncio.create_subprocess_exec(str(OUT/'alt'),str(vectors['111']['vectors']),str(path/'111'),
        stdin=asyncio.subprocess.PIPE,stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
    servers=[];active=set();errors=[];rows=[];alt_connections=[]
    def track(callback):
        async def wrapped(reader,writer):
            task=asyncio.current_task();active.add(task)
            try:await callback(reader,writer)
            except (ConnectionResetError,BrokenPipeError):pass
            except Exception as e:errors.append(repr(e))
            finally:
                active.discard(task);writer.close()
                try:await writer.wait_closed()
                except (OSError,ConnectionError):pass
        return wrapped
    async def server(callback):
        srv=await asyncio.start_server(track(callback),'127.0.0.1',0,limit=32768);servers.append(srv)
        return srv.sockets[0].getsockname()[1]
    traffic={};audio_rows={};epoch=time.monotonic()+.8;link=SharedLink(mbps,epoch,stall)
    try:
        ready=(await asyncio.wait_for(proc.stdout.readline(),5)).decode().split()
        if len(ready)!=3 or ready[0]!='READY':raise RuntimeError('AltAV helper did not become ready')
        # Darwin Python uses mach_absolute_time; C CLOCK_MONOTONIC includes a
        # different sleep epoch. Convert only the common start, and calculate
        # every measured latency within its original process clock domain.
        clock_offset=int(ready[2])/1e6-time.monotonic()
        target=int(ready[1]);epoch=time.monotonic()+.8;link=SharedLink(mbps,epoch,stall)
        async def proxy(reader,writer):
            remote_read,remote_write=await asyncio.open_connection('127.0.0.1',target,limit=32768)
            alt_connections.append(True)
            async def forward():
                while chunk:=await reader.read(16384):await link.send(remote_write,chunk,'111')
            async def reverse():
                while chunk:=await remote_read.read(16384):writer.write(chunk);await writer.drain()
            tasks=[asyncio.create_task(forward()),asyncio.create_task(reverse())]
            try:
                done,pending=await asyncio.wait(tasks,return_when=asyncio.FIRST_COMPLETED)
                for task in done:task.result()
            finally:
                for task in tasks:task.cancel()
                await asyncio.gather(*tasks,return_exceptions=True)
                remote_write.close();await remote_write.wait_closed()
        proxy_port=await server(proxy)
        async def primary(reader,writer):
            with (path/'110.h264').open('wb') as file:
                file.write(vectors['110']['config'])
                for _ in vectors['110']['frames']:
                    seq,due,n=FRAME.unpack(await reader.readexactly(FRAME.size))
                    if not 0<n<4*1024*1024:raise ValueError('unbounded primary frame')
                    payload=await reader.readexactly(n);rows.append((seq,due,time.monotonic_ns()))
                    file.write(annexb(payload))
        main_port=await server(primary)
        async def primary_source():
            reader,writer=await asyncio.open_connection('127.0.0.1',main_port)
            try:
                for i,payload in enumerate(vectors['110']['frames']):
                    due=epoch+i/30;await at(due)
                    await link.send(writer,FRAME.pack(i+1,int(due*1e9),len(payload))+payload,'110')
            finally:writer.close();await writer.wait_closed()
        async def load(label,period,payloads):
            received=asyncio.Event();samples=[];expected=[]
            mic=Microphone(opus) if label=='mic-opus-reverse-model' else None
            async def sink(reader,writer):
                digest=hashlib.sha256()
                for seq in range(round(seconds/period)):
                    number,due,n=FRAME.unpack(await reader.readexactly(FRAME.size))
                    if n>65536 or number!=seq+1:raise ValueError('invalid audio load framing')
                    body=await reader.readexactly(n)
                    if mic:mic.decode(body)
                    digest.update(body);samples.append((number,due,time.monotonic_ns()))
                traffic[label]=digest.hexdigest();received.set()
            port=await server(sink);reader,writer=await asyncio.open_connection('127.0.0.1',port)
            try:
                digest=hashlib.sha256()
                for i in range(round(seconds/period)):
                    body=mic.encode(i) if mic else payloads[i%len(payloads)];digest.update(body);expected.append(body)
                    due=epoch+i*period;await at(due)
                    await link.send(writer,FRAME.pack(i+1,int(due*1e9),len(body))+body,label)
                await asyncio.wait_for(received.wait(),15)
                if traffic.get(label)!=digest.hexdigest():raise ValueError('audio transport bytes changed')
                audio_rows[label]=pacing(samples,len(expected),seconds)
                if mic:audio_rows[label]['decoded_pcm_samples']=mic.decoded
            finally:
                if mic:mic.close()
                writer.close();await writer.wait_closed()
        proc.stdin.write(f'{proxy_port}\n{int((epoch+clock_offset)*1e6)}\n'.encode());await proc.stdin.drain();proc.stdin.close()
        # Opaque APAC reserve is deliberately NOT a fabricated codec vector.
        work=[primary_source(),load('apac-reserve-modeled',.02,[bytes(range(250))*20]),load('mic-opus-reverse-model',.02,[b''])]
        output_task=asyncio.create_task(proc.communicate())
        await asyncio.wait_for(asyncio.gather(*work),seconds+35)
        stdout,stderr=await asyncio.wait_for(output_task,20)
        await asyncio.sleep(.05)
        if proc.returncode:raise RuntimeError('production AltAV helper failed: '+stderr.decode()[-1500:])
        (path/'alt.log').write_bytes(stdout+stderr)
        alt_rows=[tuple(int(v) for v in m.groups()) for m in re.finditer(rb'FRAME (\d+) (\d+) (\d+) \d+',stdout)]
        alt_rows=[(seq,due*1000,got*1000) for seq,due,got in alt_rows]
        stats={'110':pacing(rows,seconds*30,seconds),'111':pacing(alt_rows,seconds*30,seconds)}
        if errors:raise RuntimeError('receiver errors: '+str(errors))
        hashes=await asyncio.to_thread(video.frame_hashes,path/'110.h264')
        if hashes!=vectors['110']['hashes']:raise RuntimeError('primary decoded pixels differ')
        decoded=[]
        for capture in sorted(path.glob('111-*.h264'),key=lambda p:int(p.stem.rsplit('-',1)[1])):
            decoded+=await asyncio.to_thread(video.frame_hashes,capture)
        if decoded!=[vectors['111']['hashes'][seq-1] for seq,_,_ in alt_rows]:
            raise RuntimeError('AltAV sequence/pixels disagree with independent decoded source')
        result={'integrity_pass':True,'pacing_pass':all(v['pacing_pass'] for v in stats.values()),'capacity_mbps':mbps,
            'stall_ms':stall*1000,'source_seconds':seconds,'screens':stats,'audio_loads':audio_rows,
            'alt_connections':len(alt_connections),'alt_decoded_frames':len(decoded),'bytes_by_flow':link.bytes,
            'offered_video_payload_mbps':sum(v['payload_mbps'] for v in vectors.values()),
            'wire_bins_100ms':{str(k):v*8/.1/1e6 for k,v in sorted(link.bins.items())}}
        (path/'result.json').write_text(json.dumps(result,indent=2)+'\n')
        return result
    finally:
        if proc.returncode is None:proc.terminate();await proc.wait()
        for srv in servers:srv.close();await srv.wait_closed()
        for task in tuple(active):task.cancel()
        await asyncio.gather(*tuple(active),return_exceptions=True)

def _run(seconds=10):
    if type(seconds) is not int or not 3<=seconds<=30:raise ValueError('stress duration must be 3..30 seconds')
    OUT.mkdir(parents=True,exist_ok=True);receipt=OUT/'result.json';receipt.write_text('{"passed":false,"state":"running"}\n')
    vectors=sources(seconds);opus=opus_vector()
    video.command(['clang','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-pthread',
        str(Path(__file__).with_name('media_stress_alt.c')),str(ROOT/'altscreen_render/frame_io.c'),'-o',str(OUT/'alt')])
    scenarios={};calibrations={}
    for name,capacity,stall in [('40mbps',40,0),('30mbps',30,0),('20mbps',20,0),('10mbps-overload',10,0),('40mbps-stall200',40,.2)]:
        if str(capacity) not in calibrations:
            calibrations[str(capacity)]=asyncio.run(calibrate(capacity))
            (OUT/f'calibration-{capacity}.json').write_text(json.dumps(calibrations[str(capacity)],indent=2)+'\n')
        scenarios[name]=asyncio.run(scenario(name,capacity,seconds,vectors,opus,stall))
        scenarios[name]['capacity_calibration_valid']=calibrations[str(capacity)]['passed']
        scenarios[name]['calibrated_scheduler_mbps']=calibrations[str(capacity)]['measured_scheduler_mbps']
        (OUT/name/'result.json').write_text(json.dumps(scenarios[name],indent=2)+'\n')
        print(name,json.dumps({k:scenarios[name][k] for k in ('integrity_pass','pacing_pass','alt_decoded_frames')}),flush=True)
    calibration_pass=all(v['passed'] for v in calibrations.values())
    negative_controls_pass=not scenarios['10mbps-overload']['pacing_pass'] and not scenarios['40mbps-stall200']['pacing_pass']
    passed=calibration_pass and negative_controls_pass
    result={'passed':passed,'production_ready':False,'physical_wifi_verified':False,'actual_presented_fps_verified':False,
        'target_40mbps_pacing_pass':scenarios['40mbps']['pacing_pass'],'calibrations':calibrations,
        'calibration_pass':calibration_pass,'negative_controls_pass':negative_controls_pass,
        'modeled_playout_buffer_ms':100,'source_seconds':seconds,'scenarios':scenarios,
        'real':['valid 1024x480 and 1440x542 AVC with rapid zoom at30fps','production AltAV async queue and renderer framing for111',
                'independent FFmpeg decode and source pixel comparison','production Opus encode/decode during TCP load,32kbit/s mono synthetic microphone'],
        'modeled':['110 transport frame boundary','shared FIFO loopback capacity/outage, both logical directions','APAC2Mbit/s opaque reserve; no APAC decoding in this run','microphone reverse direction on loopback'],
        'not_tested':['real iPhone encoder/admission','QNX/HMI/OMX/GPU','actual APAC under concurrent decode load','microphone acoustics','physical 2.4GHz radio'],
        'source_sha256':{name:hashlib.sha256(v['path'].read_bytes()).hexdigest() for name,v in vectors.items()},
        'code_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in (
            'tests/carplay_lab/common/media_stress.py','tests/carplay_lab/common/media_stress_alt.c',
            'hook/altscreen/altscreen_av.c','altscreen_render/frame_io.c','hook/wireless/audio_codec.c')},
        'alt_helper_sanitizers':['address','undefined']}
    receipt.write_text(json.dumps(result,indent=2)+'\n')
    if not passed:raise RuntimeError('stress acceptance/negative controls failed; inspect result.json')
    return result

def run(seconds=10):
    if type(seconds) is not int or not 3<=seconds<=30:raise ValueError('stress duration must be 3..30 seconds')
    try:return _run(seconds)
    except BaseException as error:
        receipt=OUT/'result.json'
        result=json.loads(receipt.read_text()) if receipt.exists() else {}
        result.update(passed=False,state='failed',error=repr(error))
        receipt.write_text(json.dumps(result,indent=2)+'\n')
        raise

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--seconds',type=int,default=10)
    run(parser.parse_args().seconds)
