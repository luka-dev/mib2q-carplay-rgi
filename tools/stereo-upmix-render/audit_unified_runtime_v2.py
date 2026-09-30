#!/usr/bin/env python3
"""Two-config standalone C replay on a fixed, already inspected TRAIN fixture.

Float graph correctness and exact output pruning only. Int8 pruning equality
is not source-quality recovery or a device performance measurement.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES']='-1'
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import numpy as np
import torch
import train_unified_upmix_fit as T


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def run(args):
    r=subprocess.run([str(v) for v in args],capture_output=True,text=True)
    if r.returncode:
        raise RuntimeError(r.stderr[-2000:])
    return r


def main(a):
    if a.out.exists():raise FileExistsError('Preserve previous C audit')
    T.configure('cpu');torch.set_num_threads(2)
    raw=a.fixture.read_bytes();magic,b,t,f=struct.unpack_from('<8sIII',raw)
    if (magic,b,f)!=(b'UMXSP001',4,4100) or t<64 or len(raw)!=20+b*t*f*4:
        raise ValueError('Expected the fixed four-stream TRAIN spectrum fixture')
    values=np.frombuffer(raw,dtype='<f4',offset=20).reshape(b,t,f)[:,:64].copy()
    a.out.mkdir(parents=True);fixture=a.out/'fixture.spectra'
    fixture.write_bytes(struct.pack('<8sIII',magic,b,64,f)+values.tobytes())
    protocol=dict(script_sha256=sha(Path(__file__)),original_fixture_sha256=sha(a.fixture),
                  fixture_sha256=sha(fixture),selection='first64 frames of each already inspected TRAIN8s crop, fixed before replay',
                  float_gates='all source spectral reconstruction NSE<=1e-9; each scene maxabs<=1e-4; exact expected dense count; default C output bit exact to same-compiler v1',
                  selection_gate='all versus selected eligible, scene and full padded state bit exact, protected masks zero; four states interleaved; both float/int8',
                  scope='expanded initial graph port preparation, no capacity terminal/DEV/acoustic int8 recovery, Audi deadline, FFT/DSP or hook claim')
    (a.out/'protocol.json').write_text(json.dumps(protocol,indent=2)+'\n')
    x=torch.view_as_complex(torch.from_numpy(values.reshape(b,64,1025,2,2)).contiguous())
    arms={}
    for name,checkpoint,assets,checker in [('default',a.parent,a.parent_assets,a.default_checker),('expanded',a.expanded,a.expanded_assets,a.expanded_checker)]:
        exported=json.loads((assets/'manifest.json').read_bytes())
        if exported['checkpoint_sha256']!=sha(checkpoint):raise ValueError('Different checkpoint')
        for filename,data in exported['files'].items():
            if sha(assets/filename)!=data['sha256']:raise ValueError('Changed asset')
        net,_=T.load(checkpoint,'cpu');net.eval()
        with torch.inference_mode():pred,_=net.forward_stft(x,with_scene=True)
        dest=a.out/(name+'.output');r=run([a.executable,assets/'unified_float.bin',fixture,dest])
        measured=dest.read_bytes();h=struct.unpack_from('<8sIII',measured)
        if h!=(b'UMXOT001',4,64,17552) or len(measured)!=20+4*64*17552*4:raise ValueError('Wrong C output')
        v=torch.from_numpy(np.frombuffer(measured,dtype='<f4',offset=20).copy().reshape(b,64,17552))
        masks=torch.view_as_complex(v[:,:,:16400].reshape(b,64,4,1025,2,2).contiguous())
        est=masks*x[:,:,None];ref=pred['source_stft']
        nse={source:float((est[:,:,i]-ref[:,:,i]).abs().double().square().sum()/ref[:,:,i].abs().double().square().sum().clamp_min(1e-12)) for i,source in enumerate(T.U.SOURCES)}
        fields=('family_presence','membership','family_pan','membership_update')
        scene={field:float((v[:,:,16400+i*288:16400+(i+1)*288].reshape(b,64,48,6)-pred[field]).abs().max()) for i,field in enumerate(fields)}
        performance=json.loads(r.stderr);pruning=run([checker,assets/'unified_float.bin',assets/'unified_int8.bin',fixture])
        gates=dict(sources=max(nse.values())<=1e-9,scene=max(scene.values())<=1e-4,
                   dense_macs=performance['dense_macs_s_at_93_75fps']==net.cost()['dense_macs_per_second'],pruning=True)
        if name=='default':
            old=a.out/'default_v1.output';run([a.original_executable,assets/'unified_float.bin',fixture,old])
            gates['default_old_output_bit_exact']=old.read_bytes()==measured
        arms[name]=dict(checkpoint_sha256=sha(checkpoint),assets_manifest_sha256=sha(assets/'manifest.json'),
                        source_nse=nse,scene_maxabs=scene,performance=performance,gates=gates,
                        pruning_raw_stdout=pruning.stdout,output_sha256=sha(dest),executable_sha256=sha(a.executable),checker_sha256=sha(checker))
        print(name,json.dumps(gates),flush=True)
    passed=all(all(v['gates'].values()) for v in arms.values())
    (a.out/'complete.json').write_text(json.dumps(dict(complete=True,passed=passed,protocol_sha256=sha(a.out/'protocol.json'),arms=arms,scope=protocol['scope']),indent=2)+'\n')
    if not passed:raise ValueError('C port numerical gate failed, failure preserved')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('fixture','parent','expanded','parent-assets','expanded-assets','executable','original-executable','default-checker','expanded-checker','out'):
        p.add_argument('--'+n,type=Path,required=True)
    main(p.parse_args())
