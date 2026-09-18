"""Differentiable OFFLINE stereo analysis/synthesis for the separate R1 model.

This is not a real-time PCM adapter. Frames have explicit causal right-edge
indices; whole finite inputs and zero-tail flush are available offline. Returned
waveforms are aligned to input sample0 by removing the documented analysis
padding. The caller owns extractor latency, original/estimate alignment, model
frame state and training/assessment masks. No independent signal gain is used.
"""
from dataclasses import dataclass

import torch
from torch.nn import functional as F

from r1_source_refiner import RefinerConfig


SCHEMA = 'r1-stereo-offline-hann-wola-v1'
CONTRACT = dict(schema=SCHEMA, sample_rates=[44100, 48000], fft_size=1024, hop=256,
    input='finite float32 samples,2; common original/estimate timeline supplied by caller',
    frames='complex64 1,T,2,513; frame k supports [k*256-768,(k+1)*256)',
    window='periodic Hann for both analysis and synthesis; no centered/reflected padding',
    fft_normalization='torch default: unnormalized rFFT, 1/N inverse rFFT; no signal-dependent scaling',
    padding='left768; right ceil(length/256)*256-length+768; explicit zeros only',
    synthesis='differentiable irFFT, overlap-add, and fixed window-square normalization',
    returned_waveform='input-aligned [0,length); documented 768-sample WOLA alignment offset cropped offline',
    sample_validity='all returned samples represent original input positions; no padding samples returned',
    frame_score_validity='only frames whose entire original support lies within the requested interval',
    context_scope='frame score masks do not remove past model context or prescribe BPTT boundaries',
    gain='no input-dependent or per-source normalization, AGC, clipping or loudness matching',
    model_state='caller-owned; arbitrary frame chunks may carry RefinerState, not a PCM streaming adapter',
    no_p1_six_channel_frontend=True, no_native_export=True, offline_only=True,
    pcm_streaming_implemented=False, extractor_causality_verified=False,
    target_runtime_verified=False, reconstruction_quality_approved=False, car_approved=False)


def _config(config):
    config = RefinerConfig() if config is None else config
    if type(config) is not RefinerConfig or config.fft_size != 1024:
        raise ValueError('R1 audio requires RefinerConfig with FFT1024/hop256 at44100 or48000')
    return config


def _geometry(length, config):
    if type(length) is not int or length <= 0:
        raise ValueError('Positive integer original sample length required')
    left = config.fft_size-config.hop
    aligned = ((length+config.hop-1)//config.hop)*config.hop
    right = aligned-length+left
    count = (length+left+right-config.fft_size)//config.hop+1
    return left, right, count


def _window(config, device):
    return torch.hann_window(config.fft_size, periodic=True, dtype=torch.float32, device=device)


def _validate_frames(frames, length, config):
    _, _, count = _geometry(length, config)
    if (not isinstance(frames, torch.Tensor) or frames.ndim != 4
            or frames.shape != (1, count, 2, config.bins) or frames.dtype != torch.complex64
            or not torch.isfinite(frames).all()
            or frames[..., 0].imag.any() or frames[..., -1].imag.any()):
        raise ValueError('Finite stereo complex64 1,T,2,F frames with exact padding count and real endpoints required')


@dataclass(frozen=True)
class AudioAnalysis:
    frames: torch.Tensor
    length: int
    config: RefinerConfig
    left_padding: int
    right_padding: int

    @property
    def frame_starts(self):
        # Fresh coordinate tensors: a caller cannot mutate stored score bounds.
        return torch.arange(self.frames.shape[1], device=self.frames.device, dtype=torch.int64)*self.config.hop-self.left_padding

    @property
    def frame_ends(self):
        """Exclusive right edges on the original waveform's sample timeline."""
        return self.frame_starts+self.config.fft_size

    @property
    def full_support_mask(self):
        return score_frame_mask(self, 0, self.length)

    @property
    def waveform_valid_range(self):
        return (0, self.length)

    def timeline(self):
        _validate_analysis(self)
        return dict(schema=SCHEMA, sample_rate=self.config.sample_rate,
            fft_size=self.config.fft_size, hop=self.config.hop, original_samples=self.length,
            frame_count=self.frames.shape[1], first_frame_start=-self.left_padding,
            first_frame_end=self.config.hop,
            last_frame_start=(self.frames.shape[1]-1)*self.config.hop-self.left_padding,
            last_frame_end=self.frames.shape[1]*self.config.hop,
            left_zero_padding=self.left_padding, right_zero_padding=self.right_padding,
            complete_original_support_frames=int(self.full_support_mask.sum()),
            returned_sample_start=0, returned_sample_end=self.length,
            nominal_wola_alignment_delay_samples=self.config.fft_size-self.config.hop,
            nominal_wola_alignment_delay_seconds=(self.config.fft_size-self.config.hop)/self.config.sample_rate,
            output_alignment='padding cropped offline; returned index i is original index i',
            pcm_streaming_implemented=False, real_time_callback_latency_verified=False,
            input_and_extractor_causality_not_established=True, car_approved=False)


def _validate_analysis(analysis):
    if type(analysis) is not AudioAnalysis:
        raise ValueError('R1 AudioAnalysis required')
    config = _config(analysis.config)
    left, right, _ = _geometry(analysis.length, config)
    if (type(analysis.left_padding) is not int or type(analysis.right_padding) is not int
            or analysis.left_padding != left or analysis.right_padding != right):
        raise ValueError('Analysis padding differs from the frozen R1 timeline')
    _validate_frames(analysis.frames, analysis.length, config)


def analyze(wave, config=None):
    """Analyze one complete finite stereo waveform; retain autograd to its PCM.

    Each frame uses only samples before its declared exclusive right edge.
    Frames beyond the original end are explicit zero-tail flushing, not future
    music. They are excluded by full_support_mask and score_frame_mask.
    """
    config = _config(config)
    if (not isinstance(wave, torch.Tensor) or wave.ndim != 2 or wave.shape[1] != 2
            or not wave.shape[0] or wave.dtype != torch.float32 or not torch.isfinite(wave).all()):
        raise ValueError('Finite nonempty float32 original samples,2 waveform required')
    length = wave.shape[0]
    left, right, _ = _geometry(length, config)
    joined = F.pad(wave.transpose(0, 1), (left, right))
    # Two channels, then frame, then within-frame samples. No channel folding.
    framed = joined.unfold(-1, config.fft_size, config.hop).permute(1, 0, 2).unsqueeze(0)
    spectrum = torch.fft.rfft(framed*_window(config, wave.device), n=config.fft_size, dim=-1)
    _validate_frames(spectrum, length, config)
    return AudioAnalysis(spectrum, length, config, left, right)


def synthesize(frames, length, config=None):
    """Return aligned samples,2 via explicit WOLA, not torch.istft NOLA checks.

    The padding guarantees strictly positive fixed normalization over every
    returned sample, including very short input. No tiny norm is clamped into
    validity; a broken analysis geometry is rejected instead.
    """
    config = _config(config)
    _validate_frames(frames, length, config)
    left, right, count = _geometry(length, config)
    total = left+length+right
    window = _window(config, frames.device)
    decoded = torch.fft.irfft(frames, n=config.fft_size, dim=-1)*window
    columns = decoded.permute(0, 2, 3, 1).reshape(2, config.fft_size, count)
    combined = F.fold(columns, (1, total), (1, config.fft_size),
                      stride=(1, config.hop)).reshape(2, total).transpose(0, 1)
    weights = F.fold(window.square()[None, :, None].expand(1, config.fft_size, count),
                     (1, total), (1, config.fft_size), stride=(1, config.hop)).reshape(total)
    real_weights = weights[left:left+length]
    if not torch.isfinite(real_weights).all() or not (real_weights > 0).all():
        raise ValueError('R1 WOLA normalization is not valid over the original sample interval')
    output = combined[left:left+length]/real_weights[:, None]
    if output.shape != (length, 2) or not torch.isfinite(output).all():
        raise ValueError('Nonfinite or misaligned R1 stereo reconstruction')
    return output


def score_frame_mask(analysis, start, end):
    """Complete analysis windows inside original [start,end), with no padding.

    An interval shorter than one full window returns an all-false mask, not a
    quality pass or a denominator substitute. Caller must count eligible frames.
    This is a score mask; it does not silently reset the model at split borders.
    """
    _validate_analysis(analysis)
    if (type(start) is not int or type(end) is not int
            or not 0 <= start < end <= analysis.length):
        raise ValueError('Nonempty integer score interval within original waveform required')
    return (analysis.frame_starts >= start) & (analysis.frame_ends <= end)
