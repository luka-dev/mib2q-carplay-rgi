#!/usr/bin/env python3
"""Structural stereo-to-rear scene renderer (numpy reference of docs/audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md).

Per STFT cell the stereo pair is decomposed into a coherent primary with a direction (pan + phase)
and an orthogonal residual (sections 2-3); off-centre primaries are TRANSFERRED behind as whole
places found by slow pan clustering (sections 4-5, 13.2 items 2-3); the residual is the shared
room, decorrelated between Ls and Rs; a static air copy sits on top.  No classifier, no slot,
no hold; protection is geometric plus an optional duck mask.

CLI (compatible with upmix_stem_tests.py / upmix_behavior_tests.py: unknown renderer flags are
ignored, the last two positionals are the stereo input and the 6-ch output, 48 kHz int16 WAV):

    scene_upmix.py [--n-fft 1024] [--alpha 0.7] [--surround 0.35] [--rear-delay-ms 12]
                   [--no-cluster] [--wide-pair] [--duck-mask M.npy] [--dump stats.npz] in.wav out6.wav

Library:  render(x[n,2] float, SceneConfig) -> y[n,6] (L R C LFE Ls Rs), decompose(...) for tests.
ponytail: offline float64 reference, frame loop in Python; the C port is hook/audio/scene_engine.c.
"""
import argparse
import sys
import wave
from dataclasses import dataclass, field, replace

import numpy as np

SR = 48000


# ----------------------------------------------------------------------------- config
@dataclass
class SceneConfig:
    n_fft: int = 1024
    hop: int = 256
    tau_stats_ms: float = 43.0        # statistics EMA for the Wiener scalar and the pooled slow pan (section 2)
    tau_fast_ms: float = 8.0          # per-bin fast pan for the one-hop protection weight
    tau_dir_ms: float = 300.0         # per-bin DIRECTION of the residual (ambience exists only relative to a long-averaged direction)
    erb_pool: bool = True             # pool P_L, P_R, C over ERB-like bin groups before the decomposition
    pan_lo: float = 0.10              # object map smoothstep(|p|, lo, hi) in energy pan (section 4)
    pan_hi: float = 0.35
    tau_slow_ms: float = 400.0        # slow pan for placement / permission
    release_ms: float = 2000.0        # membership release: a place does not vanish because a vocal sings over it
    fast_protect: bool = True         # w = min(w_fast, w_slow): protection within one hop
    cluster: bool = True              # stage membership by pan clustering (13.2 item 2)
    hist_tau_s: float = 3.0
    pick_every_s: float = 0.34
    admit_s: float = 1.5
    admit_mass: float = 0.08
    drop_mass: float = 0.02
    drop_s: float = 30.0
    min_abs_pan: float = 0.18
    peak_sep: float = 0.25
    k_max: int = 3
    centre_tau_s: float = 2.0
    centre_slew_per_s: float = 0.05
    ramp_s: float = 0.6
    sigma_min: float = 0.08    # candidate fix 08.10: 0.15 (a 3-bin histogram peak is always narrower than real pan scatter)
    member_mass: bool = True   # membership scaled by the place's off-centre mass share; candidate fix 08.10: off
    sigma_max: float = 0.25
    front_floor: float = 0.20
    theta_max_deg: float = 120.0      # anti-phase cells never become objects
    unmix: bool = True                # object = exact 2-source unmix of the bin against the CENTRE (13.2 item 2 refinement)
    unmix_lambda: float = 0.05        # Tikhonov floor on the unmix determinant (places near the centre amplify side noise)
    alpha: float = 0.7                # transfer fraction of the coherent off-centre primary
    energy_conserving: bool = True    # rear gain sqrt(1 - (1 - a)^2) (13.2 item 3)
    front_follows: str = "slow"       # which gain the front subtraction follows: slow | fast
    ambient_gain: float = 1.0         # G_A (the generalised side signal is the stage on double-tracked mixes)
    ambient_gain_lowmid: float = 0.7  # G_A in 250-680 Hz
    surround: float = 0.7             # level of ambient + air layers; 1.0 = the passive matrix's side level (objects are at transfer level)
    air_db: float = -9.0              # static air copy above 2.5 kHz; None = off
    hp_hz: float = 190.0              # nothing below; ramp to hp_hz + 60
    same_sign_below_hz: float = 550.0 # ambient pair in phase below this (no low-mid hole)
    side_lp_hz: float = 7000.0        # side layer rolled off above 7 kHz (DEV40 08.10: drums .17 -> .13, nothing else moves; the air layer keeps the treble behind); 0 = full band
    decorrelate_ambient: bool = True  # static per-bin phase table, different for Ls and Rs
    rear_delay_ms: float = 0.0        # signed: + delays the rear, - delays the front (rear-lead). Variant J (09.10): 0, the J525 rear doors trail the front ~5.2 ms by themselves and our rear meets the amp's front bleed in phase at the doors
    bleed_cancel: float = 0.0         # J525 bank-1: the rear-door bus = 0.707*L + 2.0*Ls; Ls -= coef*L cancels the amp's front bleed (0.355 doors, 0.251 effect bus); 0 = off, needs the cabin tone probe for the tap offset
    bleed_cancel_offset_ms: float = 0.0
    front_keep_db: float = 0.0        # front anchor: the most the transfers may take from the front, dB over ~1 s (sparse wide mixes); 0 = off
    wide_pair: bool = False           # 13.2 item 10: uncorrelated L/R takes as a transferred pair
    wide_pair_alpha: float = 0.5
    wide_env_tau_ms: float = 300.0
    duck_depth: float = 0.25          # D = depth * mask (variant I: 0.25 with the fine net c; the eager v21a wanted 0.5)
    inst_alpha: float = 0.9           # semantic stage: transfer fraction of the instrument part of a cell (variant I, 09.10: 0.9 with the fine net c)
    inst_attack_ms: float = 300.0     # eligibility rises slowly (an instrument earns the rear)
    inst_release_ms: float = 30.0     # and drops at once (protection: a vocal entering the cell wins immediately)
    inst_mask_nfft: int = 512         # the mask's STFT size (512 = the hook's control grid, 2048 = mask_net 'fine')
    inst_mask_hop: int = 512          # samples per mask row (4 x 128 control cadence, or 512 for the fine grid)
    inst_gamma: float = 1.0           # eligibility sharpening e^gamma (variant I: 1.0 for the fine net; a 94 Hz-bin ratio mask needs 2)
    inst_thresh: float = 0.0          # eligibility threshold: (e - th) / (1 - th), clipped
    inst_gamma_lo: float = 0.0        # if > 0: sharpening used below inst_gamma_split_hz instead of inst_gamma (bass harmonics share cells with instruments)
    inst_gamma_split_hz: float = 700.0
    memory_boost: float = 0.75        # song memory: a pan that was a place earlier in this song is re-admitted after admit_s*(1-boost)
    memory_s: float = 120.0           # how long a remembered place stays valid without being seen
    inst_long_tau_s: float = 0.0      # candidate: long-term evidence gate on the eligibility (0 = off): a cell must have been an instrument cell for a while
    inst_long_lo: float = 0.15        # gate = smoothstep(long-term eligibility, lo, hi)
    inst_long_hi: float = 0.40
    duck_from_mask: bool = True       # with --protect-mask and no --duck-mask: duck D = duck_depth * protect (vocal/bass/drum-dominant cells)
    duck_attack_ms: float = 30.0      # protection rises at once
    duck_release_ms: float = 300.0    # and lets go slowly
    seed: int = 7


# ----------------------------------------------------------------------------- helpers
def smoothstep(x, lo, hi):
    t = np.clip((x - lo) / max(hi - lo, 1e-9), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def ema_alpha(tau_ms, hop, sr=SR):
    if tau_ms <= 0:
        return 1.0
    return 1.0 - np.exp(-hop / sr / (tau_ms / 1000.0))


def stft(x, n_fft, hop):
    """x [n] -> frames [T, F] complex, sine window, centre-padded."""
    win = np.sin(np.pi * (np.arange(n_fft) + 0.5) / n_fft)
    pad = np.concatenate([np.zeros(n_fft), x, np.zeros(n_fft)])
    idx = np.arange(0, len(pad) - n_fft + 1, hop)
    frames = np.stack([pad[i:i + n_fft] * win for i in idx])
    return np.fft.rfft(frames, axis=1)


def istft(X, n, n_fft, hop):
    win = np.sin(np.pi * (np.arange(n_fft) + 0.5) / n_fft)
    T = X.shape[0]
    out = np.zeros(n_fft * 2 + n + n_fft)
    norm = np.zeros_like(out)
    frames = np.fft.irfft(X, n=n_fft, axis=1)
    for t in range(T):
        i = t * hop
        if i + n_fft > len(out):
            break
        out[i:i + n_fft] += frames[t] * win
        norm[i:i + n_fft] += win * win
    return (out / np.maximum(norm, 1e-6))[n_fft:n_fft + n]


def erb_groups(n_bins, n_fft, sr=SR):
    """Bin -> group index; group width grows with frequency (~2 bins < 500 Hz, 3 to 2 kHz, 4 above)."""
    freqs = np.arange(n_bins) * sr / n_fft
    width = np.where(freqs < 500, 2, np.where(freqs < 2000, 3, 4)) * (n_fft // 1024 if n_fft >= 1024 else 1)
    width = np.maximum(width, 1)
    group = np.zeros(n_bins, int)
    g, k = 0, 0
    while k < n_bins:
        w = int(width[k])
        group[k:k + w] = g
        k += w
        g += 1
    return group, g


def phase_table(n_bins, seed, sign):
    """Static per-bin phase for Ls/Rs decorrelation of the ambient layer (free in the STFT domain)."""
    rng = np.random.default_rng(seed)
    phi = rng.uniform(-np.pi, np.pi, n_bins)
    # a static per-bin phase: the per-bin MSC stays 1 (LTI), what drops is the band-limited
    # interaural correlation the ear uses; measure it as IACC over ERB bands, not Welch MSC.
    return np.exp(1j * sign * phi)



def centre_removed_pan(PL, PR, C, eps, PN=None):
    """Pan of the OFF-CENTRE content of a cell, immune to a centred source of any level.

    Model L = a·s + m', R = b·s + m' (+ symmetric ambience A):  num = P_L − Re C = a·S·(a−b) + A and
    den = Re C − P_R = b·S·(a−b) − A, so num + den = S·(a² − b²) is free of both the centre and the
    ambience (its sign is the side, its size the place energy), and a/b = num/den when ambience is
    small.  Ambience pushes den below zero for a hard-panned source, so the sign test tolerates
    1.5·P_N (the eigen ambience estimate) and the clamped ratio biases a mild pan OUTWARD, never
    toward the centre (the safe error: less transfer, never a vocal leak).  Returns (p, e, valid)."""
    num = PL - C.real
    den = C.real - PR
    sig = num + den
    tol = 1.5 * PN if PN is not None else 0.0
    left = (num >= -tol) & (den >= -tol) & (sig > eps)
    right = (num <= tol) & (den <= tol) & (sig < -eps)
    valid = left | right
    n2 = np.where(left, np.maximum(num, 0.0), np.maximum(-num, 0.0)) ** 2
    d2 = np.where(left, np.maximum(den, 0.0), np.maximum(-den, 0.0)) ** 2
    p = np.where(valid, (d2 - n2) / (n2 + d2 + 1e-30), 0.0)
    p = np.where(left, -np.abs(p), np.abs(p))
    e = np.where(valid, np.abs(sig), 0.0)
    return p, e, valid

# ----------------------------------------------------------------------------- stage clusters
class StageClusters:
    """Slow pan clustering: the rear stage is a small set of PLACES (13.2 item 2)."""

    def __init__(self, cfg, frame_s):
        self.cfg = cfg
        self.frame_s = frame_s
        self.edges = np.linspace(-1.0, 1.0, 22)
        self.centres_h = 0.5 * (self.edges[:-1] + self.edges[1:])
        self.hist = np.zeros(21)
        self.a_hist = ema_alpha(cfg.hist_tau_s * 1000.0, cfg.hop)
        self.pick_every = max(1, int(round(cfg.pick_every_s / frame_s)))
        self.dt_pick = self.pick_every * frame_s
        self.frame = 0
        self.clusters = []   # dicts: centre, sigma, mass, seen_s, absent_s, gain, admitted, present, dying
        self.a_centre = ema_alpha(cfg.centre_tau_s * 1000.0, cfg.hop * self.pick_every)   # per PICK
        self.last_pc = None
        self.last_p = None; self.last_e = None
        self.memory = []     # song-level: dicts centre, last_s (places admitted earlier in this song)
        self.song_s = 0.0
        self.tot_ref = 0.0   # slow maximum of the eligible primary power: silence = 30 dB below it (no peaks, places go absent)
        self.silent = False

    def update(self, p, ps, ok):
        """p, ps: per-group pan and primary power; ok: eligibility mask."""
        cfg = self.cfg
        h, _ = np.histogram(p[ok], bins=self.edges, weights=ps[ok])
        self.hist += self.a_hist * (h - self.hist)
        tot = float(ps[ok].sum()) if np.any(ok) else 0.0
        self.tot_ref = max(self.tot_ref * (1.0 - self.frame_s / 60.0), tot)
        self.silent = tot < 1e-3 * self.tot_ref
        self.frame += 1
        dt = self.frame_s
        self.song_s += dt
        if self.frame % self.pick_every == 0:
            self._pick()
        for c in self.clusters:
            if not c["admitted"] and c["seen_s"] >= cfg.admit_s:
                if self._shadow(c):
                    c["dying"] = True           # the same instrument seen through shared cells: not a new place
                else:
                    c["admitted"] = True
            if c["admitted"] and not c["dying"] and c["present"]:
                self._remember(c["centre"])
            alive = c["admitted"] and not c["dying"] and c["absent_s"] < cfg.drop_s
            target = 1.0 if alive else 0.0
            step = dt / cfg.ramp_s if target > c["gain"] else dt / 1.5
            c["gain"] = float(np.clip(c["gain"] + np.sign(target - c["gain"]) * step, 0.0, 1.0))
        self.clusters = [c for c in self.clusters
                         if not (c["gain"] <= 0.0 and (c["dying"] or c["absent_s"] >= cfg.drop_s))]

    def _pick(self):
        cfg = self.cfg
        total = self.hist.sum() + 1e-12
        # The centre (lead, bass, drum core) holds most of the mass of any real mix; a rear place is
        # judged against the OFF-CENTRE mass, and there is no rear stage at all when that is < 2 %.
        off = np.abs(self.centres_h) >= cfg.min_abs_pan
        total_off = self.hist[off].sum()
        if total_off < 0.02 * total:
            total_off = np.inf
        peaks = []
        for i in range(21):
            if self.hist[i] <= 0 or not off[i]:
                continue
            left = self.hist[i - 1] if i > 0 else 0.0
            right = self.hist[i + 1] if i < 20 else 0.0
            if self.hist[i] >= left and self.hist[i] >= right:
                sl = slice(max(0, i - 1), i + 2)
                hh = self.hist[sl]; cc = self.centres_h[sl]
                mass = hh.sum() / total_off
                if mass >= cfg.admit_mass * 0.5:
                    cen = float((hh * cc).sum() / (hh.sum() + 1e-12))
                    if abs(cen) > 0.9:                         # the outer bins are centred at +-0.952
                        cen = float(np.sign(cen))
                    sig = float(np.sqrt(((cc - cen) ** 2 * hh).sum() / (hh.sum() + 1e-12)))
                    peaks.append((mass, cen, float(np.clip(sig, cfg.sigma_min, cfg.sigma_max))))
        peaks.sort(reverse=True)
        if self.silent:                                    # a decayed histogram keeps its shape: no peaks in silence
            peaks = []
        chosen = []
        for mass, cen, sig in peaks:
            if all(abs(cen - c2) >= cfg.peak_sep for _, c2, _ in chosen):
                chosen.append((mass, cen, sig))
            if len(chosen) >= cfg.k_max:
                break
        matched = set()
        for c in self.clusters:
            c["present"] = False
        for mass, cen, sig in chosen:
            best, bi = None, -1
            for i, c in enumerate(self.clusters):
                if i in matched or c["dying"]:
                    continue
                d = abs(c["centre"] - cen)
                if d < 0.15 and (best is None or d < best):
                    best, bi = d, i
            if bi >= 0:
                c = self.clusters[bi]
                matched.add(bi)
                new = c["centre"] + self.a_centre * (cen - c["centre"])
                slew = cfg.centre_slew_per_s * self.dt_pick
                c["centre"] = float(np.clip(new, c["centre"] - slew, c["centre"] + slew))
                c["sigma"] += self.a_centre * (sig - c["sigma"])
                c["mass"] = mass
                thr = cfg.drop_mass if c["admitted"] else cfg.admit_mass
                c["present"] = mass >= thr
                if c["present"]:
                    c["absent_s"] = 0.0
                    c["seen_s"] += self.dt_pick
                else:
                    c["absent_s"] += self.dt_pick
                    if not c["admitted"]:
                        c["seen_s"] = 0.0                      # dwell must be continuous
            elif mass >= cfg.admit_mass:
                seen0 = self.dt_pick + (cfg.admit_s * cfg.memory_boost if self._remembered(cen) else 0.0)
                self.clusters.append(dict(centre=cen, sigma=sig, mass=mass, seen_s=seen0, absent_s=0.0,
                                          gain=0.0, admitted=False, present=True, dying=False))
                matched.add(len(self.clusters) - 1)        # a new cluster counts as matched in its own pick (its dwell starts now)
        for i, c in enumerate(self.clusters):
            if i not in matched:
                c["absent_s"] += self.dt_pick
                if not c["admitted"]:
                    c["seen_s"] = 0.0
        # merge: a younger cluster within 0.15 of an older one fades out (no one-pick step)
        self.clusters.sort(key=lambda c: -c["gain"])
        kept = []
        for c in self.clusters:
            if c["dying"] or all(abs(c["centre"] - k["centre"]) >= 0.15 for k in kept):
                kept.append(c)
            else:
                c["dying"] = True
                kept.append(c)
        self.clusters = kept
        # K <= k_max admitted places: the weakest beyond that fade
        live = sorted([c for c in self.clusters if c["admitted"] and not c["dying"]], key=lambda c: -c["mass"])
        for c in live[cfg.k_max:]:
            c["dying"] = True

    def _remember(self, centre):
        for m in self.memory:
            if abs(m["centre"] - centre) < 0.15:
                m["centre"] += 0.1 * (centre - m["centre"]); m["last_s"] = self.song_s
                return
        self.memory.append(dict(centre=float(centre), last_s=self.song_s))

    def _remembered(self, centre):
        return any(abs(m["centre"] - centre) < 0.15 and self.song_s - m["last_s"] <= self.cfg.memory_s for m in self.memory)

    def reset(self):
        """Track change: every place fades out over 1.5 s (the dying ramp), histogram and song memory cleared."""
        for c in self.clusters:
            c["dying"] = True
        self.hist[:] = 0.0
        self.memory = []
        self.song_s = 0.0

    def _shadow(self, cand):
        """True when the candidate's bins are already owned by an admitted place (overlap > 0.5)."""
        if self.last_p is None:
            return False
        p = self.last_p; e = self.last_e
        mc = np.exp(-((p - cand["centre"]) ** 2) / (2 * cand["sigma"] ** 2))
        for c in self.clusters:
            if c is cand or not c["admitted"] or c["dying"]:
                continue
            mo = np.exp(-((p - c["centre"]) ** 2) / (2 * c["sigma"] ** 2))
            ov = float((np.minimum(mc, mo) * e).sum() / ((mc * e).sum() + 1e-12))
            if ov > 0.5:
                return True
        return False

    def membership(self, p_slow, e_slow=None):
        """Per-element member weight in [0,1], image pan q, alpha scale, and the place pan."""
        cfg = self.cfg
        n = len(p_slow)
        num = np.zeros(n); q_acc = np.zeros(n); a_acc = np.zeros(n); pc_acc = np.zeros(n)
        den = np.full(n, cfg.front_floor)                       # flat front floor (no Gaussian)
        for c in self.clusters:
            if c["gain"] <= 0.0:
                continue
            d2 = (p_slow - c["centre"]) ** 2 / (2 * c["sigma"] ** 2)
            m = np.where(d2 < 3.125, np.exp(-d2), 0.0)               # clipped at 2.5 sigma
            if cfg.member_mass:
                m = c["mass"] * m
            den += m
            g = c["gain"] * m
            num += g
            q = np.sign(c["centre"]) * (0.78 + 0.17 * smoothstep(abs(c["centre"]), 0.05, 0.92))
            q_acc += g * q
            a_acc += g * smoothstep(abs(c["centre"]), cfg.min_abs_pan, 0.45)
            pc_acc += g * c["centre"]
        self.last_p = p_slow; self.last_e = e_slow if e_slow is not None else np.ones(n)
        w = num / (den + 1e-12)
        q = np.where(num > 1e-12, q_acc / (num + 1e-12), 0.0)
        a_scale = np.where(num > 1e-12, a_acc / (num + 1e-12), 0.0)
        self.last_pc = np.where(num > 1e-12, pc_acc / (num + 1e-12), 0.0)
        return w, q, a_scale


# ----------------------------------------------------------------------------- core
def decompose_frame(PL, PR, C):
    """Closed-form eigen decomposition (section 3). Returns P_N, P_S, a, b, theta, P_a, P_b (p = (P_b-P_a)/(P_a+P_b+eps))."""
    T = PL + PR
    Rt = np.sqrt(np.maximum((PL - PR) ** 2 + 4.0 * np.abs(C) ** 2, 0.0))
    # stable form of (T - Rt)/2 = 2*(PL*PR - |C|^2)/(T + Rt): no cancellation on coherent hard-panned bins
    # (the float32 C port lost 40 % of b there with the subtraction; the two agree algebraically)
    Dt = np.maximum(PL * PR - np.abs(C) ** 2, 0.0)
    PN = np.where(T + Rt > 0, 2.0 * Dt / (T + Rt + 1e-300), 0.0)
    PS = Rt
    Pa = np.maximum(PL - PN, 0.0)
    Pb = np.maximum(PR - PN, 0.0)
    # Regularised direction: as the primary vanishes (pure ambience, P_S << P_N) the direction tends to
    # the CENTRE, so the residual becomes the plain side signal (L - R)/sqrt2 instead of vanishing.
    k = 0.5 * PN + 1e-12
    a = np.sqrt((Pa + k) / (Pa + Pb + 2.0 * k))
    b = np.sqrt((Pb + k) / (Pa + Pb + 2.0 * k))
    theta = np.angle(C + 0j) * (PS / (PS + PN + 1e-12))   # phase only where a primary exists
    return PN, PS, a, b, theta, Pa, Pb


def _engine(cfg, F, G, group, freqs, duck_mask=None, inst_mask=None, prot_mask=None):
    """The per-frame engine: all tables and all state live in this generator. Prime it with next(),
    then send (L, R, t) per STFT frame and receive (subL, subR, rearL, rearR, stats_t): what leaves
    the front and what goes to Ls/Rs, as spectra of this frame. The batch render() and the
    block-wise SceneStream drive the same generator, so they cannot diverge."""
    N, H = cfg.n_fft, cfg.hop
    frame_s = H / SR
    a_s = ema_alpha(cfg.tau_stats_ms, H)
    a_slow = ema_alpha(cfg.tau_slow_ms, H)
    a_env = ema_alpha(cfg.wide_env_tau_ms, H)

    # band shaping
    hp = smoothstep(freqs, cfg.hp_hz, cfg.hp_hz + 60.0)
    lowmid = (freqs >= 250.0) & (freqs < 680.0)
    GA = np.where(lowmid, cfg.ambient_gain_lowmid, cfg.ambient_gain) * hp
    if cfg.side_lp_hz > 0:
        GA = GA * (1.0 - smoothstep(freqs, cfg.side_lp_hz, 1.4 * cfg.side_lp_hz))
    air = (10 ** (cfg.air_db / 20.0)) * smoothstep(freqs, 2000.0, 2500.0) if cfg.air_db is not None else np.zeros(F)
    same_sign = freqs <= cfg.same_sign_below_hz
    phL = phase_table(F, cfg.seed, +1.0) if cfg.decorrelate_ambient else np.ones(F, complex)
    phR = phase_table(F, cfg.seed, -1.0) if cfg.decorrelate_ambient else np.ones(F, complex)
    # keep the low band in phase (no scramble where the pair is same-sign)
    phL = np.where(same_sign, 1.0 + 0j, phL)
    phR = np.where(same_sign, 1.0 + 0j, phR)
    theta_max = np.deg2rad(cfg.theta_max_deg)

    # state
    PL = np.zeros(F); PR = np.zeros(F); C = np.zeros(F, complex)
    fPL = np.zeros(F); fPR = np.zeros(F); fC = np.zeros(F, complex)
    a_fast = ema_alpha(cfg.tau_fast_ms, H)
    dPL = np.zeros(F); dPR = np.zeros(F); dC = np.zeros(F, complex)
    a_dir = ema_alpha(cfg.tau_dir_ms, H)
    p_slow = np.zeros(G); ps_num = np.zeros(G); ps_den = np.zeros(G)
    p_slow_bin = np.zeros(F); pb_num = np.zeros(F); pb_den = np.zeros(F)
    w_hold = np.zeros(F); q_hold = np.zeros(F); a_hold = np.zeros(F); pc_hold = np.zeros(F)
    a_rel = ema_alpha(cfg.release_ms, H)
    envL = np.zeros(G); envR = np.zeros(G); envLR = np.zeros(G); envLL = np.zeros(G); envRR = np.zeros(G)
    clusters = StageClusters(cfg, frame_s) if cfg.cluster else None
    ramp_hops = 32
    # semantic stage state: eligibility [Tc, 257] on the hook's 512/128 x4 control grid -> this frame's bins
    inst_on = inst_mask is not None
    prot_on = prot_mask is not None and duck_mask is None and cfg.duck_from_mask
    if prot_on:
        pbin = np.minimum(np.round(freqs / (SR / float(cfg.inst_mask_nfft))).astype(int), prot_mask.shape[1] - 1)
        d_prot = np.zeros(F)
        a_datt = ema_alpha(cfg.duck_attack_ms, H); a_drel = ema_alpha(cfg.duck_release_ms, H)
    if inst_on:
        mbin = np.minimum(np.round(freqs / (SR / float(cfg.inst_mask_nfft))).astype(int), inst_mask.shape[1] - 1)
        e_inst = np.zeros(F)
        gamma_vec = np.where(freqs < cfg.inst_gamma_split_hz, cfg.inst_gamma_lo, cfg.inst_gamma) if cfg.inst_gamma_lo > 0 else None
        a_iatt = ema_alpha(cfg.inst_attack_ms, H); a_irel = ema_alpha(cfg.inst_release_ms, H)
        e_long = np.zeros(F); a_long = ema_alpha(cfg.inst_long_tau_s * 1000.0, H) if cfg.inst_long_tau_s > 0 else 0.0
    cold = 0
    anchor = 1.0; anc_ex = anc_rxs = anc_es = 0.0   # front anchor state (front_keep_db)
    a_anc_meas = ema_alpha(1000.0, H); a_anc_att = ema_alpha(300.0, H); a_anc_rel = ema_alpha(2000.0, H)

    gsum = lambda v: np.bincount(group, weights=v, minlength=G)
    msg = yield None
    while True:
        L, R, t = msg[0], msg[1], msg[2]
        ctl = msg[3] if len(msg) > 3 else None
        if ctl and ctl.get("reset"):                 # track change (Now Playing): song-level state only
            if clusters is not None:
                clusters.reset()
            if inst_on and cfg.inst_long_tau_s > 0:
                e_long[:] = 0.0
        st = {"places": np.full((8, 4), np.nan), "n_clusters": 0}
        iL = np.abs(L) ** 2; iR = np.abs(R) ** 2; iC = np.conj(L) * R
        if t == 0:   # initialise from the first frame (no cold burst)
            PL = iL.copy(); PR = iR.copy(); C = iC.copy()
            fPL = iL.copy(); fPR = iR.copy(); fC = iC.copy()
            dPL = iL.copy(); dPR = iR.copy(); dC = iC.copy()
        else:
            PL += a_s * (iL - PL); PR += a_s * (iR - PR); C += a_s * (iC - C)
            fPL += a_fast * (iL - fPL); fPR += a_fast * (iR - fPR); fC += a_fast * (iC - fC)
            dPL += a_dir * (iL - dPL); dPR += a_dir * (iR - dPR); dC += a_dir * (iC - dC)
        # per-bin decomposition: the direction (and the fast protection weight) must be the bin's own,
        # otherwise a centred drum bin inside a guitar-dominated group is projected onto the guitar's
        # direction and transferred behind (seen on the AlbumDB oracle master: drums .44 rear share).
        # Direction for the residual from the 43 ms statistics: ambience exists only relative to an
        # AVERAGED direction (an instantaneous direction makes every bin rank-one and u vanishes).
        _, _, ab, bb, th, _, _ = decompose_frame(dPL, dPR, dC)          # long-averaged direction (tau_dir)
        PNb, PSb, _, _, _, Pab, Pbb = decompose_frame(PL, PR, C)       # 43 ms statistics: Wiener scalar
        e_m = np.exp(-1j * th)
        y = ab * L + bb * e_m * R                       # projection on the bin's primary direction
        u = bb * L - ab * e_m * R                       # orthogonal residual (no primary)
        W = PSb / (PSb + PNb + 1e-12)                   # Wiener scalar, object term only
        # fast per-bin pan (tau_fast) for the one-hop protection weight only
        _, _, _, _, thf, fPa, fPb = decompose_frame(fPL, fPR, fC)
        blockmax = max(float((fPL + fPR).max()), 1e-12)
        fPN = np.maximum(fPL + fPR - np.sqrt((fPL - fPR) ** 2 + 4.0 * np.abs(fC) ** 2), 0.0) * 0.5
        p_bin = (fPb - fPa) / (fPa + fPb + 1e-4 * blockmax)
        anti_bin = np.abs(thf) > theta_max
        w_fast_bin = np.where(anti_bin, 0.0, smoothstep(np.abs(p_bin), cfg.pan_lo, cfg.pan_hi))
        p_cr, e_cr, v_cr = centre_removed_pan(PL, PR, C, 1e-4 * blockmax, PNb)
        # energy-weighted slow pan with a BOUNDED weight: one loud hop (cross terms of two coherent
        # sources at one frequency) must not move the estimate faster than tau_slow allows
        wgt = np.minimum(e_cr, 2.0 * pb_den + 1e-4 * blockmax)
        pb_num += a_slow * (wgt * p_cr - pb_num); pb_den += a_slow * (wgt - pb_den)
        p_slow_bin = pb_num / (pb_den + 1e-12)
        # pooled statistics per group: the slow pan, the histogram and the membership (placement)
        gPL = gsum(PL); gPR = gsum(PR)
        gC = gsum(C.real) + 1j * gsum(C.imag)
        PN, PS, a, b, theta, Pa, Pb = decompose_frame(gPL, gPR, gC)
        gdPL = gsum(dPL); gdPR = gsum(dPR); gdC = gsum(dC.real) + 1j * gsum(dC.imag)
        eps_abs = 1e-4 * max(float((gPL + gPR).max()), 1e-12)
        p, e_g, v_g = centre_removed_pan(gPL, gPR, gC, eps_abs, PN)
        wg = np.minimum(e_g, 2.0 * ps_den + eps_abs)
        ps_num += a_slow * (wg * p - ps_num); ps_den += a_slow * (wg - ps_den)
        p_slow = ps_num / (ps_den + 1e-12)
        w_fast = smoothstep(np.abs(p), cfg.pan_lo, cfg.pan_hi)
        anti = np.abs(theta) > theta_max
        w_fast = np.where(anti, 0.0, w_fast)
        if clusters is not None:
            # eligibility: a valid centre-removed place pan on the long statistics (double-tracks and
            # ambience are invalid: their L/R difference is not a panned source), no anti-phase
            dPN_, _, _, _, _, _, _ = decompose_frame(gdPL, gdPR, gdC)
            dp, de, dv = centre_removed_pan(gdPL, gdPR, gdC, eps_abs, dPN_)
            dth = np.angle(gdC + 0j)
            ok = dv & (de > 1e-3 * blockmax) & (np.abs(dth) <= theta_max) & (np.arange(G) > 0)
            fg = np.bincount(group, weights=freqs, minlength=G) / np.maximum(np.bincount(group, minlength=G), 1)
            ok &= (fg >= cfg.hp_hz) & (fg <= 6000.0)
            clusters.update(p, e_g, ok)
            # membership per BIN from the bin's own slow pan: a bin shared by a panned source and a
            # centred one sits between the place and the centre and gets ~no membership (stays front)
            w_member, q_b, a_scale = clusters.membership(p_slow_bin, pb_den)
            # permission opens with the dwell/ramp, releases slowly (protection is algebraic, §13.2-2);
            # the place's image, alpha scale and pan are held together with the membership
            fresh = w_member >= w_hold
            w_hold = np.where(fresh, w_member, w_hold + a_rel * (w_member - w_hold))
            q_hold = np.where(fresh, q_b, q_hold); a_hold = np.where(fresh, a_scale, a_hold)
            pc_hold = np.where(fresh, clusters.last_pc, pc_hold)
            w_perm = w_hold; q_b = q_hold; a_scale = a_hold
            alpha_b = cfg.alpha * a_scale
            st["n_clusters"] = sum(1 for c in clusters.clusters if c["gain"] > 0)
            for ci, c in enumerate(clusters.clusters[:8]):
                st["places"][ci] = (c["centre"], c["sigma"], c["mass"], c["gain"])
            w_member_g = np.bincount(group, weights=w_member, minlength=G) / np.maximum(np.bincount(group, minlength=G), 1)
        else:
            w_perm = smoothstep(np.abs(p_slow_bin), cfg.pan_lo, cfg.pan_hi)
            q_b = np.sign(p_slow_bin) * (0.78 + 0.17 * smoothstep(np.abs(p_slow_bin), 0.05, 0.92))
            alpha_b = np.full(F, cfg.alpha)
            w_member_g = np.bincount(group, weights=w_perm, minlength=G) / np.maximum(np.bincount(group, minlength=G), 1)
        if cfg.unmix and clusters is not None:
            w_bin = w_perm                               # the centre is removed algebraically; no pan gate needed
        else:
            w_bin = np.minimum(w_fast_bin, w_perm) if cfg.fast_protect else w_perm
        w_front_bin = w_perm if cfg.front_follows == "slow" else w_bin
        w = np.bincount(group, weights=w_bin, minlength=G) / np.maximum(np.bincount(group, minlength=G), 1)
        w_member = w_member_g
        D = np.zeros(F)
        if duck_mask is not None:
            D = cfg.duck_depth * np.clip(duck_mask[min(t, len(duck_mask) - 1)], 0.0, 1.0)
        elif prot_on:
            ctrl_p = min((t * H) // cfg.inst_mask_hop, prot_mask.shape[0] - 1)
            p_now = np.clip(prot_mask[ctrl_p][pbin], 0.0, 1.0)
            d_prot = d_prot + np.where(p_now > d_prot, a_datt, a_drel) * (p_now - d_prot)
            D = cfg.duck_depth * d_prot
        cold_gain = min(1.0, (t + 1) / ramp_hops) * anchor
        a_r = alpha_b * w_bin * (1.0 - D) * cold_gain
        a_f = alpha_b * w_front_bin * (1.0 - D) * cold_gain
        g_r = np.sqrt(np.maximum(1.0 - (1.0 - a_r) ** 2, 0.0)) if cfg.energy_conserving else a_r
        # object: widened constant-power image per group, coherent, undecorrelated
        q = q_b
        cL = np.sqrt((1.0 - q) * 0.5); cR = np.sqrt((1.0 + q) * 0.5)
        if cfg.unmix and clusters is not None:
            # Exact two-source unmix of the bin against the centre: [L;R] = [a_c;b_c]*s + [1;1]/sqrt2 * m.
            # A centred source gives s = 0 sample-exactly for ANY place; the rank-one projection could not.
            pc = np.where(fresh, w_perm * p_slow_bin + (1.0 - w_perm) * pc_hold, pc_hold)   # own pan where the bin is a member; held otherwise
            pc = np.where(np.abs(pc) > 0.9, np.sign(pc), pc)
            a_c = np.sqrt(np.clip((1.0 - pc) * 0.5, 0.0, 1.0)); b_c = np.sqrt(np.clip((1.0 + pc) * 0.5, 0.0, 1.0))
            det = a_c - b_c
            with np.errstate(divide="ignore"):    # det == 0 takes the regularised branch; 1/0 is discarded
                inv = np.where(np.abs(det) > 0.5, 1.0 / det, det / (det * det + cfg.unmix_lambda))
            s_obj = (L - R) * inv
            # bounded: the place's content can never exceed the bin's own energy
            e_bin = np.abs(L) ** 2 + np.abs(R) ** 2
            e_s = np.abs(s_obj) ** 2 + 1e-18
            s_obj = s_obj * np.minimum(1.0, np.sqrt(e_bin / e_s))
            obj = g_r * s_obj * hp
            objL = obj * cL
            objR = obj * cR
            subL = a_f * s_obj * a_c * hp                 # leaves the front at the place's pan
            subR = a_f * s_obj * b_c * hp
        else:
            obj = g_r * W * y * hp
            objL = obj * cL
            objR = obj * cR * np.exp(1j * th)
            subL = a_f * W * y * ab * hp                  # leaves the front at the primary's own pan
            subR = a_f * W * y * bb * np.exp(1j * th) * hp
        # semantic stage: the instrument part of what is still in front (external eligibility mask), at the cell's own pan
        instL = np.zeros(F, complex); instR = np.zeros(F, complex)
        if inst_on:
            ctrl = min((t * H) // cfg.inst_mask_hop, inst_mask.shape[0] - 1)
            e_now = np.clip(inst_mask[ctrl][mbin], 0.0, 1.0)
            if cfg.inst_thresh > 0:
                e_now = np.clip((e_now - cfg.inst_thresh) / (1.0 - cfg.inst_thresh), 0.0, 1.0)
            if gamma_vec is not None:
                e_now = e_now ** gamma_vec
            elif cfg.inst_gamma != 1.0:
                e_now = e_now ** cfg.inst_gamma
            if cfg.inst_long_tau_s > 0:
                e_long = e_long + a_long * (e_now - e_long)
                e_now = e_now * smoothstep(e_long, cfg.inst_long_lo, cfg.inst_long_hi)
            up = e_now > e_inst
            e_inst = e_inst + np.where(up, a_iatt, a_irel) * (e_now - e_inst)
            # the object is the masked part O = e * front; the conserving gains come from the constant
            # fraction, NOT from alpha*e (sqrt(1-(1-a e)^2) ~ sqrt(2 a e) would amplify every small eligibility)
            a_e = cfg.inst_alpha * (1.0 - D) * cold_gain * hp
            g_e = np.sqrt(np.maximum(1.0 - (1.0 - a_e) ** 2, 0.0)) if cfg.energy_conserving else a_e
            oL = e_inst * (L - subL); oR = e_inst * (R - subR)
            instL = g_e * oL; instR = g_e * oR
            subL = subL + a_e * oL
            subR = subR + a_e * oR
        # ambient: the residual as the room, decorrelated between Ls and Rs
        ambL = GA * bb * u * (1.0 - D)
        ambR = GA * (-ab) * np.exp(1j * th) * u * (1.0 - D)
        ambR = np.where(same_sign, -ambR, ambR)          # same sign below 550 Hz
        rearL = objL + instL + cfg.surround * (ambL * phL + air * L * phL) * cold_gain
        rearR = objR + instR + cfg.surround * (ambR * phR + air * R * phR) * cold_gain
        # wide uncorrelated pairs (13.2 item 10): envelope-correlated, incoherent, near-centre
        if cfg.wide_pair:
            eL = gsum(np.abs(L) ** 2); eR = gsum(np.abs(R) ** 2)
            envL += a_env * (eL - envL); envR += a_env * (eR - envR)
            envLR += a_env * (eL * eR - envLR); envLL += a_env * (eL * eL - envLL); envRR += a_env * (eR * eR - envRR)
            corr = (envLR - envL * envR) / (np.sqrt(np.maximum(envLL - envL ** 2, 0) * np.maximum(envRR - envR ** 2, 0)) + 1e-12)
            coh = PS / (PS + PN + 1e-12)
            wide = smoothstep(corr, 0.5, 0.8) * (1.0 - smoothstep(coh, 0.35, 0.6)) * (1.0 - smoothstep(np.abs(p_slow), 0.2, 0.4))
            wg = cfg.wide_pair_alpha * wide[group] * hp * cold_gain * (1.0 - D)
            gr = np.sqrt(np.maximum(1.0 - (1.0 - wg) ** 2, 0.0))
            rearL += gr * L; rearR += gr * R
            subL += wg * L; subR += wg * R
        st["p"] = p; st["PN"] = PN; st["PS"] = PS; st["w"] = w
        st["w_fast"] = np.bincount(group, weights=w_fast_bin, minlength=G) / np.maximum(np.bincount(group, minlength=G), 1); st["w_member"] = w_member
        if cfg.front_keep_db > 0:
            # front anchor: the front is X - a S (S = the subtraction at full transfer); its 1 s energy
            # Ex - 2a Rxs + a^2 Es must stay >= keep Ex; a = the smaller root, followed 0.3 s down / 2 s back
            keep = 10.0 ** (-cfg.front_keep_db / 10.0)
            inv = 1.0 / max(cold_gain, 1e-3)
            sL_full = subL * inv; sR_full = subR * inv
            anc_ex += a_anc_meas * (float(np.sum(np.abs(L) ** 2 + np.abs(R) ** 2)) - anc_ex)
            anc_rxs += a_anc_meas * (float(np.sum((np.conj(L) * sL_full).real + (np.conj(R) * sR_full).real)) - anc_rxs)
            anc_es += a_anc_meas * (float(np.sum(np.abs(sL_full) ** 2 + np.abs(sR_full) ** 2)) - anc_es)
            disc = anc_rxs * anc_rxs - anc_es * (1.0 - keep) * anc_ex
            a_max = 1.0 if anc_es <= 1e-12 or disc < 0 else (anc_rxs - np.sqrt(disc)) / anc_es
            target = min(max(a_max, 0.0), 1.0)
            anchor += (a_anc_att if target < anchor else a_anc_rel) * (target - anchor)
        st["anchor"] = anchor
        msg = yield (subL, subR, rearL, rearR, st)


def render(x, cfg=SceneConfig(), duck_mask=None, dump=None, inst_mask=None, prot_mask=None):
    """x [n,2] float in [-1,1] at 48 kHz -> out [n,6] float (L R C LFE Ls Rs)."""
    x = np.asarray(x, dtype=np.float64)
    n = x.shape[0]
    N, H = cfg.n_fft, cfg.hop
    XL = stft(x[:, 0], N, H)
    XR = stft(x[:, 1], N, H)
    T, F = XL.shape
    freqs = np.arange(F) * SR / N
    group, G = erb_groups(F, N) if cfg.erb_pool else (np.arange(F), F)
    eng = _engine(cfg, F, G, group, freqs, duck_mask, inst_mask, prot_mask)
    next(eng)
    subL = np.zeros((T, F), complex); subR = np.zeros((T, F), complex)   # what leaves the front
    rearL = np.zeros((T, F), complex); rearR = np.zeros((T, F), complex)
    stats = {"p": np.zeros((T, G)), "PN": np.zeros((T, G)), "PS": np.zeros((T, G)), "w": np.zeros((T, G)),
             "w_fast": np.zeros((T, G)), "w_member": np.zeros((T, G)), "n_clusters": np.zeros(T, int),
             "places": np.full((T, 8, 4), np.nan), "anchor": np.ones(T)}   # per frame up to 8 clusters: centre, sigma, mass, gain; front anchor gain
    for t in range(T):
        subL[t], subR[t], rearL[t], rearR[t], st = eng.send((XL[t], XR[t], t))
        for k in ("p", "PN", "PS", "w", "w_fast", "w_member", "n_clusters", "places", "anchor"):
            if k in st:
                stats[k][t] = st[k]

    fL = x[:, 0] - istft(subL, n, N, H)
    fR = x[:, 1] - istft(subR, n, N, H)
    rL = istft(rearL, n, N, H)
    rR = istft(rearR, n, N, H)
    d = int(round(abs(cfg.rear_delay_ms) * SR / 1000.0))
    if d:
        if cfg.rear_delay_ms > 0:
            rL = np.concatenate([np.zeros(d), rL[:-d]]); rR = np.concatenate([np.zeros(d), rR[:-d]])
        else:
            fL = np.concatenate([np.zeros(d), fL[:-d]]); fR = np.concatenate([np.zeros(d), fR[:-d]])
    if cfg.bleed_cancel > 0:                         # cancel the amp's own front->rear bleed with what we send on Ls/Rs
        o = int(round(cfg.bleed_cancel_offset_ms * SR / 1000.0))
        cL = np.concatenate([np.zeros(o), fL[:n - o]]) if o > 0 else fL
        cR = np.concatenate([np.zeros(o), fR[:n - o]]) if o > 0 else fR
        rL = rL - cfg.bleed_cancel * cL; rR = rR - cfg.bleed_cancel * cR
    z = np.zeros(n)
    out = np.stack([fL, fR, z, z, rL, rR], axis=1)
    if dump:
        np.savez_compressed(dump, **stats, freqs=freqs, group=group)
    return out


class SceneStream:
    """Block-wise driver of the same engine: push([m, 2]) -> [k, 6] output samples. The output lags the
    input by exactly n_fft samples (the analysis frame must be complete), plus the signed rear delay on
    the delayed pair. Overlap-add with the sine window at hop n_fft/4 has a constant window-power sum
    of 2, so a sample is final once the last frame covering it has been added; nothing looks ahead."""

    def __init__(self, cfg=SceneConfig(), duck_mask=None, inst_mask=None, prot_mask=None, keep_stats=False):
        self.cfg = cfg
        self.keep_stats = keep_stats
        self.places_log = []                  # per frame [8, 4] (centre, sigma, mass, gain) when keep_stats
        N, H = cfg.n_fft, cfg.hop
        self.N, self.H = N, H
        F = N // 2 + 1
        freqs = np.arange(F) * SR / N
        group, G = erb_groups(F, N) if cfg.erb_pool else (np.arange(F), F)
        self.eng = _engine(cfg, F, G, group, freqs, duck_mask, inst_mask, prot_mask)
        next(self.eng)
        self.win = np.sin(np.pi * (np.arange(N) + 0.5) / N)
        self.latency = N
        # analysis: pad index space (stft() prepends n_fft zeros to the signal)
        self.inbuf = np.zeros((N, 2))          # the pad; grows as samples are pushed (trimmed per hop)
        self.in_base = 0                       # pad index of inbuf[0]
        self.t = 0                             # next frame to compute (frame t covers pad[tH, tH+N))
        # synthesis: ring buffers in pad index space for subL, subR, rearL, rearR (window-power sum 2)
        self.R = 4 * N
        self.ola = np.zeros((self.R, 4))
        self.emitted = 0                       # pad index of the next output sample to emit
        d = int(round(abs(cfg.rear_delay_ms) * SR / 1000.0))
        self.delay_rear = d if cfg.rear_delay_ms > 0 else 0
        self.delay_front = d if cfg.rear_delay_ms < 0 else 0
        self.dline = np.zeros((max(d, 1), 2))  # delay line for the delayed pair
        self.dpos = 0
        self.pending_reset = False

    def reset(self):
        """Call on a track change (Now Playing): the next frame fades the stage and clears the song memory."""
        self.pending_reset = True

    def _bleed_delay(self, front):
        """The front pair delayed by bleed_cancel_offset_ms through its own ring (0 = aligned)."""
        o = int(round(self.cfg.bleed_cancel_offset_ms * SR / 1000.0))
        if o <= 0:
            return front
        if not hasattr(self, "_bline"):
            self._bline = np.zeros((o, 2)); self._bpos = 0
        out = np.empty_like(front)
        for i in range(len(front)):
            out[i] = self._bline[self._bpos]; self._bline[self._bpos] = front[i]; self._bpos = (self._bpos + 1) % o
        return out

    def _delay(self, pair):
        """Delay a [k, 2] block by d samples through the ring delay line (d = len(dline))."""
        d = self.delay_rear or self.delay_front
        if not d:
            return pair
        out = np.empty_like(pair)
        for i in range(len(pair)):
            out[i] = self.dline[self.dpos]
            self.dline[self.dpos] = pair[i]
            self.dpos = (self.dpos + 1) % d
        return out

    def push(self, block):
        block = np.asarray(block, dtype=np.float64).reshape(-1, 2)
        self.inbuf = np.concatenate([self.inbuf, block])
        N, H, R = self.N, self.H, self.R
        outs = []
        # frame t needs pad[tH : tH+N] -> inbuf[tH - in_base : tH - in_base + N]
        while self.t * H - self.in_base + N <= len(self.inbuf):
            t = self.t
            seg = self.inbuf[t * H - self.in_base: t * H - self.in_base + N]
            L = np.fft.rfft(seg[:, 0] * self.win); Rr = np.fft.rfft(seg[:, 1] * self.win)
            ctl = {"reset": True} if self.pending_reset else None
            self.pending_reset = False
            subL, subR, rearL, rearR, st_t = self.eng.send((L, Rr, t, ctl))
            if self.keep_stats:
                self.places_log.append(st_t["places"])
            fr = np.stack([np.fft.irfft(v, n=N) * self.win for v in (subL, subR, rearL, rearR)], axis=1)
            idx = (t * H + np.arange(N)) % R
            self.ola[idx] += fr
            # pad indices [tH, (t+1)H) are final now (the last covering frame is t)
            j0, j1 = t * H, (t + 1) * H
            fin = self.ola[(j0 + np.arange(H)) % R] / 2.0
            self.ola[(j0 + np.arange(H)) % R] = 0.0
            xin = self.inbuf[j0 - self.in_base: j1 - self.in_base]
            i0 = j0 - N                        # signal index of the first sample of this hop
            if i0 + H > 0:
                k0 = max(0, -i0)
                front = xin[k0:] - fin[k0:, 0:2]
                rear = fin[k0:, 2:4]
                if self.delay_rear:
                    rear = self._delay(rear)
                elif self.delay_front:
                    front = self._delay(front)
                if self.cfg.bleed_cancel > 0:
                    rear = rear - self.cfg.bleed_cancel * self._bleed_delay(front)
                z = np.zeros((len(front), 1))
                outs.append(np.concatenate([front, z, z, rear], axis=1))
            self.t += 1
            # trim the analysis buffer: frames >= t+1 start at (t+1)H
            keep_from = (t + 1) * H - self.in_base
            if keep_from > 0:
                self.inbuf = self.inbuf[keep_from:]
                self.in_base += keep_from
        return np.concatenate(outs) if outs else np.zeros((0, 6))

    def flush(self):
        """Push n_fft zeros so every sample already received comes out (the tail of a finite stream)."""
        return self.push(np.zeros((self.N, 2)))


def stream_render(x, cfg=SceneConfig(), block=768, duck_mask=None, inst_mask=None, prot_mask=None):
    """render() through SceneStream in blocks of `block` samples -> [n, 6], aligned like render()."""
    x = np.asarray(x, dtype=np.float64)
    n = x.shape[0]
    st = SceneStream(cfg, duck_mask, inst_mask, prot_mask)
    parts = [st.push(x[i:i + block]) for i in range(0, n, block)]
    parts.append(st.flush())
    return np.concatenate(parts)[:n]


def decompose(x, cfg=SceneConfig()):
    """Test hook: the per-frame statistics dump of a render (p, PN, PS, w, w_fast, w_member, n_clusters)."""
    import io
    buf = io.BytesIO()
    render(x, cfg, dump=buf)
    buf.seek(0)
    return dict(np.load(buf))


# ----------------------------------------------------------------------------- io / cli
def read_wav(path):
    with wave.open(path, "rb") as w:
        assert w.getframerate() == SR, (path, w.getframerate())
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, w.getnchannels()).astype(np.float64) / 32768.0
    if x.shape[1] == 1:
        x = np.repeat(x, 2, 1)
    return x[:, :2]


def write_wav6(path, y):
    pcm = (np.clip(y, -1.0, 1.0) * 32767.0).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(6); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())


def read_hssrmsk(path):
    """HSSRMSK0: 8-byte magic, uint32 [T, 257], float32 protect values (mask_net.write_mask)."""
    b = open(path, "rb").read()
    assert b[:8] == b"HSSRMSK0", path
    T, F = np.frombuffer(b[8:16], np.uint32)
    return np.frombuffer(b[16:16 + 4 * T * F], np.float32).reshape(T, F).astype(np.float64)


def config_from_args(argv):
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("--n-fft", type=int, default=1024)
    ap.add_argument("--alpha", type=float, default=0.7)
    ap.add_argument("--surround", type=float, default=0.7)
    ap.add_argument("--ambient-gain", type=float, default=1.0)
    ap.add_argument("--air-db", type=float, default=-9.0)
    ap.add_argument("--rear-delay-ms", type=float, default=0.0)
    ap.add_argument("--bleed-cancel", type=float, default=0.0)
    ap.add_argument("--front-keep-db", type=float, default=0.0)
    ap.add_argument("--bleed-cancel-offset-ms", type=float, default=0.0)
    ap.add_argument("--pan-lo", type=float, default=0.10)
    ap.add_argument("--pan-hi", type=float, default=0.35)
    ap.add_argument("--tau-slow-ms", type=float, default=400.0)
    ap.add_argument("--tau-fast-ms", type=float, default=8.0)
    ap.add_argument("--tau-dir-ms", type=float, default=300.0)
    ap.add_argument("--release-ms", type=float, default=2000.0)
    ap.add_argument("--no-cluster", action="store_true")
    ap.add_argument("--no-fast-protect", action="store_true")
    ap.add_argument("--no-energy-conserving", action="store_true")
    ap.add_argument("--no-decorrelate", action="store_true")
    ap.add_argument("--front-follows", default="slow")
    ap.add_argument("--wide-pair", action="store_true")
    ap.add_argument("--sigma-min", type=float, default=0.08)
    ap.add_argument("--no-member-mass", action="store_true")
    ap.add_argument("--side-lp-hz", type=float, default=7000.0)
    ap.add_argument("--no-unmix", action="store_true")
    ap.add_argument("--unmix-lambda", type=float, default=0.05)
    ap.add_argument("--duck-mask", default=None, help="npz/npy [T,F] in 0..1 (external vocal posterior)")
    ap.add_argument("--protect-mask", default=None, help="HSSRMSK0 file from the stem harness (eligibility = 1 - protect) -> semantic instrument stage")
    ap.add_argument("--instrument-mask", default=None, help="npy [Tc,257] eligibility on the 512-sample control grid (tests)")
    ap.add_argument("--inst-alpha", type=float, default=0.9)
    ap.add_argument("--inst-attack-ms", type=float, default=300.0)
    ap.add_argument("--inst-release-ms", type=float, default=30.0)
    ap.add_argument("--inst-mask-nfft", type=int, default=512)
    ap.add_argument("--inst-mask-hop", type=int, default=512)
    ap.add_argument("--inst-gamma", type=float, default=1.0)
    ap.add_argument("--inst-thresh", type=float, default=0.0)
    ap.add_argument("--inst-long-tau-s", type=float, default=0.0)
    ap.add_argument("--inst-gamma-lo", type=float, default=0.0)
    ap.add_argument("--inst-gamma-split-hz", type=float, default=700.0)
    ap.add_argument("--duck-depth", type=float, default=0.25)
    ap.add_argument("--no-duck-from-mask", action="store_true")
    ap.add_argument("--dump", default=None)
    ns, rest = ap.parse_known_args(argv)
    pos = [r for r in rest if not r.startswith("-")]
    cfg = SceneConfig(n_fft=ns.n_fft, hop=ns.n_fft // 4, alpha=ns.alpha, surround=ns.surround,
                      ambient_gain=ns.ambient_gain, air_db=ns.air_db, rear_delay_ms=ns.rear_delay_ms,
                      pan_lo=ns.pan_lo, pan_hi=ns.pan_hi, tau_slow_ms=ns.tau_slow_ms, tau_fast_ms=ns.tau_fast_ms, tau_dir_ms=ns.tau_dir_ms, release_ms=ns.release_ms,
                      cluster=not ns.no_cluster, fast_protect=not ns.no_fast_protect,
                      energy_conserving=not ns.no_energy_conserving, decorrelate_ambient=not ns.no_decorrelate,
                      front_follows=ns.front_follows, wide_pair=ns.wide_pair, unmix=not ns.no_unmix, unmix_lambda=ns.unmix_lambda,
                      sigma_min=ns.sigma_min, member_mass=not ns.no_member_mass, side_lp_hz=ns.side_lp_hz,
                      inst_alpha=ns.inst_alpha, inst_attack_ms=ns.inst_attack_ms, inst_release_ms=ns.inst_release_ms,
                      inst_mask_nfft=ns.inst_mask_nfft, inst_mask_hop=ns.inst_mask_hop, inst_gamma=ns.inst_gamma, inst_thresh=ns.inst_thresh,
                      duck_depth=ns.duck_depth, duck_from_mask=not ns.no_duck_from_mask, inst_long_tau_s=ns.inst_long_tau_s,
                      inst_gamma_lo=ns.inst_gamma_lo, inst_gamma_split_hz=ns.inst_gamma_split_hz,
                      bleed_cancel=ns.bleed_cancel, bleed_cancel_offset_ms=ns.bleed_cancel_offset_ms, front_keep_db=ns.front_keep_db)
    return cfg, ns, pos


def main(argv):
    cfg, ns, pos = config_from_args(argv)
    if len(pos) < 2:
        print(__doc__); return 2
    src, dst = pos[-2], pos[-1]
    x = read_wav(src)
    mask = None
    if ns.duck_mask:
        mask = np.load(ns.duck_mask)
        if hasattr(mask, "files"):
            mask = mask[mask.files[0]]
        assert mask.shape[1] == cfg.n_fft // 2 + 1, (mask.shape, cfg.n_fft)
    inst = None; prot = None
    if ns.protect_mask:
        prot = read_hssrmsk(ns.protect_mask); inst = 1.0 - prot
    elif ns.instrument_mask:
        inst = np.load(ns.instrument_mask).astype(np.float64); prot = 1.0 - inst
    y = render(x, cfg, duck_mask=mask, dump=ns.dump, inst_mask=inst, prot_mask=prot)
    write_wav6(dst, y)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
