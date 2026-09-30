#!/usr/bin/env python3
"""TRAIN-only causal PCM16 C chain verification against independent references."""
import os
os.environ['CUDA_VISIBLE_DEVICES']='-1'
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import wave
import numpy as np
import torch
import train_unified_upmix_fit as T
import unified_upmix_stream as S


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def pcm16(pcm):
    if not np.isfinite(pcm).all() or np.max(np.abs(pcm))>=1:raise ValueError('PCM16 input would clip; no gain rescue')
    z=np.rint(pcm*32768)
    if z.min()<-32768 or z.max()>32767:raise ValueError('PCM16 rounding overflows')
    return z.astype('<i2')


def replay(a,model,signal,tag):
    p=a.out/(tag+'.input');y=a.out/(tag+'.output');e=a.out/(tag+'.source')
    p.write_bytes(struct.pack('<8sIII',b'UMX16I01',signal.shape[0],signal.shape[1],2)+signal.astype('<i2').tobytes())
    r=subprocess.run([str(a.executable),str(model),str(p),str(y),str(e),str(a.out/(tag+'.source_float'))],capture_output=True,text=True)
    (a.out/(tag+'.log.txt')).write_text(r.stderr)
    if r.returncode:raise ValueError('C chain execution failed; all own artifacts preserved: '+tag)
    def read(path,magic,c):
        b=path.read_bytes();h=struct.unpack_from('<8sIII',b)
        if h[0]!=magic or h[1]!=signal.shape[0] or h[3]!=c or len(b)!=20+h[1]*h[2]*c*2:raise ValueError('Chain output layout differs')
        return np.frombuffer(b,dtype='<i2',offset=20).copy().reshape(h[1],h[2],c)
    return read(y,b'UMX16O01',6),read(e,b'UMX16S01',2),json.loads(r.stderr),{'input':sha(p),'output':sha(y),'source':sha(e)}


def write_wav(path,pcm):
    with wave.open(str(path),'wb') as f:
        f.setnchannels(pcm.shape[-1]);f.setsampwidth(2);f.setframerate(48000);f.writeframes(pcm.astype('<i2').tobytes())


def nse(y,r):return float(np.square(y.astype(np.float64)-r).sum()/max(1e-12,np.square(r.astype(np.float64)).sum()))


def main(a):
    if a.out.exists():raise FileExistsError('Preserve previous chain audit')
    T.configure('cpu');torch.set_num_threads(2)
    blob=json.loads((a.assets/'manifest.json').read_bytes())
    if blob['checkpoint_sha256']!=sha(a.checkpoint):raise ValueError('Different checkpoint')
    for name,row in blob['files'].items():
        if sha(a.assets/name)!=row['sha256']:raise ValueError('Changed model blob')
    data=a.fixture.read_bytes();h=struct.unpack_from('<8sIII',data)
    if h!=(b'UMXPC001',4,384000,2) or len(data)!=20+4*384000*2*4:raise ValueError('Expected original four TRAIN8s crops')
    mixture=pcm16(np.frombuffer(data,dtype='<f4',offset=20).copy().reshape(4,384000,2));a.out.mkdir(parents=True)
    protocol=dict(script_sha256=sha(Path(__file__)),fixture_sha256=sha(a.fixture),executable_sha256=sha(a.executable),
                  renderer_sha256=sha(a.renderer),checkpoint_sha256=sha(a.checkpoint),assets_manifest_sha256=sha(a.assets/'manifest.json'),
                  numeric_gates='unrounded native float source versus unrounded Torch and nativePCM16 versus once-rounded Torch source separately, each NSE<=1e-7,max<=4LSB; all6 output channels versus independent saved-source real-C rendering NSE<=1e-7,max<=4LSB; repeated/reset/future prefix bit exact',
                  latency_scope='identity pulse records sample-onset/peak delay including front alignment/renderer; acquisition and measured execution remain separate; no physical-unit/loaded-worker85ms proof',
                  input='same fixed4 TRAIN8s float mixtures rounded once to nativePCM16, no level adjustment or clipping rescue; source and front both causal, front delayed1536samples',
                  renderer='frozen settings .40,frontbass-3,frontretention.5,body0,air-9,bed delays enabled,attackoff,LFEassist default0dB,legacy inference traps',
                  int8_scope='pipeline execution/matching only, no old allsource reconstruction-screen waiver or terminal source quality/DEV/device/owner claim')
    (a.out/'protocol.json').write_text(json.dumps(protocol,indent=2)+'\n')
    net,_=T.load(a.checkpoint,'cpu');net.eval();original=np.pad(mixture.astype(np.float32)/32768,((0,0),(0,512),(0,0)))
    with torch.inference_mode():
        spectra=S.analyze(torch.from_numpy(original).permute(0,2,1));pred,_=net.forward_stft(spectra,with_scene=False)
        expected=S.synthesize(pred['source_stft'][:,:,0],mixture.shape[1]+2048,compensate=False).permute(0,2,1).numpy()*32768
    arms={};gates={}
    for mode in ('float','int8'):
        output,source,perf,hashes=replay(a,a.assets/('unified_'+mode+'.bin'),mixture,mode)
        own=dict(performance=perf,hashes=hashes)
        if mode=='float':
            fdata=(a.out/(mode+'.source_float')).read_bytes()
            if struct.unpack_from('<8sIII',fdata)!=(b'UMXFLS01',4,source.shape[1],2):raise ValueError('Float diagnostic layout differs')
            native=np.frombuffer(fdata,dtype='<f4',offset=20).reshape(source.shape)*32768
            own['source_float_nse']=nse(native,expected);own['source_float_max_lsb']=float(np.max(np.abs(native-expected)))
            rounded=np.rint(expected).astype('<i2')
            own['source_pcm16_nse']=nse(source,rounded);own['source_pcm16_max_lsb']=int(np.max(np.abs(source.astype(np.int32)-rounded.astype(np.int32))))
            own['source_pcm16_vs_unrounded_nse']=nse(source,expected)
            gates['float_source_nse']=own['source_float_nse']<=1e-7;gates['float_source_max_lsb']=own['source_float_max_lsb']<=4
            gates['pcm16_source_nse']=own['source_pcm16_nse']<=1e-7;gates['pcm16_source_max_lsb']=own['source_pcm16_max_lsb']<=4
            repeated,rs,_,_=replay(a,a.assets/'unified_float.bin',mixture,'float_repeat')
            gates['repeat_reset_exact']=np.array_equal(output,repeated) and np.array_equal(source,rs)
            changed=mixture[:1].copy();onset=changed.shape[1]//2;changed[:,onset:]=np.rint(changed[:,onset:].astype(float)*-.8).astype('<i2')
            future,fs,_,_=replay(a,a.assets/'unified_float.bin',changed,'float_future')
            gates['future_prefix_exact']=np.array_equal(output[:1,:onset],future[:,:onset]) and np.array_equal(source[:1,:onset],fs[:,:onset])
        rows=[]
        for stream in range(4):
            delayed=np.zeros((output.shape[1],2),dtype='<i2');delayed[1536:1536+mixture.shape[1]]=mixture[stream]
            folder=a.out/f'{mode}_{stream}';folder.mkdir();mix=folder/'mix.wav';inj=folder/'source.wav';dest=folder/'output.wav'
            write_wav(mix,delayed);write_wav(inj,source[stream])
            command=[str(a.renderer),'--mode','hssr-universal','--lfe','assist','--lfe-gain','0','--surround','.40','--front-bass','-3','--front-retention','.5','--bed-delays','--bed-attack','off','--rear-air','-9','--rear-body','0','--mask-drive','1','--rear-audio',str(inj),str(mix),str(dest)]
            r=subprocess.run(command,capture_output=True,text=True);(folder/'render.log.txt').write_text(r.stderr)
            if r.returncode or 'old_neural_invocations=0, old_blobs=0' not in r.stderr:raise ValueError('Independent renderer contract failed')
            with wave.open(str(dest),'rb') as f:
                if f.getnchannels()!=6 or f.getsampwidth()!=2 or f.getframerate()!=48000:raise ValueError('Wrong reference rail')
                reference=np.frombuffer(f.readframes(f.getnframes()),dtype='<i2').copy().reshape(-1,6)
            if reference.shape!=output[stream].shape:raise ValueError('Reference length differs')
            err={'nse':nse(output[stream],reference),'max_lsb':int(np.max(np.abs(output[stream].astype(np.int32)-reference.astype(np.int32)))),
                 'by_channel_nse':[nse(output[stream,:,i],reference[:,i]) for i in range(6)],'reference_sha256':sha(dest)}
            rows.append(err)
        own['independent_renderer']=rows
        gates[mode+'_all6_renderer_nse']=max(max(v['by_channel_nse']) for v in rows)<=1e-7
        gates[mode+'_all6_renderer_max_lsb']=max(v['max_lsb'] for v in rows)<=4
        arms[mode]=own;print('CAUSAL_CHAIN_ARM',mode,json.dumps(own),flush=True)
    pulse=np.zeros((1,8192,2),dtype='<i2');pulse[0,4096]=6554
    out,source,perf,hashes=replay(a,'identity',pulse,'identity_pulse')
    delays={}
    for name,signal in [('source',source[0]),('front',out[0,:,:2]),('rear',out[0,:,4:])]:
        energy=np.max(np.abs(signal.astype(np.int32)),axis=1);at=np.flatnonzero(energy)
        delays[name]=dict(first_nonzero_offset_samples=int(at[0]-4096) if len(at) else None,peak_offset_samples=int(np.argmax(energy)-4096),peak_amplitude=int(energy.max()))
    gates['identity_source_array_delay']=delays['source']['peak_offset_samples']==1536
    result=dict(complete=True,passed=all(gates.values()),gates=gates,arms=arms,identity_delays=delays,protocol_sha256=sha(a.out/'protocol.json'),limits=protocol['int8_scope']+'; '+protocol['latency_scope'])
    (a.out/'complete.json').write_text(json.dumps(result,indent=2)+'\n')
    if not result['passed']:raise ValueError('Causal chain gate failed; artifacts retained')
    print('CAUSAL_CHAIN_TRAIN_PASS',json.dumps(gates),json.dumps(delays),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('fixture','checkpoint','assets','executable','renderer','out'):p.add_argument('--'+name,type=Path,required=True)
    main(p.parse_args())
