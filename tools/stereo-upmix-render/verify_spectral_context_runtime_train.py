#!/usr/bin/env python3
"""Spectral-context saved TRAIN-only float/int8 C replay; no GPU, renderer or device claim."""
import os
os.environ['CUDA_VISIBLE_DEVICES']='-1'
import argparse
import json
from pathlib import Path
import struct
import subprocess
import numpy as np
import torch
import train_unified_upmix_fit as T
import unified_upmix_stream as S
import train_spectral_context_fit as FT
import unified_spectral_context_net as V


def wave(spectra,n):
    b,t,k,f,c=spectra.shape
    return S.synthesize(spectra.permute(0,2,1,3,4).reshape(b*k,t,f,c),n).reshape(b,k,c,n)


def score(mask,x,refs,roles):
    n=roles.shape[-1];estimate=wave(mask*x[:,:,None],n);sent=wave(mask[:,:,0,None]*refs,n)
    power=roles.square().sum((2,3));present=power>power.sum(1,keepdim=True)*1e-6
    result={}
    for i,name in enumerate(T.U.SOURCES):
        sdr=T.SN.si_sdr(torch,estimate[:,i],roles[:,i]).mean(-1)
        tx=sent[:,i].double().square().sum((1,2))/roles[:,i].double().square().sum((1,2)).clamp_min(1e-12)
        result[name]=dict(sdr_db=[float(v) if p else None for v,p in zip(sdr,present[:,i])],
                          eligible_operator_tx=[float(v) if p else None for v,p in zip(tx,present[:,i])])
    return result,estimate


def main(a):
    if a.out.exists():raise FileExistsError('Preserve previous runtime test')
    T.configure('cpu');torch.set_num_threads(2)
    assets=json.loads((a.assets/'manifest.json').read_bytes())
    if T.F.sha(a.checkpoint)!=assets['checkpoint_sha256']:raise ValueError('Exported checkpoint differs')
    for name,row in assets['files'].items():
        if T.F.sha(a.assets/name)!=row['sha256']:raise ValueError('Runtime blob changed')
    net,ck=FT.load(a.checkpoint,'cpu');net.eval()
    if ck['architecture']!=V.ARCHITECTURE:raise ValueError('Expected spectral architecture')
    raw,manifest=T.P.data(a.cache,a.initial);w=torch.from_numpy(raw)
    roles=torch.stack([w[:,6],w[:,0]+w[:,1],w[:,2:5].sum(1),w[:,5]],1)
    x=S.analyze(roles.sum(1));refs=torch.stack([S.analyze(roles[:,i]) for i in range(4)],2)
    a.out.mkdir(parents=True)
    protocol=dict(checkpoint_sha256=T.F.sha(a.checkpoint),executable_sha256=T.F.sha(a.executable),
        fixture_manifest_sha256=T.F.sha(a.initial/'manifest.json'),asset_manifest_sha256=T.F.sha(a.assets/'manifest.json'),
        script_sha256=T.F.sha(Path(__file__)),scope='four exact TRAIN8s crops, C neural graph only; dormant untrained scene still replayed structurally',
        float_gates='all-source waveformNSE<=1e-9; source SI-SDR difference<=.01dB where present; each scene/state field maxabs<=1e-4; exact dense operation count',
        int8_screen='eligible medianSDR>=float-.3dB, no eligiblecrop>-1dB loss; each protected medianSDR>=float-.3dB; source component TX values are diagnostics, not literal audible-leak gates',
        limits='No DEV/C rear, QAT, device NEON validation, loaded deadlines, FFT/DSP latency or live hook. Native Windows scalar/auto-vectorized int8 is not Krait performance.')
    T.F.write_json(a.out/'protocol.json',protocol)
    b,frames=x.shape[:2];pcm=a.out/'input.spectra'
    with pcm.open('wb') as f:
        f.write(struct.pack('<8sIII',b'UMXSP001',b,frames,1025*4));f.write(torch.view_as_real(x).numpy().astype('<f4').tobytes())
    with torch.inference_mode():pred,_=net.forward_stft(x,with_scene=True)
    reference,ref_wave=score(pred['source_masks'],x,refs,roles)
    results={};scene_names=('family_presence','membership','family_pan','membership_update')
    for mode in ('float','int8'):
        dest=a.out/(mode+'.output')
        process=subprocess.run([str(a.executable),str(a.assets/('unified_'+mode+'.bin')),str(pcm),str(dest)],capture_output=True,text=True)
        (a.out/(mode+'.log')).write_text(process.stderr)
        if process.returncode:raise RuntimeError(process.stderr)
        performance=json.loads(process.stderr)
        data=dest.read_bytes();magic,nb,nt,nf=struct.unpack_from('<8sIII',data)
        if (magic,nb,nt,nf)!=(b'UMXOT001',b,frames,17552) or len(data)!=20+b*frames*nf*4:raise ValueError('C output layout/length differs')
        values=torch.from_numpy(np.frombuffer(data,dtype='<f4',offset=20).copy().reshape(b,frames,nf))
        if not torch.isfinite(values).all():raise ValueError('Nonfinite C output')
        mask=torch.view_as_complex(values[:,:,:16400].reshape(b,frames,4,1025,2,2).contiguous())
        scene={name:float((values[:,:,16400+i*288:16400+(i+1)*288].reshape(b,frames,48,6)-pred[name]).abs().max()) for i,name in enumerate(scene_names)}
        metrics,measured_wave=score(mask,x,refs,roles)
        parity={}
        for i,name in enumerate(T.U.SOURCES):
            parity[name]=float((measured_wave[:,i]-ref_wave[:,i]).double().square().sum()/ref_wave[:,i].double().square().sum().clamp_min(1e-12))
        results[mode]=dict(performance=performance,quality=metrics,waveform_vs_torch_nse=parity,scene_max_abs_error=scene,
                           output_sha256=T.F.sha(dest),blob_sha256=T.F.sha(a.assets/('unified_'+mode+'.bin')))
        if mode=='float':
            gates=dict(all_source_waveform_parity=max(parity.values())<=1e-9,scene_parity=max(scene.values())<=1e-4,
                       dense_macs_exact=performance['dense_macs_s_at_93_75fps']==net.cost()['all_source_dense_macs_s'])
            for name in T.U.SOURCES:
                diff=[abs(c-r) for c,r in zip(metrics[name]['sdr_db'],reference[name]['sdr_db']) if c is not None and r is not None]
                gates[name+'_sdr_parity']=bool(diff) and max(diff)<=.01
            results[mode]['gates']=gates
            if not all(gates.values()):
                T.F.write_json(a.out/'failed_float.json',dict(protocol_sha256=T.F.sha(a.out/'protocol.json'),results=results))
                raise ValueError('C float graph parity gate failed')
    med=lambda xs:float(np.median([v for v in xs if v is not None]))
    old,new=(results[k]['quality'] for k in ('float','int8'))
    gates=dict(eligible_sdr_no_more_than_03db_loss=med(new['eligible']['sdr_db'])>=med(old['eligible']['sdr_db'])-.3,
               no_eligible_crop_over_1db_loss=min(n-o for n,o in zip(new['eligible']['sdr_db'],old['eligible']['sdr_db']))>=-1)
    for name in ('vocal','drums','bass'):
        gates[name+'_sdr_no_more_than_03db_loss']=med(new[name]['sdr_db'])>=med(old[name]['sdr_db'])-.3
    T.F.write_json(a.out/'complete.json',dict(complete=True,float_verified=True,checkpoint_sha256=T.F.sha(a.checkpoint),int8_screen_gates=gates,
        int8_screen_pass=all(gates.values()),reference=reference,arms=results,protocol_sha256=T.F.sha(a.out/'protocol.json'),limits=protocol['limits']))
    print('SPECTRAL_RUNTIME_TRAIN_CHECK_COMPLETE',json.dumps(dict(int8_screen=gates,performance={k:v['performance'] for k,v in results.items()},quality={k:v['quality'] for k,v in results.items()})),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('cache','initial','checkpoint','assets','executable','out'):p.add_argument('--'+name,type=Path,required=True)
    main(p.parse_args())
