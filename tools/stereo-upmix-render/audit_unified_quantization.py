#!/usr/bin/env python3
"""Separate weight/activation rounding on the same TRAIN C fixture.

CPU-only mechanism diagnostic; no recipe promotion, gate change or QAT training.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES']='-1'
import argparse
import json
from pathlib import Path
import struct
import subprocess
import numpy as np
import torch
import verify_unified_upmix_runtime as V
T=V.T
S=V.S


def masks(path,b,frames):
    data=path.read_bytes();magic,nb,nt,nf=struct.unpack_from('<8sIII',data)
    if (magic,nb,nt,nf)!=(b'UMXOT001',b,frames,17552) or len(data)!=20+b*frames*nf*4:raise ValueError('C output layout differs')
    values=torch.from_numpy(np.frombuffer(data,dtype='<f4',offset=20).copy().reshape(b,frames,nf))
    if not torch.isfinite(values).all():raise ValueError('Nonfinite diagnostic output')
    return torch.view_as_complex(values[:,:,:16400].reshape(b,frames,4,1025,2,2).contiguous())


def main(a):
    if a.out.exists():raise FileExistsError('Preserve quantization diagnostic')
    T.configure('cpu');torch.set_num_threads(2)
    baseline=json.loads((a.reference/'complete.json').read_bytes());build=json.loads(a.build.read_bytes())
    if not baseline['float_verified']:raise ValueError('Verified C float reference required')
    raw,_=T.P.data(a.cache,a.initial);w=torch.from_numpy(raw)
    roles=torch.stack([w[:,6],w[:,0]+w[:,1],w[:,2:5].sum(1),w[:,5]],1)
    data=(a.reference/'input.spectra').read_bytes();magic,b,frames,nf=struct.unpack_from('<8sIII',data)
    if (magic,b,nf)!=(b'UMXSP001',roles.shape[0],4100) or len(data)!=20+b*frames*nf*4:raise ValueError('Saved exact input differs')
    parts=torch.from_numpy(np.frombuffer(data,dtype='<f4',offset=20).copy().reshape(b,frames,1025,2,2))
    x=torch.view_as_complex(parts.contiguous());refs=torch.stack([S.analyze(roles[:,i]) for i in range(4)],2)
    if not torch.equal(x,S.analyze(roles.sum(1))):raise ValueError('TRAIN actual mixture differs from saved numerical fixture')
    for mode in ('float','int8'):
        if T.F.sha(a.reference/(mode+'.output'))!=baseline['arms'][mode]['output_sha256']:raise ValueError('Reference output changed')
        if T.F.sha(a.assets/('unified_'+mode+'.bin'))!=baseline['arms'][mode]['blob_sha256']:raise ValueError('Exported blob changed')
    a.out.mkdir(parents=True)
    protocol=dict(build_sha256=T.F.sha(a.build),reference_check_sha256=T.F.sha(a.reference/'complete.json'),
        input_sha256=T.F.sha(a.reference/'input.spectra'),script_sha256=T.F.sha(Path(__file__)),
        scope='exact same four TRAIN8s fixtures and saved input spectra; matrices/stream/reset unchanged except one diagnostic rounding factor; no DEV/weights trained/device/quality promotion')
    T.F.write_json(a.out/'protocol.json',protocol)
    ref_mask=masks(a.reference/'float.output',b,frames);quality,reference_wave=V.score(ref_mask,x,refs,roles)
    results={}
    for tag,blob in [('weights_only','int8'),('activations_only','float')]:
        executable=a.tools/('upmix_quant_'+tag+'.exe')
        if T.F.sha(executable)!=build['builds'][tag]['executable_sha256']:raise ValueError('Diagnostic executable changed')
        dest=a.out/(tag+'.output')
        result=subprocess.run([str(executable),str(a.assets/('unified_'+blob+'.bin')),str(a.reference/'input.spectra'),str(dest)],capture_output=True,text=True)
        (a.out/(tag+'.log')).write_text(result.stderr)
        if result.returncode:raise RuntimeError('Diagnostic runtime failed')
        m=masks(dest,b,frames);metrics,wave=V.score(m,x,refs,roles)
        parity={s:float((wave[:,i]-reference_wave[:,i]).double().square().sum()/reference_wave[:,i].double().square().sum().clamp_min(1e-12)) for i,s in enumerate(T.U.SOURCES)}
        median=lambda xs:float(np.median([v for v in xs if v is not None]))
        deltas={s:median(metrics[s]['sdr_db'])-median(quality[s]['sdr_db']) for s in T.U.SOURCES}
        results[tag]=dict(quality=metrics,median_sdr_delta_float_db=deltas,waveform_nse_float=parity,
            output_sha256=T.F.sha(dest),executable_sha256=T.F.sha(executable),performance=json.loads(result.stderr))
    T.F.write_json(a.out/'complete.json',dict(complete=True,protocol_sha256=T.F.sha(a.out/'protocol.json'),results=results,
        reference_float=quality,reference_full_int8=baseline['arms']['int8']['quality'],original_int8_screen_pass=baseline['int8_screen_pass'],
        limits='Rounding mechanisms interact: single-factor errors are not additive and cannot prove QAT recovery. This checkpoint fits four crops and is not a broad candidate. PC timing of diagnostic arithmetic is not deployment timing. Original quality failure remains.'))
    print('QUANTIZATION_FACTORS_TRAIN_COMPLETE',json.dumps({k:dict(delta=v['median_sdr_delta_float_db'],nse=v['waveform_nse_float']) for k,v in results.items()}),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('cache','initial','reference','assets','build','tools','out'):p.add_argument('--'+name,type=Path,required=True)
    main(p.parse_args())
