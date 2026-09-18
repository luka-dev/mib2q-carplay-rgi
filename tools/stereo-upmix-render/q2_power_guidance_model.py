"""Matched Q2 FiLM/control vs direct normalized-power carrier experiment.

Original stereo remains the only normal input. Source identities are not rear
roles. A supplied positive prior must be derived exclusively from declared
TRAIN targets; the training plan binds that provenance, not this math module.
No old Q2 code, objective, acceptance threshold or runtime export is changed.
"""
import hashlib

import torch

from q2_local_guidance_model import (Q2Config, Q2LocalGuidanceModel, Q2State,
                                    _blocks, _held)

ARCHITECTURE='q2-explicit-power-carrier-v1'
MODES=('film_control','direct_power')
CONTRACT=dict(architecture=ARCHITECTURE,original_only_inference=True,
    prior='fixed positive TRAIN-only source-power prior; caller binds dataset/core derivation',
    initialization='same inherited parameters/RNG; mask real coefficients prior; power-logit rows zero weights/log(prior) bias',
    direct='add (normalized held power-fraction odds - prior) times original to extraction; retain inherited grounded residual',
    disabled='no direct correction; bit-identical architecture controls at equal inherited parameters',
    before_first_update='prior carrier before global frame3; no future block',
    oracle='separate inherited TRAIN/token-only method; actual four-frame-held raw reference fractions',
    numerical='predicted fractions clamp only to float32 tiny and 1-eps; oracle exact endpoints handled explicitly',
    initial_predicted_parity='mathematical prior*X, not bit-exact sigmoid/odds roundtrip; bounded float32 test tolerance',
    total_gain_bound=None,residual_bound=None,role_status='UNKNOWN',
    family_is_rear_permission=False,quality_approved=False,native_export_supported=False,car_approved=False)


def normalized_fraction_odds(fraction,*,predicted=False):
    """B,T,K,band fractions -> channel-shared K-normalized nonnegative odds.

    Exact oracle1 endpoints split mass among exclusive heads; all-zero oracle
    fractions yield zero. No semantic/activity threshold or straight-through
    snapping is introduced. Predicted clamping is numerical endpoint protection.
    """
    if (not isinstance(fraction,torch.Tensor) or fraction.dtype!=torch.float32
        or fraction.ndim!=4 or not torch.isfinite(fraction).all()
        or (fraction<0).any() or (fraction>1).any() or type(predicted)is not bool):
        raise ValueError('Finite float32 B,T,K,band fractions in [0,1] required')
    f=fraction
    if predicted:
        limits=torch.finfo(f.dtype);f=f.clamp(min=limits.tiny,max=1.-limits.eps)
    exclusive=f>=1.;count=exclusive.sum(2,keepdim=True)
    odds=f/torch.where(f<1.,1.-f,torch.ones_like(f));total=odds.sum(2,keepdim=True)
    return torch.where(count>0,exclusive.float()/count.clamp_min(1),
                       odds/torch.where(total>0,total,torch.ones_like(total)))


class Q2PowerGuidanceModel(Q2LocalGuidanceModel):
    def __init__(self,config=None,*,prior,carrier_mode,prior_partition='TRAIN'):
        if carrier_mode not in MODES or prior_partition!='TRAIN':
            raise ValueError('Explicit carrier mode and TRAIN-only prior required')
        c=Q2Config() if config is None else config
        if type(c)is not Q2Config:raise ValueError('Q2Config required')
        p=torch.as_tensor(prior,dtype=torch.float32).detach().cpu().clone()
        if (p.shape!=(len(c.classes),) or not torch.isfinite(p).all() or not (p>0).all()
            or abs(float(p.sum())-1.)>1e-6):
            raise ValueError('Positive finite float32 prior with unit sum required')
        # Parent is constructed first. No new random draw or trainable parameter.
        super().__init__(c)
        self._carrier_mode=carrier_mode
        self.register_buffer('power_prior',p)
        self.register_buffer('carrier_mode_code',torch.tensor(MODES.index(carrier_mode),dtype=torch.int64))
        self.register_buffer('_bound_prior',p.clone(),persistent=False)
        self._bound_prior_sha256=hashlib.sha256(p.numpy().tobytes()).hexdigest()
        widths=torch.tensor([hi-lo for lo,hi in zip(self.band_edges[:-1],self.band_edges[1:])])
        self.register_buffer('_power_band_index',torch.repeat_interleave(torch.arange(c.bands),widths),persistent=False)
        with torch.no_grad():
            self.mask_bias.zero_();self.mask_bias[:,0].copy_(p);self.mask_bias[:,2].copy_(p)
            indices=torch.arange(len(c.classes))*7+1
            self.descriptor_head.weight[indices]=0.;self.descriptor_head.bias[indices]=p.log()

    @property
    def carrier_mode(self):return self._carrier_mode

    @property
    def prior_sha256(self):return self._bound_prior_sha256

    def _check_carrier_binding(self):
        if (self._carrier_mode not in MODES or self.carrier_mode_code.dtype!=torch.int64
            or self.carrier_mode_code.shape!=() or int(self.carrier_mode_code)!=MODES.index(self._carrier_mode)
            or self.power_prior.dtype!=torch.float32 or not torch.equal(self.power_prior,self._bound_prior)):
            raise ValueError('Immutable carrier mode/prior binding changed')

    def _finite_parameters(self):
        self._check_carrier_binding();super()._finite_parameters()

    def load_state_dict(self,state_dict,strict=True,assign=False):
        try:
            self._check_carrier_binding()
            if (not isinstance(state_dict,dict) or 'power_prior' not in state_dict or 'carrier_mode_code' not in state_dict
                or not isinstance(state_dict['power_prior'],torch.Tensor) or not isinstance(state_dict['carrier_mode_code'],torch.Tensor)
                or not torch.equal(state_dict['power_prior'].detach().cpu(),self._bound_prior.detach().cpu())
                or not torch.equal(state_dict['carrier_mode_code'].detach().cpu(),self.carrier_mode_code.detach().cpu())):
                raise ValueError('Checkpoint carrier mode/prior differs')
        except BaseException:
            self._state_owner=object();self._generation+=1;raise
        return super().load_state_dict(state_dict,strict=strict,assign=assign)

    def state_to_payload(self,state):
        return dict(schema='q2-power-guidance-owned-state-v1',carrier_mode=self.carrier_mode,
                    prior_sha256=self.prior_sha256,base=super().state_to_payload(state))

    def state_from_payload(self,payload):
        if (not isinstance(payload,dict) or set(payload)!={'schema','carrier_mode','prior_sha256','base'}
            or payload['schema']!='q2-power-guidance-owned-state-v1' or payload['carrier_mode']!=self.carrier_mode
            or payload['prior_sha256']!=self.prior_sha256):
            raise ValueError('Serialized carrier mode/prior differs')
        return super().state_from_payload(payload['base'])

    def _forward(self,x,state,mode,oracle):
        self._check_carrier_binding();self._validate(x,state,mode)
        state=self.initial_state(x.shape[0],mode=mode) if state is None else state
        out,carried=super()._forward(x,state,mode,oracle)
        prior=self.power_prior[None,None,:,None]
        if mode=='disabled':
            weights=prior.expand(x.shape[0],x.shape[1],-1,self.config.bands)
        else:
            if oracle is None:
                fraction=out['descriptors'][...,0]
            else:
                blocks,_,ends=_blocks(oracle,state.oracle_sum,state.frames%4)
                fraction=_held(blocks,state.held_oracle,ends,x.shape[1])[...,1]
            weights=normalized_fraction_odds(fraction,predicted=oracle is None)
            available=(torch.arange(x.shape[1],device=x.device)+state.frames)>=3
            weights=torch.where(available[None,:,None,None],weights,prior)
        if self.carrier_mode=='direct_power' and mode!='disabled':
            delta=(weights-prior).index_select(-1,self._power_band_index)
            correction=x[:,:,None]*delta[:,:,:,None,:,None]
            extraction=out['extraction_ri']+correction
            out=dict(out,extraction_ri=extraction,sources_ri=extraction+out['refinement_ri'])
        out['power_carrier_weights']=weights
        if any(not torch.isfinite(value).all() for value in out.values()):raise ValueError('Nonfinite power-carrier output')
        return out,carried

    def specification(self):
        result=super().specification()
        result.update(architecture=ARCHITECTURE,contract=dict(CONTRACT),carrier_mode=self.carrier_mode,
                      prior=self.power_prior.detach().cpu().tolist(),prior_sha256=self.prior_sha256,
                      prior_partition_required='TRAIN',additional_trainable_parameters=0,
                      additional_persistent_buffer_bytes=self.power_prior.numel()*4+8,
                      additional_runtime_buffer_bytes=self.power_prior.numel()*8+8+self._power_band_index.numel()*8,
                      carrier_mode_and_prior_bound_in_checkpoint=True,
                      carrier_mode_and_prior_bound_in_recurrent_payload=True,
                      direct_carrier_arithmetic_inherited_mac_estimate_excludes=True,
                      car_approved=False,quality_approved=False,role_status='UNKNOWN')
        return result


Model=Q2PowerGuidanceModel
