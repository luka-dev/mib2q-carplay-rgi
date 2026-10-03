"""Track B joint front/centre/rear estimator; independent research interface.

Full-bin complex stereo operators, causal band/global state, explicit dry
downmix projection, and separately accounted optional rear duplication.
No hook ABI, trained weights, int8/device or acoustic qualification claim.
"""
from dataclasses import asdict, dataclass
import math

import torch
from torch import nn
from torch.nn import functional as F


CHANNELS = ("FL", "FR", "C", "Ls", "Rs")
ARCHITECTURE = "joint-scene-band-global-v1"


@dataclass(frozen=True)
class Config:
    sample_rate: int = 48000
    fft_size: int = 1024
    hop: int = 256
    width: int = 10
    bands: int = 32
    depth: int = 3
    global_width: int = 64
    coefficient_limit: float = 2.0


class FrequencyBlock(nn.Module):
    def __init__(self, width, dilation):
        super().__init__()
        self.norm = nn.LayerNorm(width)
        self.depthwise = nn.Conv1d(width, width, 3, padding=dilation,
                                   dilation=dilation, groups=width)
        self.pointwise = nn.Conv1d(width, width, 1)

    def forward(self, x):
        y = self.norm(x.transpose(1, 2)).transpose(1, 2)
        return (x + self.pointwise(F.silu(self.depthwise(y)))) / math.sqrt(2)


class JointSceneNet(nn.Module):
    def __init__(self, config=None):
        super().__init__()
        self.config = config or Config()
        c = self.config
        if c.fft_size != 1024 or c.hop != 256 or min(c.width, c.bands, c.depth, c.global_width) < 1:
            raise ValueError("Explicit positive dimensions and 1024/256 analysis required")
        self.bins = c.fft_size // 2 + 1
        self.input_projection = nn.Conv1d(12, c.width, 1)
        self.frequency = nn.ModuleList(FrequencyBlock(c.width, 2**i) for i in range(c.depth))
        self.band_norm = nn.LayerNorm(c.width)
        self.band_gru = nn.GRU(c.width, c.width, batch_first=True)
        self.global_encode = nn.Linear(c.bands * c.width, c.global_width)
        self.global_gru = nn.GRU(c.global_width, c.global_width, batch_first=True)
        self.global_decode = nn.Linear(c.global_width, c.bands * c.width)
        self.operator_head = nn.Conv1d(2 * c.width, len(CHANNELS) * 2 * 2, 1)
        nn.init.normal_(self.operator_head.weight, std=.005)
        nn.init.zeros_(self.operator_head.bias)
        h = math.sqrt(.5)
        d = torch.tensor([[1., 0., h, h, 0.], [0., 1., h, 0., h]])
        self.register_buffer("downmix", d)
        self.register_buffer("right_inverse", d.T @ torch.linalg.inv(d @ d.T))
        identity = torch.zeros(5, 2, dtype=torch.complex64)
        identity[0, 0] = identity[1, 1] = 1
        self.register_buffer("identity_operators", identity)
        self.register_buffer("input_identity", torch.eye(2, dtype=torch.complex64))
        endpoints = torch.ones(self.bins)
        endpoints[0] = endpoints[-1] = 0
        self.register_buffer("imaginary_endpoint", endpoints)
        position = torch.linspace(-1, 1, self.bins)
        self.register_buffer("position", torch.stack([position, position.square()]))
        rotations = torch.tensor([1+0j, -1j, -1+0j, 1j], dtype=torch.complex64)
        self.register_buffer("phase_advance", rotations[torch.arange(self.bins) % 4])

    def features(self, x, previous):
        power = x.abs().square()
        scale = power.mean((-1, -2), keepdim=True).clamp_min(1e-12).sqrt()
        z = x / scale
        pl, pr = (power / scale.square()).unbind(-1)
        total = (pl + pr).clamp_min(1e-6)
        cross = z[..., 0] * z[..., 1].conj()
        local = torch.stack([torch.log1p(pl), torch.log1p(pr), (pl-pr)/total,
                             2*cross.real/total, 2*cross.imag/total,
                             torch.log1p(total*.5)], -1)
        past = torch.cat([previous[:, None], z[:, :-1]], 1)
        advance = 2*z*past.conj()*self.phase_advance[None, None, :, None]
        advance /= (z.abs().square() + past.abs().square()).clamp_min(1e-6)
        phase = torch.stack([advance[..., 0].real, advance[..., 0].imag,
                             advance[..., 1].real, advance[..., 1].imag], -1)
        pos = self.position.T[None, None].expand(x.shape[0], x.shape[1], -1, -1)
        return torch.cat([local, phase, pos], -1), z[:, -1]

    def forward(self, x, state=None, effect_gain=0.):
        """[B,T,513,2] complex -> five channels plus reusable past-only state.

        Effects derive from actual baseline rear audio. They cannot absorb an
        arbitrary unexplained residual. Caller detaches state for TBPTT.
        """
        if x.ndim != 4 or x.shape[-2:] != (self.bins, 2) or not x.is_complex():
            raise ValueError("Complex [batch,time,513,stereo] required")
        if not math.isfinite(effect_gain) or not 0 <= effect_gain <= .25:
            raise ValueError("Research duplication gain must be in [0,.25]")
        b, t = x.shape[:2]
        c = self.config
        state = {} if state is None else state
        previous = state.get("previous", torch.zeros_like(x[:, 0]))
        features, previous = self.features(x, previous)
        local = F.silu(self.input_projection(features.reshape(b*t, self.bins, 12).transpose(1, 2)))
        for block in self.frequency:
            local = block(local)
        pooled = F.adaptive_avg_pool1d(local, c.bands).transpose(1, 2).reshape(b, t, c.bands, c.width)
        sequence = self.band_norm(pooled).permute(0, 2, 1, 3).reshape(b*c.bands, t, c.width)
        band, band_state = self.band_gru(sequence, state.get("band"))
        band = band.reshape(b, c.bands, t, c.width).permute(0, 2, 1, 3)
        global_input = F.silu(self.global_encode(pooled.flatten(2)))
        global_context, global_state = self.global_gru(global_input, state.get("global"))
        global_context = self.global_decode(global_context).reshape(b, t, c.bands, c.width)
        context = (band + global_context) / math.sqrt(2)
        context = F.interpolate(context.reshape(b*t, c.bands, c.width).transpose(1, 2),
                                size=self.bins, mode="linear", align_corners=True)
        raw = c.coefficient_limit * self.operator_head(torch.cat([local, context], 1)).tanh()
        raw = raw.transpose(1, 2).reshape(b, t, self.bins, 5, 2, 2)
        operators = self.identity_operators + torch.complex(
            raw[..., 0], raw[..., 1] * self.imaginary_endpoint[None, None, :, None, None])
        d, right = self.downmix.to(x.dtype), self.right_inverse.to(x.dtype)
        error = self.input_identity - torch.einsum("oc,btfci->btfoi", d, operators)
        operators = operators + torch.einsum("co,btfoi->btfci", right, error)
        base = torch.einsum("btfci,btfi->btfc", operators, x)
        effect = torch.cat([torch.zeros_like(base[..., :3]), effect_gain*base[..., 3:]], -1)
        return {"base":base, "effect":effect, "output":base+effect, "operators":operators}, {
            "previous":previous, "band":band_state, "global":global_state}

    def specification(self):
        c = self.config
        f = self.bins
        counts = dict(input_projection=f*12*c.width,
                      frequency=c.depth*f*(3*c.width+c.width*c.width),
                      band_gru=c.bands*6*c.width*c.width,
                      global_encode=c.bands*c.width*c.global_width,
                      global_gru=6*c.global_width*c.global_width,
                      global_decode=c.global_width*c.bands*c.width,
                      operator_head=f*(2*c.width)*20)
        return dict(architecture=ARCHITECTURE, config=asdict(c),
                    parameters=sum(p.numel() for p in self.parameters()),
                    learned_channels=list(CHANNELS), dense_neural_macs_per_second=sum(counts.values())*c.sample_rate/c.hop,
                    dense_neural_macs_per_frame=counts, excludes="FFT/WOLA, features, projection, complex application, normalization, nonlinearities and LFE DSP",
                    future_neural_frames=0, framing_window_ms=1000*c.fft_size/c.sample_rate,
                    device_measured=False, int8_supported=False, production_ready=False)


def analysis(wave, fft_size=1024, hop=256):
    """Past-only frames, no centered padding: [N,C] -> [1,T,F,C]."""
    window = torch.sin(math.pi*(torch.arange(fft_size, device=wave.device)+.5)/fft_size)
    frames = wave.T.unfold(-1, fft_size, hop)
    return torch.fft.rfft(frames * window, dim=-1).permute(1, 2, 0)[None]


def synthesize(spectra, fft_size=1024, hop=256):
    """Differentiable normalized overlap-add for a finite diagnostic excerpt."""
    if spectra.shape[0] != 1:
        raise ValueError("One diagnostic excerpt at a time")
    window = torch.sin(math.pi*(torch.arange(fft_size, device=spectra.device)+.5)/fft_size)
    frames = torch.fft.irfft(spectra[0].permute(2, 0, 1), n=fft_size, dim=-1) * window
    length = fft_size + hop*(spectra.shape[1]-1)
    output = F.fold(frames.transpose(1, 2), (1, length), (1, fft_size), stride=(1, hop))[:, 0, 0].T
    weights = F.fold(window.square()[None, :, None].expand(1, -1, spectra.shape[1]),
                     (1, length), (1, fft_size), stride=(1, hop))[0, 0, 0]
    return output / weights.clamp_min(1e-6)[:, None]
