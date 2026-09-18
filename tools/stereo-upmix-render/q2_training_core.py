"""Q2 supervised fit core: original-only inference, separate source/hint losses.

CPU shared FFT -> explicit real float32 device transfer. Ground-truth descriptors
are loss targets, never normal model arguments. Oracle is a named TRAIN-only
diagnostic. Crops have explicit real-past and scored-core geometry.
"""
import math

import numpy as np
import torch
from torch.nn import functional as F

from q2_local_guidance_model import (Q2Config, supervised_guidance_targets,
                                     band_edges, _band_sum, ORACLE_TOKEN)
from r1_refiner_audio import analyze, synthesize
from r1_source_refiner import RefinerConfig

OBJECTIVE = 'q2-source-nmse-magnitude-local-guidance-error-scale-v1'


def prepare_case(row, config, device='cpu'):
    x=np.asarray(row['mixture']);s=np.asarray(row['targets'])
    k=len(config.classes);rate=config.sample_rate
    if (x.dtype!=np.float32 or x.ndim!=2 or x.shape[1]!=2 or len(x)<2048
        or s.dtype!=np.float32 or s.shape!=(len(x),k,2)
        or not np.isfinite(x).all() or not np.isfinite(s).all()
        or tuple(row['classes'])!=config.classes or row['sample_rate']!=rate
        or row['partition'] not in ('TRAIN','DEV')):
        raise ValueError('Aligned finite declared TRAIN/DEV original/source PCM required')
    start,stop=row['core_start'],row['core_stop']
    if type(start)is not int or type(stop)is not int or not 0<=start<stop<=len(x):
        raise ValueError('Explicit valid PCM core required')
    known=np.asarray(row['reconstruction_known']);detect=np.asarray(row['detection_known'])
    if any(a.dtype!=np.bool_ or a.shape!=(k,) for a in (known,detect)) or not known.any():
        raise ValueError('Separate source reconstruction/detection completeness masks required')
    ac=RefinerConfig(sample_rate=rate,fft_size=1024)
    original=analyze(torch.from_numpy(x.copy()),ac)
    frames=torch.stack([analyze(torch.from_numpy(s[:,j].copy()),ac).frames for j in range(k)],2)
    xr=torch.view_as_real(original.frames).contiguous()
    sr=torch.view_as_real(frames).contiguous()
    valid=(original.frame_starts>=start)&(original.frame_ends<=stop)
    if not valid.any():raise ValueError('No complete windows inside the scored core')
    truth=supervised_guidance_targets(xr,sr,config)
    # A descriptor update summarizes four input frames: all four must lie in
    # the declared core, not just its last frame.
    block_valid=F.conv1d(F.pad(valid.float()[None,None],(3,0)),torch.ones(1,1,4))[0,0]==4
    weights=torch.full((config.bins,),2.);weights[[0,-1]]=1.
    source_power=(sr.square().sum(-1)*weights).sum(-1).mean(-1)/config.fft_size**2
    mix_power=(xr.square().sum(-1)*weights).sum(-1).mean(-1)/config.fft_size**2
    scale=source_power[:,valid].mean((0,1))
    denominator=torch.maximum(scale,mix_power[:,valid].mean().expand_as(scale)*.01).clamp_min(1e-9)
    tensor=lambda v:v.to(device)
    return dict(original=tensor(xr),targets=tensor(sr),valid=tensor(valid),
                block_valid=tensor(block_valid),truth={n:tensor(v) for n,v in truth.items()},
                known=tensor(torch.from_numpy(known.copy())),detection_known=tensor(torch.from_numpy(detect.copy())),
                denominator=tensor(denominator),weights=tensor(weights),
                length=len(x),core=[start,stop],config=config,partition=row['partition'],
                id=row['id'],song_id=row['song_id'],artist=row['artist'])


def _masked_mean(value,mask):
    mask=torch.broadcast_to(mask,value.shape)
    # Multiplication would let NaN in an unknown target poison a loss; case
    # construction already rejects nonfinite audio, and where isolates masks.
    return torch.where(mask,value,torch.zeros_like(value)).sum()/mask.sum().clamp_min(1)


def objective(outputs,case,start,stop):
    target=case['targets'][:,start:stop];p=outputs['sources_ri'];c=case['config']
    if p.shape!=target.shape:raise ValueError('Prediction/target geometry differs')
    valid=case['valid'][start:stop];known=case['known']
    if not valid.any() or not known.any():raise ValueError('Loss needs known complete source windows')
    error=(p-target).square().sum(-1)
    power_error=(error*case['weights']).sum(-1).mean(-1)/c.fft_size**2
    source=_masked_mean(power_error/case['denominator'],valid[None,:,None]&known[None,None,:])
    pm=p.square().sum(-1).clamp_min(1e-12).sqrt()
    tm=target.square().sum(-1).clamp_min(1e-12).sqrt()
    magnitude_error=((pm-tm).abs()*case['weights']).sum(-1).mean(-1)/c.fft_size**1.5
    magnitude=_masked_mean(magnitude_error/case['denominator'].sqrt(),valid[None,:,None]&known[None,None,:])
    truth=case['truth'];updates=outputs['updated'] & case['block_valid'][None,start:stop]
    if not torch.equal(outputs['updated'],truth['updated'][:,start:stop]):
        raise ValueError('Guidance/model cadence differs from frozen target timeline')
    detection_mask=updates[:,:,None,None]&case['detection_known'][None,None,:,None]
    hint_target=truth['block_guidance'][:,start:stop]
    activity=_masked_mean(F.binary_cross_entropy_with_logits(outputs['activity_logits'],hint_target[...,0],reduction='none'),detection_mask)
    descriptor_mask=detection_mask[...,None]&truth['block_descriptor_known'][:,start:stop]
    descriptor=_masked_mean(F.smooth_l1_loss(outputs['descriptors'],hint_target[...,1:],reduction='none'),descriptor_mask)
    # Error scale is a separate calibration target, never a learned downweight
    # on reconstruction loss, a hard source gate or musical-role permission.
    with torch.no_grad():
        be=_band_sum(error.detach()*case['weights'],band_edges(c)).mean(-2)/c.fft_size**2
        x=case['original'][:,start:stop]
        bp=_band_sum(x.square().sum(-1)*case['weights'],band_edges(c)).mean(-2)/c.fft_size**2
        log_error=((be+1e-9)/(bp[:,:,None]+1e-9)).log().clamp(-8,8)
    uncertainty=_masked_mean(F.smooth_l1_loss(outputs['log_error_scale'],log_error,reduction='none'),
                             updates[:,:,None,None]&known[None,None,:,None])
    loss=source+.1*magnitude+.05*activity+.05*descriptor+.01*uncertainty
    if not torch.isfinite(loss):raise ValueError('Nonfinite objective')
    return loss,dict(source=source.detach(),magnitude=magnitude.detach(),activity=activity.detach(),
                     descriptor=descriptor.detach(),uncertainty=uncertainty.detach(),total=loss.detach())


def forward_chunk(model,case,start,stop,state=None,arm='predicted'):
    x=case['original'][:,start:stop]
    if arm=='oracle_train':
        if case['partition']!='TRAIN':raise ValueError('Oracle forbidden outside TRAIN')
        return model.forward_oracle_train(x,case['truth']['raw_guidance'][:,start:stop],state,
                                          partition='train',diagnostic_token=ORACLE_TOKEN)
    if arm not in ('predicted','disabled'):raise ValueError('Unknown experiment arm')
    return model(x,state,guidance_mode=arm)


@torch.no_grad()
def predict_case(model,case,arm='predicted',chunk_frames=64):
    if type(chunk_frames)is not int or not 1<=chunk_frames<=2048:raise ValueError('Bounded frame chunk')
    state=None;pieces=[]
    for start in range(0,case['original'].shape[1],chunk_frames):
        out,state=forward_chunk(model,case,start,min(start+chunk_frames,case['original'].shape[1]),state,arm)
        pieces.append(out['sources_ri'].detach().cpu())
    return torch.cat(pieces,1)


def source_pcm(spectra,case):
    if spectra.device.type!='cpu':raise ValueError('Explicit CPU synthesis transfer required')
    ac=RefinerConfig(sample_rate=case['config'].sample_rate,fft_size=1024)
    return torch.stack([synthesize(torch.view_as_complex(spectra[:,:,j].contiguous()),case['length'],ac)
                        for j in range(len(case['config'].classes))],1)


def fit_scalars(rows,normalization='source-relative'):
    """TRAIN-only L/R-shared gains; explicit raw or equal-song PCM objective.

    Source-relative uses per-row PCM power floors, NOT a claim to optimize the
    STFT/hint training objective. The full-source pilot requires every aggregate
    target complete; an unknown row raises instead of silently dropping it.
    """
    if normalization not in ('source-relative','raw-pcm'):raise ValueError('Explicit scalar objective required')
    numerator=None;denominator=None
    for row in rows:
        if row['partition']!='TRAIN':raise ValueError('Scalar fitting is TRAIN-only')
        a,b=row['core_start'],row['core_stop'];x=row['mixture'][a:b].astype('f8');s=row['targets'][a:b].astype('f8')
        if not np.asarray(row['reconstruction_known']).all():raise ValueError('Scalar fit requires complete targets')
        if normalization=='raw-pcm':
            value=(x[:,None]*s).sum((0,2));den=np.full(s.shape[1],float((x*x).sum()))
        else:
            floor=np.maximum((s*s).mean((0,2)),max(float((x*x).mean())*.01,1e-9))
            value=(x[:,None]*s).mean((0,2))/floor;den=float((x*x).mean())/floor
        numerator=value if numerator is None else numerator+value
        denominator=den if denominator is None else denominator+den
    if numerator is None or not (denominator>0).all():raise ValueError('No active TRAIN mixture')
    return numerator/denominator


def pcm_metrics(prediction,row,scalars):
    a,b=row['core_start'],row['core_stop'];p=np.asarray(prediction)[a:b].astype('f8')
    s=row['targets'][a:b].astype('f8');x=row['mixture'][a:b].astype('f8')
    if p.shape!=s.shape or not np.isfinite(p).all():raise ValueError('Finite aligned predicted PCM required')
    mixpower=float((x*x).mean());result=[]
    for k,name in enumerate(row['classes']):
        power=(s[:,k]**2).mean(0);error=((p[:,k]-s[:,k])**2).mean(0)
        base=((scalars[k]*x-s[:,k])**2).mean(0)
        known=bool(row['reconstruction_known'][k]);active=known&(power>=max(1e-6,mixpower*1e-4))
        nmse=[float(error[c]/power[c]) if active[c] else None for c in range(2)]
        baseline=[float(base[c]/power[c]) if active[c] else None for c in range(2)]
        result.append(dict(identity=name,known=known,active=active.tolist(),target_power=power.tolist(),
                           mse=error.tolist(),nmse=nmse,train_scalar_nmse=baseline,
                           mute_nmse=[1. if v else None for v in active],
                           output_power=(p[:,k]**2).mean(0).tolist()))
    return dict(id=row['id'],song_id=row['song_id'],artist=row['artist'],partition=row['partition'],
                core_samples=[a,b],families=result,peak=float(np.abs(p).max()),
                role_status='UNKNOWN',quality_approved=False,car_approved=False)
