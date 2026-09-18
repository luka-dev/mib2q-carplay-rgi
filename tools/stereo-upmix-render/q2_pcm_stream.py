"""Original-only full-song CPU reference: existing Q2 PCM DSP + aligned dry.

Only exact base/power Q2 classes are admitted. No target/oracle input, hidden
device transfer, role permission, renderer, per-source gain or native claim.
The existing one-frame FFT/WOLA implementation is reused unchanged.
"""
import re

import torch

from q2_local_guidance_model import Q2LocalGuidanceModel
from q2_power_guidance_model import Q2PowerGuidanceModel
from q2_local_guidance_audio import (Q2PCMStream, DELAY, META_SHAPES, TIMELINE_KEYS,
                                     _cpu_wave, _model_geometry, _stamp)


SCHEMA='q2-original-only-full-song-cpu-pcm-v1'
CONTRACT=dict(schema=SCHEMA, supported_models=['Q2LocalGuidanceModel','Q2PowerGuidanceModel'],
    input='original CPU float32[N,2] only; no target or oracle argument',
    process='N input -> N delayed source samples and N identically delayed original samples',
    sources='CPU float32[N,K,2]; full stereo for each declared identity aggregate',
    original='delayed_original CPU float32[N,2]',
    residual='delayed_original - sources.sum(dim1), arithmetic closure not semantic truth',
    latency_samples=1024, rates=[44100,48000], max_chunk_samples=65536,
    full_song_length_bound=None, history='bounded existing Q2 state +1024 stereo dry samples',
    frontend='unchanged q2_local_guidance_audio.Q2PCMStream: original FFT1024/hop256, one-frame cadence',
    finish='flush once:1024output samples if any input, else0; no infinite neural-tail extension',
    scoring='use timeline.valid_output_slice; negative original positions are startup, never targets',
    fixed_device='CPU model/PCM required; caller moves/clones a checkpoint explicitly',
    checkpoint_hash='optional caller-declared file SHA; not authenticated by this PCM adapter',
    reset='drop prior tail/history; checkpoint binding must be explicitly re-supplied after reset',
    requires_grad_flags_preserved=True, allocation_free=False,
    source_role_status='UNKNOWN', native_inference=False, target_runtime_verified=False,
    full_callback_verified=False, car_approved=False)


def _require(condition,message):
    if not condition:raise ValueError(message)


def _checkpoint(value):
    _require(value is None or (type(value)is str and re.fullmatch('[0-9a-f]{64}',value)),
             'Checkpoint SHA must be explicitly lowercase SHA256 or unbound None')
    return value


def _admit(model):
    _require(type(model) in (Q2LocalGuidanceModel,Q2PowerGuidanceModel),
             'Only exact base/power Q2 CPU reference models are admitted')
    _model_geometry(model)
    _require(all(not value.is_floating_point() or value.dtype==torch.float32
                 for value in list(model.parameters())+list(model.buffers())),
             'Float32 Q2 model required; no hidden precision/device conversion')
    if type(model)is Q2PowerGuidanceModel:
        model._check_carrier_binding()
        return dict(model_type=type(model).__name__,carrier_mode=model.carrier_mode,
                    prior_sha256=model.prior_sha256)
    return dict(model_type=type(model).__name__,carrier_mode=None,prior_sha256=None)


class Q2OriginalPCMStream:
    """Bounded-call, continuous-song CPU inference with a separate dry FIFO.

    Both stream states commit only after all returned PCM is finite. Calls must
    be serialized and the functional model must not be altered via unsafe .data
    writes. Parameter/buffer versions, ownership, generation and mode are checked.
    The public process signature contains no source truth/target channels.
    """
    def __init__(self,model,guidance_mode='predicted',*,sample_rate=None,
                 max_chunk_samples=65536,checkpoint_sha256=None):
        _require(type(max_chunk_samples)is int and 1<=max_chunk_samples<=65536,
                 'Explicit bounded process capacity1..65536 required')
        self._model=model;self._model_metadata=_admit(model)
        self._max_chunk_samples=max_chunk_samples
        self._stream=Q2PCMStream(model,guidance_mode,sample_rate)
        self._dry=torch.zeros(DELAY,2,dtype=torch.float32)
        self._checkpoint_sha256=_checkpoint(checkpoint_sha256)
        self._binding=_stamp(model)

    @property
    def model(self):return self._model

    @property
    def guidance_mode(self):return self._stream.guidance_mode

    @property
    def sample_rate(self):return self._stream.sample_rate

    @property
    def classes(self):return self._stream.classes

    @property
    def received(self):return self._stream.received

    @property
    def emitted(self):return self._stream.emitted

    @property
    def ended(self):return self._stream.ended

    @property
    def delay_samples(self):return DELAY

    @property
    def max_chunk_samples(self):return self._max_chunk_samples

    def _live(self):
        _require(self._stream.model is self._model and _admit(self._model)==self._model_metadata
                 and _stamp(self._model)==self._binding,
                 'Model/checkpoint/carrier binding changed; reset required')
        self._stream._live()

    @torch.no_grad()
    def reset(self,*,guidance_mode=None,checkpoint_sha256=None):
        metadata=_admit(self.model)
        mode=self.guidance_mode if guidance_mode is None else guidance_mode
        _require(mode in ('predicted','disabled'),'No oracle/target mode is available')
        binding=_checkpoint(checkpoint_sha256)
        # Construct before replacing either existing stream: bad reset arguments
        # do not discard a usable old timeline or its pending dry samples.
        stream=Q2PCMStream(self.model,mode,self.sample_rate)
        _require(stream.classes==self.classes,'Source vocabulary changed; construct a new stream')
        self._stream=stream;self._dry=torch.zeros(DELAY,2,dtype=torch.float32)
        self._checkpoint_sha256=binding;self._model_metadata=metadata;self._binding=_stamp(self.model)

    def _snapshot(self):
        s=self._stream
        return s._work,s.received,s.emitted,s.ended

    def _rollback(self,old):
        s=self._stream;s._work,s.received,s.emitted,s.ended=old

    def _result(self,block,original):
        residual=original-block['sources'].sum(1)
        _require(bool(torch.isfinite(original).all()) and bool(torch.isfinite(residual).all()),
                 'Dry/residual PCM arithmetic overflow; no timeline consumed')
        start,stop=block['sample_start']-DELAY,block['sample_stop']-DELAY
        n=len(original);total=block['original_input_samples']
        lo=max(0,min(n,-start));hi=max(lo,min(n,total-start))
        timeline=dict(output_start=block['sample_start'],output_stop=block['sample_stop'],
            original_start=start,original_stop=stop,valid_output_slice=[lo,hi],
            valid_original_start=max(0,start+lo),valid_original_stop=max(0,start+hi),
            received_original_samples=total,delay_samples=DELAY,sample_rate=self.sample_rate,
            flush=block['flush'],zero_padding_samples=block['zero_padding_samples'],
            frame_indices=block['frame_indices'].tolist(),frame_starts=block['frame_starts'].tolist(),
            frame_ends=block['frame_ends'].tolist(),frame_full_support=block['frame_full_support'].tolist(),
            frame_uses_left_padding=block['frame_uses_left_padding'].tolist(),
            frame_uses_right_padding=block['frame_uses_right_padding'].tolist())
        return dict(block,schema=SCHEMA,delayed_original=original,residual=residual,timeline=timeline,
            model_metadata=dict(self._model_metadata),checkpoint_sha256=self._checkpoint_sha256,
            checkpoint_hash_is_caller_declared=True,contract=dict(CONTRACT),car_approved=False)

    @torch.no_grad()
    def process(self,original):
        self._live();_cpu_wave(original)
        _require(len(original)<=self.max_chunk_samples,'Split input into admitted bounded PCM chunks')
        joined=torch.cat((self._dry,original),0)
        dry=joined[:len(original)].clone();next_dry=joined[len(original):].clone()
        old=self._snapshot()
        try:
            block=self._stream.push(original)
            result=self._result(block,dry);self._live()
        except BaseException:
            self._rollback(old);raise
        self._dry=next_dry
        return result

    @torch.no_grad()
    def flush(self):
        self._live();old=self._snapshot()
        try:
            block=self._stream.finish()
            dry=self._dry.clone() if self.received else self._dry[:0].clone()
            result=self._result(block,dry)
            # finish intentionally ended the underlying stream; check binding,
            # not _live() which properly refuses an already ended stream.
            _require(_admit(self.model)==self._model_metadata and _stamp(self.model)==self._binding,
                     'Model changed during flush; no timeline consumed')
        except BaseException:
            self._rollback(old);raise
        self._dry.zero_()
        return result


@torch.no_grad()
def render_original_audio(model,original,guidance_mode='predicted',*,chunk_samples=4093,
                          checkpoint_sha256=None):
    """Allocating finite-song convenience through the real streaming reference."""
    _cpu_wave(original)
    _require(type(chunk_samples)is int and 1<=chunk_samples<=65536,'Bounded integer PCM chunk required')
    stream=Q2OriginalPCMStream(model,guidance_mode,checkpoint_sha256=checkpoint_sha256)
    blocks=[stream.process(original[start:start+chunk_samples]) for start in range(0,len(original),chunk_samples)]
    blocks.append(stream.flush())
    names=('sources','delayed_original','residual')
    result={name:torch.cat([b[name][slice(*b['timeline']['valid_output_slice'])] for b in blocks],0)
            for name in names}
    _require(all(len(result[name])==len(original) for name in names),'Aligned song sample count mismatch')
    for name in (*META_SHAPES,'updated',*TIMELINE_KEYS):
        result[name]=torch.cat([b[name] for b in blocks],0)
    result.update(schema=SCHEMA,sample_rate=stream.sample_rate,classes=stream.classes,
        delay_samples=DELAY,model_metadata=blocks[-1]['model_metadata'],checkpoint_sha256=checkpoint_sha256,
        block_timelines=[b['timeline'] for b in blocks],input_samples=len(original),
        source_role_status='UNKNOWN',car_approved=False,contract=dict(CONTRACT))
    return result
