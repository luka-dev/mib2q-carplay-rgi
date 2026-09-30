#!/usr/bin/env python3
"""TRAIN-only fixed-decoder upper bound using privileged per-frame features.

Weighted least squares finds an optimistic eligible reconstruction in the
SAVED decoder basis. Optional bounded coordinate descent respects tanh outputs.
Encoder bypassed, protected objectives ignored, no model weights changed.
This optimistic diagnostic is not a separator, a generalization or product pass.
"""
import argparse
import json
from pathlib import Path
import time
import torch
import train_unified_upmix_fit as T
import unified_upmix_stream as S
import unified_upmix_audio as H


def main(a):
    if a.out.exists():raise FileExistsError('Preserve prior diagnostic')
    T.configure('cpu');torch.set_num_threads(2)
    audit=json.loads((a.parent/'complete.json').read_bytes())
    checkpoint=a.parent/'joint_audio/last.pt'
    if not audit['audit_verified'] or T.F.sha(checkpoint)!=audit['arms']['joint_audio']['checkpoint_sha256']:
        raise ValueError('Audited joint20k weights required')
    net,ck=T.load(checkpoint,'cpu');net.eval()
    if ck['step']!=20000:raise ValueError('Wrong diagnostic checkpoint')
    raw,fixture=T.P.data(a.cache,a.initial);w=torch.from_numpy(raw)
    roles=torch.stack([w[:,6],w[:,0]+w[:,1],w[:,2:5].sum(1),w[:,5]],1)
    n=roles.shape[-1];mixture=S.analyze(roles.sum(1));target=S.analyze(roles[:,0])
    saved_latents={};hooks=[]
    if a.bounded:
        for index,dec in enumerate(net.dec):
            def capture(module,inputs,outputs,index=index):saved_latents[index]=inputs[0].detach().clone()
            hooks.append(dec[-1].register_forward_hook(capture))
    with torch.inference_mode():pred,_=net.forward_stft(mixture,with_scene=False)
    for hook in hooks:hook.remove()
    current=H.real_endpoints(pred['source_stft'][:,:,0]);target=H.real_endpoints(target)
    oracle=torch.zeros_like(target);bounded=torch.zeros_like(target) if a.bounded else None;bands=[];begin=time.monotonic()
    for index,(lo,hi) in enumerate(net.bands):
        last=net.dec[index][-1];dim=last.weight.shape[1]
        weight=last.weight.detach().double().reshape(hi-lo,4,2,2,dim)[:,0]
        bias=last.bias.detach().double().reshape(hi-lo,4,2,2)[:,0]
        x=mixture[:,:,lo:hi].reshape(-1,hi-lo,2).to(torch.complex128)
        y=target[:,:,lo:hi].reshape(-1,hi-lo,2).to(torch.complex128)
        ar=x.real[...,None]*weight[...,0,:]-x.imag[...,None]*weight[...,1,:]
        ai=x.imag[...,None]*weight[...,0,:]+x.real[...,None]*weight[...,1,:]
        br=x.real*bias[...,0]-x.imag*bias[...,1]
        bi=x.imag*bias[...,0]+x.real*bias[...,1]
        if lo==0:ai[:,0]=0;bi[:,0]=0
        if hi==T.SN.N_BINS:ai[:,-1]=0;bi[:,-1]=0
        matrix=torch.stack([ar,ai],-2).reshape(x.shape[0],-1,dim)
        base=torch.stack([br,bi],-1).reshape(x.shape[0],-1)
        truth=torch.stack([y.real,y.imag],-1).reshape(x.shape[0],-1)
        gram=matrix.transpose(1,2)@matrix
        ridge=gram.diagonal(dim1=-2,dim2=-1).mean(-1).clamp_min(1e-12)*1e-10
        rhs=matrix.transpose(1,2)@(truth-base)[...,None]
        latent=torch.linalg.solve(gram+ridge[:,None,None]*torch.eye(dim,dtype=torch.float64),rhs)[...,0]
        result=(matrix@latent[...,None])[...,0]+base
        result=result.reshape(x.shape[0],hi-lo,2,2)
        result=torch.complex(result[...,0],result[...,1]).reshape_as(target[:,:,lo:hi])
        oracle[:,:,lo:hi]=result.to(target.dtype)
        den=target[:,:,lo:hi].abs().double().square().sum().clamp_min(1e-12)
        old=float((current[:,:,lo:hi]-target[:,:,lo:hi]).abs().double().square().sum()/den)
        new=float((oracle[:,:,lo:hi]-target[:,:,lo:hi]).abs().double().square().sum()/den)
        if not torch.isfinite(latent).all() or new>old+1e-6:raise ValueError('Privileged projection failed to bound saved prediction')
        record=dict(index=index,lo_bin=lo,hi_bin=hi,latent_dimensions=dim,
            eligible_output_real_dimensions=(hi-lo)*4,decoder_eligible_weight_rank=int(torch.linalg.matrix_rank(weight.reshape(-1,dim))),
            current_spectral_nse=old,oracle_spectral_nse=new,
            unconstrained_latent_max=float(latent.abs().max()),outside_tanh_fraction=float((latent.abs()>1).double().mean()))
        if a.bounded:
            feasible=saved_latents[index].reshape(x.shape[0],dim).double()
            if float(feasible.abs().max())>1:raise ValueError('Saved tanh latent outside bounds')
            rhs_vector=rhs[...,0]
            gradient=(gram@feasible[...,None])[...,0]-rhs_vector
            diagonal=gram.diagonal(dim1=-2,dim2=-1).clamp_min(1e-20)
            # Each exact scalar minimization is feasible and cannot increase
            # the quadratic. Fixed64cycles; report residual, no optimum claim.
            for cycle in range(64):
                for j in range(dim):
                    delta=(feasible[:,j]-gradient[:,j]/diagonal[:,j]).clamp(-1,1)-feasible[:,j]
                    feasible[:,j]+=delta
                    gradient+=delta[:,None]*gram[:,:,j]
            desired=(feasible-gradient/diagonal).clamp(-1,1)
            bounded_result=(matrix@feasible[...,None])[...,0]+base
            bounded_result=bounded_result.reshape(x.shape[0],hi-lo,2,2)
            bounded[:,:,lo:hi]=torch.complex(bounded_result[...,0],bounded_result[...,1]).reshape_as(target[:,:,lo:hi]).to(target.dtype)
            bounded_nse=float((bounded[:,:,lo:hi]-target[:,:,lo:hi]).abs().double().square().sum()/den)
            if bounded_nse>old+1e-6 or float(feasible.abs().max())>1:raise ValueError('Bounded feasible projection failed')
            record.update(bounded_spectral_nse=bounded_nse,bounded_latent_max=float(feasible.abs().max()),
                          projected_coordinate_step_max=float((desired-feasible).abs().max()),
                          projected_coordinate_step_p95=float(torch.quantile((desired-feasible).abs().flatten(),.95)))
        bands.append(record)
        print('DECODER_BOUND',index+1,len(net.bands),'nse',old,new,flush=True)
    with torch.inference_mode():
        current_wave=S.synthesize(current,n);oracle_wave=S.synthesize(oracle,n)
        summaries={}
        variants=[('saved_encoder',current_wave,current),('privileged_fixed_decoder',oracle_wave,oracle)]
        if a.bounded:variants.append(('privileged_bounded_decoder',S.synthesize(bounded,n),bounded))
        for label,y,spectrum in variants:
            summaries[label]=dict(sdr_db=T.SN.si_sdr(torch,y,roles[:,0]).mean(-1).tolist(),
                waveform_nse=((y-roles[:,0]).double().square().sum((1,2))/roles[:,0].double().square().sum((1,2))).tolist(),
                spectral_nse=float((H.real_endpoints(spectrum)-target).abs().double().square().sum()/target.abs().double().square().sum()))
    if T.F.sha(checkpoint)!=audit['arms']['joint_audio']['checkpoint_sha256']:raise ValueError('Checkpoint changed')
    a.out.mkdir(parents=True)
    T.F.write_json(a.out/'complete.json',dict(complete=True,scope='four exact TRAIN crops, saved decoder basis, privileged unbounded latent per frame; no training/DEV/teacher/C/device/listening',
        checkpoint_sha256=T.F.sha(checkpoint),fixture_manifest_sha256=T.F.sha(a.initial/'manifest.json'),
        script_hashes={Path(m.__file__).name:T.F.sha(Path(m.__file__)) for m in (T,T.U,S,H)}|{Path(__file__).name:T.F.sha(Path(__file__))},
        projection='float64 per-frame eligible squared spectral error; trace-relative1e-10 ridge, physical real endpoints; unconstrained oracle ignores tanh; optional64cycle bounded coordinate descent starts from saved feasible features; protected objectives ignored',
        bounded=a.bounded,
        limits='Unbounded quality is optimistic and can violate tanh. Bounded quality is feasible in this fixed eligible decoder, not proof the encoder can infer its features or jointly preserve protected sources. Finite coordinate descent is not a certified optimum. No universal architecture limit or generalization claim. SI-SDR is not the minimized spectral error.',
        quality=summaries,bands=bands,seconds=time.monotonic()-begin))
    print('FIXED_DECODER_TRAIN_BOUND_COMPLETE',json.dumps(summaries),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('cache','initial','parent','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--bounded',action='store_true');main(p.parse_args())
