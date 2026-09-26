#!/usr/bin/env python3
"""Per-bin protect/rear mask network at the car's STFT geometry (Phase 0 of
docs/audio/UPMIX_SOURCE_SEPARATION_PLAN.md).

  cache   MoisesDB songs -> 48 kHz int16 waveforms of four groups
          (vocals / drums+percussion / bass / other) + the artist split
  train   per-band GRU context + per-bin MLP -> four sigmoid masks per bin,
          asymmetric loss (protected material sent rear costs 4x), CUDA
  eval    mask-space report on DEV: how much vocal energy the "other" mask
          would send rear vs how much rearable energy it keeps
  export  per-control masks of a DEV excerpt for the renderer's --protect-mask

STFT = FFT 512, hop 128, half-sine window, 48 kHz, exactly hook/audio/hssr_dsp.c,
so the mask never needs a second transform in the DSP.  Runs on the training
PC (Windows, RTX 5080); numpy/scipy/soundfile/torch only.
"""
import argparse
import json
import math
import os
import re
import sys
import time

import numpy as np

SR = 48000
N_FFT = 512
HOP = 128
N_BINS = N_FFT // 2 + 1
CONTROL = 4                                   # the DSP's control cadence, hops
GROUPS = ("vocals", "drums", "bass", "other")
GROUP_OF_STEM = {"vocals": 0, "drums": 1, "percussion": 1, "bass": 2}   # everything else -> other
BAND_EDGES_HZ = [0, 60, 90, 125, 170, 220, 280, 350, 440, 550, 680, 830, 1000, 1200, 1450, 1750,
                 2100, 2500, 3000, 3600, 4300, 5100, 6000, 7000, 8200, 9600, 11200, 13000, 15000,
                 17500, 20000, 22000, 24001]
N_BANDS = 32


def bin_band():
    """Same rounding as hssr_dsp.c: bin frequency = (bin*48000 + 256) // 512."""
    band = np.zeros(N_BINS, np.int64)
    for b in range(N_BINS):
        hz = (b * SR + N_FFT // 2) // N_FFT
        k = 0
        while k + 1 < N_BANDS and hz >= BAND_EDGES_HZ[k + 1]:
            k += 1
        band[b] = k
    return band


# ---------------------------------------------------------------- cache
def cache(a):
    import soundfile as sf
    from scipy.signal import resample_poly
    from concurrent.futures import ProcessPoolExecutor
    os.makedirs(a.out, exist_ok=True)
    tracks = sorted(d for d in os.listdir(a.moisesdb) if os.path.isdir(os.path.join(a.moisesdb, d)))
    # artist-disjoint split, seeded like the q6d one (181/40/19 on 240 songs)
    metas = {t: json.load(open(os.path.join(a.moisesdb, t, "data.json"))) for t in tracks}
    artists = sorted({m["artist"] for m in metas.values()})
    rng = np.random.default_rng(20260911)
    rng.shuffle(artists)
    n = len(artists)
    dev_a = set(artists[: max(1, round(n * 0.17))])
    test_a = set(artists[len(dev_a): len(dev_a) + max(1, round(n * 0.12))])
    split = {t: ("dev" if metas[t]["artist"] in dev_a else "test" if metas[t]["artist"] in test_a else "train")
             for t in tracks}
    json.dump({"split": split, "groups": GROUPS, "sr": SR}, open(os.path.join(a.out, "split.json"), "w"), indent=1)
    jobs = [(os.path.join(a.moisesdb, t), os.path.join(a.out, t + ".npz"), metas[t]) for t in tracks
            if not os.path.exists(os.path.join(a.out, t + ".npz"))]
    print(f"{len(tracks)} songs, {len(jobs)} to cache; split train/dev/test = "
          f"{sum(v == 'train' for v in split.values())}/{sum(v == 'dev' for v in split.values())}/"
          f"{sum(v == 'test' for v in split.values())}")
    with ProcessPoolExecutor(a.jobs) as ex:
        for i, r in enumerate(ex.map(_cache_one, jobs)):
            print(f"  [{i + 1}/{len(jobs)}] {r}", flush=True)


def _cache_one(job):
    import soundfile as sf
    from scipy.signal import resample_poly
    track_dir, out, meta = job
    groups = [None] * 4
    for stem in meta["stems"]:
        g = GROUP_OF_STEM.get(stem["stemName"], 3)
        for t in stem["tracks"]:
            p = os.path.join(track_dir, stem["stemName"], t["id"] + ".wav")
            if not os.path.exists(p):
                continue
            x, sr = sf.read(p, dtype="float32", always_2d=True)
            if x.shape[1] == 1:
                x = np.repeat(x, 2, 1)
            x = x.T
            if groups[g] is None:
                groups[g] = x.copy()
            else:
                m = min(groups[g].shape[1], x.shape[1])
                groups[g] = groups[g][:, :m] + x[:, :m]
    n = min(g.shape[1] for g in groups if g is not None)
    stack = np.zeros((4, 2, n), np.float32)
    for gi, g in enumerate(groups):
        if g is not None:
            stack[gi] = g[:, :n]
    stack = resample_poly(stack, 160, 147, axis=2).astype(np.float32)
    peak = np.abs(stack.sum(0)).max() + 1e-9
    stack *= 0.7 / peak                                  # the harness level
    np.savez(out, wav=(np.clip(stack, -1, 1) * 32767).astype(np.int16),
             present=np.array([g is not None for g in groups]))
    return f"{meta['artist']} - {meta['song']}: {stack.shape[2] / SR:.0f} s"


# ---------------------------------------------------------------- torch side
def _torch():
    import torch
    return torch


def stft(torch, x):
    """x: [..., n] float -> complex [..., frames, 257] with the DSP's half-sine window."""
    win = torch.sin(math.pi * (torch.arange(N_FFT, device=x.device, dtype=x.dtype) + 0.5) / N_FFT)
    shp = x.shape
    X = torch.stft(x.reshape(-1, shp[-1]), N_FFT, HOP, N_FFT, win, center=False, return_complex=True)
    return X.reshape(*shp[:-1], N_BINS, -1).transpose(-1, -2)


def features(torch, L, R):
    """Per-bin [log|L|^2, log|R|^2, cos IPD, sin IPD] and per-band context [32 x 5]."""
    pl = (L.real ** 2 + L.imag ** 2)
    pr = (R.real ** 2 + R.imag ** 2)
    cross = L * torch.conj(R)
    mag = torch.sqrt(pl * pr) + 1e-9
    f = torch.stack([torch.log(pl + 1e-7), torch.log(pr + 1e-7), cross.real / mag, cross.imag / mag], -1)
    return f


class MaskNet:
    """Per-band GRU context + per-bin MLP.  ~0.18 M MAC per control."""

    def __init__(self, torch, band, ctx=64, proj=8, hidden=32):
        nn = torch.nn
        self.torch = torch
        self.band = torch.as_tensor(band)
        bins_per_band = torch.zeros(N_BANDS).scatter_add_(0, self.band, torch.ones(N_BINS))
        self.pool = torch.zeros(N_BANDS, N_BINS).scatter_(0, self.band[None], torch.ones(1, N_BINS))
        self.pool = self.pool / bins_per_band.clamp(min=1)[:, None]
        self.gru = nn.GRU(N_BANDS * 5, ctx, batch_first=True)
        self.ctx_proj = nn.Linear(ctx, N_BANDS * proj)
        self.mlp = nn.Sequential(nn.Linear(4 + proj + 1, hidden), nn.Tanh(), nn.Linear(hidden, 4))
        self.proj = proj
        self.modules = nn.ModuleList([self.gru, self.ctx_proj, self.mlp])
        self.pos = (torch.arange(N_BINS, dtype=torch.float32) / N_BINS)

    def to(self, dev):
        self.modules.to(dev); self.band = self.band.to(dev); self.pool = self.pool.to(dev); self.pos = self.pos.to(dev)
        return self

    def parameters(self):
        return self.modules.parameters()

    def forward(self, f):
        """f: [B, T, 257, 4] at control cadence -> masks [B, T, 257, 4] in (0,1)."""
        torch = self.torch
        B, T = f.shape[:2]
        e = f[..., :2].mean(-1)                                   # log energy per bin
        bandf = torch.einsum("kn,btnf->btkf", self.pool, f)       # [B,T,32,4]
        bande = torch.einsum("kn,btn->btk", self.pool, e)[..., None]
        ctx_in = torch.cat([bandf, bande], -1).reshape(B, T, -1)
        h, _ = self.gru(ctx_in)                                    # [B,T,ctx]
        c = self.ctx_proj(h).reshape(B, T, N_BANDS, self.proj)    # [B,T,32,proj]
        cb = c[:, :, self.band]                                   # [B,T,257,proj]
        x = torch.cat([f, cb, self.pos.expand(B, T, N_BINS)[..., None]], -1)
        return torch.sigmoid(self.mlp(x))


def master(torch, mix, rng):
    """Cheap 'mastering': gain, tilt, soft limiter.  mix [B,2,n]."""
    B = mix.shape[0]
    g = 10 ** (torch.empty(B, 1, 1, device=mix.device).uniform_(0, 9) / 20)
    y = mix * g
    return torch.tanh(y * 1.2) / 1.2                              # soft knee near 0.8


def load_split(cache_dir):
    s = json.load(open(os.path.join(cache_dir, "split.json")))["split"]
    return {k: [t for t, v in s.items() if v == k and os.path.exists(os.path.join(cache_dir, t + ".npz"))]
            for k in ("train", "dev", "test")}


def batch_windows(torch, cache_dir, tracks, seconds, batch, rng, dev):
    n = int(seconds * SR)
    wavs = []
    for _ in range(batch):
        t = tracks[rng.integers(len(tracks))]
        z = np.load(os.path.join(cache_dir, t + ".npz"))
        w = z["wav"]
        if w.shape[2] <= n:
            continue
        s = rng.integers(0, w.shape[2] - n)
        wavs.append(w[:, :, s:s + n])
    x = torch.as_tensor(np.stack(wavs), device=dev).float() / 32768   # [B,4,2,n]
    return x


def targets_and_features(torch, x, rng, augment):
    """x [B,4,2,n] group waveforms -> f [B,Tc,257,4], irm [B,Tc,257,4], weight [B,Tc,257]."""
    B = x.shape[0]
    if augment:
        gains = 10 ** (torch.empty(B, 4, 1, 1, device=x.device).uniform_(-6, 6) / 20)
        x = x * gains
    mix = x.sum(1)                                                # [B,2,n]
    if augment:
        mix_in = master(torch, mix, rng)
    else:
        mix_in = mix
    X = stft(torch, mix_in)                                       # [B,2,T,257]
    G = stft(torch, x)                                            # [B,4,2,T,257]
    P = (G.real ** 2 + G.imag ** 2).sum(2)                        # [B,4,T,257]
    irm = P / (P.sum(1, keepdim=True) + 1e-9)
    f = features(torch, X[:, 0], X[:, 1])                         # [B,T,257,4]
    T = (f.shape[1] // CONTROL) * CONTROL
    f = f[:, CONTROL - 1:T:CONTROL]                               # the control hop's features
    irm = irm[:, :, :T].reshape(B, 4, T // CONTROL, CONTROL, N_BINS).mean(3).permute(0, 2, 3, 1)
    Pm = P.sum(1)[:, :T].reshape(B, T // CONTROL, CONTROL, N_BINS).mean(2)
    weight = torch.clamp(torch.log10(Pm + 1e-9) + 6, 0, 6) / 6    # salience: silent cells count nothing
    return f, irm, weight


def loss_fn(torch, m, irm, w):
    """Asymmetric BCE: protected energy sent rear costs 4x; + temporal smoothness."""
    protect = irm[..., 0] + irm[..., 1] + irm[..., 2]              # vocal+drums+bass share of the cell
    bce = -(irm * torch.log(m + 1e-6) + (1 - irm) * torch.log(1 - m + 1e-6))   # [B,T,257,4]
    per_cell = bce.mean(-1) * (1 + 3 * protect)
    # the "other" mask on a protected cell is the leak we care about most
    leak = m[..., 3] * protect
    smooth = (m[:, 1:] - m[:, :-1]).abs().mean(-1)
    core = (per_cell * w).sum() / (w.sum() + 1e-9) + 2.0 * (leak * w).sum() / (w.sum() + 1e-9)
    return core + 0.05 * (smooth * w[:, 1:]).sum() / (w[:, 1:].sum() + 1e-9)


def mask_space_metrics(torch, m, irm, w):
    """Energy-weighted: vocal energy the 'other' mask would send rear; rearable energy it keeps."""
    Pv = irm[..., 0] * w; Po = irm[..., 3] * w; Pd = irm[..., 1] * w
    return {"vocal_to_rear": float((Pv * m[..., 3]).sum() / (Pv.sum() + 1e-9)),
            "drums_to_rear": float((Pd * m[..., 3]).sum() / (Pd.sum() + 1e-9)),
            "other_kept_rear": float((Po * m[..., 3]).sum() / (Po.sum() + 1e-9)),
            "irm_mae": float(((m - irm).abs().mean(-1) * w).sum() / (w.sum() + 1e-9))}


def train(a):
    torch = _torch()
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    torch.manual_seed(a.seed)
    rng = np.random.default_rng(a.seed)
    split = load_split(a.cache)
    print(f"device {dev}; train {len(split['train'])} dev {len(split['dev'])} songs", flush=True)
    net = MaskNet(torch, bin_band(), ctx=a.ctx, proj=a.proj, hidden=a.hidden).to(dev)
    n_par = sum(p.numel() for p in net.parameters())
    macs = 3 * (N_BANDS * 5 * a.ctx + a.ctx * a.ctx) + a.ctx * N_BANDS * a.proj + N_BINS * ((4 + a.proj + 1) * a.hidden + a.hidden * 4)
    print(f"params {n_par}, ~{macs} MAC per control = {macs * SR / HOP / CONTROL / 1e6:.1f} M MAC/s", flush=True)
    opt = torch.optim.AdamW(net.parameters(), lr=a.lr, weight_decay=1e-4)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, a.steps)
    os.makedirs(a.out, exist_ok=True)
    best = 1e9
    t0 = time.time()
    for step in range(1, a.steps + 1):
        x = batch_windows(torch, a.cache, split["train"], a.seconds, a.batch, rng, dev)
        with torch.no_grad():
            f, irm, w = targets_and_features(torch, x, rng, augment=True)
        m = net.forward(f)
        loss = loss_fn(torch, m, irm, w)
        opt.zero_grad(); loss.backward()
        torch.nn.utils.clip_grad_norm_(net.parameters(), 2.0)
        opt.step(); sched.step()
        if step % a.eval_every == 0 or step == a.steps:
            met = evaluate(torch, net, a.cache, split["dev"], dev, a.seconds, songs=a.eval_songs)
            line = f"step {step} loss {loss.item():.4f} dev " + " ".join(f"{k}={v:.3f}" for k, v in met.items()) + f"  {time.time() - t0:.0f}s"
            print(line, flush=True)
            score = met["vocal_to_rear"] * 4 + (1 - met["other_kept_rear"]) + met["irm_mae"]
            if score < best:
                best = score
                torch.save({"state": net.modules.state_dict(), "ctx": a.ctx, "proj": a.proj, "hidden": a.hidden,
                            "step": step, "dev": met}, os.path.join(a.out, "mask_net.pt"))
                with open(os.path.join(a.out, "best.txt"), "w") as fh:
                    fh.write(line + "\n")
    print("best", open(os.path.join(a.out, "best.txt")).read().strip())


def evaluate(torch, net, cache_dir, tracks, dev, seconds, songs=20):
    net.modules.eval()
    acc = {}
    rng = np.random.default_rng(0)
    with torch.no_grad():
        for t in tracks[:songs]:
            z = np.load(os.path.join(cache_dir, t + ".npz"))
            w = z["wav"]
            n = int(seconds * SR)
            if w.shape[2] <= n:
                continue
            # the loudest-vocal window, like the harness
            v = w[0].astype(np.float32)
            e = np.cumsum((v ** 2).sum(0)); idx = np.arange(0, w.shape[2] - n, 5 * SR)
            s = idx[int(np.argmax(e[idx + n - 1] - e[idx]))] if len(idx) else 0
            x = torch.as_tensor(w[None, :, :, s:s + n], device=dev).float() / 32768
            f, irm, wt = targets_and_features(torch, x, rng, augment=False)
            m = net.forward(f)
            for k, val in mask_space_metrics(torch, m, irm, wt).items():
                acc.setdefault(k, []).append(val)
    net.modules.train()
    return {k: float(np.median(v)) for k, v in acc.items()}


def export(a):
    """Write the per-control 'protect' mask (1 - m_other) of one DEV excerpt for --protect-mask."""
    torch = _torch()
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    ck = torch.load(a.model, map_location=dev)
    net = MaskNet(torch, bin_band(), ctx=ck["ctx"], proj=ck["proj"], hidden=ck["hidden"]).to(dev)
    net.modules.load_state_dict(ck["state"]); net.modules.eval()
    import soundfile as sf
    x, sr = sf.read(a.wav, dtype="float32", always_2d=True)
    assert sr == SR and x.shape[1] == 2
    xt = torch.as_tensor(x.T[None], device=dev)
    with torch.no_grad():
        X = stft(torch, xt)
        f = features(torch, X[:, 0], X[:, 1])
        T = (f.shape[1] // CONTROL) * CONTROL
        m = net.forward(f[:, CONTROL - 1:T:CONTROL])[0]            # [Tc,257,4]
    prot = (1 - m[..., 3]).cpu().numpy().astype(np.float32)
    with open(a.out, "wb") as fh:
        fh.write(b"HSSRMSK0"); fh.write(np.array([prot.shape[0], N_BINS], np.uint32).tobytes()); fh.write(prot.tobytes())
    print(f"{a.out}: {prot.shape[0]} controls x {N_BINS} bins, mean protect {prot.mean():.3f}")


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("cache"); c.add_argument("--moisesdb", required=True); c.add_argument("--out", required=True); c.add_argument("--jobs", type=int, default=6)
    t = sub.add_parser("train"); t.add_argument("--cache", required=True); t.add_argument("--out", required=True)
    t.add_argument("--steps", type=int, default=3000); t.add_argument("--batch", type=int, default=16); t.add_argument("--seconds", type=float, default=6.0)
    t.add_argument("--lr", type=float, default=1e-3); t.add_argument("--seed", type=int, default=1)
    t.add_argument("--ctx", type=int, default=64); t.add_argument("--proj", type=int, default=8); t.add_argument("--hidden", type=int, default=32)
    t.add_argument("--eval-every", type=int, default=250); t.add_argument("--eval-songs", type=int, default=20)
    e = sub.add_parser("export"); e.add_argument("--model", required=True); e.add_argument("--wav", required=True); e.add_argument("--out", required=True)
    a = ap.parse_args()
    {"cache": cache, "train": train, "export": export}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
