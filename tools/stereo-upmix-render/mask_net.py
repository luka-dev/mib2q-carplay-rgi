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
# Cached groups (v2, 2026-09-25 after the Codex review): keep the musical roles
# apart in the cache and MERGE at training time, so a grouping change never
# rebuilds 65 GB.  The original (stemName, trackType) labels are kept per song.
GROUPS = ("lead_vocal", "other_vocal", "drum_core", "cymbals", "percussion", "bass", "other")
# First model (conservative): vocals whole and in front, percussion with the
# drum core in front (congas/claps anchor the foreground), only cymbals/hi-hat
# and "other" may go rear.
DEFAULT_MERGE = "vocals=lead_vocal+other_vocal;drum_core=drum_core+percussion;cymbals=cymbals;bass=bass;other=other"
PROTECT = ("vocals", "drum_core", "bass")          # never rear
ELIGIBLE = ("other", "cymbals")                     # rear candidates (the DSP's spatial gates decide how much)


def group_of(stem_name, track_type):
    t = (track_type or "").lower()
    if stem_name == "vocals":
        return 0 if "lead" in t else 1
    if stem_name == "drums":
        return 3 if any(k in t for k in ("hi_hat", "hihat", "cymbal", "ride", "crash", "overhead")) else 2
    if stem_name == "percussion":
        return 4
    if stem_name == "bass":
        return 5
    return 6


def parse_merge(spec):
    """'name=g1+g2;...' -> (names, index lists into GROUPS)."""
    names, idx = [], []
    for part in spec.split(";"):
        name, members = part.split("=")
        names.append(name)
        idx.append([GROUPS.index(m) for m in members.split("+")])
    return names, idx
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
    jobs = [(os.path.join(a.moisesdb, t), os.path.join(a.out, t + ".npy"), metas[t]) for t in tracks
            if not os.path.exists(os.path.join(a.out, t + ".npy"))]
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
    groups = [None] * len(GROUPS)
    labels = []
    for stem in meta["stems"]:
        for t in stem["tracks"]:
            g = group_of(stem["stemName"], t.get("trackType"))
            p = os.path.join(track_dir, stem["stemName"], t["id"] + ".wav")
            if not os.path.exists(p):
                continue
            labels.append({"stem": stem["stemName"], "type": t.get("trackType"), "group": GROUPS[g]})
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
    stack = np.zeros((len(GROUPS), 2, n), np.float32)
    for gi, g in enumerate(groups):
        if g is not None:
            stack[gi] = g[:, :n]
    stack = resample_poly(stack, 160, 147, axis=2).astype(np.float32)
    peak = np.abs(stack.sum(0)).max() + 1e-9
    stack *= 0.7 / peak                                  # the harness level
    np.save(out, (np.clip(stack, -1, 1) * 32767).astype(np.int16))     # [7, 2, n] int16, mmap-able
    json.dump({"present": [g is not None for g in groups], "labels": labels, "artist": meta["artist"],
               "song": meta["song"], "genre": meta.get("genre")}, open(out[:-4] + ".json", "w"))
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


N_FFT_FINE = 2048           # mask-net-only analysis: 23.4 Hz bins, hop 512 = the control cadence
N_BINS_FINE = N_FFT_FINE // 2 + 1


def stft_fine(torch, x):
    """x [..., n] -> complex [..., frames, 1025] at hop 512 (= one DSP control), Hann, centred."""
    win = torch.hann_window(N_FFT_FINE, device=x.device, dtype=x.dtype)
    shp = x.shape
    X = torch.stft(x.reshape(-1, shp[-1]), N_FFT_FINE, HOP * CONTROL, N_FFT_FINE, win, center=True, return_complex=True)
    return X.reshape(*shp[:-1], N_BINS_FINE, -1).transpose(-1, -2)


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

    def __init__(self, torch, band, ctx=64, proj=8, hidden=32, n_out=4, neigh=0):
        nn = torch.nn
        self.torch = torch
        self.band = torch.as_tensor(band)
        bins_per_band = torch.zeros(N_BANDS).scatter_add_(0, self.band, torch.ones(N_BINS))
        self.pool = torch.zeros(N_BANDS, N_BINS).scatter_(0, self.band[None], torch.ones(1, N_BINS))
        self.pool = self.pool / bins_per_band.clamp(min=1)[:, None]
        self.gru = nn.GRU(N_BANDS * 5, ctx, batch_first=True)
        self.ctx_proj = nn.Linear(ctx, N_BANDS * proj)
        self.neigh = neigh                                   # +-neigh bins of per-bin features as extra inputs
        self.mlp = nn.Sequential(nn.Linear(4 * (1 + 2 * neigh) + proj + 1, hidden), nn.Tanh(), nn.Linear(hidden, n_out))
        self.n_out = n_out
        self.proj = proj
        self.modules = nn.ModuleList([self.gru, self.ctx_proj, self.mlp])
        self.pos = (torch.arange(N_BINS, dtype=torch.float32) / N_BINS)

    def to(self, dev):
        self.modules.to(dev); self.band = self.band.to(dev); self.pool = self.pool.to(dev); self.pos = self.pos.to(dev)
        return self

    def parameters(self):
        return self.modules.parameters()

    def forward(self, f):
        """f: [B, T, 257, 4] at control cadence -> masks [B, T, 257, n_out] in (0,1)."""
        torch = self.torch
        f = standardize(torch, f).clamp(-6, 6)
        B, T = f.shape[:2]
        e = f[..., :2].mean(-1)                                   # log energy per bin
        bandf = torch.einsum("kn,btnf->btkf", self.pool, f)       # [B,T,32,4]
        bande = torch.einsum("kn,btn->btk", self.pool, e)[..., None]
        ctx_in = torch.cat([bandf, bande], -1).reshape(B, T, -1)
        h, _ = self.gru(ctx_in)                                    # [B,T,ctx]
        c = self.ctx_proj(h).reshape(B, T, N_BANDS, self.proj)    # [B,T,32,proj]
        cb = c[:, :, self.band]                                   # [B,T,257,proj]
        parts = [f]
        for d in range(1, self.neigh + 1):                        # a partial's neighbours: frequency context
            parts.append(torch.cat([f[:, :, d:], f[:, :, -1:].expand(B, T, d, 4)], 2))
            parts.append(torch.cat([f[:, :, :1].expand(B, T, d, 4), f[:, :, :-d]], 2))
        x = torch.cat(parts + [cb, self.pos.expand(B, T, N_BINS)[..., None]], -1)
        return torch.sigmoid(self.mlp(x))

    def _logits(self, f):
        torch = self.torch
        f = standardize(torch, f).clamp(-6, 6)
        B, T = f.shape[:2]
        e = f[..., :2].mean(-1)
        bandf = torch.einsum("kn,btnf->btkf", self.pool, f)
        bande = torch.einsum("kn,btn->btk", self.pool, e)[..., None]
        h, _ = self.gru(torch.cat([bandf, bande], -1).reshape(B, T, -1))
        cb = self.ctx_proj(h).reshape(B, T, N_BANDS, self.proj)[:, :, self.band]
        parts = [f]
        for d in range(1, self.neigh + 1):
            parts.append(torch.cat([f[:, :, d:], f[:, :, -1:].expand(B, T, d, 4)], 2))
            parts.append(torch.cat([f[:, :, :1].expand(B, T, d, 4), f[:, :, :-d]], 2))
        return self.mlp(torch.cat(parts + [cb, self.pos.expand(B, T, N_BINS)[..., None]], -1))


class TFNet(MaskNet):
    """v5: the per-bin MLP cannot tell a vocal partial from a guitar partial
    from four numbers and a band context (v2-v4 protected everything).  Give
    every bin its own memory and its harmonic neighbourhood: a bidirectional
    GRU across the 257 bins per control (harmonic structure) and a causal GRU
    over time per bin (vibrato, syllables vs held partials), weights shared
    across bins.  ~1.9 M MAC per control at hidden 16 (Q3 was 3.9 M/frame at
    4x the frame rate)."""

    def __init__(self, torch, band, ctx=64, proj=8, hidden=16, n_out=4, neigh=2):
        MaskNet.__init__(self, torch, band, ctx=ctx, proj=proj, hidden=hidden, n_out=n_out, neigh=neigh)
        nn = torch.nn
        n_in = 4 * (1 + 2 * neigh) + proj + 1
        self.inp = nn.Linear(n_in, 24)
        self.freq = nn.GRU(24, hidden, batch_first=True, bidirectional=True)
        self.time = nn.GRU(2 * hidden, hidden, batch_first=True)
        self.head = nn.Linear(3 * hidden, n_out)
        self.modules = nn.ModuleList([self.gru, self.ctx_proj, self.inp, self.freq, self.time, self.head])
        self.hidden = hidden

    def forward(self, f):
        return self.torch.sigmoid(self._logits(f))

    def _logits_tf(self, f):
        torch = self.torch
        f = standardize(torch, f).clamp(-6, 6)
        B, T = f.shape[:2]
        e = f[..., :2].mean(-1)
        bandf = torch.einsum("kn,btnf->btkf", self.pool, f)
        bande = torch.einsum("kn,btn->btk", self.pool, e)[..., None]
        h, _ = self.gru(torch.cat([bandf, bande], -1).reshape(B, T, -1))
        cb = self.ctx_proj(h).reshape(B, T, N_BANDS, self.proj)[:, :, self.band]
        parts = [f]
        for d in range(1, self.neigh + 1):
            parts.append(torch.cat([f[:, :, d:], f[:, :, -1:].expand(B, T, d, 4)], 2))
            parts.append(torch.cat([f[:, :, :1].expand(B, T, d, 4), f[:, :, :-d]], 2))
        x = torch.tanh(self.inp(torch.cat(parts + [cb, self.pos.expand(B, T, N_BINS)[..., None]], -1)))   # [B,T,257,24]
        fq, _ = self.freq(x.reshape(B * T, N_BINS, -1))                                   # over bins
        fq = fq.reshape(B, T, N_BINS, 2 * self.hidden)
        tm, _ = self.time(fq.permute(0, 2, 1, 3).reshape(B * N_BINS, T, 2 * self.hidden))  # over time, per bin
        tm = tm.reshape(B, N_BINS, T, self.hidden).permute(0, 2, 1, 3)
        return self.head(torch.cat([fq, tm], -1))

    def _logits(self, f):
        return self._logits_tf(f)


class ConvNet:
    """v8: the fine 2048-point analysis (23 Hz bins) seen through a frequency
    convolution with stride 4, so every DSP bin's decision is made from the
    harmonic structure of its 187 Hz neighbourhood at 23 Hz resolution, plus a
    small causal GRU per bin over time.  ~0.6 M MAC per control (~55 M MAC/s)
    on top of one 2048-point FFT per control."""

    FINE = True

    def __init__(self, torch, band, ch=16, hidden=8, n_out=5, **_):
        nn = torch.nn
        self.torch = torch
        self.c1 = nn.Conv1d(4, ch, kernel_size=8, stride=4, padding=4)    # 1025 -> 257
        self.c2 = nn.Conv1d(ch, ch, kernel_size=5, padding=2)
        self.c3 = nn.Conv1d(ch, ch, kernel_size=5, padding=2)
        self.time = nn.GRU(ch, hidden, batch_first=True)
        self.head = nn.Linear(ch + hidden, n_out)
        self.modules = nn.ModuleList([self.c1, self.c2, self.c3, self.time, self.head])
        self.hidden = hidden; self.n_out = n_out; self.ch = ch

    def to(self, dev):
        self.modules.to(dev); return self

    def parameters(self):
        return self.modules.parameters()

    def _logits(self, f):
        """f [B, T, 1025, 4] fine features -> logits [B, T, 257, n_out]."""
        torch = self.torch
        f = standardize(torch, f).clamp(-6, 6)
        B, T = f.shape[:2]
        x = f.reshape(B * T, N_BINS_FINE, 4).transpose(1, 2)              # [BT, 4, 1025]
        x = torch.tanh(self.c1(x))[:, :, :N_BINS]                          # [BT, ch, 257]
        x = torch.tanh(self.c2(x) + x)
        x = torch.tanh(self.c3(x) + x)
        x = x.transpose(1, 2).reshape(B, T, N_BINS, self.ch)               # [B, T, 257, ch]
        tm, _ = self.time(x.permute(0, 2, 1, 3).reshape(B * N_BINS, T, self.ch))
        tm = tm.reshape(B, N_BINS, T, self.hidden).permute(0, 2, 1, 3)
        return self.head(torch.cat([x, tm], -1))

    def forward(self, f):
        return self.torch.sigmoid(self._logits(f))


def probe_descriptor(torch, f, neigh=2):
    """The probe's per-bin descriptor from the 512 features [B,T,257,4]:
    log power (L+R) of the +-neigh bins relative to their local mean, the local
    mean, cos/sin IPD, bin position -> [B,T,257, 2*neigh+1 + 1 + 2 + 1]."""
    B, T = f.shape[:2]
    lp = torch.log(torch.exp(f[..., 0]) + torch.exp(f[..., 1]))                # [B,T,257]
    pad = torch.nn.functional.pad(lp, (neigh, neigh), mode="replicate")
    idx = torch.arange(N_BINS, device=f.device) + neigh
    offs = torch.arange(-neigh, neigh + 1, device=f.device)
    desc = pad[..., (idx[:, None] + offs[None])]                                # [B,T,257,2n+1]
    mean = desc.mean(-1, keepdim=True)
    pos = (torch.arange(N_BINS, device=f.device).float() / N_BINS).expand(B, T, N_BINS)[..., None]
    return torch.cat([desc - mean, mean, f[..., 2:4], pos], -1)


class ProbeNet:
    """v10: exactly the probe's recipe inside the training pipeline (astra
    review 3): the descriptor above, a 64-64 tanh MLP per bin, ONE eligibility
    logit, plain BCE on owned cells, no ambiguous term, no contamination
    penalty, no clamp.  Reproduce the probe's DEV AUC first; extend after."""

    SINGLE = True

    def __init__(self, torch, band, hidden=64, neigh=2, n_out=1, **_):
        nn = torch.nn
        self.torch = torch
        self.neigh = neigh
        n_in = 2 * neigh + 1 + 1 + 2 + 1
        self.mlp = nn.Sequential(nn.Linear(n_in, hidden), nn.Tanh(), nn.Linear(hidden, hidden), nn.Tanh(), nn.Linear(hidden, n_out))
        self.modules = nn.ModuleList([self.mlp])
        self.n_out = n_out
        self.SINGLE = n_out == 1

    def to(self, dev):
        self.modules.to(dev); return self

    def parameters(self):
        return self.modules.parameters()

    def _logits(self, f):
        d = standardize(self.torch, probe_descriptor(self.torch, f, self.neigh)).clamp(-6, 6)
        return self.mlp(d)

    def forward(self, f):
        lg = self._logits(f)
        return self.torch.sigmoid(lg) if self.n_out == 1 else self.torch.softmax(lg, -1)


def build_net(torch, arch, band, **kw):
    if arch == "probe":
        return ProbeNet(torch, band, hidden=kw.get("hidden", 64), neigh=kw.get("neigh", 2), n_out=kw.get("n_out", 1))
    if arch == "probe_roles":
        return ProbeNet(torch, band, hidden=kw.get("hidden", 64), neigh=kw.get("neigh", 2), n_out=kw["n_out"])
    if arch == "conv2048":
        return ConvNet(torch, band, ch=kw.get("ctx", 16), hidden=kw.get("hidden", 8), n_out=kw.get("n_out", 5))
    return (TFNet if arch == "tf" else MaskNet)(torch, band, **kw)


def net_is_fine(net):
    return getattr(net, "FINE", False)


def master(torch, mix, rng):
    """Cheap 'mastering': gain, tilt, soft limiter.  mix [B,2,n]."""
    B = mix.shape[0]
    g = 10 ** (torch.empty(B, 1, 1, device=mix.device).uniform_(0, 6) / 20)
    y = mix * g
    return torch.tanh(y * 1.2) / 1.2                              # soft knee near 0.8


def load_split(cache_dir):
    s = json.load(open(os.path.join(cache_dir, "split.json")))["split"]
    return {k: [t for t, v in s.items() if v == k and os.path.exists(os.path.join(cache_dir, t + ".npy"))]
            for k in ("train", "dev", "test")}


def merge_groups(w, merge_idx):
    """[7, 2, n] cached groups -> [G, 2, n] merged (numpy, int16 sums in int32)."""
    return np.stack([w[idx].astype(np.int32).sum(0) for idx in merge_idx]).astype(np.float32)


def batch_windows(torch, cache_dir, tracks, seconds, batch, rng, dev, merge_idx):
    n = int(seconds * SR)
    wavs = []
    for _ in range(batch):
        t = tracks[rng.integers(len(tracks))]
        w = np.load(os.path.join(cache_dir, t + ".npy"), mmap_mode="r")   # reads only the window
        if w.shape[2] <= n:
            continue
        s = rng.integers(0, w.shape[2] - n)
        wavs.append(merge_groups(np.asarray(w[:, :, s:s + n]), merge_idx))
    return torch.as_tensor(np.stack(wavs), device=dev) / 32768         # [B,G,2,n]


TARGET_HOP = "mean"   # "mean" = IRM averaged over the control's 4 hops; "single" = the control hop only (aligned with the features)


def targets_and_features(torch, x, rng, augment, fine=False):
    """x [B,G,2,n] group waveforms -> f [B,Tc,257,4], irm [B,Tc,257,G], weight [B,Tc,257], power [B,Tc,257,G]."""
    B, G = x.shape[:2]
    if augment:
        gains = 10 ** (torch.empty(B, G, 1, 1, device=x.device).uniform_(-6, 6) / 20)
        x = x * gains
    mix = x.sum(1)                                                # [B,2,n]
    if augment:
        mix_in = master(torch, mix, rng)
    else:
        mix_in = mix
    X = stft(torch, mix_in)                                       # [B,2,T,257]
    Gs = stft(torch, x)                                           # [B,G,2,T,257]
    P = (Gs.real ** 2 + Gs.imag ** 2).sum(2)                      # [B,G,T,257]
    irm = P / (P.sum(1, keepdim=True) + 1e-9)
    f = features(torch, X[:, 0], X[:, 1])                         # [B,T,257,4]
    T = (f.shape[1] // CONTROL) * CONTROL
    f = f[:, CONTROL - 1:T:CONTROL]                               # the control hop's features
    if fine:
        Xf = stft_fine(torch, mix_in)                             # [B,2,Tf,1025], frame k centred on k*512
        ff = features(torch, Xf[:, 0], Xf[:, 1])[:, 1:]           # frame k+1 ~ the DSP's control k (hop 4k-1 ends at 512k+384)
        n_t = min(ff.shape[1], f.shape[1])
        f = ff[:, :n_t]
        f_cut = n_t
    else:
        f_cut = f.shape[1]
    if TARGET_HOP == "single":
        irm = irm[:, :, CONTROL - 1:T:CONTROL].permute(0, 2, 3, 1)
        Pc = P[:, :, CONTROL - 1:T:CONTROL].permute(0, 2, 3, 1)
    else:
        irm = irm[:, :, :T].reshape(B, G, T // CONTROL, CONTROL, N_BINS).mean(3).permute(0, 2, 3, 1)
        Pc = P[:, :, :T].reshape(B, G, T // CONTROL, CONTROL, N_BINS).mean(3).permute(0, 2, 3, 1)   # [B,Tc,257,G]
    irm, Pc = irm[:, :f_cut], Pc[:, :f_cut]
    Pm = Pc.sum(-1)
    weight = torch.clamp(torch.log10(Pm + 1e-9) + 6, 0, 6) / 6    # salience: silent cells count nothing
    return f, irm, weight, Pc


class Roles:
    """Indices of the protected / rear-eligible outputs for a merge config."""

    def __init__(self, names):
        self.names = names
        self.protect = [names.index(n) for n in PROTECT if n in names]
        self.eligible = [names.index(n) for n in ELIGIBLE if n in names]
        self.vocals = names.index("vocals") if "vocals" in names else names.index("lead_vocal")


def rear_eligibility(m, roles):
    """What the DSP may send rear: the eligible masks summed, bounded to 1 (or
    the single eligibility output of a ProbeNet)."""
    if m.shape[-1] == 1:
        return m[..., 0]
    return torch_clamp_sum(m, roles.eligible)


def torch_clamp_sum(m, idx):
    return m[..., idx].sum(-1).clamp(max=1.0)


# Input standardisation (v9).  The probe showed the cells are separable (DEV
# AUC 0.81 on the local spectrum alone) while every mask net produced the same
# eligibility in vocal- and other-owned cells: the nets saw raw log powers
# (-16..+8) through tanh/GRU gates and saturated.  The C runtime standardises
# its features too (hssr_neon.c).  Stats are measured on the first training
# batches, frozen, and saved in the checkpoint.
FEAT_STATS = {"mean": None, "std": None}


def standardize(torch, f):
    if FEAT_STATS["mean"] is None:
        return f
    mu = torch.as_tensor(FEAT_STATS["mean"], device=f.device, dtype=f.dtype)
    sd = torch.as_tensor(FEAT_STATS["std"], device=f.device, dtype=f.dtype)
    return (f - mu) / sd


def fit_feat_stats(torch, f):
    """f [B,T,bins,4] -> per-feature mean/std (over batch, time, bins), clamped ±6 at use."""
    flat = f.reshape(-1, f.shape[-1])
    FEAT_STATS["mean"] = flat.mean(0).tolist()
    FEAT_STATS["std"] = (flat.std(0) + 1e-3).tolist()


LOSS_W = {"leak": 3.0, "leakp": 1.0, "missed": 2.0}   # overridden by --w-leak/--w-leakp/--w-missed
LOSS_MODE = "own"     # "own" = v4 ownership loss on probabilities; "balanced" = v7 (Codex review 2)


def loss_probe(torch, logits, irm, w, roles):
    """One eligibility logit [B,T,257,1]; positives = cells an eligible group owns
    (> 0.6 of the power), negatives = cells a protected group owns; each side
    averaged (the probe's balanced sampling); nothing else."""
    valid = (w > 0.15).float()
    elig_t = irm[..., roles.eligible].sum(-1)
    prot_t = irm[..., roles.protect].sum(-1)
    pos = (elig_t > 0.6).float() * valid
    neg = (prot_t > 0.6).float() * valid
    bce = torch.nn.functional.binary_cross_entropy_with_logits(logits[..., 0], (elig_t > 0.6).float(), reduction="none")
    return (bce * pos).sum() / (pos.sum() + 1.0) + (bce * neg).sum() / (neg.sum() + 1.0)


def loss_softmax(torch, logits, irm, w, P, roles):
    """astra review 3, point 4: roles as a softmax over fractional stem-power
    targets (the IRMs of a cell sum to 1), soft-target cross-entropy weighted
    by salience.  Eligibility = p_other + p_cymbals, no clamp needed."""
    valid = (w > 0.15).float()
    logp = torch.log_softmax(logits, -1)
    ce = -(irm * logp).sum(-1)
    return (ce * valid).sum() / (valid.sum() + 1.0)


def dev_auc_vo(torch, score, irm, w, roles):
    """The probe's AUC: vocal-owned vs other-owned cells on the eligibility score."""
    valid = w > 0.15
    io = roles.names.index("other")
    pos = score[(irm[..., io] > 0.6) & valid]
    neg = score[(irm[..., roles.vocals] > 0.6) & valid]
    if len(pos) == 0 or len(neg) == 0:
        return float("nan")
    pos = pos[torch.randperm(len(pos), device=pos.device)[:4000]]; neg = neg[torch.randperm(len(neg), device=neg.device)[:4000]]
    return float((pos[:, None] > neg[None, :]).float().mean())


def dev_auc(torch, logits, irm, w, roles):
    """AUC of the eligibility logit, eligible-owned vs protected-owned cells."""
    valid = w > 0.15
    sc = logits[..., 0] if logits.shape[-1] == 1 else rear_eligibility(torch.softmax(logits, -1), roles)
    pos = sc[(irm[..., roles.eligible].sum(-1) > 0.6) & valid]
    neg = sc[(irm[..., roles.protect].sum(-1) > 0.6) & valid]
    if len(pos) == 0 or len(neg) == 0:
        return float("nan")
    pos = pos[torch.randperm(len(pos), device=pos.device)[:4000]]; neg = neg[torch.randperm(len(neg), device=neg.device)[:4000]]
    return float((pos[:, None] > neg[None, :]).float().mean())


def loss_balanced(torch, logits, irm, w, P, roles, ramp):
    """v7 (Codex review 2): per-output BCE-with-logits on CONFIDENT cells only
    (owned > 0.6 = positive, < 0.2 = negative), positives and negatives of every
    output normalised separately so the sparse eligible cells weigh as much as
    the plentiful protected ones; a soft regression on the ambiguous cells at
    low weight; silent cells excluded by a validity mask.  The contamination
    penalty (vocal energy admitted rear in vocal-owned cells) is ramped in by
    `ramp` (0..1) only after support is established."""
    valid = (w > 0.15).float()
    bce = torch.nn.functional.binary_cross_entropy_with_logits(logits, irm, reduction="none")   # [B,T,257,G]
    total = 0.0
    for g in range(irm.shape[-1]):
        pos = (irm[..., g] > 0.6).float() * valid
        neg = (irm[..., g] < 0.2).float() * valid
        total = total + (bce[..., g] * pos).sum() / (pos.sum() + 1.0) + (bce[..., g] * neg).sum() / (neg.sum() + 1.0)
    amb = ((irm > 0.2) & (irm < 0.6)).float() * valid[..., None]
    total = total + 0.25 * (bce * amb).sum() / (amb.sum() + 1.0)
    m = torch.sigmoid(logits)
    elig = rear_eligibility(m, roles)
    Pv = P[..., roles.vocals] * (irm[..., roles.vocals] > 0.3).float() * valid
    leak = (Pv * elig).sum() / (Pv.sum() + 1e-9)
    smooth = (m[:, 1:] - m[:, :-1]).abs().mean(-1)
    return total / irm.shape[-1] + ramp * LOSS_W["leak"] * leak + 0.05 * (smooth * valid[:, 1:]).sum() / (valid[:, 1:].sum() + 1e-9)


def loss_fn(torch, m, irm, w, P, roles):
    """Asymmetric BCE on the ideal ratio masks (protected cells 4x) + a leak term on
    the vocal ENERGY the eligible masks would admit rear, normalised per frame so
    quiet vocal passages (consonants, breath) matter as much as loud ones
    (Codex review 2026-09-25) + temporal smoothness."""
    protect = irm[..., roles.protect].sum(-1)
    bce = -(irm * torch.log(m + 1e-6) + (1 - irm) * torch.log(1 - m + 1e-6))
    per_cell = bce.mean(-1) * (1 + 3 * protect)
    elig = rear_eligibility(m, roles)                              # [B,T,257]
    # v4: placement is decided where a source DOMINATES a cell.  Vocal bleed
    # sits in nearly every cell at 93.75 Hz bins, so an energy-accounting leak
    # over all cells punished eligibility everywhere and the net protected
    # everything (v2 0.016, v3 0.047 of "other" admitted).  Leak = vocal energy
    # admitted rear in cells the voice owns (share > 0.3), per frame; missed
    # support = eligible energy held back in cells an eligible group owns (> 0.6).
    Pv = P[..., roles.vocals] * (irm[..., roles.vocals] > 0.3).float()
    frame_voc = Pv.sum(-1)                                         # [B,T]
    active = (frame_voc > 1e-4 * P.sum(-1).sum(-1).mean()).float()
    leak_frame = (Pv * elig).sum(-1) / (frame_voc + 1e-9)
    leak = (leak_frame * active).sum() / (active.sum() + 1e-9)
    Pp = P[..., roles.protect].sum(-1) * (irm[..., roles.protect].sum(-1) > 0.5).float()
    leak_p = (Pp * elig).sum() / (Pp.sum() + 1e-9)
    missed = 0.0
    for g in roles.eligible:
        Pg = P[..., g] * (irm[..., g] > 0.6).float()
        missed = missed + (Pg * (1 - elig)).sum() / (Pg.sum() + 1e-9)
    missed = missed / max(1, len(roles.eligible))
    smooth = (m[:, 1:] - m[:, :-1]).abs().mean(-1)
    core = ((per_cell * w).sum() / (w.sum() + 1e-9) + LOSS_W["leak"] * leak +
            LOSS_W["leakp"] * leak_p + LOSS_W["missed"] * missed)
    return core + 0.05 * (smooth * w[:, 1:]).sum() / (w[:, 1:].sum() + 1e-9)


def mask_space_metrics(torch, m, irm, w, P, roles):
    """vocal_to_rear = per-frame-normalised vocal energy admitted rear (the leak);
    *_kept_rear = share of that group's energy the eligible masks keep for the rear."""
    elig = rear_eligibility(m, roles)
    Pv = P[..., roles.vocals]; fv = Pv.sum(-1); act = (fv > 1e-4 * P.sum(-1).sum(-1).mean()).float()
    out = {"vocal_to_rear": float((((Pv * elig).sum(-1) / (fv + 1e-9)) * act).sum() / (act.sum() + 1e-9))}
    for name in roles.names:
        if name == "vocals":
            continue
        Pg = P[..., roles.names.index(name)]
        out[f"{name}_to_rear"] = float((Pg * elig).sum() / (Pg.sum() + 1e-9))
    # dominant-cell versions (what the stem harness measures: cells a stem owns)
    Pv = P[..., roles.vocals] * (irm[..., roles.vocals] > 0.5).float()
    out["vocal_own_to_rear"] = float((Pv * elig).sum() / (Pv.sum() + 1e-9))
    for g in roles.eligible:
        Pg = P[..., g] * (irm[..., g] > 0.6).float()
        out[f"{roles.names[g]}_own_to_rear"] = float((Pg * elig).sum() / (Pg.sum() + 1e-9))
    if m.shape[-1] == irm.shape[-1]:
        out["irm_mae"] = float(((m - irm).abs().mean(-1) * w).sum() / (w.sum() + 1e-9))
    else:
        out["irm_mae"] = float(((elig - irm[..., roles.eligible].sum(-1)).abs() * w).sum() / (w.sum() + 1e-9))
    return out


def prefetch(make_batch, depth=3):
    """Prepare the next batches on worker threads while the GPU trains on this one
    (the mmap reads and the numpy merge dominate a step otherwise)."""
    from concurrent.futures import ThreadPoolExecutor
    ex = ThreadPoolExecutor(depth)
    pending = [ex.submit(make_batch) for _ in range(depth)]
    while True:
        fut = pending.pop(0)
        pending.append(ex.submit(make_batch))
        yield fut.result()


def train(a):
    torch = _torch()
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    torch.manual_seed(a.seed)
    rng = np.random.default_rng(a.seed)
    split = load_split(a.cache)
    names, merge_idx = parse_merge(a.merge)
    roles = Roles(names)
    print(f"device {dev}; train {len(split['train'])} dev {len(split['dev'])} songs; outputs {names}; "
          f"protect {[names[i] for i in roles.protect]} eligible {[names[i] for i in roles.eligible]}", flush=True)
    net = build_net(torch, a.arch, bin_band(), ctx=a.ctx, proj=a.proj, hidden=a.hidden, n_out=len(names), neigh=a.neigh).to(dev)
    n_par = sum(p.numel() for p in net.parameters())
    n_in = 4 * (1 + 2 * a.neigh) + a.proj + 1
    if a.arch in ("probe", "probe_roles"):
        n_in = 2 * a.neigh + 1 + 1 + 2 + 1
        macs = N_BINS * (n_in * a.hidden + a.hidden * a.hidden + a.hidden * (1 if a.arch == "probe" else len(names)))
    elif a.arch == "conv2048":
        ch, hd = a.ctx, a.hidden
        macs = N_BINS * (8 * 4 * ch + 2 * 5 * ch * ch + 3 * (ch * hd + hd * hd) + (ch + hd) * len(names))
    elif a.arch == "tf":
        macs = (3 * (N_BANDS * 5 * a.ctx + a.ctx * a.ctx) + a.ctx * N_BANDS * a.proj +
                N_BINS * (n_in * 24 + 2 * 3 * (24 * a.hidden + a.hidden ** 2) + 3 * (2 * a.hidden * a.hidden + a.hidden ** 2)
                          + 3 * a.hidden * len(names)))
    else:
        macs = (3 * (N_BANDS * 5 * a.ctx + a.ctx * a.ctx) + a.ctx * N_BANDS * a.proj +
                N_BINS * (n_in * a.hidden + a.hidden * len(names)))
    print(f"params {n_par}, ~{macs} MAC per control = {macs * SR / HOP / CONTROL / 1e6:.1f} M MAC/s", flush=True)
    opt = torch.optim.AdamW(net.parameters(), lr=a.lr, weight_decay=1e-4)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, a.steps)
    os.makedirs(a.out, exist_ok=True)
    best = 1e9
    t0 = time.time()
    batches = prefetch(lambda: batch_windows(torch, a.cache, split["train"], a.seconds, a.batch, rng, "cpu", merge_idx))
    with torch.no_grad():                                 # feature statistics from 8 batches, then frozen
        fs = [targets_and_features(torch, next(batches).to(dev), rng, augment=True, fine=net_is_fine(net))[0] for _ in range(8)]
        fs = torch.cat(fs)
        fit_feat_stats(torch, probe_descriptor(torch, fs, net.neigh) if isinstance(net, ProbeNet) else fs)
    print("feature mean", [round(v, 2) for v in FEAT_STATS["mean"]], "std", [round(v, 2) for v in FEAT_STATS["std"]], flush=True)
    for step in range(1, a.steps + 1):
        x = next(batches).to(dev, non_blocking=True)
        with torch.no_grad():
            f, irm, w, P = targets_and_features(torch, x, rng, augment=True, fine=net_is_fine(net))
        if LOSS_MODE == "probe":
            loss = loss_probe(torch, net._logits(f), irm, w, roles)
        elif LOSS_MODE == "softmax":
            loss = loss_softmax(torch, net._logits(f), irm, w, P, roles)
        elif LOSS_MODE == "balanced":
            logits = net._logits(f)
            ramp = min(1.0, max(0.0, (step - a.ramp_start) / max(1, a.ramp_steps)))
            loss = loss_balanced(torch, logits, irm, w, P, roles, ramp)
        else:
            m = net.forward(f)
            loss = loss_fn(torch, m, irm, w, P, roles)
        opt.zero_grad(); loss.backward()
        torch.nn.utils.clip_grad_norm_(net.parameters(), 2.0)
        opt.step(); sched.step()
        if step % a.eval_every == 0 or step == a.steps:
            met = evaluate(torch, net, a.cache, split["dev"], dev, a.seconds, merge_idx, roles, songs=a.eval_songs)
            line = f"step {step} loss {loss.item():.4f} dev " + " ".join(f"{k}={v:.3f}" for k, v in met.items()) + f"  {time.time() - t0:.0f}s"
            print(line, flush=True)
            score = (met["vocal_own_to_rear"] * 4 + (1 - met["other_own_to_rear"]) +
                     0.5 * (1 - met.get("cymbals_own_to_rear", 1.0)) + met["irm_mae"]) if LOSS_MODE not in ("probe", "softmax") else -met["auc"]
            if score < best:
                best = score
                torch.save({"state": net.modules.state_dict(), "ctx": a.ctx, "proj": a.proj, "hidden": a.hidden,
                            "neigh": a.neigh, "arch": a.arch, "names": names, "merge": a.merge, "step": step, "dev": met,
                            "feat_stats": dict(FEAT_STATS)},
                           os.path.join(a.out, "mask_net.pt"))
                with open(os.path.join(a.out, "best.txt"), "w") as fh:
                    fh.write(line + "\n")
    print("best", open(os.path.join(a.out, "best.txt")).read().strip())


_EVAL_START = {}


def evaluate(torch, net, cache_dir, tracks, dev, seconds, merge_idx, roles, songs=20):
    net.modules.eval()
    acc = {}
    rng = np.random.default_rng(0)
    with torch.no_grad():
        for t in tracks[:songs]:
            w = np.load(os.path.join(cache_dir, t + ".npy"), mmap_mode="r")
            n = int(seconds * SR)
            if w.shape[2] <= n:
                continue
            if (t, n) not in _EVAL_START:
                # the loudest-vocal window, like the harness (lead + other vocal); found once
                v = np.asarray(w[0]).astype(np.float32) + np.asarray(w[1]).astype(np.float32)
                e = np.cumsum((v ** 2).sum(0)); idx = np.arange(0, w.shape[2] - n, 5 * SR)
                _EVAL_START[(t, n)] = idx[int(np.argmax(e[idx + n - 1] - e[idx]))] if len(idx) else 0
            s = _EVAL_START[(t, n)]
            x = torch.as_tensor(merge_groups(np.asarray(w[:, :, s:s + n]), merge_idx)[None], device=dev) / 32768
            f, irm, wt, P = targets_and_features(torch, x, rng, augment=False, fine=net_is_fine(net))
            lg = net._logits(f)
            m = net.forward(f)
            for k, val in mask_space_metrics(torch, m, irm, wt, P, roles).items():
                acc.setdefault(k, []).append(val)
            acc.setdefault("auc", []).append(dev_auc(torch, lg, irm, wt, roles))
            acc.setdefault("auc_vo", []).append(dev_auc_vo(torch, rear_eligibility(m, roles), irm, wt, roles))
    net.modules.train()
    return {k: float(np.median(v)) for k, v in acc.items()}


def load_model(model_path):
    """-> (torch, net, roles, device).  Old 4-output checkpoints map to vocals/drum_core/bass/other."""
    torch = _torch()
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    ck = torch.load(model_path, map_location=dev)
    names = ck.get("names", ["vocals", "drum_core", "bass", "other"])
    if ck.get("feat_stats"):
        FEAT_STATS.update(ck["feat_stats"])
    net = build_net(torch, ck.get("arch", "mlp"), bin_band(), ctx=ck["ctx"], proj=ck["proj"], hidden=ck["hidden"],
                    n_out=len(names), neigh=ck.get("neigh", 0)).to(dev)
    net.modules.load_state_dict(ck["state"]); net.modules.eval()
    return torch, net, Roles(names), dev


def protect_mask(torch, net, roles, dev, x):
    """x [2, n] float stereo -> per-control protect mask [Tc, 257] = 1 - rear eligibility."""
    with torch.no_grad():
        xt = torch.as_tensor(x[None], device=dev, dtype=torch.float32)
        if net_is_fine(net):
            Xf = stft_fine(torch, xt)
            f = features(torch, Xf[:, 0], Xf[:, 1])[:, 1:]
        else:
            X = stft(torch, xt)
            f = features(torch, X[:, 0], X[:, 1])
            T = (f.shape[1] // CONTROL) * CONTROL
            f = f[:, CONTROL - 1:T:CONTROL]
        m = net.forward(f)[0]
        return (1 - rear_eligibility(m, roles)).cpu().numpy().astype(np.float32)


def write_mask(path, prot):
    with open(path, "wb") as fh:
        fh.write(b"HSSRMSK0"); fh.write(np.array([prot.shape[0], N_BINS], np.uint32).tobytes()); fh.write(prot.tobytes())


def audit(a):
    """Codex review 2: before any wider net, overfit ONE balanced excerpt with the
    pure supervised loss and check the eligible-owned cells actually open.  If
    they do not, the collapse is an objective/implementation bug, not capacity."""
    torch = _torch()
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    torch.manual_seed(0)
    rng = np.random.default_rng(0)
    split = load_split(a.cache)
    names, merge_idx = parse_merge(a.merge); roles = Roles(names)
    LOSS_W.update(leak=0.0, leakp=0.0, missed=0.0)
    # a window where both the voice and "other" are present
    best = None
    for t in split["train"][:60]:
        w = np.load(os.path.join(a.cache, t + ".npy"), mmap_mode="r")
        n = int(a.seconds * SR)
        if w.shape[2] <= n:
            continue
        for _ in range(4):
            st = rng.integers(0, w.shape[2] - n)
            g = merge_groups(np.asarray(w[:, :, st:st + n]), merge_idx)
            ev = (g[roles.vocals] ** 2).sum(); eo = (g[names.index("other")] ** 2).sum(); et = (g ** 2).sum()
            score = min(ev, eo) / (et + 1e-9)
            if best is None or score > best[0]:
                best = (score, t, st, g)
    score, t, st, g = best
    print(f"excerpt {t[:8]} @ {st / SR:.1f}s: vocal {100 * (g[roles.vocals] ** 2).sum() / (g ** 2).sum():.0f} % / other "
          f"{100 * (g[names.index('other')] ** 2).sum() / (g ** 2).sum():.0f} % of energy", flush=True)
    x = torch.as_tensor(g[None], device=dev) / 32768
    net = build_net(torch, a.arch, bin_band(), ctx=a.ctx, proj=a.proj, hidden=a.hidden, n_out=len(names), neigh=a.neigh).to(dev)
    f, irm, wt, P = targets_and_features(torch, x, rng, augment=False, fine=net_is_fine(net))
    own_o = (irm[..., names.index("other")] > 0.6); own_v = (irm[..., roles.vocals] > 0.5)
    print(f"cells: other-owned {int(own_o.sum())}, vocal-owned {int(own_v.sum())}, total {irm.shape[1] * N_BINS}; "
          f"IRM_other mean in other-owned cells {float(irm[..., names.index('other')][own_o].mean()):.3f}", flush=True)
    opt = torch.optim.Adam(net.parameters(), lr=3e-3)
    for step in range(1, a.steps + 1):
        m = net.forward(f)
        loss = loss_fn(torch, m, irm, wt, P, roles)
        opt.zero_grad(); loss.backward()
        gn = float(sum(p.grad.norm() ** 2 for p in net.parameters() if p.grad is not None) ** 0.5)
        opt.step()
        if step % 50 == 0 or step == 1:
            with torch.no_grad():
                mo = m[..., names.index("other")]
                print(f"step {step:4d} loss {loss.item():.4f} grad {gn:.3f}  m_other in other-owned cells {float(mo[own_o].mean()):.3f} "
                      f"(target {float(irm[..., names.index('other')][own_o].mean()):.3f}); m_other in vocal-owned {float(mo[own_v].mean()):.3f}; "
                      f"elig(other+cymb) own-other {float(rear_eligibility(m, roles)[own_o].mean()):.3f}", flush=True)


def probe(a):
    """Decisive separability check, independent of the training pipeline: gather
    balanced cells (vocal-owned vs other-owned) from TRAIN songs, describe each
    cell by its local spectrum only, fit a small classifier, report the AUC on
    DEV cells.  AUC ~0.5 = the features cannot tell the two apart per cell;
    AUC >> 0.5 with the mask nets at 0.74/0.79 = the training path is broken."""
    torch = _torch()
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    rng = np.random.default_rng(0)
    split = load_split(a.cache)
    names, merge_idx = parse_merge(a.merge); roles = Roles(names)
    io = names.index("other")

    def gather(tracks, per_song, fine):
        Xs, ys = [], []
        for t in tracks:
            w = np.load(os.path.join(a.cache, t + ".npy"), mmap_mode="r")
            n = int(a.seconds * SR)
            if w.shape[2] <= n:
                continue
            st = rng.integers(0, w.shape[2] - n)
            x = torch.as_tensor(merge_groups(np.asarray(w[:, :, st:st + n]), merge_idx)[None], device=dev) / 32768
            f, irm, wt, P = targets_and_features(torch, x, rng, augment=False, fine=fine)
            f = f[0]; irm = irm[0]; wt = wt[0]
            valid = wt > 0.15
            own_v = (irm[..., roles.vocals] > 0.6) & valid
            own_o = (irm[..., io] > 0.6) & valid
            if fine:                                        # describe DSP bin b by fine bins 4b-8 .. 4b+8 (log power L+R) + IPD at 4b
                lp = torch.log(torch.exp(f[..., 0]) + torch.exp(f[..., 1]))          # [T,1025]
                pad = torch.nn.functional.pad(lp, (8, 8), value=lp.min().item())
                idx = torch.arange(N_BINS, device=dev) * 4 + 8
                offs = torch.arange(-8, 9, device=dev)
                desc = pad[:, (idx[:, None] + offs[None]).clamp(0, pad.shape[1] - 1)]  # [T,257,17]
                ipd = f[:, idx.clamp(0, N_BINS_FINE - 1), 2:4]
                desc = torch.cat([desc - desc.mean(-1, keepdim=True), desc.mean(-1, keepdim=True), ipd], -1)
            else:
                lp = torch.log(torch.exp(f[..., 0]) + torch.exp(f[..., 1]))          # [T,257]
                pad = torch.nn.functional.pad(lp, (2, 2), value=lp.min().item())
                idx = torch.arange(N_BINS, device=dev) + 2
                offs = torch.arange(-2, 3, device=dev)
                desc = pad[:, idx[:, None] + offs[None]]
                desc = torch.cat([desc - desc.mean(-1, keepdim=True), desc.mean(-1, keepdim=True), f[..., 2:4]], -1)
            posb = torch.arange(N_BINS, device=dev).float()[None].expand(desc.shape[0], -1)[..., None] / N_BINS
            desc = torch.cat([desc, posb], -1)
            for sel, y in ((own_v, 0.0), (own_o, 1.0)):
                d = desc[sel]
                if len(d) > per_song:
                    d = d[torch.randperm(len(d), device=dev)[:per_song]]
                Xs.append(d); ys.append(torch.full((len(d),), y, device=dev))
        return torch.cat(Xs), torch.cat(ys)

    for fine in (False, True):
        Xtr, ytr = gather(split["train"][:a.songs], 3000, fine)
        Xdv, ydv = gather(split["dev"][:20], 3000, fine)
        mu, sd = Xtr.mean(0), Xtr.std(0) + 1e-6
        Xtr, Xdv = (Xtr - mu) / sd, (Xdv - mu) / sd
        clf = torch.nn.Sequential(torch.nn.Linear(Xtr.shape[1], 64), torch.nn.Tanh(), torch.nn.Linear(64, 64), torch.nn.Tanh(), torch.nn.Linear(64, 1)).to(dev)
        opt = torch.optim.Adam(clf.parameters(), lr=3e-3)
        for step in range(a.steps):
            i = torch.randint(0, len(Xtr), (4096,), device=dev)
            loss = torch.nn.functional.binary_cross_entropy_with_logits(clf(Xtr[i])[:, 0], ytr[i])
            opt.zero_grad(); loss.backward(); opt.step()
        with torch.no_grad():
            def auc(X, y):
                sc = clf(X)[:, 0]
                pos, neg = sc[y > 0.5], sc[y < 0.5]
                return float((pos[:, None] > neg[None, :5000]).float().mean()) if len(pos) and len(neg) else float("nan")
            acc = float(((clf(Xdv)[:, 0] > 0).float() == ydv).float().mean())
            print(f"{'2048 fine (17 bins @23 Hz + IPD)' if fine else '512 (5 bins @94 Hz + IPD)':34s} train AUC {auc(Xtr[:20000], ytr[:20000]):.3f}  "
                  f"DEV AUC {auc(Xdv, ydv):.3f}  DEV acc {acc:.3f}  (n train {len(Xtr)}, dev {len(Xdv)})", flush=True)


def export(a):
    """Write the per-control protect mask of one excerpt for the renderer's --protect-mask."""
    torch, net, roles, dev = load_model(a.model)
    import soundfile as sf
    x, sr = sf.read(a.wav, dtype="float32", always_2d=True)
    assert sr == SR and x.shape[1] == 2
    prot = protect_mask(torch, net, roles, dev, x.T)
    write_mask(a.out, prot)
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
    t.add_argument("--merge", default=DEFAULT_MERGE, help="output groups as name=cached+cached;...")
    t.add_argument("--neigh", type=int, default=0, help="+-bins of per-bin features fed to the MLP")
    t.add_argument("--arch", default="mlp", choices=("mlp", "tf", "conv2048", "probe", "probe_roles"),
                   help="tf = per-bin GRUs over frequency and time; conv2048 = 2048-point analysis + frequency conv (ctx=channels, hidden=time GRU)")
    t.add_argument("--loss", default="own", choices=("own", "balanced", "probe", "softmax"))
    t.add_argument("--target-hop", default="mean", choices=("mean", "single"))
    t.add_argument("--ramp-start", type=int, default=1200, help="balanced: step where the leak penalty starts ramping in")
    t.add_argument("--ramp-steps", type=int, default=1000)
    e = sub.add_parser("export"); e.add_argument("--model", required=True); e.add_argument("--wav", required=True); e.add_argument("--out", required=True)
    pr = sub.add_parser("probe"); pr.add_argument("--cache", required=True); pr.add_argument("--songs", type=int, default=60)
    pr.add_argument("--seconds", type=float, default=30.0); pr.add_argument("--steps", type=int, default=3000); pr.add_argument("--merge", default=DEFAULT_MERGE)
    u = sub.add_parser("audit"); u.add_argument("--cache", required=True); u.add_argument("--steps", type=int, default=400); u.add_argument("--seconds", type=float, default=6.0)
    u.add_argument("--ctx", type=int, default=64); u.add_argument("--proj", type=int, default=8); u.add_argument("--hidden", type=int, default=32)
    u.add_argument("--neigh", type=int, default=0); u.add_argument("--arch", default="mlp", choices=("mlp", "tf", "conv2048")); u.add_argument("--merge", default=DEFAULT_MERGE)
    for k in ("leak", "leakp", "missed"):
        t.add_argument(f"--w-{k}", type=float, default=LOSS_W[k])
    a = ap.parse_args()
    if a.cmd == "train":
        LOSS_W.update(leak=a.w_leak, leakp=a.w_leakp, missed=a.w_missed)
        global LOSS_MODE, TARGET_HOP
        LOSS_MODE = a.loss
        TARGET_HOP = a.target_hop
    {"cache": cache, "train": train, "export": export, "audit": audit, "probe": probe}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
