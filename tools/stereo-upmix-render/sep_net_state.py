#!/usr/bin/env python3
"""Review-35 causal matrix with explicit slow eligible evidence and fast roles.

The base separator stays byte-identical for historical experiment hashes. This
module reuses its training/loss code and replaces only the BandSepNet class.
"""

import argparse
import json
import math
from pathlib import Path
from types import SimpleNamespace

import sep_net as SN


STATE_DIM = 8
STATE_TAU = 8.0
RESET_SECONDS = 2.0
_BASE_BAND_NET = SN.BandSepNet


def __getattr__(name):
    return getattr(SN, name)


def ema_time(torch, x, mode, tau=STATE_TAU):
    """Causal EMA over [batch, time, bands, features], with optional 2 s reset.

    The chunked closed form is differentiable and carries state over arbitrary
    1024-frame chunks. It never reads a later frame to produce an earlier one.
    """
    alpha = math.exp(-SN.HOP / (SN.SR * tau))
    reset_frames = round(RESET_SECONDS * SN.SR / SN.HOP)
    chunk = reset_frames if mode == "reset2" else 1024
    out = []
    carry = None
    for start in range(0, x.shape[1], chunk):
        if mode == "reset2":
            carry = None
        block = x[:, start:start + chunk]
        k = torch.arange(block.shape[1], device=x.device, dtype=x.real.dtype)
        shape = (1, -1) + (1,) * (x.ndim - 2)
        decay = (alpha ** k).reshape(shape)
        inv = (alpha ** (-k)).reshape(shape)
        integral = (1 - alpha) * decay * torch.cumsum(block * inv, dim=1)
        state = integral if carry is None else integral + (alpha * decay) * carry[:, None]
        carry = state[:, -1]
        out.append(state)
    return torch.cat(out, dim=1)


class DualStateBandSepNet(_BASE_BAND_NET):
    """Same matrix trunk, 8 s eligible-evidence EMA and current-frame role head."""

    state_mode = "persistent"

    def __init__(self, torch, feat=48, h1=32, h2=32, hg=32, brnn=0,
                 lookahead=0, bidir=False, dec_skip=False, norm="window", mask="matrix2"):
        if mask != "matrix2" or lookahead or bidir or brnn:
            raise ValueError("dual-state probe requires causal matrix2, no lookahead/bidir/band-RNN")
        super().__init__(torch, feat, h1, h2, hg, brnn, lookahead,
                         bidir, dec_skip, norm, mask)
        nn = torch.nn
        self.state_proj = nn.Linear(h2, STATE_DIM)
        self.role_head = nn.Linear(feat, 4)
        base_input = h2 + hg + (feat if self.dec_skip else 0)
        self.dec = nn.ModuleList([
            nn.Linear(base_input + STATE_DIM + 4, (b - a) * 8)
            for a, b in self.bands
        ])
        for decoder in self.dec:
            nn.init.normal_(decoder.weight, std=0.01)
            with torch.no_grad():
                decoder.bias.zero_()
                decoder.bias.view(-1, 8)[:, 0] = 0.3
                decoder.bias.view(-1, 8)[:, 2] = 0.3
        self.modules[-1] = self.dec
        self.modules.append(self.state_proj)
        self.modules.append(self.role_head)
        self.macs += SN.N_BINS * 8 * (STATE_DIM + 4)
        self.macs += len(self.bands) * (h2 * STATE_DIM + feat * 4)

    def separate_stft(self, X):
        torch = self.torch
        B, T = X.shape[:2]
        if self.norm == "stream":
            scale = self.running_level(X.abs().mean(dim=(2, 3)))[:, :, None, None].clamp(min=1e-6)
        else:
            scale = X.abs().mean(dim=(1, 2, 3), keepdim=True) + 1e-6
        Xn = X / scale
        lm = torch.log(X.abs() + 1e-5)
        lm = (lm - SN.STATS["mean"]) / SN.STATS["std"] if SN.STATS["mean"] is not None else lm
        features = torch.cat([Xn.real, Xn.imag, lm.clamp(-6, 6)], -1)
        e = torch.stack([
            enc(features[:, :, a:b, :].reshape(B, T, -1))
            for enc, (a, b) in zip(self.enc, self.bands)
        ], 2).tanh()
        nb = e.shape[2]
        h, _ = self.gru1(e.permute(0, 2, 1, 3).reshape(B * nb, T, self.feat))
        h, _ = self.gru2(h)
        h = h.reshape(B, nb, T, self.h2).permute(0, 2, 3, 1).reshape(B * T, self.h2, nb)
        h = h + self.mix1(h)
        h = h + self.mix2(h)
        h = h.reshape(B, T, self.h2, nb)
        g, _ = self.ggru(h.mean(-1))
        hband = h.permute(0, 1, 3, 2)

        role_band = self.role_head(e)
        role_prob = role_band.softmax(-1)
        proposal = self.state_proj(hband).tanh()
        slow = ema_time(torch, role_prob[..., 3, None] * proposal, self.state_mode)
        self.last_slow_state = slow
        self.role_logits = torch.cat([
            role_band[:, :, i:i + 1].expand(-1, -1, b - a, -1)
            for i, (a, b) in enumerate(self.bands)
        ], dim=2)

        parts = [hband, g[:, :, None, :].expand(-1, -1, nb, -1)]
        if self.dec_skip:
            parts.append(e)
        parts.extend((slow, role_prob))
        z = torch.cat(parts, -1)
        m = torch.cat([
            decoder(z[:, :, i]).reshape(B, T, b - a, 8)
            for i, (decoder, (a, b)) in enumerate(zip(self.dec, self.bands))
        ], dim=2)
        diagonal = torch.complex(m[..., 0:4:2], m[..., 1:4:2])
        cross = torch.complex(m[..., 4:8:2], m[..., 5:8:2])
        self.last_matrix = (diagonal, cross)
        self.last_mask = diagonal
        return self.apply_last_filter(X)


def _factory(mode):
    class Arm(DualStateBandSepNet):
        state_mode = mode
    return Arm


def load(model_path):
    torch = SN._torch()
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    ck = torch.load(model_path, map_location=dev)
    mode = ck.get("state_mode")
    if mode not in ("persistent", "reset2"):
        raise ValueError(f"checkpoint missing review-35 state_mode: {model_path}")
    net = _factory(mode)(
        torch, feat=ck["feat"], h1=ck["h1"], h2=ck["h2"], hg=ck["bhg"],
        brnn=ck["brnn"], lookahead=ck["lookahead"], bidir=ck["bidir"],
        dec_skip=ck["dec_skip"], norm=ck["norm"], mask=ck["mask"]
    ).to(dev)
    net.modules.load_state_dict(ck["state"])
    net.modules.eval()
    SN.STATS.update(ck["stats"])
    return torch, net, dev


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    args = parser.parse_args()
    config = json.loads(args.config.read_text(encoding="utf-8"))
    mode = config.pop("state_mode")
    if mode not in ("persistent", "reset2"):
        parser.error("state_mode must be persistent or reset2")
    config["save_at"] = set(config["save_at"])
    a = SimpleNamespace(**config)
    SN.BandSepNet = _factory(mode)
    try:
        SN.train(a)
    finally:
        SN.BandSepNet = _BASE_BAND_NET
    for name in ("last.pt", "step4000.pt", "sep_net.pt"):
        path = Path(a.out) / name
        if path.exists():
            torch = SN._torch()
            ck = torch.load(path, map_location="cpu")
            ck["state_mode"] = mode
            torch.save(ck, path)


if __name__ == "__main__":
    main()
