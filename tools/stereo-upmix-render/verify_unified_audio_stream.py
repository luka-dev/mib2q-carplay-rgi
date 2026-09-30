#!/usr/bin/env python3
"""CPU TRAIN end-to-end C FFT/neural/WOLA verification, no device claim."""
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


def replay(executable,model,signal,folder,tag):
    b,n,c=signal.shape
    source=folder/(tag+'.input');dest=folder/(tag+'.output')
    source.write_bytes(struct.pack('<8sIII',b'UMXPC001',b,n,c)+signal.astype('<f4').tobytes())
    result=subprocess.run([str(executable),str(model),str(source),str(dest)],capture_output=True,text=True)
    (folder/(tag+'.log')).write_text(result.stderr)
    if result.returncode:raise RuntimeError('C audio stream failed: '+result.stderr)
    data=dest.read_bytes();head=struct.unpack_from('<8sIII',data)
    if head!=(b'UMXAU001',b,n+S.DELAY,2) or len(data)!=20+b*(n+S.DELAY)*2*4:raise ValueError('C PCM layout/length differs')
    y=np.frombuffer(data,dtype='<f4',offset=20).copy().reshape(b,n+S.DELAY,2)
    if not np.isfinite(y).all():raise ValueError('Nonfinite C output')
    return torch.from_numpy(y).permute(0,2,1),json.loads(result.stderr),T.F.sha(dest)


def nse(y,ref):return float((y-ref).double().square().sum()/ref.double().square().sum().clamp_min(1e-12))


def main(a):
    if a.out.exists():raise FileExistsError('Preserve prior stream audit')
    T.configure('cpu');torch.set_num_threads(2)
    assets=json.loads((a.assets/'manifest.json').read_bytes())
    if T.F.sha(a.checkpoint)!=assets['checkpoint_sha256']:raise ValueError('Wrong exported checkpoint')
    for name,row in assets['files'].items():
        if T.F.sha(a.assets/name)!=row['sha256']:raise ValueError('Model blob changed')
    net,_=T.load(a.checkpoint,'cpu');net.eval()
    raw,_=T.P.data(a.cache,a.initial);w=torch.from_numpy(raw)
    roles=torch.stack([w[:,6],w[:,0]+w[:,1],w[:,2:5].sum(1),w[:,5]],1)
    mixture=roles.sum(1);n=mixture.shape[-1]
    a.out.mkdir(parents=True)
    protocol=dict(checkpoint_sha256=T.F.sha(a.checkpoint),executable_sha256=T.F.sha(a.executable),
        script_sha256=T.F.sha(Path(__file__)),fixture_manifest_sha256=T.F.sha(a.initial/'manifest.json'),asset_manifest_sha256=T.F.sha(a.assets/'manifest.json'),
        gates='identity NSE<=1e-10; float real PCM path NSE<=1e-7; eligible SI-SDR max delta<=.01dB; repeat/reset exact; future-changed suffix leaves complete emitted-hop prefix exact',
        quantized_scope='recorded against the original int8 neural-only PCM; no int8 acoustic gate waiver or scene/device claim',
        scope='four exact TRAIN8s PCM crops; standalone C actual past FFT, unified neural graph, physical endpoints, eligible iFFT/Hann WOLA; device/renderer scheduling and semantic scene excluded')
    T.F.write_json(a.out/'protocol.json',protocol)
    input_pcm=mixture.permute(0,2,1).numpy()
    with torch.inference_mode():
        pred,_=net.forward_stft(S.analyze(mixture),with_scene=False)
        reference=S.synthesize(pred['source_stft'][:,:,0],n)
    identity,timing,identity_hash=replay(a.executable,'identity',input_pcm,a.out,'identity')
    results={'identity':dict(nse=nse(identity[:,:,S.DELAY:],mixture),performance=timing,output_sha256=identity_hash)}
    for mode in ('float','int8'):
        y,timing,h=replay(a.executable,a.assets/('unified_'+mode+'.bin'),input_pcm,a.out,mode)
        aligned=y[:,:,S.DELAY:]
        result=dict(waveform_vs_torch_nse=nse(aligned,reference),output_sha256=h,performance=timing,
            eligible_sdr_db=T.SN.si_sdr(torch,aligned,roles[:,0]).mean(-1).tolist(),
            maximum_sample_difference=float((aligned-reference).abs().max()))
        if mode=='float':
            repeated,_,_=replay(a.executable,a.assets/'unified_float.bin',input_pcm,a.out,'float_repeat')
            changed=input_pcm[:1].copy();onset=n//2;changed[:,onset:]*=-.8
            future,_,_=replay(a.executable,a.assets/'unified_float.bin',changed,a.out,'float_future')
            prefix=(onset//T.SN.HOP)*T.SN.HOP
            old_score=T.SN.si_sdr(torch,reference,roles[:,0]).mean(-1)
            new_score=T.SN.si_sdr(torch,aligned,roles[:,0]).mean(-1)
            result.update(reset_max_abs_error=float((y-repeated).abs().max()),
                future_emitted_prefix_max_abs_error=float((y[:1,:,:prefix]-future[:,:,:prefix]).abs().max()),
                eligible_sdr_max_delta_db=float((new_score-old_score).abs().max()))
            gates=dict(identity=results['identity']['nse']<=1e-10,pcm_parity=result['waveform_vs_torch_nse']<=1e-7,
                reset_exact=result['reset_max_abs_error']==0,future_prefix_exact=result['future_emitted_prefix_max_abs_error']==0,
                eligible_sdr_delta=result['eligible_sdr_max_delta_db']<=.01)
            result['gates']=gates
            if not all(gates.values()):
                T.F.write_json(a.out/'failed_float.json',dict(protocol=protocol,result=result,identity=results['identity']))
                raise ValueError('C waveform stream audit failed')
        else:
            # Exact previous quantized neural predictions distinguish FFT error
            # from the already recorded post-training quantization quality loss.
            neural=json.loads((a.neural_check/'complete.json').read_bytes())
            if T.F.sha(a.assets/'unified_int8.bin')!=neural['arms']['int8']['blob_sha256']:raise ValueError('Neural-only int8 reference differs')
            data=(a.neural_check/'int8.output').read_bytes();magic,b,frames,nf=struct.unpack_from('<8sIII',data)
            if (magic,b,nf)!=(b'UMXOT001',mixture.shape[0],17552):raise ValueError('Neural reference layout differs')
            values=torch.from_numpy(np.frombuffer(data,dtype='<f4',offset=20).copy().reshape(b,frames,nf))
            masks=torch.view_as_complex(values[:,:,:16400].reshape(b,frames,4,1025,2,2).contiguous())
            previous=S.synthesize(masks[:,:,0]*S.analyze(mixture),n)
            result['waveform_vs_previous_int8_neural_nse']=nse(aligned,previous)
            result['neural_only_int8_screen_pass']=neural['int8_screen_pass']
            result['neural_only_check_sha256']=T.F.sha(a.neural_check/'complete.json')
        results[mode]=result
    T.F.write_json(a.out/'complete.json',dict(complete=True,float_verified=True,protocol_sha256=T.F.sha(a.out/'protocol.json'),results=results,
        limits='TRAIN only. Int8 original quality failure remains. Dormant scene is not used to render. No C renderer, NEON execution, device latency/deadlines or owner ears. Array delay1536samples; hop acquisition<=512samples; measured PC execution is separate.'))
    print('UNIFIED_C_WAVEFORM_TRAIN_COMPLETE',json.dumps(results),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('cache','initial','checkpoint','assets','executable','neural-check','out'):p.add_argument('--'+name,type=Path,required=True)
    main(p.parse_args())
