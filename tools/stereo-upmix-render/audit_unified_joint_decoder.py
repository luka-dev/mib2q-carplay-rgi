#!/usr/bin/env python3
"""TRAIN-only joint fixed-basis/tanh feasibility diagnostic, CPU only.

Privileged per-frame features minimize a source-normalized squared spectral
proxy. This is not the training L1/multires objective, a causal separator, or an
architecture ceiling. All four outputs share the SAME16 features per band.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES']='-1'
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
        raise ValueError('Audited joint20k required')
    prepared=json.loads((a.parent/'prepared.json').read_bytes())
    weights=torch.tensor([1]+[prepared['auxiliary_weight']/3*n*n for n in prepared['normalizers'][1:]],dtype=torch.float64)
    scales=weights.sqrt()
    net,ck=T.load(checkpoint,'cpu');net.eval()
    if ck['step']!=20000:raise ValueError('Wrong checkpoint')
    raw,_=T.P.data(a.cache,a.initial);w=torch.from_numpy(raw)
    roles=torch.stack([w[:,6],w[:,0]+w[:,1],w[:,2:5].sum(1),w[:,5]],1)
    n=roles.shape[-1];x=S.analyze(roles.sum(1))
    refs=torch.stack([S.analyze(roles[:,i]) for i in range(4)],3)
    refs=H.real_endpoints(refs.permute(0,1,3,2,4)).permute(0,1,3,2,4)
    latents={};hooks=[]
    for index,dec in enumerate(net.dec):
        def capture(module,inputs,outputs,index=index):latents[index]=inputs[0].detach().clone()
        hooks.append(dec[-1].register_forward_hook(capture))
    with torch.inference_mode():pred,_=net.forward_stft(x,with_scene=False)
    for hook in hooks:hook.remove()
    saved=H.real_endpoints(pred['source_stft']).permute(0,1,3,2,4)
    bounded=torch.zeros_like(refs);bands=[];start=time.monotonic()
    for index,(lo,hi) in enumerate(net.bands):
        last=net.dec[index][-1];dim=last.weight.shape[1]
        weight=last.weight.detach().double().reshape(hi-lo,4,2,2,dim)
        bias=last.bias.detach().double().reshape(hi-lo,4,2,2)
        mixture=x[:,:,lo:hi].reshape(-1,hi-lo,2).to(torch.complex128)
        truth=refs[:,:,lo:hi].reshape(-1,hi-lo,4,2).to(torch.complex128)
        ar=mixture.real[:,:,None,:,None]*weight[...,0,:]-mixture.imag[:,:,None,:,None]*weight[...,1,:]
        ai=mixture.imag[:,:,None,:,None]*weight[...,0,:]+mixture.real[:,:,None,:,None]*weight[...,1,:]
        br=mixture.real[:,:,None,:]*bias[...,0]-mixture.imag[:,:,None,:]*bias[...,1]
        bi=mixture.imag[:,:,None,:]*bias[...,0]+mixture.real[:,:,None,:]*bias[...,1]
        if lo==0:ai[:,0]=0;bi[:,0]=0
        if hi==T.SN.N_BINS:ai[:,-1]=0;bi[:,-1]=0
        matrix=torch.stack([ar,ai],-2)*scales[None,None,:,None,None,None]
        base=torch.stack([br,bi],-1)*scales[None,None,:,None,None]
        target=torch.stack([truth.real,truth.imag],-1)*scales[None,None,:,None,None]
        matrix=matrix.reshape(mixture.shape[0],-1,dim);base=base.reshape(mixture.shape[0],-1);target=target.reshape(mixture.shape[0],-1)
        feasible=latents[index].reshape(mixture.shape[0],dim).double()
        if float(feasible.abs().max())>1:raise ValueError('Saved tanh features outside bounds')
        # Verify affine construction reproduces the actual simultaneous outputs.
        replay=(matrix@feasible[...,None])[...,0]+base
        actual=torch.view_as_real(saved[:,:,lo:hi].reshape_as(truth)).double()*scales[None,None,:,None,None]
        mapping_nse=float((replay-actual.reshape_as(replay)).square().sum()/actual.square().sum().clamp_min(1e-12))
        if mapping_nse>1e-10:raise ValueError('Joint affine mapping differs from saved outputs')
        old=float((replay-target).square().sum()/target.square().sum().clamp_min(1e-12))
        gram=matrix.transpose(1,2)@matrix
        rhs=(matrix.transpose(1,2)@(target-base)[...,None])[...,0]
        gradient=(gram@feasible[...,None])[...,0]-rhs
        diagonal=gram.diagonal(dim1=-2,dim2=-1).clamp_min(1e-20)
        for cycle in range(64):
            for j in range(dim):
                delta=(feasible[:,j]-gradient[:,j]/diagonal[:,j]).clamp(-1,1)-feasible[:,j]
                feasible[:,j]+=delta;gradient+=delta[:,None]*gram[:,:,j]
        desired=(feasible-gradient/diagonal).clamp(-1,1)
        result=(matrix@feasible[...,None])[...,0]+base
        new=float((result-target).square().sum()/target.square().sum().clamp_min(1e-12))
        if new>old+1e-8 or float(feasible.abs().max())>1 or not torch.isfinite(result).all():raise ValueError('Joint bounded feasibility failed')
        result=result.reshape(mixture.shape[0],hi-lo,4,2,2)/scales[None,None,:,None,None]
        output=torch.complex(result[...,0],result[...,1]).reshape_as(refs[:,:,lo:hi])
        bounded[:,:,lo:hi]=output.to(refs.dtype)
        source_errors={}
        for i,name in enumerate(T.U.SOURCES):
            den=refs[:,:,lo:hi,i].abs().double().square().sum().clamp_min(1e-12)
            source_errors[name]=dict(saved_nse=float((saved[:,:,lo:hi,i]-refs[:,:,lo:hi,i]).abs().double().square().sum()/den),
                                    bounded_nse=float((bounded[:,:,lo:hi,i]-refs[:,:,lo:hi,i]).abs().double().square().sum()/den))
        bands.append(dict(index=index,lo_bin=lo,hi_bin=hi,affine_mapping_nse=mapping_nse,
            saved_weighted_nse=old,bounded_weighted_nse=new,sources=source_errors,
            projected_coordinate_step_max=float((desired-feasible).abs().max()),
            projected_coordinate_step_p95=float(torch.quantile((desired-feasible).abs().flatten(),.95))))
        print('JOINT_DECODER_BOUND',index+1,len(net.bands),old,new,flush=True)
        del matrix,gram,rhs,gradient,result,ar,ai,br,bi
    summaries={}
    present=roles.square().sum((2,3))>roles.square().sum((1,2,3))[:,None]*1e-6
    for label,spectrum in [('saved_encoder',saved),('privileged_joint_bounded',bounded)]:
        metrics={}
        with torch.inference_mode():
            for i,name in enumerate(T.U.SOURCES):
                output=S.synthesize(spectrum[:,:,:,i],n)
                scores=T.SN.si_sdr(torch,output,roles[:,i]).mean(-1)
                den=roles[:,i].double().square().sum((1,2)).clamp_min(1e-12)
                mse=(output-roles[:,i]).double().square().sum((1,2))/den
                metrics[name]=dict(sdr_db=[float(v) if p else None for v,p in zip(scores,present[:,i])],
                                  waveform_nse=[float(v) if p else None for v,p in zip(mse,present[:,i])],
                                  spectral_nse=float((spectrum[:,:,:,i]-refs[:,:,:,i]).abs().double().square().sum()/refs[:,:,:,i].abs().double().square().sum().clamp_min(1e-12)))
        summaries[label]=metrics
    if T.F.sha(checkpoint)!=audit['arms']['joint_audio']['checkpoint_sha256']:raise ValueError('Checkpoint changed')
    a.out.mkdir(parents=True)
    T.F.write_json(a.out/'complete.json',dict(complete=True,checkpoint_sha256=T.F.sha(checkpoint),
        script_sha256=T.F.sha(Path(__file__)),fixture_manifest_sha256=T.F.sha(a.initial/'manifest.json'),
        prepared_sha256=T.F.sha(a.parent/'prepared.json'),weights_squared_spectral=weights.tolist(),
        scope='four exact TRAIN8s crops; saved four-source decoder, one shared bounded16-feature vector per band/frame; privileged targets; no weights trained or DEV accessed',
        projection='source-normalized weighted squared spectral proxy, not training L1/multires loss;64 exact-coordinate cycles starting at saved tanh features; total band error nonincreasing, individual source can trade off',
        limits='Feasible outputs do not prove a causal encoder can infer features. Finite optimization is not a certified optimum/upper quality ceiling. No rear, holdout, int8, device or ears proof.',
        quality=summaries,bands=bands,seconds=time.monotonic()-start))
    print('JOINT_DECODER_BOUND_COMPLETE',json.dumps(summaries),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('cache','initial','parent','out'):p.add_argument('--'+name,type=Path,required=True)
    main(p.parse_args())
