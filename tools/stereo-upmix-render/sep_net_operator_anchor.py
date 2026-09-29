#!/usr/bin/env python3
"""Causal 2x2 eligible-audio model with an explicit slow operator and fit-target loss."""

import argparse
import json
from pathlib import Path
from types import SimpleNamespace

import numpy as np

import diag_spatial_oracle as SO
import mask_net as MN
import sep_net as SN
import sep_net_state as SS


TAU_SECONDS = 8.0
FAST_FRACTION = 0.5
FIT_END = round(29.9 * SN.SR / SN.HOP)
TEST_START = round(30.1 * SN.SR / SN.HOP)
MIN_ELIGIBLE_FIT_POWER = 0.01
MAX_TEACHER_COEFFICIENT = 2.0
_BASE_BAND_NET = SN.BandSepNet
_BASE_AUDIO_LOSS = SN.loss_audio
_ACTIVE_NET = None
_OPERATOR_WEIGHT = 0.0


def __getattr__(name):
    return getattr(SN, name)


class AnchorBandSepNet(_BASE_BAND_NET):
    """Same band model, half fast matrix and half causal 8 s EMA matrix."""

    def __init__(self, torch, feat=32, h1=20, h2=20, hg=20, brnn=0,
                 lookahead=0, bidir=False, dec_skip=True, norm="stream", mask="matrix2"):
        if mask != "matrix2" or norm != "stream" or lookahead or bidir or brnn:
            raise ValueError("operator anchor needs stream-normalized causal matrix2 without band RNN")
        super().__init__(torch, feat, h1, h2, hg, brnn, lookahead, bidir, dec_skip, norm, mask)
        global _ACTIVE_NET
        _ACTIVE_NET = self

    def apply_last_filter(self, Z):
        if getattr(self, "_collect_only", False):
            return Z
        return super().apply_last_filter(Z)

    def separate_stft(self, X):
        torch = self.torch
        self.last_input_X = X
        self._collect_only = True
        try:
            super().separate_stft(X)
        finally:
            self._collect_only = False
        diagonal, cross = self.last_matrix
        fast = torch.stack((diagonal[..., 0].real, diagonal[..., 0].imag,
                            diagonal[..., 1].real, diagonal[..., 1].imag,
                            cross[..., 0].real, cross[..., 0].imag,
                            cross[..., 1].real, cross[..., 1].imag), -1)
        initial = fast[:, :1]
        slow = initial + SS.ema_time(torch, fast - initial, "persistent", tau=TAU_SECONDS)
        self.last_slow_vector = slow
        final = FAST_FRACTION * fast + (1 - FAST_FRACTION) * slow
        diagonal = torch.stack((torch.complex(final[..., 0], final[..., 1]),
                                torch.complex(final[..., 2], final[..., 3])), -1)
        cross = torch.stack((torch.complex(final[..., 4], final[..., 5]),
                             torch.complex(final[..., 6], final[..., 7])), -1)
        self.last_matrix = (diagonal, cross)
        self.last_mask = diagonal
        return self.apply_last_filter(X)


def teacher_vector(torch, X, E):
    """Detached 30 s stem-fitted target, weighted by second-window mixture power."""
    targets = []
    valid = []
    with torch.no_grad():
        for mix, elig in zip(X, E):
            fit_x, fit_e = mix[:FIT_END], elig[:FIT_END]
            ratio = float((fit_e.abs() ** 2).sum() / (fit_x.abs() ** 2).sum().clamp_min(1e-20))
            valid.append(ratio >= MIN_ELIGIBLE_FIT_POWER)
            _, matrix = SO.fit_operators(torch, fit_x, fit_e)
            # Never let nearly empty frequency bins set an unbounded target.
            matrix = matrix * (MAX_TEACHER_COEFFICIENT / matrix.abs().clamp_min(MAX_TEACHER_COEFFICIENT))
            columns = torch.stack((matrix[:, 0, 0], matrix[:, 1, 1],
                                   matrix[:, 0, 1], matrix[:, 1, 0]), -1)
            targets.append(torch.view_as_real(columns).reshape(SN.N_BINS, 8))
    return torch.stack(targets), torch.as_tensor(valid, device=X.device)


def operator_loss(torch, net, X, E):
    if X.shape[1] <= TEST_START + 100:
        raise ValueError("operator target requires a 30 s fit and later evaluation segment")
    target, valid = teacher_vector(torch, X, E)
    if not bool(valid.any()):
        return net.last_slow_vector.sum() * 0
    pred = net.last_slow_vector[:, TEST_START:]
    power = (X[:, TEST_START:].abs() ** 2).sum(-1).mean(1)
    weight = (power / power.mean(-1, keepdim=True).clamp_min(1e-20)).sqrt().clamp(max=10).detach()
    per_bin = ((pred - target[:, None]) ** 2).mean((1, 3))
    per_song = (per_bin * weight).sum(-1) / weight.sum(-1).clamp_min(1e-8)
    return per_song[valid].mean()


def wrapped_audio_loss(torch, est, tgt, mix=None, floor_rel=None, scale=1.0):
    base = _BASE_AUDIO_LOSS(torch, est, tgt, mix, floor_rel, scale)
    if _OPERATOR_WEIGHT:
        base = base + _OPERATOR_WEIGHT * operator_loss(
            torch, _ACTIVE_NET, _ACTIVE_NET.last_input_X, tgt)
    return base


def gradient_norm(torch, net, objective):
    for p in net.parameters():
        p.grad = None
    objective().backward()
    return float(torch.sqrt(sum((p.grad ** 2).sum() for p in net.parameters() if p.grad is not None)))


def calibrate(cache, teacher_cache, out):
    torch = SN._torch()
    device = "cuda" if torch.cuda.is_available() else "cpu"
    torch.manual_seed(1)
    rng = np.random.default_rng(1)
    split = MN.load_split(str(cache))
    truth = [str(cache / (song + ".npy")) for song in split["train"]]
    pseudo = [str(f) for f in sorted(teacher_cache.glob("*.npy")) if f.with_suffix(".json").exists()]
    net = AnchorBandSepNet(torch).to(device)
    batches = [SN.windows(torch, (truth, pseudo), 46.0, 1, rng, with_sources=True) for _ in range(4)]
    with torch.no_grad():
        lm = torch.cat([torch.log(SN.stft(torch, b[0].to(device)).abs() + 1e-5).flatten()
                        for b in batches])
        SN.STATS["mean"], SN.STATS["std"] = float(lm.mean()), float(lm.std() + 1e-3)
    rows = []
    for index, batch in enumerate(batches, 1):
        mix, elig, vocal, drums, bass = (part.to(device) for part in batch)
        X, E = SN.stft(torch, mix), SN.stft(torch, elig)
        V, D, B = (SN.stft(torch, source) for source in (vocal, drums, bass))
        target, valid = teacher_vector(torch, X, E)

        def base_loss():
            est = net.separate_stft(X)
            return _BASE_AUDIO_LOSS(torch, est, E) + SN.multires_loss(
                torch, SN.istft(torch, est, mix.shape[-1]), elig)

        def source_loss():
            net.separate_stft(X)
            return SN.loss_source_attr_operator(torch, net, E, V, D, B)

        def anchor_loss():
            net.separate_stft(X)
            return operator_loss(torch, net, X, E)

        base = gradient_norm(torch, net, base_loss)
        source = gradient_norm(torch, net, source_loss)
        anchor = gradient_norm(torch, net, anchor_loss) if bool(valid.any()) else None
        if not np.isfinite(base) or not np.isfinite(source) or source <= 0:
            raise ValueError(f"invalid source gradients on batch {index}: {base}, {source}")
        if anchor is not None and (not np.isfinite(anchor) or anchor <= 0):
            raise ValueError(f"invalid anchor gradient on batch {index}: {anchor}")
        rows.append({"batch": index, "fit_eligible_power_ratio": float((E[:, :FIT_END].abs() ** 2).sum()
                                                            / (X[:, :FIT_END].abs() ** 2).sum().clamp_min(1e-20)),
                     "valid_anchor": bool(valid.any()), "base_grad": base,
                     "source_grad": source, "anchor_grad": anchor})
        print(f"batch {index}: valid={bool(valid.any())} base={base:.6g} "
              f"source={source:.6g} anchor={anchor}", flush=True)
    ratios_source = [r["base_grad"] / r["source_grad"] for r in rows]
    ratios_anchor = [r["base_grad"] / r["anchor_grad"] for r in rows if r["anchor_grad"]]
    if len(ratios_anchor) < 3:
        raise ValueError("fewer than three valid operator-target batches")
    result = {"rows": rows, "source_weight": float(.20 * np.median(ratios_source)),
              "operator_weight": float(.20 * np.median(ratios_anchor)),
              "neural_mac_per_second": float(net.macs * SN.FPS),
              "parameters": sum(p.numel() for p in net.parameters())}
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("weights:", json.dumps(result), flush=True)


def train(config):
    global _OPERATOR_WEIGHT
    data = json.loads(config.read_text(encoding="utf-8"))
    _OPERATOR_WEIGHT = float(data.pop("operator_weight"))
    data["save_at"] = set(data["save_at"])
    a = SimpleNamespace(**data)
    SN.BandSepNet = AnchorBandSepNet
    SN.loss_audio = wrapped_audio_loss
    try:
        SN.train(a)
    finally:
        SN.BandSepNet = _BASE_BAND_NET
        SN.loss_audio = _BASE_AUDIO_LOSS
    for name in ("last.pt", "step4000.pt", "sep_net.pt"):
        path = Path(a.out) / name
        if path.exists():
            torch = SN._torch()
            ck = torch.load(path, map_location="cpu")
            ck["operator_anchor_version"] = 1
            ck["operator_weight"] = _OPERATOR_WEIGHT
            ck["anchor_tau_seconds"] = TAU_SECONDS
            ck["anchor_fast_fraction"] = FAST_FRACTION
            torch.save(ck, path)


def load(model_path):
    torch = SN._torch()
    device = "cuda" if torch.cuda.is_available() else "cpu"
    ck = torch.load(model_path, map_location=device)
    if ck.get("operator_anchor_version") != 1:
        raise ValueError("not a review-42 operator-anchor checkpoint")
    net = AnchorBandSepNet(torch, feat=ck["feat"], h1=ck["h1"], h2=ck["h2"], hg=ck["bhg"],
                           brnn=ck["brnn"], lookahead=ck["lookahead"], bidir=ck["bidir"],
                           dec_skip=ck["dec_skip"], norm=ck["norm"], mask=ck["mask"]).to(device)
    net.modules.load_state_dict(ck["state"])
    net.modules.eval()
    SN.STATS.update(ck["stats"])
    return torch, net, device


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="command", required=True)
    c = sub.add_parser("calibrate")
    c.add_argument("cache", type=Path)
    c.add_argument("teacher_cache", type=Path)
    c.add_argument("--out", type=Path, required=True)
    t = sub.add_parser("train")
    t.add_argument("--config", type=Path, required=True)
    a = p.parse_args()
    if a.command == "calibrate":
        calibrate(a.cache, a.teacher_cache, a.out)
    else:
        train(a.config)


if __name__ == "__main__":
    main()
