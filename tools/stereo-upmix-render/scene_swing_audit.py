#!/usr/bin/env python3
"""Swing audit on dumped DEV40 stems (scene_dev40_dump_stems.py on the PC, see docs/audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md §15): render the mix with scene_upmix (alpha 0.7), measure the
harness's per-50 ms rear share of one stem, and print its 1 s medians next to the renderer's own
internals (admitted places, member weight in the stem's groups, pooled pan, Wiener w)."""
import sys, os, json, io, numpy as np
sys.path.insert(0, os.getcwd())
import scene_upmix as SU, upmix_behavior_tests as T
npz, stem_name = sys.argv[1], sys.argv[2]
alpha = float(sys.argv[3]) if len(sys.argv) > 3 else 0.7
over = {k: (float(v) if v not in ('True','False') else v == 'True') for k, v in (a.split('=') for a in sys.argv[4:])}
z = np.load(npz); names = list(z["names"]); meta = json.loads(str(z["meta"]))
stems = {s: z["stem_" + s].astype(np.float64) for s in names}
mix = sum(stems.values()); lead = stems[stem_name]; rest = mix - lead
cfg = SU.SceneConfig(alpha=alpha, **over)
buf = io.BytesIO(); out6 = SU.render(mix.T, cfg, dump=buf)            # render takes [n, 2]
out6 = out6.T if out6.shape[0] != 6 else out6
buf.seek(0); st = np.load(buf)
share, w, dt = T.rear_share(lead, rest, out6)
m = T.metrics(share, w, dt, np.ones(len(share), bool))
print(f"{meta.get('artist')} - {meta.get('song')}  stem={stem_name} alpha={alpha}: median {m['median_rear']:.3f} range_1s {m['range_1s']:.3f} onset_jump {m.get('onset_jump', float('nan')):.3f}")
# per-second ownership: ERB groups where the stem's power beats the rest by 3x within that second
freqs = st["freqs"]; group = st["group"]; G = int(group.max()) + 1
win = np.hanning(cfg.n_fft)
def gpow(x, s0):
    seg = x[0][int(s0 * 48000):int(s0 * 48000) + cfg.n_fft * 45].reshape(-1, cfg.n_fft) * win
    P = (np.abs(np.fft.rfft(seg, axis=1)) ** 2).mean(0)
    return np.bincount(group, weights=P, minlength=G)
hop_s = cfg.hop / 48000.0
nfr = st["w_member"].shape[0]
print(f"{'s':>3s} {'rear':>5s} {'places':>6s} {'nown':>4s} {'w_memb':>6s} {'w':>5s} {'pan':>5s} {'u/front':>7s}")
ps = np.asarray(share); pdt = dt
for s0 in range(0, 60):
    i0, i1 = int(s0 / pdt), int((s0 + 1) / pdt)
    f0, f1 = int(s0 / hop_s), min(int((s0 + 1) / hop_s), nfr)
    if i1 > len(ps) or f1 <= f0 or (s0 + 1) * 48000 > lead.shape[1]: break
    gl, gr = gpow(lead, s0), gpow(rest, s0)
    own = np.where((gl > 3 * gr) & (gl > 1e-6 * gl.max()))[0]
    r = np.nanmedian(ps[i0:i1])
    if len(own):
        wm = np.nanmean(st["w_member"][f0:f1][:, own]); ww = np.nanmean(st["w"][f0:f1][:, own])
        pp = np.nanmean(st["p"][f0:f1][:, own]); sn = np.nanmean(st["PN"][f0:f1][:, own]) / (np.nanmean(st["PS"][f0:f1][:, own]) + 1e-12)
    else:
        wm = ww = pp = sn = float('nan')
    nc = np.nanmean(st["n_clusters"][f0:f1])
    pl = st["places"][min(f1 - 1, nfr - 1)]
    pls = " ".join(f"[{c:+.2f} s{sg:.2f} m{m:.2f} g{g:.1f}]" for c, sg, m, g in pl if not np.isnan(c))
    print(f"{s0:3d} {r:5.2f} {nc:6.1f} {len(own):4d} {wm:6.2f} {ww:5.2f} {pp:+5.2f} {sn:7.2f}  {pls}")
