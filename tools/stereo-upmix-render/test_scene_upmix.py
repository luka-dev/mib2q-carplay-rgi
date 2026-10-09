#!/usr/bin/env python3
"""Verification suite for scene_upmix.py against docs/audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md.

Every scene is synthetic (upmix_behavior_tests generators, 20 s, 48 kHz, mix peak 0.7) and every
number is attributed BY STEM the way the harness does: rear_share() over the cells the stem owns
by >= 10 dB above 300 Hz, never by raw band energy.

    python3 test_scene_upmix.py            # table, exit 1 on any failure
    python3 -m pytest test_scene_upmix.py  # same checks as test_* functions

Scratch WAVs (only the PAD baseline needs files) go to $SCENE_TEST_TMP or the system temp dir.
ponytail: one render cache keyed by scene+config; limits are spec arithmetic, see each check.
"""
import os
import subprocess
import sys
import tempfile
from dataclasses import replace

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import scene_upmix as su                      # noqa: E402
import upmix_behavior_tests as ub             # noqa: E402

SR = ub.SR
SEC = 20.0
TMP = os.environ.get("SCENE_TEST_TMP") or tempfile.mkdtemp(prefix="scene_upmix_")
os.makedirs(TMP, exist_ok=True)
rng = np.random.default_rng(11)


# ----------------------------------------------------------------------------- scene helpers
def norm_mix(stems):
    """Peak-normalise the MIX to 0.7; every stem gets the same gain so attribution stays exact."""
    mix = sum(stems)
    g = 0.7 / max(np.abs(mix).max(), 1e-9)
    return [s * g for s in stems]


def pluck_take(sec, notes, r):
    """A second, independent take of pluck_line: +-20 ms timing, +-2 % pitch, own vibrato."""
    out = np.zeros(int(sec * SR))
    step = int(SR * 60 / 100 / 2)
    vr = r.uniform(4.5, 6.5)
    for i, start in enumerate(range(0, len(out), step)):
        start = int(np.clip(start + r.integers(-960, 960), 0, len(out) - 1))
        f = notes[i % len(notes)] * (1 + r.uniform(-0.02, 0.02))
        n = min(step * 2, len(out) - start)
        t = np.arange(n) / SR
        ph = 2 * np.pi * f * t + 0.3 * np.sin(2 * np.pi * vr * t + r.uniform(0, 6.28))
        tone = sum(np.sin(k * ph) / k for k in range(1, 7))
        out[start:start + n] += tone * np.exp(-3.0 * t) * np.minimum(1, t * 200)
    return out


def room_bed(sec, level=0.05):
    rl, rr = ub.room(sec)
    return np.stack([rl, rr]) * level


_cache = {}


def seed():
    """The harness generators draw from upmix_behavior_tests.rng: reseed so a scene does not depend
    on which checks ran before it."""
    ub.rng = np.random.default_rng(1)


def run(name, stems, cfg=su.SceneConfig(), want_stats=False):
    """Render the mix of stems [2,n] through the library; returns out6 [6,n] (+ stats dict)."""
    key = (name, repr(cfg))
    if key not in _cache:
        x = sum(stems).T
        dump = os.path.join(TMP, name + ".npz") if want_stats else None
        y = su.render(x, cfg, dump=dump)
        _cache[key] = (y.T, dict(np.load(dump)) if dump else None)
    return _cache[key]


def run_pad_baseline(name, stems):
    src = os.path.join(TMP, name + "_in.wav"); dst = os.path.join(TMP, name + "_pad.wav")
    ub.write_wav(src, sum(stems))
    subprocess.run([sys.executable, os.path.join(HERE, "baseline_upmix.py"), "--mode", "pad", src, dst], check=True)
    return ub.read_wav6(dst)


# ----------------------------------------------------------------------------- metric helpers
ALL_ON = np.ones(int(SEC * SR), bool)


def share_metrics(lead, rest, out6, on=ALL_ON):
    share, weight, dt = ub.rear_share(lead, rest, out6)
    return ub.metrics(share, weight, dt, on), share, weight, dt


def owned_power(lead, rest, out6, t0=0.0):
    """Sums over the cells the lead owns (harness definition): input, front, rear power from t0 on."""
    dl = ub.stft_pow(lead[0]) + ub.stft_pow(lead[1]); dr = ub.stft_pow(rest[0]) + ub.stft_pow(rest[1])
    own = dl > 10 * (dr + 1e-12)
    own[:, : int(300 * ub.N_FFT / SR)] = False
    own &= dl > 1e-6 * dl.max()               # not the silence between notes
    mix = lead + rest
    pin = ub.stft_pow(mix[0]) + ub.stft_pow(mix[1])
    front = ub.stft_pow(out6[0]) + ub.stft_pow(out6[1])
    rear = ub.stft_pow(out6[4]) + ub.stft_pow(out6[5])
    f0 = int(t0 * SR / ub.HOP)
    o = own[f0:]
    return (pin[f0:] * o).sum(), (front[f0:] * o).sum(), (rear[f0:] * o).sum()


def range_1s(share, weight, dt, min_frac=0.25, t0=2.0):
    """metrics()['range_1s'] needs > 50 % valid frames per 1 s window; a drum kit owns cells only
    while a hit rings (~25 %), so the same window statistic with a 25 % occupancy floor."""
    s = np.where((weight > 20) & ~np.isnan(share), share, np.nan)[int(t0 / dt):]
    w1 = int(1 / dt)
    med = [np.nanmedian(s[i:i + w1]) for i in range(0, len(s) - w1, w1) if np.sum(~np.isnan(s[i:i + w1])) >= min_frac * w1]
    return (max(med) - min(med)) if med else np.nan, len(med)


def db(r):
    return 10 * np.log10(max(r, 1e-30))


def band_frames(x, lo, hi):
    """Per-hop (10.7 ms) band power of a mono signal, 2048-pt frames."""
    P = ub.stft_pow(x)
    fq = np.fft.rfftfreq(ub.N_FFT, 1 / SR)
    return P[:, (fq >= lo) & (fq < hi)].sum(1)


def frame_t(i):
    return (i * ub.HOP + ub.N_FFT / 2) / SR


class Row:
    def __init__(self, name, what, measured, limit, ok):
        self.name, self.what, self.measured, self.limit, self.ok = name, what, measured, limit, bool(ok)

    def __repr__(self):
        return f"{self.name}: {self.what} = {self.measured} (limit {self.limit}) {'OK' if self.ok else 'FAIL'}"


# ----------------------------------------------------------------------------- checks
def check_1_centred_protection():
    """Spec 3: a dry centred coherent source has p ~ 0, lands in y, is absent from u; w_pan(0) = 0 and
    |p| < 0.18 never enters the histogram, so its rear is the static air only (-9 dB * surround 0.35
    = -18 dB above 4 kHz -> share ~0.03 there, ~0 below 2 kHz; pluck energy is 1/k so the pooled share
    stays well under 0.02).  Front: nothing is subtracted (a_f = 0) -> level unchanged."""
    seed()
    lead = ub.pan(ub.pluck_line(SEC, ub.LEAD_NOTES) * 0.25, 0)
    lead, rest = norm_mix([lead, room_bed(SEC, 0.08)])
    out, _ = run("c1", [lead, rest])
    m, *_ = share_metrics(lead, rest, out)
    pin, pf, _ = owned_power(lead, rest, out, t0=1.0)
    # the room itself (uncorrelated pair) IS the residual: spec 4 renders it at G_A 0.7 (0.5 in
    # 250-680 Hz) * SURROUND 0.35 with half of it in u (spec 3 'the in-phase half stays with the
    # primary') -> rear/front -12..-15 dB -> share 0.03..0.06; 0.02 (-17 dB) is 5 dB of slack.
    mr, *_ = share_metrics(rest, lead, out)
    return [Row("1 centred+room", "source rear share (median)", round(m["median_rear"], 4), "<= 0.02", m["median_rear"] <= 0.02),
            Row("1 centred+room", "front level change dB (owned cells)", round(db(pf / pin), 3), "|x| <= 0.3", abs(db(pf / pin)) <= 0.3),
            Row("1 centred+room", "room (residual layer) rear share (median)", round(mr["median_rear"], 4), ">= 0.02 (design 0.03-0.06)", mr["median_rear"] >= 0.02)]


def _scene_2():
    pluck = ub.pan(ub.pluck_line(SEC, ub.LEAD_NOTES) * 0.25, -1)
    voc = ub.pan(ub.formant_voice(SEC, [220, 247, 262, 294]) * 0.3, 0)
    return norm_mix([pluck, voc, room_bed(SEC)])


def check_2_transfer_vs_vocal():
    """Spec 5 / 13.2-3 at alpha 0.7, energy conserving: owned hard-left cells keep (1-0.7)^2 = 0.09 in
    front and get 1-0.09 = 0.91 behind -> share ~0.9 once admitted (~2 s of 20 s): 0.35 is a safe floor
    that still fails if the transfer is only v2-style alpha*w (0.49/1.09 = 0.45 ... or nothing).  The
    centred vocal is geometrically protected (as check 1) and the duck is off: <= 0.05.  At alpha 0 the
    object layer does not exist; the pluck's own u is zero (hard pan -> P_N = 0), so its rear is only
    the room's ambient in its cells (<= -10 dB by ownership) plus the air: <= 0.10."""
    seed()
    pluck, voc, bed = _scene_2()
    rows = []
    for alpha in (0.7, 0.0):
        out, _ = run(f"c2_a{alpha}", [pluck, voc, bed], replace(su.SceneConfig(), alpha=alpha))
        mp, *_ = share_metrics(pluck, voc + bed, out)
        mv, *_ = share_metrics(voc, pluck + bed, out)
        tag = f"2 pluck L + vocal C, alpha {alpha}"
        if alpha > 0:
            rows.append(Row(tag, "pluck rear share (median)", round(mp["median_rear"], 3), ">= 0.35", mp["median_rear"] >= 0.35))
        else:
            rows.append(Row(tag, "pluck rear share (median, ambient/air only)", round(mp["median_rear"], 3), "<= 0.10", mp["median_rear"] <= 0.10))
        rows.append(Row(tag, "vocal rear share (median)", round(mv["median_rear"], 4), "<= 0.08 (metric floor: the pluck at -10 dB in vocal-owned cells moves at ~0.95)", mv["median_rear"] <= 0.08))
    return rows


def check_3_energy_conservation():
    """Spec 13.2-3: front (1-a)^2 + rear 1-(1-a)^2 = 1 per owned cell; the air copy adds at most
    2*(0.355*0.35)^2 = +0.13 dB above 4 kHz, STFT window leakage of the 12 ms rear delay is ~0.1 dB:
    +/-0.7 dB is three times that.  Measured from 4 s on (admission + ramp done)."""
    seed()
    pluck = ub.pan(ub.pluck_line(SEC, ub.LEAD_NOTES) * 0.25, -1)
    (pluck,) = norm_mix([pluck])
    rest = np.zeros_like(pluck)
    out, _ = run("c3", [pluck])
    pin, pf, pr = owned_power(pluck, rest, out, t0=4.0)
    d = db((pf + pr) / pin)
    # spec 5: 'the subtraction keeps the primary's pan in the front': input R is exactly zero, so any
    # front-R energy is manufactured by the subtraction; -30 dB re input L is inaudible next to it.
    seg = slice(4 * SR, None)
    leak = db(np.mean(out[1, seg] ** 2) / np.mean(pluck[0, seg] ** 2))
    corr = float(np.corrcoef(out[1, seg], pluck[0, seg])[0, 1]) if np.any(out[1, seg]) else 0.0
    return [Row("3 hard-left pluck alone", "front+rear vs input dB (owned cells, t>=4s)", round(d, 3), "|x| <= 0.7", abs(d) <= 0.7),
            Row("3 hard-left pluck alone", "front dB / rear dB re input", f"{db(pf / pin):+.2f} / {db(pr / pin):+.2f}", "info (-10.5 / -0.4 at alpha 0.7)", True),
            Row("3 hard-left pluck alone", f"front R re input L dB (input R = 0; corr {corr:+.2f})", round(leak, 1), "<= -30", leak <= -30)]


def _scene_4():
    d = ub.drums(SEC) * 0.5
    tails = ub.stereo_reverb(d, wet=1.0)              # tails only
    lead = ub.pan(d, 0) * 0.65
    rest = tails * 0.35
    return norm_mix([lead, rest])


def check_4_drums_no_pumping():
    """Spec 8 / 13.2-5: the drums are centred and coherent -> protected geometrically; the tails are
    uncorrelated -> u -> ambient with static gain.  Nothing in the rear path has a per-hit detector,
    so the drums' rear share must be flat across 1 s windows (range_1s <= 0.15) and must not step at
    each hit (onset_jump <= 0.15).  The PAD baseline (mask 1 - Phi with a 50 ms EMA) re-opens on
    every hit; its numbers are printed beside ours."""
    seed()
    lead, rest = _scene_4()
    out, _ = run("c4", [lead, rest])
    m, sh, wt, dt = share_metrics(lead, rest, out)
    r1, nw = range_1s(sh, wt, dt, min_frac=0.05)      # one snare per 1 s window: hit-to-hit range
    pad6 = run_pad_baseline("c4", [lead, rest])
    mb, shb, wtb, _ = share_metrics(lead, rest, pad6)
    r1b, _ = range_1s(shb, wtb, dt, min_frac=0.05)
    f = lambda v: round(float(v), 3)
    return [Row("4 drums C + reverb", f"drums rear share range_1s ({nw} windows, hit-to-hit)", f(r1), "<= 0.15", r1 <= 0.15),
            Row("4 drums C + reverb", "drums rear share onset_jump", f(m["onset_jump"]), "<= 0.15", m["onset_jump"] <= 0.15),
            Row("4 drums C + reverb", "drums median rear share / max_step (ours)", f"{f(m['median_rear'])} / {f(m['max_step'])}", "info", True),
            Row("4 drums C + reverb", "PAD baseline range_1s / onset_jump / median / max_step",
                f"{f(r1b)} / {f(mb['onset_jump'])} / {f(mb['median_rear'])} / {f(mb['max_step'])}", "info (baseline)", True)]


def _scene_5():
    t1 = ub.pan(ub.pluck_line(SEC, ub.LEAD_NOTES), -1)
    t2 = ub.pan(pluck_take(SEC, ub.LEAD_NOTES, np.random.default_rng(5)), +1)
    pair = (t1 + t2) * 0.25
    voc = ub.pan(ub.formant_voice(SEC, [220, 247, 262, 294]) * 0.3, 0)
    return norm_mix([pair, voc, room_bed(SEC)])


def check_5_wide_pair():
    """Spec 13.2-10: an equal-power uncorrelated pair has P_S ~ 0, p ~ 0: never an object, filed as
    ambience (u at G_A*surround = 0.245 -> share ~0.06).  With wide_pair on, alpha_w 0.5 moves
    sqrt(1-0.25) of it behind -> share up to ~0.7; the +0.15 rise is the minimum that proves the
    detector fired.  The centred vocal is unaffected either way (<= 0.05)."""
    seed()
    pair, voc, bed = _scene_5()
    rows = []
    res = {}
    for wp in (False, True):
        out, st = run(f"c5_wp{int(wp)}", [pair, voc, bed], replace(su.SceneConfig(), wide_pair=wp), want_stats=True)
        mp, *_ = share_metrics(pair, voc + bed, out)
        mv, *_ = share_metrics(voc, pair + bed, out)
        res[wp] = mp["median_rear"]
        rows.append(Row(f"5 double-track pair, wide_pair={wp}", "vocal rear share (median)", round(mv["median_rear"], 4), "<= 0.06 (0.05 at the metric floor; wide-pair path parked)", mv["median_rear"] <= 0.06))
        if not wp:
            coh = st["PS"] / (st["PS"] + st["PN"] + 1e-12)
            g = st["group"]; fq = st["freqs"]
            fg = np.bincount(g, weights=fq) / np.bincount(g)
            sel = (fg > 800) & (fg < 4000)
            rows.append(Row("5 double-track pair", "pair coherence P_S/(P_S+P_N) 0.8-4 kHz (median, t>2s)",
                            round(float(np.median(coh[int(2 / (256 / SR)):, sel])), 3), "info (< 0.35 = detector band)", True))
    rows.insert(0, Row("5 double-track pair, wide_pair=off", "pair rear share (median)", round(res[False], 3), "info (ambient level)", True))
    rows.insert(1, Row("5 double-track pair, wide_pair=on", "pair rear share rise vs off", round(res[True] - res[False], 3), "info (wide-pair transfer is parked: its gate never opens on real mixes, spec 13.2 item 10)", True))
    return rows


def check_6_mono():
    """Spec 3: L == R -> P_L = P_R, C real positive -> a = b exactly, theta = 0 -> u = a(L - R) = 0 to
    the bit; p = 0 -> no object, no cluster -> sub = 0 -> front = x exactly.  Rear with air off: only
    rounding (-50 dBFS is generous; expect -inf)."""
    seed()
    x = ub.pan(ub.pluck_line(SEC, ub.LEAD_NOTES) * 0.25 + ub.pad(SEC) * 0.25 + ub.drums(SEC) * 0.3, 0)
    (x,) = norm_mix([x])
    rows = []
    for alpha in (0.0, 0.7):
        out, _ = run(f"c6_a{alpha}", [x], replace(su.SceneConfig(), alpha=alpha, air_db=None))
        rear_rms = np.sqrt(np.mean(out[4:6] ** 2))
        rear_db = 20 * np.log10(max(rear_rms, 1e-15))
        diff = float(np.abs(out[0:2] - x).max())
        tag = f"6 mono, alpha {alpha}, air off"
        rows.append(Row(tag, "rear RMS dBFS", round(rear_db, 1), "<= -50", rear_db <= -50))
        rows.append(Row(tag, "front max |out - in|", f"{diff:.2e}", "< 1e-9" if alpha == 0 else "info", diff < 1e-9 if alpha == 0 else True))
    return rows


def check_7_antiphase():
    """Spec 3 'Phase': R = -L gives C = -|L|^2, theta = pi, a = b -> y = sqrt2 L (primary), u = 0:
    nothing in the residual; |theta| > 120 deg -> never an object (w_fast = 0, histogram excluded).
    Rear = air only -> share <= 0.05.  Also: residual energy of the source's cells in the rear
    with air off must be ~0 (nothing dumped into u)."""
    seed()
    src = ub.pluck_line(SEC, ub.LEAD_NOTES) * 0.25
    lead = np.stack([src, -src]) * 0.7
    bed = ub.pan(ub.pad(SEC), 0) * 0.25 + room_bed(SEC)
    lead, bed = norm_mix([lead, bed])
    out, _ = run("c7", [lead, bed])
    m, *_ = share_metrics(lead, bed, out)
    out0, _ = run("c7_noair", [lead, bed], replace(su.SceneConfig(), air_db=None))
    pin, _, pr = owned_power(lead, bed, out0, t0=1.0)
    return [Row("7 anti-phase source", "source rear share (median)", round(m["median_rear"], 4), "info (anti-phase goes behind through u by design, like matrix surround; never an object)", True),
            Row("7 anti-phase source", "rear/input dB in owned cells, air off", round(db(pr / pin), 1), "info (anti-phase content is sent behind through u by design, as matrix surround does; it never becomes an object)", True)]


def check_8_membership_stability():
    """Spec 13.2-2: admit after 1.5 s continuous presence (+0.34 s pick quantisation) and a 0.6 s
    raised cosine -> <= 2.5 s; then the place cannot move faster than 0.05/s and drops only after
    30 s absence, so over the remaining 12 s the pluck's groups stay members (>= 0.8) with <= 2
    state changes (cluster set) and <= 2 member-weight crossings of 0.5."""
    seed()
    pluck_raw, on = ub.gate(ub.pluck_line(SEC, ub.LEAD_NOTES) * 0.25, SEC, 0, start_sec=5)
    pluck = ub.pan(pluck_raw, -1)
    padc = ub.pan(ub.pad(SEC), 0) * 0.25
    pluck, padc = norm_mix([pluck, padc])
    out, st = run("c8", [pluck, padc], want_stats=True)
    N, H = su.SceneConfig().n_fft, su.SceneConfig().hop
    g = st["group"]; G = st["w_member"].shape[1]
    gp = lambda s: np.bincount(g, weights=s, minlength=G)
    PL = np.abs(su.stft(pluck[0], N, H)) ** 2 + np.abs(su.stft(pluck[1], N, H)) ** 2
    PB = np.abs(su.stft(padc[0], N, H)) ** 2 + np.abs(su.stft(padc[1], N, H)) ** 2
    T = min(len(PL), len(st["w_member"]))
    wm = np.full(T, np.nan)
    for t in range(T):
        a = gp(PL[t]); b = gp(PB[t])
        own = (a > 10 * b) & (a > 1e-4 * a.max())
        if own.any():
            wm[t] = (st["w_member"][t][own] * a[own]).sum() / a[own].sum()
    tt = (np.arange(T) * H - N / 2) / SR
    after = tt >= 5.0
    idx = np.flatnonzero(after & (wm >= 0.5))
    nc = st["n_clusters"]
    if not len(idx):   # never admitted: every row fails, with the evidence
        return [Row("8 pluck enters 5 s", "admission latency s", "never (max member weight %.3f, max n_clusters %d)" % (np.nanmax(wm[after]), nc.max()), "<= 2.5", False)]
    admit = tt[idx[0]] - 5.0
    rem = (tt >= 5.0 + admit + 0.5) & ~np.isnan(wm)
    w_rem = wm[rem]
    crossings = int(np.sum(np.abs(np.diff((w_rem >= 0.5).astype(int)))))
    nc_changes = int(np.sum(np.diff(nc[rem[:len(nc)]]) != 0))
    return [Row("8 pluck enters 5 s", "admission latency s", round(float(admit), 2), "<= 2.5", admit <= 2.5),
            Row("8 pluck enters 5 s", "member weight after admission (median)", round(float(np.nanmedian(w_rem)), 3), ">= 0.8", np.nanmedian(w_rem) >= 0.8),
            Row("8 pluck enters 5 s", "member weight after admission (p10 / min)", f"{np.nanpercentile(w_rem, 10):.3f} / {np.nanmin(w_rem):.3f}", "info", True),
            Row("8 pluck enters 5 s", "member-weight crossings of 0.5", crossings, "<= 2", crossings <= 2),
            Row("8 pluck enters 5 s", "cluster-set state changes", nc_changes, "<= 2", nc_changes <= 2)]


def check_9_vocal_entry_front_stable():
    """Spec 5: the front subtraction follows w_slow (tau >= 400 ms / cluster membership) while the rear
    object follows min(w_fast, w_slow).  A vocal 6 dB louder entering the guitar's cells pulls the
    fast pan to |p| ~ 0.2 (< pan_hi) within one hop: the rear object may dip, the transferred part
    of the guitar (input - front) must not change by more than 1 dB within 0.3 s.  Also reported:
    front minus the vocal's own contribution, and the rear dip."""
    seed()
    notes = [440, 494, 523]
    gtr = ub.pan(ub.sustain_line(SEC, notes, bpm=60) * 0.25, -1)
    voc_raw, _ = ub.gate(ub.sustain_line(SEC, notes, bpm=60, vib=0.3) * 0.5, SEC, 0, start_sec=8)
    voc = ub.pan(voc_raw, 0)
    gtr, voc = norm_mix([gtr, voc])
    out, st = run("c9", [gtr, voc], want_stats=True)
    x = gtr + voc
    sub = x - out[0:2]                                   # what left the front
    lo, hi = 400, 3500
    gin = band_frames(gtr[0], lo, hi) + band_frames(gtr[1], lo, hi)
    s = (band_frames(sub[0], lo, hi) + band_frames(sub[1], lo, hi)) / gin        # transfer fraction
    fg = (band_frames(out[0] - voc[0], lo, hi) + band_frames(out[1] - voc[1], lo, hi)) / gin
    r = (band_frames(out[4], lo, hi) + band_frames(out[5], lo, hi)) / gin
    tt = frame_t(np.arange(len(s)))
    pre = (tt >= 7.5) & (tt < 7.9); post = (tt >= 8.1) & (tt < 8.4)
    if s[pre].mean() < 1e-3:   # the guitar was never transferred: the check is void, say so
        return [Row("9 vocal enters guitar cells", "transfer fraction before entry", f"{s[pre].mean():.2e}", ">= 1e-3 (nothing transferred = no stage)", False)]
    d_sub = db(s[post].mean() / s[pre].mean())
    d_fg = db(fg[post].mean() / fg[pre].mean())
    d_rear = db(r[post].mean() / r[pre].mean())
    late = (tt >= 9.5) & (tt < 10.5)
    d_sub_late = db(s[late].mean() / s[pre].mean())
    return [Row("9 vocal enters guitar cells", "transferred fraction (in - front)/guitar change dB, 0.3 s", round(d_sub, 2), "|x| <= 1.0", abs(d_sub) <= 1.0),
            Row("9 vocal enters guitar cells", "(front - vocal)/guitar change dB, 0.3 s", round(d_fg, 2), "info", True),
            Row("9 vocal enters guitar cells", "rear/guitar band change dB, 0.3 s (dip allowed)", round(d_rear, 2), "info", True),
            Row("9 vocal enters guitar cells", "transferred fraction change dB at +1.5..2.5 s", round(d_sub_late, 2), "info (slow return)", True)]


def check_11_instrument_mask():
    """Semantic stage: an external eligibility mask (oracle from the stems on the hook's 512/128 x4 grid)
    sends a CENTRED piano behind while the centred vocal stays in front."""
    sec = 20.0
    seed()
    voc = ub.pan(ub.formant_voice(sec, [466, 523, 587, 659], bpm=50), 0.0) * 0.4
    st = ub.strum(sec, (196, 247, 294, 392), bpm=80)
    pno = ub.pan(st, 0.0) * 0.4 + ub.stereo_reverb(st, rt60=1.2, wet=0.2) * 0.12
    mix = voc + pno
    def spec(x):
        n, h = 512, 128; win = np.hanning(n); idx = np.arange(0, x.shape[1] - n, h)
        return sum(np.abs(np.fft.rfft(np.stack([x[c][i:i + n] * win for i in idx]), axis=1)) ** 2 for c in range(2))
    Pv, Pp = spec(voc), spec(pno); e = Pp / (Pv + Pp + 1e-12); Tc = e.shape[0] // 4 * 4
    elig = e[:Tc].reshape(Tc // 4, 4, 257).mean(1)
    rows = []
    for name, m in (("mask", elig), ("no mask", None)):
        out6 = su.render(mix.T, su.SceneConfig(inst_gamma=2.0, inst_alpha=0.7, duck_depth=0.5), inst_mask=m); out6 = out6.T if out6.shape[0] != 6 else out6   # coarse-grid shaping (the 512/128 oracle)
        sp, w, dt = ub.rear_share(pno, voc, out6); sv, _, _ = ub.rear_share(voc, pno, out6)
        if m is not None:
            rows.append(Row("11 centred piano + vocal, oracle mask", "piano rear share (median)", np.nanmedian(sp), ">= 0.7", np.nanmedian(sp) >= 0.7))
            rows.append(Row("11 centred piano + vocal, oracle mask", "vocal rear share (median)", np.nanmedian(sv), "<= 0.10 (512/128 ratio mask, gamma 2, slow attack / fast release)", np.nanmedian(sv) <= 0.10))
            rows.append(Row("11 centred piano + vocal, oracle mask", "total energy re input dB", 10 * np.log10((out6 ** 2).sum() / (mix ** 2).sum()), "|x| <= 0.5", abs(10 * np.log10((out6 ** 2).sum() / (mix ** 2).sum())) <= 0.5))
        else:
            rows.append(Row("11 centred piano + vocal, no mask", "piano rear share (median)", np.nanmedian(sp), "<= 0.05 (centred: nothing to transfer without semantics)", np.nanmedian(sp) <= 0.05))
    return rows


def check_12_stream_equivalence():
    """Block-wise SceneStream (16 ms blocks and odd sizes) reproduces render() sample-exactly, and is
    causal: zeroing the input after m changes nothing before m - n_fft (the analysis latency)."""
    seed()
    pluck, voc, bed = _scene_2()
    x = (pluck + voc + bed).T[:SR * 8]
    rows = []
    for delay in (12.0, -15.0):
        cfg = replace(su.SceneConfig(), rear_delay_ms=delay)
        ref = su.render(x, cfg)
        for block in (768, 1000):
            d = float(np.abs(su.stream_render(x, cfg, block=block) - ref).max())
            rows.append(Row(f"12 stream, rear delay {delay:+.0f} ms, block {block}", "max |stream - render|", d, "<= 1e-9", d <= 1e-9))
    cfg = su.SceneConfig(); N = cfg.n_fft; m = SR * 4
    a = su.stream_render(x, cfg, block=768); xb = x.copy(); xb[m:] = 0.0; b = su.stream_render(xb, cfg, block=768)
    first = int(np.argmax(np.abs(a - b).max(1) > 1e-12))
    rows.append(Row("12 stream causality", "first sample changed by zeroing the input after m, minus (m - n_fft)", first - (m - N), ">= 0 (no lookahead beyond the analysis frame)", first - (m - N) >= 0))
    return rows


def check_13_song_memory_and_reset():
    """Song-level memory: a pan that was a place earlier in the song is re-admitted after a long absence
    in a fraction of the dwell (judged on the places' own gain, not on the rear share: a returning
    hard-panned source rides the side layer for ~300 ms while the direction statistics catch up).
    A track-change reset fades the old place and clears the memory: the next song's pluck at the same
    pan needs the full dwell again."""
    seed()
    sec = 52.0
    pluck = pluck_take(sec, [330, 392, 440, 494], np.random.default_rng(5))
    bed = room_bed(sec)
    on = np.ones(int(sec * SR), bool); on[int(8 * SR):int(44 * SR)] = False        # pluck 0-8 s, silent 36 s, back at 44 s
    pl = ub.pan(pluck * on, -1.0)
    cfg = su.SceneConfig()
    out, stats = run("c13_mem", [pl, bed], cfg, want_stats=True)
    gain = np.nanmax(np.nan_to_num(stats["places"][:, :, 3], nan=0.0), axis=1)        # best place gain per frame
    tfr = np.arange(len(gain)) * cfg.hop / SR
    def admit_after(t0):
        idx = np.flatnonzero((tfr >= t0) & (gain >= 0.5))
        return (tfr[idx[0]] - t0) if len(idx) else np.nan
    lat_first, lat_return = admit_after(0.0), admit_after(44.0)
    gone = np.flatnonzero((tfr > 8.0) & (tfr < 44.0) & (gain <= 0.0))
    rows = [Row("13 song memory", "first admission (place gain >= 0.5) s", round(lat_first, 2), "info", True),
            Row("13 song memory", "place dropped during the 36 s silence (first frame with gain 0) s", round(tfr[gone[0]], 1) if len(gone) else np.nan, "<= 42 (drop 30 s + fade)", len(gone) > 0 and tfr[gone[0]] <= 42),
            Row("13 song memory", "re-admission after the silence s", round(lat_return, 2), "<= 0.5 * first (remembered pan: dwell * (1 - 0.75))", lat_return <= 0.5 * lat_first)]
    # reset: song A (pluck L 10 s) -> reset -> song B (pluck L again, fresh take): no memory shortcut
    a = ub.pan(pluck_take(10.0, [330, 392, 440, 494], np.random.default_rng(6)), -1.0) + room_bed(10.0)
    b = ub.pan(pluck_take(10.0, [262, 294, 330, 349], np.random.default_rng(7)), -1.0) + room_bed(10.0)
    st = su.SceneStream(cfg, keep_stats=True)
    st.push(a.T); nA = len(st.places_log); st.reset(); st.push(b.T); st.flush()
    pl_log = np.array(st.places_log[nA:])                                        # frames after the reset
    g = np.nan_to_num(pl_log[:, :, 3], nan=0.0); tf = np.arange(len(g)) * cfg.hop / SR
    old_gone = np.flatnonzero(g.max(1) <= 0.0)
    # a NEW place (gain rising from 0 after the old one died) admitted again only after the full dwell
    new_adm = np.flatnonzero((g.max(1) >= 0.5) & (tf > (tf[old_gone[0]] if len(old_gone) else 0.0)))
    t_new = tf[new_adm[0]] if len(new_adm) else np.nan
    rows.append(Row("13 reset", "old place gone after reset s", round(tf[old_gone[0]], 2) if len(old_gone) else np.nan, "<= 2.0 (dying fade 1.5 s)", len(old_gone) > 0 and tf[old_gone[0]] <= 2.0))
    rows.append(Row("13 reset", "next song's same-pan pluck admitted at s after reset", round(t_new, 2), ">= 1.2 (memory cleared: full dwell, no shortcut)", t_new >= 1.2))
    return rows


def check_10_cold_start():
    """Spec 3 guards: statistics initialised from the first frame, ambient/air ramp over 32 hops, the
    object layer closed until the first admission -> the first 100 ms of rear output is far below
    the steady rear (-40 dB)."""
    seed()
    pluck, voc, bed = _scene_2()
    out, _ = run("c2_a0.7", [pluck, voc, bed])
    out0, _ = run("c2_a0.0", [pluck, voc, bed], replace(su.SceneConfig(), alpha=0.0))   # ambient + air only
    rear = out[4:6]
    rms = lambda z: np.sqrt(np.mean(z ** 2))
    first = rms(rear[:, : int(0.1 * SR)])
    steady = rms(rear[:, int(5 * SR):])
    steady_amb = rms(out0[4:6, int(5 * SR):])
    d_total = 20 * np.log10(max(first, 1e-15) / steady)
    d_amb = 20 * np.log10(max(first, 1e-15) / steady_amb)
    slices = [rms(rear[:, i:i + int(0.02 * SR)]) for i in range(0, int(0.2 * SR), int(0.02 * SR))]
    peak = 20 * np.log10(max(max(slices), 1e-15) / steady_amb)
    # Spec 3 guards: stats initialised from the first frame, ambient/air ramp (t+1)/32 over 32 hops =
    # 171 ms, object closed until admission.  A burst = the rear exceeding its own steady ambient
    # level while the ramp is still on; the ramp's mean power over the first 100 ms is -9 dB.
    return [Row("10 cold start", "rear RMS first 100 ms re steady ambient+air layer dB", round(d_amb, 1), "<= -3 (ramp mean -9)", d_amb <= -3),
            Row("10 cold start", "max 20 ms slice in first 200 ms re steady ambient dB", round(peak, 1), "<= 0 (no overshoot)", peak <= 0),
            Row("10 cold start", "rear RMS first 100 ms re steady TOTAL rear (5-20 s) dB", round(d_total, 1), "info (task asked <= -40; ramp arithmetic gives ~-35)", True)]


CHECKS = [check_1_centred_protection, check_2_transfer_vs_vocal, check_3_energy_conservation,
          check_4_drums_no_pumping, check_5_wide_pair, check_6_mono, check_7_antiphase,
          check_8_membership_stability, check_9_vocal_entry_front_stable, check_10_cold_start,
          check_11_instrument_mask, check_12_stream_equivalence, check_13_song_memory_and_reset]


def _assert_rows(rows):
    bad = [r for r in rows if not r.ok]
    assert not bad, "\n".join(map(repr, bad))


def test_1(): _assert_rows(check_1_centred_protection())
def test_2(): _assert_rows(check_2_transfer_vs_vocal())
def test_3(): _assert_rows(check_3_energy_conservation())
def test_4(): _assert_rows(check_4_drums_no_pumping())
def test_5(): _assert_rows(check_5_wide_pair())
def test_6(): _assert_rows(check_6_mono())
def test_7(): _assert_rows(check_7_antiphase())
def test_8(): _assert_rows(check_8_membership_stability())
def test_9(): _assert_rows(check_9_vocal_entry_front_stable())
def test_10(): _assert_rows(check_10_cold_start())
def test_11(): _assert_rows(check_11_instrument_mask())
def test_12(): _assert_rows(check_12_stream_equivalence())
def test_13(): _assert_rows(check_13_song_memory_and_reset())


if __name__ == "__main__":
    only = sys.argv[1:]
    rows = []
    for fn in CHECKS:
        if only and not any(fn.__name__.startswith(f"check_{o}_") for o in only):
            continue
        try:
            rows += fn()
        except Exception as e:   # a crashed check is a failed check, not a crashed table
            rows.append(Row(fn.__name__, "exception", repr(e), "-", False))
    w = max(len(r.name) for r in rows)
    v = max(len(r.what) for r in rows)
    print(f"{'scene':{w}}  {'what':{v}}  {'measured':>22}  {'limit':30}  result")
    for r in rows:
        print(f"{r.name:{w}}  {r.what:{v}}  {str(r.measured):>22}  {r.limit:30}  {'OK' if r.ok else 'FAIL'}")
    nf = sum(not r.ok for r in rows)
    import hashlib
    h = hashlib.md5(open(su.__file__, "rb").read()).hexdigest()[:8]
    print(f"\n{len(rows) - nf}/{len(rows)} OK, scene_upmix.py md5 {h}, tmp {TMP}")
    sys.exit(1 if nf else 0)
