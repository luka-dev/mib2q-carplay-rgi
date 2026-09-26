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
        torch = self.torch
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
        return torch.sigmoid(self.head(torch.cat([fq, tm], -1)))


def build_net(torch, arch, band, **kw):
    return (TFNet if arch == "tf" else MaskNet)(torch, band, **kw)


def master(torch, mix, rng):
    """Cheap 'mastering': gain, tilt, soft limiter.  mix [B,2,n]."""
    B = mix.shape[0]
    g = 10 ** (torch.empty(B, 1, 1, device=mix.device).uniform_(0, 9) / 20)
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


def targets_and_features(torch, x, rng, augment):
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
    irm = irm[:, :, :T].reshape(B, G, T // CONTROL, CONTROL, N_BINS).mean(3).permute(0, 2, 3, 1)
    Pc = P[:, :, :T].reshape(B, G, T // CONTROL, CONTROL, N_BINS).mean(3).permute(0, 2, 3, 1)   # [B,Tc,257,G]
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
    """What the DSP may send rear: the eligible masks summed, bounded to 1."""
    return torch_clamp_sum(m, roles.eligible)


def torch_clamp_sum(m, idx):
    return m[..., idx].sum(-1).clamp(max=1.0)


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
    core = (per_cell * w).sum() / (w.sum() + 1e-9) + 3.0 * leak + 1.0 * leak_p + 2.0 * missed
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
    out["irm_mae"] = float(((m - irm).abs().mean(-1) * w).sum() / (w.sum() + 1e-9))
    return out


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
    if a.arch == "tf":
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
    for step in range(1, a.steps + 1):
        x = batch_windows(torch, a.cache, split["train"], a.seconds, a.batch, rng, dev, merge_idx)
        with torch.no_grad():
            f, irm, w, P = targets_and_features(torch, x, rng, augment=True)
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
                     0.5 * (1 - met.get("cymbals_own_to_rear", 1.0)) + met["irm_mae"])
            if score < best:
                best = score
                torch.save({"state": net.modules.state_dict(), "ctx": a.ctx, "proj": a.proj, "hidden": a.hidden,
                            "neigh": a.neigh, "arch": a.arch, "names": names, "merge": a.merge, "step": step, "dev": met},
                           os.path.join(a.out, "mask_net.pt"))
                with open(os.path.join(a.out, "best.txt"), "w") as fh:
                    fh.write(line + "\n")
    print("best", open(os.path.join(a.out, "best.txt")).read().strip())


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
            # the loudest-vocal window, like the harness (lead + other vocal)
            v = np.asarray(w[0]).astype(np.float32) + np.asarray(w[1]).astype(np.float32)
            e = np.cumsum((v ** 2).sum(0)); idx = np.arange(0, w.shape[2] - n, 5 * SR)
            s = idx[int(np.argmax(e[idx + n - 1] - e[idx]))] if len(idx) else 0
            x = torch.as_tensor(merge_groups(np.asarray(w[:, :, s:s + n]), merge_idx)[None], device=dev) / 32768
            f, irm, wt, P = targets_and_features(torch, x, rng, augment=False)
            m = net.forward(f)
            for k, val in mask_space_metrics(torch, m, irm, wt, P, roles).items():
                acc.setdefault(k, []).append(val)
    net.modules.train()
    return {k: float(np.median(v)) for k, v in acc.items()}


def load_model(model_path):
    """-> (torch, net, roles, device).  Old 4-output checkpoints map to vocals/drum_core/bass/other."""
    torch = _torch()
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    ck = torch.load(model_path, map_location=dev)
    names = ck.get("names", ["vocals", "drum_core", "bass", "other"])
    net = build_net(torch, ck.get("arch", "mlp"), bin_band(), ctx=ck["ctx"], proj=ck["proj"], hidden=ck["hidden"],
                    n_out=len(names), neigh=ck.get("neigh", 0)).to(dev)
    net.modules.load_state_dict(ck["state"]); net.modules.eval()
    return torch, net, Roles(names), dev


def protect_mask(torch, net, roles, dev, x):
    """x [2, n] float stereo -> per-control protect mask [Tc, 257] = 1 - rear eligibility."""
    with torch.no_grad():
        X = stft(torch, torch.as_tensor(x[None], device=dev, dtype=torch.float32))
        f = features(torch, X[:, 0], X[:, 1])
        T = (f.shape[1] // CONTROL) * CONTROL
        m = net.forward(f[:, CONTROL - 1:T:CONTROL])[0]
        return (1 - rear_eligibility(m, roles)).cpu().numpy().astype(np.float32)


def write_mask(path, prot):
    with open(path, "wb") as fh:
        fh.write(b"HSSRMSK0"); fh.write(np.array([prot.shape[0], N_BINS], np.uint32).tobytes()); fh.write(prot.tobytes())


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
    t.add_argument("--arch", default="mlp", choices=("mlp", "tf"), help="tf = per-bin GRUs over frequency and time")
    e = sub.add_parser("export"); e.add_argument("--model", required=True); e.add_argument("--wav", required=True); e.add_argument("--out", required=True)
    a = ap.parse_args()
    {"cache": cache, "train": train, "export": export}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
