"""Tiny source-conditioned complex residual research model, not authored 5.1.

Inputs are aligned original/estimated stereo rFFT frames supplied by the caller.
This module contains no FFT/WOLA, source extractor, role classifier, cabin
renderer, learned checkpoint, native export or target timing claim. It predicts
an additive complex residual, not gains applied only to the estimated source.
"""
from dataclasses import asdict, dataclass
import math

import torch
from torch import nn
from torch.nn import functional as F


ARCHITECTURE = 'r1-local-complex-source-residual-v1'
CONDITIONING = ('mixture_and_estimate', 'estimate_only')
CONTRACT = dict(architecture=ARCHITECTURE,
    inputs='aligned complex64 original X and preliminary estimate E: B,T,2,F; real DC/Nyquist',
    output='stereo complex64 E + learned direct complex residual; no 5.1/front-folded decoder',
    residual='four real output channels at every rFFT bin; not a mask multiplied by E or X',
    features='Re(E),Im(E),Re(X),Im(X), divided by fixed sqrt(fft_size); no signal-dependent normalization',
    estimate_only='replace X by zero before any feature, scale or state computation; same architecture and parameter count',
    identity_initialization='zero residual head gives exact numerical E; IEEE signed-zero representation is not a promised byte invariant',
    frequency='shared narrow pointwise projection plus three-bin depthwise convolution; every bin retained',
    time='three-tap depthwise causal FIR on current/two past hidden frames; no future or recurrent feedback',
    activation='bias-free fixed leaky-ReLU slope1/8; no exp, log, sigmoid or tanh in inference graph',
    phase='complex stereo information retained; arbitrary trained phase errors still require source-fidelity evaluation',
    zero_input='zero history plus X=E=0 gives exact numerical zero for every finite weight set',
    state='model-owned two-frame hidden history; explicit detach clones; reset by passing None; reload invalidates old states',
    query='E anchors source identity; no promise of unique reconstruction from an entirely missing estimate',
    role_permission='outside model; this output does not approve relocation or identify backing versus lead',
    context_limit='local frame context only; not a long musical-role/genre model',
    fft_wola_implemented=False, native_export_supported=False, target_runtime_verified=False,
    pretrained_weights_bundled=False, model_quality_approved=False, genre_generalization_established=False,
    car_approved=False)


@dataclass(frozen=True)
class RefinerConfig:
    sample_rate: int = 44100
    fft_size: int = 1024
    width: int = 8
    conditioning: str = 'mixture_and_estimate'

    def __post_init__(self):
        if (type(self.sample_rate) is not int or self.sample_rate not in (44100, 48000)
                or type(self.fft_size) is not int or self.fft_size not in (256, 512, 1024, 2048)
                or type(self.width) is not int or self.width not in (4, 8)
                or type(self.conditioning) is not str or self.conditioning not in CONDITIONING):
            raise ValueError('Explicit bounded R1 RefinerConfig required')

    @property
    def bins(self):
        return self.fft_size // 2 + 1

    @property
    def hop(self):
        """Declared external analysis cadence, not an FFT implementation."""
        return self.fft_size // 4

    @property
    def past_frames(self):
        return 2


@dataclass(frozen=True)
class RefinerState:
    owner: object
    history: torch.Tensor
    frames: int

    def detach(self):
        return RefinerState(self.owner, self.history.detach().clone(), self.frames)


class SourceRefiner(nn.Module):
    """One-source learnability baseline with a separate explicit frame API.

    Role permission, source fidelity, alignment, STFT consistency and rendering
    are caller-owned. A noncausal extractor upstream makes the whole chain
    noncausal even though this model has no neural lookahead. Bias-free layers
    and leaky-ReLU are positively homogeneous in exact arithmetic, not a claim
    of exact floating-point invariance at extreme levels/underflow.
    """
    def __init__(self, config=None):
        super().__init__()
        self.config = config if config is not None else RefinerConfig()
        if type(self.config) is not RefinerConfig:
            raise ValueError('RefinerConfig required; old P1 configurations are not compatible')
        c = self.config
        self._state_owner = object()
        self.input_projection = nn.Conv1d(8, c.width, 1, bias=False)
        self.frequency = nn.Conv1d(c.width, c.width, 3, padding=1, groups=c.width, bias=False)
        self.temporal = nn.Parameter(torch.empty(c.width, 3))
        nn.init.kaiming_uniform_(self.temporal, a=math.sqrt(5))
        self.residual_head = nn.Conv1d(c.width, 4, 1, bias=False)
        nn.init.zeros_(self.residual_head.weight)
        endpoint = torch.ones(c.bins)
        endpoint[0] = endpoint[-1] = 0.
        self.register_buffer('imaginary_endpoint', endpoint)

    def load_state_dict(self, *args, **kwargs):
        # A rejected Torch load can still have copied some valid parameters.
        # Invalidate stream ownership after either successful or failed reload.
        try:
            return super().load_state_dict(*args, **kwargs)
        finally:
            self._state_owner = object()

    def _validate(self, original, estimate, state):
        c = self.config
        weights = self.input_projection.weight
        if weights.dtype != torch.float32:
            raise ValueError('R1 research inference uses float32 weights and complex64 frames')
        for value in (original, estimate):
            if (not isinstance(value, torch.Tensor) or value.ndim != 4
                    or not value.shape[0] or not value.shape[1] or value.shape[2:] != (2, c.bins)
                    or value.dtype != torch.complex64 or value.device != weights.device
                    or not torch.isfinite(value).all()
                    or value[..., 0].imag.any() or value[..., -1].imag.any()):
                raise ValueError('Aligned finite complex64 B,T,2,F rFFT frames with real endpoints required')
        if original.shape != estimate.shape:
            raise ValueError('Original and estimate need the same batch/frame/stereo/frequency shape')
        if state is not None:
            if (type(state) is not RefinerState or state.owner is not self._state_owner
                    or type(state.frames) is not int or state.frames < 1
                    or not isinstance(state.history, torch.Tensor)
                    or state.history.shape != (original.shape[0], c.past_frames, c.width, c.bins)
                    or state.history.dtype != torch.float32 or state.history.device != weights.device
                    or not torch.isfinite(state.history).all()):
                raise ValueError('Invalid, foreign, detached-wrong-shape or reloaded R1 frame state')

    def forward(self, original, estimate, state=None):
        self._validate(original, estimate, state)
        c = self.config
        batch, steps = estimate.shape[:2]
        conditioned = (original if c.conditioning == 'mixture_and_estimate'
                       else torch.zeros_like(original))
        # No X-derived gain/envelope/phase side channel survives estimate_only.
        scale = math.sqrt(c.fft_size)
        features = torch.cat((estimate.real, estimate.imag, conditioned.real, conditioned.imag), 2) / scale
        local = F.leaky_relu(self.input_projection(features.reshape(batch*steps, 8, c.bins)), .125)
        local = F.leaky_relu(self.frequency(local), .125).reshape(batch, steps, c.width, c.bins)
        history = (local.new_zeros(batch, c.past_frames, c.width, c.bins)
                   if state is None else state.history)
        joined = torch.cat((history, local), 1)
        temporal = torch.zeros_like(local)
        for lag in range(3):
            start = c.past_frames-lag
            temporal = temporal + joined[:, start:start+steps] * self.temporal[:, lag][None, None, :, None]
        temporal = F.leaky_relu(temporal, .125)
        raw = self.residual_head(temporal.reshape(batch*steps, c.width, c.bins))
        raw = raw.reshape(batch, steps, 4, c.bins) * scale
        residual = torch.complex(raw[:, :, :2], raw[:, :, 2:] * self.imaginary_endpoint)
        result = estimate + residual
        history = joined[:, -c.past_frames:].clone()
        if not torch.isfinite(result).all() or not torch.isfinite(history).all():
            raise ValueError('Nonfinite R1 result or state; no state is committed')
        carried = RefinerState(self._state_owner, history, steps+(0 if state is None else state.frames))
        return result, carried

    def specification(self):
        c = self.config
        per_frame = dict(input_projection=8*c.width*c.bins,
            depthwise_frequency=3*c.width*c.bins,
            depthwise_temporal=3*c.width*c.bins,
            residual_head=4*c.width*c.bins)
        expected_parameters = 18*c.width
        parameters = sum(v.numel() for v in self.parameters())
        if parameters != expected_parameters:
            raise ValueError('R1 implementation differs from its analytic parameter contract')
        return dict(architecture=ARCHITECTURE, config=asdict(c), parameters=parameters,
            parameter_bytes_float32=parameters*4, fixed_buffer_bytes=self.imaginary_endpoint.numel()*4,
            output_channels=['source_left', 'source_right'], output_is='estimate_plus_direct_complex_residual',
            dense_macs_per_frame=sum(per_frame.values()), breakdown_per_frame=per_frame,
            nominal_external_hop_samples=c.hop,
            dense_macs_per_second=sum(per_frame.values())*c.sample_rate/c.hop,
            state_floats_per_source_batch_item=c.past_frames*c.width*c.bins,
            state_tensor_bytes_per_source_batch_item=c.past_frames*c.width*c.bins*4,
            past_hidden_frames=c.past_frames, nominal_past_seconds=c.past_frames*c.hop/c.sample_rate,
            additional_neural_future_frames=0, frequency_receptive_bins=3,
            additions_activations_scaling_validation_memory_fft_and_renderer_excluded=True,
            dense_macs_are_not_arm_instructions_or_cycles=True,
            analysis_synthesis_latency_caller_owned=True, externally_shared_mixture_fft_supported=True,
            source_count_cost='per-source neural features/state scale linearly; FFT sharing is caller-owned',
            callback_allocations_bounded_and_removed=False, native_export_supported=False,
            target_runtime_verified=False, pretrained_weights_bundled=False,
            model_quality_approved=False, genre_generalization_established=False, car_approved=False)
