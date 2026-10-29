#!/usr/bin/env python3
"""Golden fixtures for the C port of scene_upmix.py (spec §18).

  python3 scene_port_fixtures.py OUT_DIR

Writes:
  functions.npz   random inputs -> outputs of the pure per-frame functions (decompose_frame,
                  centre_removed_pan, smoothstep, ema_alpha), the tables (erb_groups, phase_table, the
                  sine window, freqs) and an STFT/ISTFT round trip, all at the default config (1024/256).
  stream_*.npz    block-exact goldens of SceneStream on two scenes: the per-frame analysis frames
                  XL/XR (what the C framer must produce), the engine's per-frame outputs subL/subR/
                  rearL/rearR, the per-frame stats (p, PN, PS, w, w_fast, w_member, n_clusters, places)
                  and the final [n,6] output, plus the input signal and the config as JSON.
                  stream_spatial: pluck hard-left + centred vocal + room bed, no mask.
                  stream_mask:    the same with the 512/128 oracle instrument mask + duck (variant J).
"""
import json, os, sys, dataclasses, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scene_upmix as su, upmix_behavior_tests as ub, test_scene_upmix as ts

out = sys.argv[1]; os.makedirs(out, exist_ok=True)
rng = np.random.default_rng(2026)
cfg = su.SceneConfig()
N, H = cfg.n_fft, cfg.hop; F = N // 2 + 1
freqs = np.arange(F) * su.SR / N
group, G = su.erb_groups(F, N)

# ---------------------------------------------------------------- pure functions
n = 4096
PL = rng.random(n) * 1e-3; PR = rng.random(n) * 1e-3
C = (rng.standard_normal(n) + 1j * rng.standard_normal(n)) * np.sqrt(PL * PR) * rng.random(n)
PN, PS, a, b, th, Pa, Pb = su.decompose_frame(PL, PR, C)
eps = 1e-4 * float((PL + PR).max())
p_cr, e_cr, v_cr = su.centre_removed_pan(PL, PR, C, eps, PN)
x_ss = rng.standard_normal(n) * 2
np.savez_compressed(os.path.join(out, "functions.npz"),
    PL=PL, PR=PR, C=C, PN=PN, PS=PS, a=a, b=b, theta=th, Pa=Pa, Pb=Pb, eps=eps, p_cr=p_cr, e_cr=e_cr, v_cr=v_cr,
    ss_x=x_ss, ss_lo=-0.5, ss_hi=1.5, ss_y=su.smoothstep(x_ss, -0.5, 1.5),
    ema_tau_ms=np.array([8.0, 43.0, 300.0, 400.0, 2000.0, 30.0]), ema_alpha=np.array([su.ema_alpha(t, H) for t in (8.0, 43.0, 300.0, 400.0, 2000.0, 30.0)]),
    group=group, G=G, freqs=freqs, window=np.sin(np.pi * (np.arange(N) + 0.5) / N),
    phL=su.phase_table(F, cfg.seed, +1.0), phR=su.phase_table(F, cfg.seed, -1.0),
    stft_in=(sig := rng.standard_normal(48000) * 0.1), stft_frames=su.stft(sig, N, H), istft_out=su.istft(su.stft(sig, N, H), len(sig), N, H))

# ---------------------------------------------------------------- stream goldens
def golden(name, x, cfg, inst_mask=None, prot_mask=None):
    xs = np.asarray(x, dtype=np.float64)
    XL = su.stft(xs[:, 0], N, H); XR = su.stft(xs[:, 1], N, H); T = XL.shape[0]
    eng = su._engine(cfg, F, G, group, freqs, None, inst_mask, prot_mask); next(eng)
    subL = np.zeros((T, F), complex); subR = np.zeros((T, F), complex); rearL = np.zeros((T, F), complex); rearR = np.zeros((T, F), complex)
    stats = {"p": np.zeros((T, G)), "PN": np.zeros((T, G)), "PS": np.zeros((T, G)), "w": np.zeros((T, G)), "w_fast": np.zeros((T, G)),
             "w_member": np.zeros((T, G)), "n_clusters": np.zeros(T, int), "places": np.full((T, 8, 4), np.nan)}
    for t in range(T):
        subL[t], subR[t], rearL[t], rearR[t], st = eng.send((XL[t], XR[t], t))
        for k in stats:
            if k in st: stats[k][t] = st[k]
    y = su.stream_render(xs, cfg, block=768, inst_mask=inst_mask, prot_mask=prot_mask)
    ref = su.render(xs, cfg, inst_mask=inst_mask, prot_mask=prot_mask)
    assert np.abs(y - ref).max() < 1e-9
    np.savez_compressed(os.path.join(out, f"stream_{name}.npz"), x=xs, XL=XL, XR=XR, subL=subL, subR=subR, rearL=rearL, rearR=rearR,
                        out6=y, **stats, config=json.dumps(dataclasses.asdict(cfg)),
                        **({"inst_mask": inst_mask, "prot_mask": prot_mask} if inst_mask is not None else {}))
    print(name, "frames", T, "samples", len(xs))

ts.seed(); pluck, voc, bed = ts._scene_2()
x = (pluck + voc + bed).T[: su.SR * 6]
golden("spatial", x, cfg)
# mask case: 512/128 x4 oracle eligibility of the vocal vs the rest (the vocal is 'protected', everything else eligible)
def spec(v):
    w = np.hanning(512); idx = np.arange(0, v.shape[1] - 512, 128)
    return sum(np.abs(np.fft.rfft(np.stack([v[c][i:i + 512] * w for i in idx]), axis=1)) ** 2 for c in range(2))
Pv = spec(voc[:, : su.SR * 6]); Po = spec((pluck + bed)[:, : su.SR * 6])
e = Po / (Pv + Po + 1e-12); Tc = e.shape[0] // 4 * 4; elig = e[:Tc].reshape(Tc // 4, 4, 257).mean(1)
golden("mask", x, cfg, inst_mask=elig, prot_mask=1.0 - elig)
print("fixtures in", out)
