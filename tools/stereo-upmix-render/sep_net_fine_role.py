#!/usr/bin/env python3
"""Review-37 per-bin role evidence with explicit current-frame matrix route."""

import argparse
import json
from pathlib import Path
from types import SimpleNamespace

import sep_net as SN
import sep_net_state as Previous


ROLE_CONTEXT = 8
_BASE_BAND_NET = SN.BandSepNet


def __getattr__(name):
    return getattr(SN, name)


class FineRoleBandSepNet(Previous.DualStateBandSepNet):
    role_mode = "fine"
    state_mode = "persistent"

    def __init__(self, torch, feat=48, h1=32, h2=32, hg=32, brnn=0,
                 lookahead=0, bidir=False, dec_skip=False, norm="window", mask="matrix2"):
        super().__init__(torch, feat, h1, h2, hg, brnn, lookahead,
                         bidir, dec_skip, norm, mask)
        nn = torch.nn
        self.role_context = nn.Linear(feat, ROLE_CONTEXT)
        self.fine_head = nn.Linear(ROLE_CONTEXT + 6, 4)
        self.role_map = nn.Linear(4, 8, bias=False)
        nn.init.normal_(self.role_map.weight, std=.01)
        del self.role_head
        self.modules[-1] = self.role_context
        self.modules.append(self.fine_head)
        self.modules.append(self.role_map)
        self.macs -= len(self.bands) * feat * 4
        self.macs += len(self.bands) * feat * ROLE_CONTEXT
        self.macs += SN.N_BINS * ((ROLE_CONTEXT + 6) * 4 + 4 * 8)

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
            enc(features[:, :, a:b].reshape(B, T, -1))
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

        context = self.role_context(e).tanh()
        logits_blocks = []
        band_probs = []
        for index, (a, b) in enumerate(self.bands):
            local = features[:, :, a:b]
            if self.role_mode == "coarse":
                local = local.mean(2, keepdim=True).expand(-1, -1, b - a, -1)
            joined = torch.cat((context[:, :, index:index + 1].expand(-1, -1, b - a, -1),
                                local), dim=-1)
            logits = self.fine_head(joined)
            logits_blocks.append(logits)
            band_probs.append(logits.softmax(-1).mean(2))
        self.role_logits = torch.cat(logits_blocks, dim=2)
        role_prob = self.role_logits.softmax(-1)
        role_band = torch.stack(band_probs, dim=2)
        proposal = self.state_proj(hband).tanh()
        slow = Previous.ema_time(torch, role_band[..., 3, None] * proposal, self.state_mode)
        self.last_slow_state = slow

        z = torch.cat((hband, g[:, :, None].expand(-1, -1, nb, -1),
                       e if self.dec_skip else hband[..., :0], slow, role_band), dim=-1)
        base = torch.cat([
            decoder(z[:, :, index]).reshape(B, T, b - a, 8)
            for index, (decoder, (a, b)) in enumerate(zip(self.dec, self.bands))
        ], dim=2)
        m = base + self.role_map(role_prob - .25)
        diagonal = torch.complex(m[..., 0:4:2], m[..., 1:4:2])
        cross = torch.complex(m[..., 4:8:2], m[..., 5:8:2])
        self.last_matrix = (diagonal, cross)
        self.last_mask = diagonal
        return self.apply_last_filter(X)


def _factory(mode):
    class Arm(FineRoleBandSepNet):
        role_mode = mode
    return Arm


def load(model_path):
    torch = SN._torch()
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    ck = torch.load(model_path, map_location=dev)
    mode = ck.get("role_mode")
    if mode not in ("fine", "coarse"):
        raise ValueError(f"checkpoint missing review-37 role_mode: {model_path}")
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
    mode = config.pop("role_mode")
    if mode not in ("fine", "coarse"):
        parser.error("role_mode must be fine or coarse")
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
            ck["role_mode"] = mode
            ck["state_mode"] = "persistent"
            torch.save(ck, path)


if __name__ == "__main__":
    main()
