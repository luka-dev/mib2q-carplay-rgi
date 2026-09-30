#!/usr/bin/env python3
"""Saved-weight past-only waveform replay on the exact four frozen TRAIN crops."""
import argparse
import json
from pathlib import Path
import torch
import train_unified_upmix_fit as T
import unified_upmix_stream as S


def main(a):
    T.configure('cpu')
    net,ck=T.load(a.checkpoint,'cpu');net.eval()
    if ck.get('step')!=10000:raise ValueError('Terminal fitted checkpoint required')
    raw,m=T.P.data(a.cache,a.initial)
    roles=torch.from_numpy(raw)
    mixture=roles.sum(1)
    n=mixture.shape[-1]
    with torch.inference_mode():
        spectra=S.analyze(mixture)
        identity=S.synthesize(spectra,n)
        identity_nse=float((identity-mixture).square().sum()/mixture.square().sum())
        pred,_=net.forward_stft(spectra,with_scene=False)
        full=S.synthesize(pred['source_stft'][:,:,0],n)
        stream=S.Stream(net,batch=4)
        parts=[];start=0
        for end in [1,337,512,1759,23119,n]:
            parts.append(stream.push(mixture[...,start:end]));start=end
        parts.append(stream.finish())
        delayed=torch.cat(parts,-1)
        assert delayed.shape==(4,2,n+S.DELAY)
        replay=delayed[...,S.DELAY:]
        chunk_nse=float((full-replay).square().sum()/full.square().sum().clamp_min(1e-12))
        centered,_=net.forward_stft(T.SN.stft(torch,mixture),with_scene=False)
        old=T.SN.istft(torch,centered['source_stft'][:,:,0],n)
        target=roles[:,6]
        scores={}
        for name,wave in [('centered_fit_transform',old),('causal_stream_transform',replay)]:
            scores[name]=dict(sdr_db=T.SN.si_sdr(torch,wave,target).mean(-1).tolist(),
                             after2s_sdr_db=T.SN.si_sdr(torch,wave[...,2*S.SR:],target[...,2*S.SR:]).mean(-1).tolist())
        changed=mixture.clone();changed[...,n//2:]*=-5
        altered,_=net.forward_stft(S.analyze(changed),with_scene=False)
        altered=S.synthesize(altered['source_stft'][:,:,0],n+S.DELAY,compensate=False)
        before=n//2//S.HOP*S.HOP
        prefix_nse=float((delayed[...,:before]-altered[...,:before]).square().sum()/delayed[...,:before].square().sum().clamp_min(1e-12))
    if identity_nse>1e-10 or chunk_nse>1e-9 or prefix_nse>1e-9:raise ValueError('Trained real-waveform replay failed')
    result=dict(verified=True,scope='TRAIN-only saved fitted weights; real past-only waveform structure, not DEV/device/scene/car result',
                checkpoint_sha256=T.F.sha(a.checkpoint),checkpoint_mode=ck.get('mode'),
                fixture_manifest_sha256=T.F.sha(a.initial/'manifest.json'),
                scripts={Path(module.__file__).name:T.F.sha(Path(module.__file__)) for module in (T,T.U,S)},
                crop_ids=[r['song'] for r in m['windows']],identity_nse=identity_nse,
                trained_chunk_nse=chunk_nse,future_input_prefix_nse=prefix_nse,
                delay_samples=S.DELAY,delay_ms=1000*S.DELAY/S.SR,quality=scores,
                note='Transform startup/end padding differs from centered-reflect fitting. Broad recipe must train on the causal transform.')
    if a.out.exists():raise FileExistsError('Preserve prior verification')
    T.F.write_json(a.out,result)
    print('UNIFIED_REAL_TRAIN_STREAM_VERIFIED',json.dumps(result),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['cache','initial','checkpoint','out']:p.add_argument('--'+name,type=Path,required=True)
    main(p.parse_args())
