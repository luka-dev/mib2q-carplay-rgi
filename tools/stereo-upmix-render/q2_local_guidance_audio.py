"""CPU PCM reference for Q2; one shared original-stereo FFT, K source WOLAs.

Streaming accepts/emits exactly N samples with a fixed1024-sample delay. Hann
FFT1024/hop256 uses received samples only; finish explicitly pads zeros to
reconstruct the original finite input duration, then drains the remaining FIFO.
It does not promise to preserve an arbitrarily long neural tail beyond that
duration. Continuous zero input after music can still contain causal model tails.

The model consumes real quadratures; complex FFT tensors exist ONLY in this CPU
frontend/synthesis. No implicit device transfer/fallback, gain, clipping, source
permission or cabin routing. This allocating Torch adapter is NOT a native or
allocation-free callback. Model weights retain their requires_grad flags. Caller
must select eval(), serialize calls, and reset after reload/seek/weight mutation.
"""
from dataclasses import dataclass

import torch
from torch.nn import functional as F


FFT, HOP, DELAY, BANDS = 1024, 256, 1024, 32
SCHEMA = 'q2-local-guidance-cpu-pcm-reference-v1'
DESCRIPTORS = ('source_power_fraction', 'source_flux', 'stereo_balance',
               'coherence_real', 'coherence_imag')
META_SHAPES = dict(activity_logits=('K', BANDS), activity=('K', BANDS),
                   descriptors=('K', BANDS, 5), log_error_scale=('K', BANDS))
TIMELINE_KEYS = ('frame_indices', 'frame_starts', 'frame_ends', 'frame_full_support',
                 'frame_uses_left_padding', 'frame_uses_right_padding')
CONTRACT = dict(schema=SCHEMA, sample_rates=[44100, 48000], fft_size=FFT, hop=HOP,
    window='periodic Hann analysis and synthesis; torch default FFT normalization',
    model_input='full original stereo float32 real quadratures 1,T,2,513,2',
    sources='K config identity aggregates, full stereo; not rear roles',
    delay_samples=DELAY, delay_breakdown=dict(wola=768, fifo=256),
    frame_support='frame k covers [k*256-768,(k+1)*256) on original timeline',
    analysis='one original-stereo rFFT per hop, then real view; no band reduction of audio',
    synthesis='one batched K-source stereo irFFT and fixed window-square WOLA',
    stream_cadence='one model frame per call, independent of external PCM chunk sizes',
    batch_cadence='vectorized reference; numerical tolerance versus one-frame stream, not byte identity',
    finish='zero pad required windows then drain FIFO; returns1024 samples if input nonempty',
    cpu_reference_only=True, no_implicit_device_transfer=True, no_resampling=True,
    no_signal_dependent_gain=True, no_clipping=True, no_cabin_routing=True,
    allocation_free=False, native_export_supported=False,
    realtime_budget_verified=False, role_status='UNKNOWN', car_approved=False)


def _require(condition, message):
    if not condition:
        raise ValueError(message)


def _geometry(config):
    rate, classes = getattr(config, 'sample_rate', None), getattr(config, 'classes', None)
    _require(type(rate) is int and rate in (44100, 48000)
        and getattr(config, 'fft_size', None) == FFT and getattr(config, 'hop', None) == HOP
        and getattr(config, 'bins', None) == 513 and getattr(config, 'bands', None) == BANDS
        and type(classes) is tuple and 1 <= len(classes) <= 32
        and all(type(name) is str and name.isidentifier() for name in classes)
        and len(set(classes)) == len(classes), 'Q2 FFT1024/hop256/32bands and explicit classes/rate required')
    return rate, classes


def _cpu_wave(wave):
    _require(isinstance(wave, torch.Tensor) and wave.dtype == torch.float32
        and wave.ndim == 2 and wave.shape[1] == 2 and wave.device.type == 'cpu'
        and bool(torch.isfinite(wave).all()), 'Finite CPU float32[N,2] PCM required; no device fallback')


def _model_geometry(model, sample_rate=None):
    rate, classes = _geometry(getattr(model, 'config', None))
    if sample_rate is not None:
        _require(type(sample_rate) is int and sample_rate == rate, 'PCM/model sample rate mismatch; no resampling')
    _require(not model.training and hasattr(model, '_state_owner')
        and type(getattr(model, 'generation', None)) is int, 'Eval Q2 model with owned generation required')
    _require(all(v.device.type == 'cpu' for v in list(model.parameters())+list(model.buffers())),
             'CPU model required; no implicit MPS/CUDA transfer or fallback')
    return rate, classes


def _stamp(model):
    return (model._state_owner, model.generation, id(model.config), repr(model.config),
        tuple((name, id(value), value._version, value.dtype, value.device,
               tuple(value.shape), value.requires_grad)
              for name, value in list(model.named_parameters())+list(model.named_buffers())))


def _length(length):
    _require(type(length) is int and length >= 0, 'Nonnegative integer original length required')
    if not length:
        return 0, 0, 0
    right = ((length+HOP-1)//HOP)*HOP-length+FFT-HOP
    return FFT-HOP, right, (length+HOP-1)//HOP+3


@dataclass(frozen=True)
class AudioAnalysis:
    frames_ri: torch.Tensor
    length: int
    config: object
    left_padding: int
    right_padding: int

    @property
    def frame_starts(self):
        return torch.arange(self.frames_ri.shape[1], dtype=torch.int64)*HOP-(FFT-HOP)

    @property
    def frame_ends(self):
        return self.frame_starts+FFT

    @property
    def full_support_mask(self):
        return (self.frame_starts >= 0)&(self.frame_ends <= self.length)


def analyze(wave, config):
    """Differentiable CPU batch frontend, without model inference or transfers.

    All513 original stereo bins are retained. The caller explicitly transfers
    these REAL tensors if training a model on another device. No target enters.
    """
    _geometry(config); _cpu_wave(wave)
    left, right, count = _length(len(wave))
    if not count:
        return AudioAnalysis(wave.new_empty((1,0,2,513,2)), 0, config, 0, 0)
    padded = F.pad(wave.transpose(0,1), (left, right))
    framed = padded.unfold(-1, FFT, HOP).permute(1,0,2)[None]
    window = torch.hann_window(FFT, periodic=True, dtype=torch.float32)
    frames = torch.view_as_real(torch.fft.rfft(framed*window, n=FFT, dim=-1)).contiguous()
    _require(frames.shape == (1,count,2,513,2) and bool(torch.isfinite(frames).all()),
             'Analysis arithmetic overflow or geometry mismatch')
    return AudioAnalysis(frames, len(wave), config, left, right)


def synthesize(sources_ri, length, config):
    """Differentiable CPU K-source WOLA, aligned to original samples[0,length).

    Input shape is1,T,K,2,513,2. An explicit `.cpu()` by a training caller is
    distinct from this helper silently falling back; this helper rejects other
    devices. Autograd through an explicitly transferred tensor is retained.
    """
    _, classes = _geometry(config); k = len(classes)
    left, right, count = _length(length)
    _require(isinstance(sources_ri, torch.Tensor) and sources_ri.dtype == torch.float32
        and sources_ri.device.type == 'cpu' and sources_ri.shape == (1,count,k,2,513,2)
        and bool(torch.isfinite(sources_ri).all()), 'Finite aligned CPU source quadratures required')
    _require(not bool(sources_ri[...,0,1].any()) and not bool(sources_ri[...,-1,1].any()),
             'Real source DC/Nyquist endpoints required')
    if not count:
        return sources_ri.new_empty((0,k,2))
    window = torch.hann_window(FFT, periodic=True, dtype=torch.float32)
    spectrum = torch.view_as_complex(sources_ri.contiguous())
    decoded = torch.fft.irfft(spectrum, n=FFT, dim=-1)*window
    total = left+length+right
    columns = decoded.permute(0,2,3,4,1).reshape(k*2,FFT,count)
    joined = F.fold(columns, (1,total), (1,FFT), stride=(1,HOP)).reshape(k,2,total).permute(2,0,1)
    weights = F.fold(window.square()[None,:,None].expand(1,FFT,count),
                     (1,total), (1,FFT), stride=(1,HOP)).reshape(total)
    real_weights = weights[left:left+length]
    _require(bool(torch.isfinite(real_weights).all()) and bool((real_weights > 0).all()),
             'Invalid fixed WOLA normalization')
    result = joined[left:left+length]/real_weights[:,None,None]
    _require(bool(torch.isfinite(result).all()), 'Synthesis arithmetic overflow')
    return result


@dataclass
class _Work:
    history: torch.Tensor
    pending: torch.Tensor
    signal: torch.Tensor
    weight: torch.Tensor
    queue: torch.Tensor
    fill: int = 0
    frames: int = 0
    model_state: object = None

    def clone(self):
        return _Work(*(v.clone() for v in (self.history,self.pending,self.signal,self.weight,self.queue)),
                     self.fill,self.frames,None if self.model_state is None else self.model_state.detach())


class Q2PCMStream:
    """CPU one-in/one-out inference; bounded history, output scales with call N.

    Metadata is source-local, in original input coordinates. Padded/held frames
    are explicit; `updated` reports the model's four-hop update phase. Failed
    input or numeric checks commit neither PCM nor stream state. The model must
    obey the functional explicit-state API; unsafe `.data` mutation is forbidden.
    """
    def __init__(self, model, guidance_mode='predicted', sample_rate=None):
        _require(guidance_mode in ('predicted','disabled'), 'Unknown guidance mode')
        self.model, self.guidance_mode = model, guidance_mode
        self.sample_rate, self.classes = _model_geometry(model, sample_rate)
        self.k = len(self.classes)
        self.delay_samples, self.delay_seconds = DELAY, DELAY/self.sample_rate
        self.reset()

    @torch.no_grad()
    def reset(self):
        _require(_model_geometry(self.model, self.sample_rate) == (self.sample_rate,self.classes),
                 'Rate/classes changed; create a new stream')
        z = lambda *shape: torch.zeros(*shape, dtype=torch.float32)
        self._work = _Work(z(FFT,2),z(HOP,2),z(FFT,self.k,2),z(FFT),z(HOP,self.k,2))
        self._window = torch.hann_window(FFT, periodic=True, dtype=torch.float32)
        self._window2 = self._window.square()
        self._stamp = _stamp(self.model)
        self._reset_mode = self.guidance_mode
        self.received = self.emitted = 0
        self.ended = False

    def _live(self):
        _require(_model_geometry(self.model, self.sample_rate) == (self.sample_rate,self.classes)
            and self.guidance_mode in ('predicted','disabled')
            and self.guidance_mode == self._reset_mode
            and not self.ended and self._stamp == _stamp(self.model),
            'Stream ended or model/config/requires_grad changed; reset required')

    def _frame(self, work):
        history = torch.cat((work.history[HOP:],work.pending),0)
        spectrum = torch.fft.rfft(history.transpose(0,1)*self._window, n=FFT, dim=-1)
        quadratures = torch.view_as_real(spectrum)[None,None].contiguous()
        _require(bool(torch.isfinite(quadratures).all()), 'Analysis arithmetic overflow')
        result, carried = self.model(quadratures,work.model_state,guidance_mode=self.guidance_mode)
        _require(isinstance(result,dict) and all(name in result for name in
            ('sources_ri','updated',*META_SHAPES)), 'Q2 source-local frame result required')
        sources = result['sources_ri']
        expected = [('sources_ri',(1,1,self.k,2,513,2),torch.float32),('updated',(1,1),torch.bool)]
        expected += [(name,(1,1,*[self.k if v=='K' else v for v in shape]),torch.float32)
                     for name,shape in META_SHAPES.items()]
        for name,shape,dtype in expected:
            value=result[name]
            _require(isinstance(value,torch.Tensor) and value.shape==shape and value.dtype==dtype
                and value.device.type=='cpu' and bool(torch.isfinite(value).all()),
                'Invalid/nonfinite Q2 frame output: '+name)
        _require(not bool(sources[...,0,1].any()) and not bool(sources[...,-1,1].any()),
                 'Real source endpoints required')
        _require(bool(((result['activity']>=0)&(result['activity']<=1)).all())
            and bool(result['updated'][0,0]) == ((work.frames+1)%4==0)
            and hasattr(carried,'detach'), 'Probability, four-hop update and detachable state required')
        decoded = (torch.fft.irfft(torch.view_as_complex(sources[0,0].contiguous()),
                   n=FFT,dim=-1)*self._window).permute(2,0,1)
        signal, weight = work.signal+decoded, work.weight+self._window2
        if work.frames < 3:
            emitted = torch.zeros_like(work.queue)
        else:
            _require(bool((weight[:HOP]>0).all()), 'Invalid WOLA normalization')
            emitted = signal[:HOP]/weight[:HOP,None,None]
        _require(bool(torch.isfinite(signal).all()) and bool(torch.isfinite(weight).all())
            and bool(torch.isfinite(emitted).all()), 'Synthesis arithmetic overflow')
        work.history = history
        work.signal = torch.cat((signal[HOP:],torch.zeros_like(signal[:HOP])),0)
        work.weight = torch.cat((weight[HOP:],torch.zeros_like(weight[:HOP])),0)
        work.queue = emitted.clone()
        work.model_state = carried.detach()
        index = work.frames
        work.frames += 1; work.fill = 0
        return index,{name:result[name][0,0].clone() for name in (*META_SHAPES,'updated')}

    def _consume(self, work, wave):
        out = wave.new_empty((len(wave),self.k,2)); frames=[]; pos=0
        while pos < len(wave):
            count = min(HOP-work.fill,len(wave)-pos)
            out[pos:pos+count] = work.queue[work.fill:work.fill+count]
            work.pending[work.fill:work.fill+count] = wave[pos:pos+count]
            work.fill += count; pos += count
            if work.fill == HOP:
                frames.append(self._frame(work))
        return out,frames

    def _block(self, sources, frames, original_samples, *, flush=False, padding=0):
        indices=torch.tensor([row[0] for row in frames],dtype=torch.int64)
        starts,ends=indices*HOP-(FFT-HOP),(indices+1)*HOP
        meta={name:(torch.stack([row[1][name] for row in frames]) if frames else
                   torch.empty((0,*[self.k if v=='K' else v for v in shape]),dtype=torch.float32))
              for name,shape in META_SHAPES.items()}
        meta['updated']=(torch.stack([row[1]['updated'] for row in frames]) if frames else
                         torch.empty(0,dtype=torch.bool))
        return dict(meta,sources=sources,sample_start=self.emitted,sample_stop=self.emitted+len(sources),
            frame_indices=indices,frame_starts=starts,frame_ends=ends,
            frame_full_support=(starts>=0)&(ends<=original_samples),
            frame_uses_left_padding=starts<0,frame_uses_right_padding=ends>original_samples,
            original_input_samples=original_samples,flush=flush,zero_padding_samples=padding,
            delay_samples=DELAY,sample_rate=self.sample_rate,classes=self.classes,
            descriptor_names=DESCRIPTORS,role_status='UNKNOWN')

    @torch.no_grad()
    def push(self, wave):
        self._live(); _cpu_wave(wave)
        if not len(wave):
            return self._block(wave.new_empty((0,self.k,2)),[],self.received)
        work=self._work.clone()
        out,frames=self._consume(work,wave)
        self._live()
        result=self._block(out,frames,self.received+len(wave))
        self._work=work;self.received+=len(wave);self.emitted+=len(out)
        return result

    @torch.no_grad()
    def finish(self):
        self._live()
        if not self.received:
            result=self._block(self._work.queue.new_empty((0,self.k,2)),[],0,flush=True)
            self.ended=True
            return result
        _,padding,_=_length(self.received)
        work=self._work.clone()
        prefix,frames=self._consume(work,torch.zeros(padding,2,dtype=torch.float32))
        _require(work.fill==0, 'Flush must end on a complete hop')
        count=DELAY-padding
        _require(0<count<=HOP, 'Invalid FIFO drain')
        out=torch.cat((prefix,work.queue[:count]),0)
        _require(len(out)==DELAY, 'Invalid flush length')
        self._live()
        result=self._block(out,frames,self.received,flush=True,padding=padding)
        self._work=work;self.emitted+=len(out);self.ended=True
        return result


@torch.no_grad()
def render_audio(model, wave, guidance_mode='predicted', *, chunk_samples=4093):
    """Aligned offline inference through the actual one-frame PCM stream.

    Residual X-sum(sources) is arithmetic closure, not source/role correctness.
    No independent source gain or clipping is applied, including to residual.
    """
    _require(type(chunk_samples) is int and chunk_samples>0, 'Positive integer chunk size required')
    stream=Q2PCMStream(model,guidance_mode);_cpu_wave(wave)
    blocks=[stream.push(wave[start:start+chunk_samples]) for start in range(0,len(wave),chunk_samples)]
    blocks.append(stream.finish())
    delayed=torch.cat([block['sources'] for block in blocks],0)
    sources=delayed[DELAY:DELAY+len(wave)] if len(wave) else delayed
    residual=wave-sources.sum(1)
    _require(bool(torch.isfinite(residual).all()), 'Residual arithmetic overflow')
    result={name:torch.cat([block[name] for block in blocks],0)
            for name in (*META_SHAPES,'updated',*TIMELINE_KEYS)}
    result.update(sources=sources,residual=residual,sample_rate=stream.sample_rate,
        delay_samples=DELAY,delay_seconds=stream.delay_seconds,input_samples=len(wave),
        flush_padding_samples=blocks[-1]['zero_padding_samples'],guidance_mode=guidance_mode,
        classes=stream.classes,descriptor_names=DESCRIPTORS,role_status='UNKNOWN',
        contract=dict(CONTRACT),car_approved=False)
    return result
