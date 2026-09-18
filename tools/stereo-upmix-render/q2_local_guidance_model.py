"""Q2 quality-reference source-local guidance, real-valued causal spectral graph.

Source identities are not musical roles. This module has no rear permission,
PCM frontend, native export, or demonstrated target-hardware budget. Normal
forward consumes original stereo only. The explicitly named TRAIN oracle is a
decoder witness, never an inference option or an inferability claim.
"""
from dataclasses import asdict, dataclass
import hashlib
import json
import math

import torch
from torch import nn
from torch.nn import functional as F


CLASSES = ('lead_vocal', 'backing_vocal', 'other_vocal', 'bass', 'drums',
           'high_percussion', 'percussion', 'guitar', 'keys', 'strings_winds',
           'other_plucked', 'other_unknown')
DESCRIPTORS = ('source_power_fraction', 'source_flux', 'stereo_balance',
               'coherence_real', 'coherence_imag')
ARCHITECTURE = 'q2-local-tf-guidance-real-quadrature-reference-v2-static-carrier'
ORACLE_TOKEN = 'TRAIN_ONLY_TARGET_GUIDANCE_DECODER_WITNESS'
CONTRACT = dict(architecture=ARCHITECTURE, original_only_inference=True,
    input='float32 B,T,2,513,2 stereo rFFT real/imag; real endpoints',
    guidance='per-source/per-band supervised acoustic descriptors, soft activity and prediction-error scale',
    guidance_cadence='completed four-frame block, available on frame3,7,...; held between updates',
    audio='full-bin bounded dynamic diagonal complex mask plus unbounded static coefficient bias and grounded residual',
    static_carrier='zero-init coefficient bias AFTER bounded dynamic term; exact finite scalar-times-original submodel; no RNG draw',
    audio_context='frequency kernel5; causal dilated temporal kernels3 at1,2,4,8; 31 frames',
    global_context='all32 bands mixed through a learned cross-band bottleneck before shared per-band GRU',
    disabled='only explicit predicted acoustic/activity/error hints removed; contextual encoder and all heads retained',
    uncertainty='auxiliary detached prediction-error target; not source-label certainty or rear permission',
    zero='zero entire input/history gives exact zero audio; nonzero history may produce tails',
    classes_are_disjoint_dataset_identities=True, family_is_rear_permission=False,
    role_prediction_implemented=False, role_status='UNKNOWN',
    oracle='separate TRAIN-only method with explicit token; no target argument in normal forward',
    output_units='original unnormalized rFFT quadratures; fixed sqrtFFT encoder scale restored',
    no_independent_source_agc=True, all_model_tensors_real_float32=True,
    source_quality_approved=False, native_export_supported=False, car_approved=False)


@dataclass(frozen=True)
class Q2Config:
    sample_rate: int = 48000
    fft_size: int = 1024
    bands: int = 32
    width: int = 24
    band_hidden: int = 48
    update_hops: int = 4
    classes: tuple = CLASSES

    def __post_init__(self):
        if (type(self.sample_rate) is not int or self.sample_rate not in (44100, 48000)
            or any(type(getattr(self, k)) is not int or getattr(self, k) != v for k, v in
                   dict(fft_size=1024, bands=32, update_hops=4).items())
            or type(self.width) is not int or type(self.band_hidden) is not int
            or (self.width, self.band_hidden) not in ((24, 48), (12, 24))
            or type(self.classes) is not tuple or not 1 <= len(self.classes) <= 32
            or len(set(self.classes)) != len(self.classes)
            or any(type(x) is not str or not x or not x.isidentifier() for x in self.classes)):
            raise ValueError('Q2 fixed reference24/48 or separate student12/24, explicit identity vocabulary required')

    @property
    def hop(self): return 256

    @property
    def bins(self): return 513

    @property
    def dilations(self): return (1, 2, 4, 8)


def band_edges(config):
    return tuple(i + (config.bins-config.bands)*i*i//config.bands**2
                 for i in range(config.bands+1))


def _band_sum(value, edges):
    # No per-bin Python loop or dense513x32 multiply. Summation is float32;
    # tests/report distinguish rounding from exact arithmetic identities.
    padded = F.pad(value.cumsum(-1), (1, 0))
    lo = torch.tensor(edges[:-1], dtype=torch.long, device=value.device)
    hi = torch.tensor(edges[1:], dtype=torch.long, device=value.device)
    return padded.index_select(-1, hi)-padded.index_select(-1, lo)


def _safe_ratio(numerator, denominator):
    return numerator/torch.where(denominator > 0, denominator, torch.ones_like(denominator))


def _quadratures(value, bins=513):
    if (not isinstance(value, torch.Tensor) or value.dtype != torch.float32 or value.ndim < 4
        or value.shape[-3:] != (2, bins, 2) or not torch.isfinite(value).all()
        or value[..., 0, 1].any() or value[..., -1, 1].any()):
        raise ValueError('Finite float32 stereo real rFFT quadratures with real endpoints required')


def _powers(value, edges):
    """Scale-free stereo band statistics and log physical power, no float64.

    One common current-frame scale only protects intermediate arithmetic. Audio
    is never divided by this scale on output, nor independently normalized.
    """
    scale = value.abs().amax(dim=(-3, -2, -1))
    safe = torch.where(scale > 0, scale, torch.ones_like(scale))
    z = value/safe[..., None, None, None]
    weights = z.new_full((z.shape[-2],), 2.); weights[[0, -1]] = 1.
    p = _band_sum(z.square().sum(-1)*weights, edges).clamp_min(0)
    l, r = p[..., 0, :], p[..., 1, :]
    cross_r = _band_sum((z[..., 0, :, 0]*z[..., 1, :, 0]
                         + z[..., 0, :, 1]*z[..., 1, :, 1])*weights, edges)
    cross_i = _band_sum((z[..., 0, :, 1]*z[..., 1, :, 0]
                         - z[..., 0, :, 0]*z[..., 1, :, 1])*weights, edges)
    power = (l+r)*.5
    valid = (power > 0) & (scale[..., None] > 0)
    log_power = torch.log(torch.where(power > 0, power, torch.ones_like(power)))
    log_power = log_power+2*torch.log(safe[..., None])-2*math.log(1024.)
    log_power = torch.where(valid, log_power, torch.zeros_like(log_power))
    balance = _safe_ratio(l-r, l+r)
    # Positive relative stabilizer, with sqrt evaluated on1 in the zero case:
    # zero backward must not contain an unused sqrt(0) derivative.
    total = l+r
    product = (l+1e-6*total)*(r+1e-6*total)
    denom = torch.sqrt(torch.where(total > 0, product, torch.ones_like(product)))
    return power, log_power, valid, balance, (cross_r/denom).clamp(-1,1), (cross_i/denom).clamp(-1,1)


def _flux(log_power, valid, previous_log, previous_valid):
    pl = torch.cat((previous_log[:, None], log_power[:, :-1]), 1)
    pv = torch.cat((previous_valid[:, None], valid[:, :-1]), 1)
    active = torch.tanh(.5*(log_power-pl))
    return torch.where(valid & pv, active,
                       torch.where(valid, torch.ones_like(active),
                                   torch.where(pv, -torch.ones_like(active), torch.zeros_like(active))))


def supervised_guidance_targets(original_ri, sources_ri, config=None, *,
                                activity_relative=1e-4, activity_absolute=1e-9,
                                spatial_power_floor=1e-12):
    """Reference-only descriptors for a complete reset-origin sequence.

    Returns RAW per-frame activity/descriptor values and validity. The harness
    must intersect `descriptor_known`/`activity_known` with dataset class_known
    and full-original-window/core masks. No file label is silently a negative.
    `block_guidance` is the causal four-frame arithmetic mean, held after block
    completion; zero before frame3. Supervise only `updated` frames. Uncertainty
    has NO invented ground truth here: fit detached prediction error separately.
    """
    c = Q2Config() if config is None else config
    _quadratures(original_ri); _quadratures(sources_ri)
    if (original_ri.ndim != 5 or sources_ri.ndim != 6
        or sources_ri.shape[:2] != original_ri.shape[:2]
        or sources_ri.shape[2] != len(c.classes) or sources_ri.device != original_ri.device
        or not original_ri.shape[0] or not original_ri.shape[1]
        or any(type(v) not in (int, float) or not math.isfinite(v) or v <= 0
               for v in (activity_relative, activity_absolute, spatial_power_floor))):
        raise ValueError('Aligned full-sequence sources and explicit positive target floors required')
    edges = band_edges(c)
    _, mlx, mv, *_ = _powers(original_ri, edges)
    _, sl, sv, balance, cr, ci = _powers(sources_ri, edges)
    flux = _flux(sl, sv, torch.zeros_like(sl[:, 0]), torch.zeros_like(sv[:, 0]))
    both = sv & mv[:, :, None]
    fraction = torch.where(both, torch.sigmoid(sl-mlx[:, :, None]),
                           torch.where(sv, torch.ones_like(sl), torch.zeros_like(sl)))
    relative_threshold=torch.where(mv,mlx+math.log(activity_relative),
                                   mlx.new_full((),math.log(activity_absolute)))
    threshold = torch.maximum(relative_threshold[:, :, None],sl.new_full((),math.log(activity_absolute)))
    active = (sv & (sl > threshold)).float()
    descriptors = torch.stack((fraction, flux, balance, cr, ci), -1)
    spatial_known = sv & (sl > math.log(spatial_power_floor))
    known = torch.stack((torch.ones_like(sv), torch.ones_like(sv), spatial_known,
                         spatial_known, spatial_known), -1)
    raw = torch.cat((active[..., None], descriptors), -1)
    held = torch.zeros_like(raw); updated = torch.zeros(original_ri.shape[:2], dtype=torch.bool, device=raw.device)
    n = original_ri.shape[1]//4
    if n:
        block = raw[:, :n*4].reshape(raw.shape[0], n, 4, *raw.shape[2:]).mean(2)
        positions = torch.arange(raw.shape[1], device=raw.device)
        index = ((positions+1)//4).clamp_max(n)
        lookup = torch.cat((torch.zeros_like(block[:, :1]), block), 1)
        held = lookup.index_select(1, index)
        updated[:, 3:n*4:4] = True
    # A block descriptor is only known if ALL contributing frames are known;
    # callers can score power/activity independently of spatial quietness.
    block_known = torch.zeros_like(known)
    if n:
        bk = known[:, :n*4].reshape(known.shape[0], n, 4, *known.shape[2:]).all(2)
        block_known = torch.cat((torch.zeros_like(bk[:, :1]), bk), 1).index_select(1, index)
    return dict(raw_guidance=raw, block_guidance=held, descriptor_known=known,
                block_descriptor_known=block_known, activity_known=torch.ones_like(active, dtype=torch.bool),
                updated=updated, source_log_power=sl, source_power_present=sv)


@dataclass(frozen=True)
class Q2State:
    owner: object
    frames: int
    mode: str
    previous_log_power: torch.Tensor
    previous_power_valid: torch.Tensor
    encoder_histories: tuple
    block_sum: torch.Tensor
    band_hidden: torch.Tensor
    held_context: torch.Tensor
    held_raw_guidance: torch.Tensor
    oracle_sum: torch.Tensor
    held_oracle: torch.Tensor

    def detach(self):
        clone = lambda x: x.detach().clone()
        return Q2State(self.owner, self.frames, self.mode, clone(self.previous_log_power),
                       clone(self.previous_power_valid), tuple(clone(x) for x in self.encoder_histories),
                       clone(self.block_sum), clone(self.band_hidden), clone(self.held_context),
                       clone(self.held_raw_guidance), clone(self.oracle_sum), clone(self.held_oracle))


def _blocks(values, pending, phase):
    """Completed blocks only, at most one partial prefix/suffix; vectorized middle."""
    size = values.shape[1]; start = 0; blocks = []; ends = []
    if phase:
        take = min(4-phase, size)
        pending = pending+values[:, :take].sum(1); start = take
        if phase+take < 4: return values[:, :0], pending, []
        blocks.append((pending*.25)[:, None]); ends.append(take-1)
    count = (size-start)//4
    if count:
        blocks.append(values[:, start:start+count*4].reshape(values.shape[0], count, 4,
                                                            *values.shape[2:]).mean(2))
        ends.extend(range(start+3, start+count*4, 4)); start += count*4
    pending = values[:, start:].sum(1)
    return (torch.cat(blocks, 1) if blocks else values[:, :0]), pending, ends


def _held(blocks, old, ends, steps):
    if not ends: return old[:, None].expand(-1, steps, *old.shape[1:])
    time = torch.arange(steps, device=old.device)
    end_tensor = torch.tensor(ends, device=old.device)
    index = (time[:, None] >= end_tensor[None]).sum(-1)
    return torch.cat((old[:, None], blocks), 1).index_select(1, index)


class Q2LocalGuidanceModel(nn.Module):
    def __init__(self, config=None):
        super().__init__(); self.config = Q2Config() if config is None else config
        if type(self.config) is not Q2Config: raise ValueError('Q2Config required')
        c=self.config; w,h,k=c.width,c.band_hidden,len(c.classes)
        self._state_owner=object(); self._generation=0; self.band_edges=band_edges(c)
        self.frequency_encoder=nn.Conv1d(4,w,5,padding=2,bias=False)
        self.temporal_kernels=nn.ParameterList([nn.Parameter(torch.empty(w,3)) for _ in c.dilations])
        for value in self.temporal_kernels: nn.init.uniform_(value,-1/math.sqrt(3),1/math.sqrt(3))
        self.audio_mix=nn.Conv1d(w,w,1,bias=False)
        self.band_projection=nn.Linear(w+7,h,bias=False)
        self.cross_band_in=nn.Linear(c.bands*h,h//2,bias=False)
        self.cross_band_out=nn.Linear(h//2,c.bands*h,bias=False)
        self.band_gru=nn.GRU(h,h,bias=False,batch_first=True)
        self.descriptor_head=nn.Linear(h,k*7,bias=True)
        self.context_film=nn.Linear(h,k*w,bias=False)
        self.hint_film=nn.Linear(7,w,bias=False)
        self.query_scale=nn.Parameter(torch.zeros(k,w))
        self.mask_head=nn.Parameter(torch.zeros(k,w,4))
        # Coefficient bias, NOT additive audio: it multiplies original stereo.
        # After the dynamic bound so any finite scalar control is representable.
        # Zeros perform no RNG draw and preserve the previous core initialization.
        self.mask_bias=nn.Parameter(torch.zeros(k,4))
        self.refinement_head=nn.Parameter(torch.zeros(k,w,4))

    @property
    def generation(self): return self._generation

    def _finite_parameters(self):
        for value in self.parameters():
            if value.dtype != torch.float32 or not torch.isfinite(value).all():
                raise ValueError('Finite real float32 model parameters required')

    def load_state_dict(self, state_dict, strict=True, assign=False):
        try:
            if strict is not True or assign is not False: raise ValueError('Strict copying checkpoint load required')
            expected=super().state_dict()
            if not isinstance(state_dict,dict) or state_dict.keys() != expected.keys(): raise ValueError('Q2 checkpoint keys differ')
            for name,value in state_dict.items():
                if (not isinstance(value,torch.Tensor) or value.dtype != expected[name].dtype
                    or value.shape != expected[name].shape or not torch.isfinite(value).all()):
                    raise ValueError('Invalid Q2 checkpoint tensor: '+name)
            return super().load_state_dict(state_dict,strict=True,assign=False)
        finally:
            self._state_owner=object(); self._generation+=1

    def initial_state(self, batch_size=1, *, mode='predicted'):
        if type(batch_size) is not int or batch_size <= 0 or mode not in ('predicted','disabled','oracle_train'):
            raise ValueError('Positive batch size and explicit execution mode required')
        self._finite_parameters(); c=self.config; b=batch_size;k=len(c.classes)
        z=lambda *shape: self.mask_head.new_zeros(shape)
        return Q2State(self._state_owner,0,mode,z(b,c.bands),z(b,c.bands).bool(),
            tuple(z(b,2*d,c.width,c.bins) for d in c.dilations),z(b,c.bands,c.band_hidden),
            z(1,b*c.bands,c.band_hidden),z(b,c.bands,c.band_hidden),z(b,k,c.bands,7),
            z(b,k,c.bands,6),z(b,k,c.bands,6))

    def _state_weight_sha(self):
        digest=hashlib.sha256(json.dumps(asdict(self.config),sort_keys=True).encode())
        for name,value in self.state_dict().items():
            digest.update(name.encode()+b'\0');digest.update(value.detach().cpu().contiguous().numpy().tobytes())
        return digest.hexdigest()

    def state_to_payload(self,state):
        """Detached CPU tensors, explicitly bound to current exact weights/config.

        This is a model state, NOT optimizer/RNG/dataloader resume. The training
        harness must bind those separately. No Python owner object is serialized.
        """
        if type(state) is not Q2State: raise ValueError('Q2 owned state required')
        batch=state.previous_log_power.shape[0]
        self._validate(self.mask_head.new_zeros(batch,1,2,513,2),state,state.mode)
        cpu=lambda value:value.detach().cpu().clone()
        fields={name:cpu(getattr(state,name)) for name in
                ('previous_log_power','previous_power_valid','block_sum','band_hidden',
                 'held_context','held_raw_guidance','oracle_sum','held_oracle')}
        fields['encoder_histories']=tuple(cpu(v) for v in state.encoder_histories)
        return dict(schema='q2-detached-owned-state-v1',config=asdict(self.config),
                    model_sha256=self._state_weight_sha(),frames=state.frames,mode=state.mode,tensors=fields)

    def state_from_payload(self,payload):
        """Rebind a serialized state ONLY to identical loaded weights/config."""
        expected={'previous_log_power','previous_power_valid','block_sum','band_hidden',
                  'held_context','held_raw_guidance','oracle_sum','held_oracle','encoder_histories'}
        if (not isinstance(payload,dict) or set(payload)!={'schema','config','model_sha256','frames','mode','tensors'}
            or payload['schema']!='q2-detached-owned-state-v1' or payload['config']!=asdict(self.config)
            or payload['model_sha256']!=self._state_weight_sha() or not isinstance(payload['tensors'],dict)
            or set(payload['tensors'])!=expected): raise ValueError('Q2 serialized state binding mismatch')
        tensors=payload['tensors']
        if (type(tensors['encoder_histories']) is not tuple
            or any(not isinstance(v,torch.Tensor) for name,v in tensors.items() if name!='encoder_histories')
            or any(not isinstance(v,torch.Tensor) for v in tensors['encoder_histories'])):
            raise ValueError('Q2 serialized state tensors required')
        copied={name:value.detach().to(self.mask_head.device).clone() for name,value in tensors.items()
                if name!='encoder_histories'}
        copied['encoder_histories']=tuple(value.detach().to(self.mask_head.device).clone()
                                          for value in tensors['encoder_histories'])
        state=Q2State(owner=self._state_owner,frames=payload['frames'],mode=payload['mode'],**copied)
        if state.previous_log_power.ndim!=2: raise ValueError('Q2 serialized state batch malformed')
        self._validate(self.mask_head.new_zeros(state.previous_log_power.shape[0],1,2,513,2),state,state.mode)
        return state

    def _validate(self,x,state,mode):
        self._finite_parameters(); _quadratures(x)
        if (x.ndim != 5 or not x.shape[0] or not x.shape[1] or x.device != self.mask_head.device
            or mode not in ('predicted','disabled','oracle_train')): raise ValueError('Q2 input/mode/device mismatch')
        if state is None: return
        c=self.config;b=x.shape[0];k=len(c.classes)
        if (type(state) is not Q2State or state.owner is not self._state_owner
            or type(state.frames) is not int or state.frames<0 or state.mode != mode
            or type(state.encoder_histories) is not tuple or len(state.encoder_histories)!=4):
            raise ValueError('Q2 state owner, mode or phase mismatch; reset after reload')
        expected=[(state.previous_log_power,(b,c.bands),torch.float32),
                  (state.previous_power_valid,(b,c.bands),torch.bool),
                  (state.block_sum,(b,c.bands,c.band_hidden),torch.float32),
                  (state.band_hidden,(1,b*c.bands,c.band_hidden),torch.float32),
                  (state.held_context,(b,c.bands,c.band_hidden),torch.float32),
                  (state.held_raw_guidance,(b,k,c.bands,7),torch.float32),
                  (state.oracle_sum,(b,k,c.bands,6),torch.float32),
                  (state.held_oracle,(b,k,c.bands,6),torch.float32)]
        expected += [(v,(b,2*d,c.width,c.bins),torch.float32) for v,d in zip(state.encoder_histories,c.dilations)]
        for value,shape,dtype in expected:
            if (not isinstance(value,torch.Tensor) or value.shape != shape or value.dtype != dtype
                or value.device != x.device or not torch.isfinite(value).all()): raise ValueError('Invalid Q2 owned state')

    def forward(self, original_ri, state=None, guidance_mode='predicted'):
        if guidance_mode not in ('predicted','disabled'): raise ValueError('No oracle/targets in inference forward')
        return self._forward(original_ri,state,guidance_mode,None)

    def forward_oracle_train(self, original_ri, raw_guidance, state=None, *, partition, diagnostic_token):
        """Explicit nondeployable decoder witness. Caller must bind TRAIN provenance."""
        if not self.training or partition != 'train' or diagnostic_token != ORACLE_TOKEN:
            raise ValueError('Oracle requires training mode, TRAIN partition and explicit diagnostic token')
        self._validate(original_ri,state,'oracle_train')
        shape=(*original_ri.shape[:2],len(self.config.classes),self.config.bands,6)
        if (not isinstance(raw_guidance,torch.Tensor) or raw_guidance.dtype != torch.float32
            or raw_guidance.device != original_ri.device or raw_guidance.shape != shape
            or not torch.isfinite(raw_guidance).all() or (raw_guidance[...,:2]<0).any()
            or (raw_guidance[...,:2]>1).any() or (raw_guidance[...,2:].abs()>1.0001).any()):
            raise ValueError('Oracle needs bounded reference activity/acoustics, not source PCM')
        return self._forward(original_ri,state,'oracle_train',raw_guidance.detach())

    def _forward(self,x,state,mode,oracle):
        self._validate(x,state,mode); c=self.config;b,t=x.shape[:2];k=len(c.classes);w=c.width;h=c.band_hidden
        state=self.initial_state(b,mode=mode) if state is None else state
        p,lp,pv,balance,cr,ci=_powers(x,self.band_edges)
        flux=_flux(lp,pv,state.previous_log_power,state.previous_power_valid)
        shape=torch.log1p(c.bands*_safe_ratio(p,p.sum(-1,keepdim=True)))
        features=torch.stack((shape,flux,balance,cr,ci),-1)
        q=x.permute(0,1,2,4,3).reshape(b*t,4,c.bins)/math.sqrt(c.fft_size)
        local=F.leaky_relu(self.frequency_encoder(q),.125).reshape(b,t,w,c.bins)
        histories=[]
        for dilation,kernel,history in zip(c.dilations,self.temporal_kernels,state.encoder_histories):
            joined=torch.cat((history,local),1)
            filtered=sum(joined[:, j*dilation:j*dilation+t]*kernel[:,j][None,None,:,None] for j in range(3))
            histories.append(joined[:,-2*dilation:].clone())
            local=F.leaky_relu(local+filtered,.125)
        audio=F.leaky_relu(self.audio_mix(local.reshape(b*t,w,c.bins)),.125).reshape(b,t,w,c.bins)
        maximum=audio.abs().amax(dim=(-2,-1),keepdim=True)
        normalized=audio/torch.where(maximum>0,maximum,torch.ones_like(maximum))
        counts=audio.new_tensor([hi-lo for lo,hi in zip(self.band_edges[:-1],self.band_edges[1:])])
        pooled=(_band_sum(normalized,self.band_edges)/counts).permute(0,1,3,2)
        pos=audio.new_tensor([(lo+hi-1)/(2*(c.bins-1)) for lo,hi in zip(self.band_edges[:-1],self.band_edges[1:])])
        position=torch.stack((pos,pos.square()),-1)[None,None].expand(b,t,-1,-1)
        projected=F.leaky_relu(self.band_projection(torch.cat((pooled,features,position),-1)),.125)
        blocks,pending,ends=_blocks(projected,state.block_sum,state.frames%4)
        hidden=state.band_hidden; contexts=blocks; raw=state.held_raw_guidance[:,None][:,:0]
        if ends:
            n=blocks.shape[1]
            cross=self.cross_band_out(F.leaky_relu(self.cross_band_in(blocks.flatten(-2)),.125)).reshape(b,n,c.bands,h)
            recurrent_input=F.leaky_relu(blocks+cross,.125).permute(0,2,1,3).reshape(b*c.bands,n,h)
            recurrent,hidden=self.band_gru(recurrent_input,hidden)
            contexts=recurrent.reshape(b,c.bands,n,h).permute(0,2,1,3)
            raw=self.descriptor_head(contexts).reshape(b,n,c.bands,k,7).permute(0,1,3,2,4)
        held_context=_held(contexts,state.held_context,ends,t)
        held_raw=_held(raw,state.held_raw_guidance,ends,t)
        logits=held_raw[...,0];activity=torch.sigmoid(logits)
        desc=torch.cat((torch.sigmoid(held_raw[...,1:2]),torch.tanh(held_raw[...,2:6])), -1)
        log_error=held_raw[...,6].clamp(-8,8)
        hints=torch.cat((2*activity[...,None]-1,2*desc[...,:1]-1,desc[...,1:],torch.tanh(log_error[...,None]/4)), -1)
        oracle_sum=state.oracle_sum; held_oracle=state.held_oracle
        if oracle is not None:
            ob,oracle_sum,oe=_blocks(oracle,state.oracle_sum,state.frames%4)
            if oe != ends: raise RuntimeError('Oracle/encoder block phase differs')
            om=_held(ob,state.held_oracle,ends,t); held_oracle=om[:,-1].clone()
            hints=torch.cat((2*om[...,:2]-1,om[...,2:],torch.zeros_like(om[...,:1])), -1)
            available=(torch.arange(t,device=x.device)+state.frames)>=3
            hints=torch.where(available[None,:,None,None,None],hints,torch.zeros_like(hints))
        context_scale=self.context_film(held_context).reshape(b,t,c.bands,k,w).permute(0,1,3,2,4)
        hint_scale=self.hint_film(hints)
        if mode=='disabled': hint_scale=torch.zeros_like(hint_scale)
        scale=1+.5*torch.tanh(context_scale+hint_scale+self.query_scale[None,None,:,None])
        # Piecewise band guidance is conditioning, not the output resolution.
        index=torch.repeat_interleave(torch.arange(c.bands,device=x.device),counts.long())
        full_scale=scale.index_select(3,index).permute(0,1,2,4,3)
        guided=audio[:,:,None]*full_scale
        mask_features=normalized[:,:,None]*full_scale
        mask_raw=torch.einsum('btkwf,kwd->btkfd',mask_features,self.mask_head)
        masks=2*torch.tanh(mask_raw*.5)+self.mask_bias[None,None,:,None]
        residual=torch.einsum('btkwf,kwd->btkfd',guided,self.refinement_head)*math.sqrt(c.fft_size)
        masks=masks.reshape(b,t,k,c.bins,2,2).permute(0,1,2,4,3,5)
        residual=residual.reshape(b,t,k,c.bins,2,2).permute(0,1,2,4,3,5)
        xr,xi=x[:,:,None,...,0],x[:,:,None,...,1];mr,mi=masks[...,0],masks[...,1]
        extracted=torch.stack((mr*xr-mi*xi,mr*xi+mi*xr),-1)
        endpoint=x.new_ones(c.bins);endpoint[[0,-1]]=0
        endpoint=torch.stack((torch.ones_like(endpoint),endpoint),-1)
        extracted=extracted*endpoint;residual=residual*endpoint
        sources=extracted+residual
        updated=torch.zeros(b,t,dtype=torch.bool,device=x.device)
        if ends: updated[:,ends]=True
        output=dict(sources_ri=sources,extraction_ri=extracted,refinement_ri=residual,
                    activity_logits=logits,activity=activity,descriptors=desc,
                    log_error_scale=log_error,guidance_scale=scale,band_features=features,updated=updated)
        carried=Q2State(self._state_owner,state.frames+t,mode,lp[:,-1].clone(),pv[:,-1].clone(),
            tuple(histories),pending,hidden.clone(),held_context[:,-1].clone(),held_raw[:,-1].clone(),
            oracle_sum.clone(),held_oracle.clone())
        if any(not torch.isfinite(value).all() for value in output.values()):
            raise ValueError('Q2 nonfinite output; no owned state committed')
        return output,carried

    def specification(self):
        c=self.config;f,w,h,k,n=c.bins,c.width,c.band_hidden,len(c.classes),c.bands
        fast=dict(frequency_encoder=f*4*w*5,temporal_depthwise=f*w*3*len(c.dilations),
                  audio_pointwise=f*w*w,band_projection=n*(w+7)*h,
                  source_mask_and_residual=f*k*w*8)
        slow=dict(cross_band=2*n*h*(h//2),per_band_gru=n*6*h*h,
                  descriptor_heads=n*h*k*7,context_film=n*h*k*w)
        # Hint projection/FiLM are currently evaluated every frame, including
        # held controls. A future native optimization is NOT counted as done.
        fast['hint_projection']=n*k*7*w
        fast['context_film_current_implementation']=slow.pop('context_film')
        perframe=sum(fast.values())+sum(slow.values())/4
        state=self.initial_state()
        tensors=(state.previous_log_power,state.previous_power_valid,*state.encoder_histories,
                 state.block_sum,state.band_hidden,state.held_context,state.held_raw_guidance,
                 state.oracle_sum,state.held_oracle)
        return dict(architecture=ARCHITECTURE,contract=dict(CONTRACT),classes=list(c.classes),
            profile='quality_reference' if w==24 else 'unqualified_student',
            parameters=sum(p.numel() for p in self.parameters()),
            parameter_bytes_float32=4*sum(p.numel() for p in self.parameters()),
            state_bytes_per_batch=sum(v.numel()*v.element_size() for v in tensors),
            band_edges_bins=list(self.band_edges),band_edges_hz=[min(v*c.sample_rate/c.fft_size,c.sample_rate/2) for v in self.band_edges],
            direct_audio_history_frames=30,direct_audio_receptive_frames=31,
            direct_audio_receptive_seconds=31*c.hop/c.sample_rate,
            past_frame_span_seconds=30*c.hop/c.sample_rate,
            full_stft_support_seconds=(c.fft_size+30*c.hop)/c.sample_rate,
            recurrent_history='causal unbounded compressed state per band; global cross-band context',
            neural_future_frames=0,guidance_update_hops=4,guidance_first_available_frame=3,
            source_local_guidance_shape=['B','T',k,n,7],descriptor_names=list(DESCRIPTORS),
            dynamic_mask_real_and_imaginary_limit=2.,static_mask_coefficient_bias_bound=None,
            static_mask_bias_parameters=4*k,static_mask_bias_adds_per_frame=4*k*f,
            total_mask_amplitude_bound=None,residual_amplitude_bound=None,
            dense_mac_per_frame=perframe,dense_mac_per_second=perframe*c.sample_rate/c.hop,
            dense_mac_fast=fast,dense_mac_per_control=slow,
            excludes=['FFT and K inverse FFT/WOLA','band sums/cumulative sums/features/log/sqrt',
                      'FiLM/complex mask products/nonlinearities','state copies/validation/memory traffic',
                      'cabin renderer/normalization/scheduler'],
            cost_is_not_arm_time=True,mps_real_float32_graph=True,
            oracle_available_only_in_separate_train_diagnostic=True,
            car_approved=False,role_status='UNKNOWN')
