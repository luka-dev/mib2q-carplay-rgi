#!/usr/bin/env python3
"""test_clusters.py — hook/audio/scene_clusters.c against clusters.npz (and the Python StageClusters).

Part 1 replays the fixture: per frame update(p, ps, ok) then membership(memb_p_slow, memb_e_slow),
compares hist / silent / places (centre, sigma, mass, gain) / memb_w, memb_q, memb_a, memb_pc.
Part 2 drives the C and scene_upmix.StageClusters (float64 oracle) through synthetic multi-place runs
(merges, k_max, shadow, silence, reset, song memory, and the 8-slot ceiling) and compares the FULL
discrete state per frame (count, admitted/present/dying, centre, sigma, mass, seen_s, absent_s, gain).

A mismatch the float32 port cannot avoid (a place whose gain decays to the rounding residue of an exact
zero: ~1e-15 in float64, ~1e-8 or 0 in float32, so the num > 1e-12 gate of q/a/pc, the gain > 0 test or
the removal differ for one frame while w ~ 1e-8 either way; or the 2.5-sigma kernel clip hit within
rounding) is shown as EXPLAINED and does not fail the test.

    python3 test_clusters.py [--fixtures DIR] [--lib libscene.so]
Builds build/scene_port/libscene_clusters.so from scene_clusters.c when --lib is not given.
"""
import argparse
import ctypes
import json
import os
import re
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
HOOK = os.path.join(ROOT, "hook", "audio")
BUILD = os.path.join(ROOT, "build", "scene_port")
FIXTURES = ("/private/tmp/claude-501/-Users-luka-Desktop-AUDI-2-Patches-mhi2-carplay/"
            "68989c4d-7e7a-4c32-aa08-b7c2a6c66c89/scratchpad/port_fixtures")
sys.path.insert(0, os.path.join(HERE, ".."))
import scene_upmix  # noqa: E402  (the reference; never edited here)

VANISH = 1e-6       # a gain below this is a rounding residue of an exact zero


# ----------------------------------------------------------------------------- header -> ctypes
def parse_header():
    """struct scene_config / scene_place / scene_memory / scene_clusters as ctypes, from the header text."""
    text = open(os.path.join(HOOK, "scene_engine.h")).read()
    defs = {}
    for m in re.finditer(r"^#define[ \t]+(SCENE_\w+)[ \t]+([^\n]+?)[ \t]*(?:/\*[^\n]*)?$", text, re.M):
        defs[m.group(1)] = int(eval(m.group(2), {}, dict(defs)))
    types = {"int": ctypes.c_int, "float": ctypes.c_float, "uint32_t": ctypes.c_uint32}
    structs = {}
    for m in re.finditer(r"struct (\w+) \{([^}]*)\}", text, re.S):
        body = re.sub(r"/\*.*?\*/", "", m.group(2), flags=re.S)
        fields = []
        for decl in body.split(";"):
            decl = " ".join(decl.split())
            if not decl:
                continue
            mt = re.match(r"(struct \w+|\w+) (.*)", decl)
            tname, rest = mt.group(1), mt.group(2)
            ct = structs.get(tname[7:]) if tname.startswith("struct ") else types.get(tname)
            if ct is None:
                fields = None
                break
            for d in rest.split(","):
                md = re.match(r"(\w+)(?:\[(.+)\])?$", d.strip())
                n = md.group(2)
                fields.append((md.group(1), ct * int(eval(n, {}, dict(defs))) if n else ct))
        if fields is not None:
            structs[m.group(1)] = type(m.group(1), (ctypes.Structure,), {"_fields_": fields})
    return defs, structs


def build_lib(path):
    os.makedirs(BUILD, exist_ok=True)
    lib = os.path.join(BUILD, "libscene_clusters.so") if path is None else path
    if path is None:
        subprocess.check_call(["cc", "-O2", "-std=gnu99", "-ffp-contract=off", "-Wall", "-Wextra", "-fPIC",
                               "-shared", "-I" + HOOK, os.path.join(HOOK, "scene_clusters.c"), "-lm", "-o", lib])
    return ctypes.CDLL(lib)


def c_config(Config, cfg_dict):
    cfg = Config()
    for name, _ in Config._fields_:
        v = cfg_dict["front_follows"] == "slow" if name == "front_follows_slow" else cfg_dict[name]
        setattr(cfg, name, v)
    return cfg


class CClusters:
    """ctypes wrapper of one struct scene_clusters."""

    def __init__(self, lib, structs, cfg_dict, frame_s):
        self.lib = lib
        self.S = structs["scene_clusters"]
        self.c = self.S()
        self.cfg = c_config(structs["scene_config"], cfg_dict)
        f32 = np.ctypeslib.ndpointer(np.float32, flags="C_CONTIGUOUS")
        i32 = np.ctypeslib.ndpointer(np.int32, flags="C_CONTIGUOUS")
        P = ctypes.POINTER(self.S)
        lib.scene_clusters_init.argtypes = [P, ctypes.POINTER(structs["scene_config"]), ctypes.c_float]
        lib.scene_clusters_update.argtypes = [P, f32, f32, i32, ctypes.c_int]
        lib.scene_clusters_membership.argtypes = [P, f32, f32, ctypes.c_int, f32, f32, f32]
        lib.scene_clusters_reset.argtypes = [P]
        lib.scene_clusters_n_live.argtypes = [P]
        lib.scene_clusters_n_live.restype = ctypes.c_int
        lib.scene_clusters_init(ctypes.byref(self.c), ctypes.byref(self.cfg), frame_s)

    def update(self, p, ps, ok):
        self.lib.scene_clusters_update(ctypes.byref(self.c), np.ascontiguousarray(p, np.float32),
                                       np.ascontiguousarray(ps, np.float32), np.ascontiguousarray(ok, np.int32), len(p))

    def membership(self, p_slow, e_slow):
        n = len(p_slow)
        w = np.zeros(n, np.float32); q = np.zeros(n, np.float32); a = np.zeros(n, np.float32)
        self.lib.scene_clusters_membership(ctypes.byref(self.c), np.ascontiguousarray(p_slow, np.float32),
                                           np.ascontiguousarray(e_slow, np.float32), n, w, q, a)
        return w, q, a, np.array(self.c.last_pc[:n])

    def reset(self):
        self.lib.scene_clusters_reset(ctypes.byref(self.c))

    def n_live(self):
        return self.lib.scene_clusters_n_live(ctypes.byref(self.c))

    def places(self):
        return [dict(centre=k.centre, sigma=k.sigma, mass=k.mass, seen_s=k.seen_s, absent_s=k.absent_s, gain=k.gain,
                     admitted=bool(k.admitted), present=bool(k.present), dying=bool(k.dying))
                for k in self.c.c[:self.c.n]]

    @property
    def hist(self):
        return np.array(self.c.hist[:])


# ----------------------------------------------------------------------------- comparison
class Report:
    def __init__(self, name):
        self.name = name; self.bad = []; self.explained = []; self.errs = {}

    def err(self, key, e):
        self.errs[key] = max(self.errs.get(key, 0.0), float(e))

    def note(self, t, what, detail, why=None):
        (self.explained if why else self.bad).append((t, what, detail + (" -> EXPLAINED: " + why if why else "")))

    def finish(self, summary, extra_lines=()):
        print(summary)
        for line in extra_lines:
            print("  " + line)
        print("  max abs err  " + " | ".join("%s %.3g" % (k, v) for k, v in self.errs.items()))
        for b in self.explained[:12]:
            print("  frame %d %s: %s" % b)
        if len(self.explained) > 12:
            print("  ... %d more explained" % (len(self.explained) - 12))
        for b in self.bad[:30]:
            print("  MISMATCH frame %d %s: %s" % b)
        if len(self.bad) > 30:
            print("  ... %d more" % (len(self.bad) - 30))
        frames = sorted({b[0] for b in self.bad})
        if not self.bad:
            print("  PASS%s" % (" (%d explained single-frame float32/float64 threshold crossings in %d frames)"
                                % (len(self.explained), len({b[0] for b in self.explained})) if self.explained else ""))
            return True
        print("  %d mismatches in %d frames, first at %d" % (len(self.bad), len(frames), frames[0]))
        print("  FAIL")
        return False


def explain_bin(k, b, w_c, w_r, places_c, pb):
    """Why one bin of q/a/pc/w can differ although the state agrees (see the module docstring)."""
    if k in ("q", "a", "pc") and abs(w_c[b]) < VANISH and abs(w_r[b]) < VANISH:
        return "num > 1e-12 gate on a vanishing gain (w C %.2g ref %.2g)" % (w_c[b], w_r[b])
    for pl in places_c:
        if pl["gain"] > 0 and abs((pb - pl["centre"]) ** 2 / (2 * pl["sigma"] ** 2) - 3.125) < 1e-5:
            return "2.5-sigma kernel clip (d2 = 3.125) crossed within rounding"
    return None


def compare_membership(rep, t, outs, p_slow, places_c, tol=1e-4):
    """outs: (key, C array, ref array) with 'w' first; abs tolerance, and the max relative to the frame max."""
    w_c, w_r = outs[0][1], outs[0][2]
    for k, cv, rv in outs:
        d = np.abs(cv - rv); e = float(d.max()); rep.err(k, e)
        scale = float(np.abs(rv).max())
        rep.err(k + "_rel", e / scale if scale > 0 else 0.0)
        if e > tol:
            bins = np.flatnonzero(d > tol)
            whys = [explain_bin(k, int(b), w_c, w_r, places_c, float(p_slow[b])) for b in bins]
            detail = "%d bins, worst bin %d: C %.6g ref %.6g" % (len(bins), int(d.argmax()), cv[d.argmax()], rv[d.argmax()])
            rep.note(t, "memb_" + k, detail, whys[0] if all(whys) else None)


def vanishing_extra(pl, rl, key="gain"):
    """The lists differ only by clusters whose gain is a rounding residue (the removal rule crossed)."""
    def sig(c):
        return round(c["centre"], 3)
    a, b = sorted(map(sig, pl)), sorted(map(sig, rl))
    extra_c = [c for c in pl if a.count(sig(c)) > b.count(sig(c))]
    extra_r = [c for c in rl if b.count(sig(c)) > a.count(sig(c))]
    if (extra_c or extra_r) and all(abs(c[key]) < VANISH for c in extra_c + extra_r):
        return "removal rule crossed on a vanishing gain (%s)" % ", ".join("%.2g" % c[key] for c in extra_c + extra_r)
    return None


# ----------------------------------------------------------------------------- part 1: fixture
def run_fixture(lib, structs, path):
    f = np.load(path, allow_pickle=True)
    cfg = json.loads(str(f["config"]))
    frame_s = float(f["frame_s"]); T = f["p"].shape[0]
    cc = CClusters(lib, structs, cfg, frame_s)
    rep = Report("fixture")
    for t in range(T):
        cc.update(f["p"][t], f["ps"][t], f["ok"][t])
        w, q, a, pc = cc.membership(f["memb_p_slow"][t], f["memb_e_slow"][t])
        href = f["hist"][t]; scale = max(float(np.abs(href).max()), 1e-30)
        e = float(np.abs(cc.hist - href).max()); rep.err("hist", e); rep.err("hist_rel", e / scale)
        if e / scale > 1e-4:
            rep.note(t, "hist", "max abs %.3g rel %.3g" % (e, e / scale))
        if bool(cc.c.silent) != bool(f["silent"][t]):
            rep.note(t, "silent", "C %d ref %d (tot_ref C %.4g)" % (cc.c.silent, f["silent"][t], cc.c.tot_ref))
        pref = f["places"][t]; nref = int((~np.isnan(pref[:, 0])).sum()); pl = cc.places()
        if min(cc.c.n, 8) != nref:
            rl = [dict(centre=r[0], gain=r[3]) for r in pref[:nref]]
            rep.note(t, "n_clusters", "C %d ref %d; C %s; ref %s" % (cc.c.n, nref, pl, pref[:nref].tolist()), vanishing_extra(pl, rl))
        else:
            for i in range(nref):
                for j, k in enumerate(("centre", "sigma", "mass", "gain")):
                    e = abs(pl[i][k] - pref[i, j]); rep.err(k, e)
                    if e > 1e-5:
                        rep.note(t, "place%d.%s" % (i, k), "C %.7g ref %.7g" % (pl[i][k], pref[i, j]))
        compare_membership(rep, t, [("w", w, f["memb_w"][t]), ("q", q, f["memb_q"][t]), ("a", a, f["memb_a"][t]),
                                    ("pc", pc, f["memb_pc"][t])], f["memb_p_slow"][t], pl)
    return rep.finish("fixture %s: %d frames, G=%d, F=%d" % (os.path.basename(path), T, f["p"].shape[1], f["memb_w"].shape[1]),
                      ["tolerances: hist 1e-4 relative to the frame max, places 1e-5, membership 1e-4"])


# ----------------------------------------------------------------------------- part 2: synthetic oracle
def synthetic(frame_s, places, seconds, k_per_place=12, scatter=0.06, centre_scatter=0.04, silence=None, resets=(),
              field=None, G=134, F=513, seed=3):
    """Per-frame p/ps/ok and membership inputs. places: (centre, t0, t1, level or level(s)); silence (t0, t1)
    drops everything 60 dB; field (t0, t1, pan) piles membership energy at one pan (the shadow test)."""
    rng = np.random.default_rng(seed)
    T = int(seconds / frame_s)
    p = np.zeros((T, G)); ps = np.zeros((T, G)); ok = np.zeros((T, G), bool)
    for t in range(T):
        s = t * frame_s
        n_centre = 60                                   # the centre holds most of the mass, as in a real mix
        p[t, :n_centre] = rng.normal(0.0, centre_scatter, n_centre); ps[t, :n_centre] = rng.uniform(0.5, 2.0, n_centre)
        g = n_centre
        for cen, t0, t1, lvl in places:
            if t0 <= s < t1:
                k = k_per_place
                p[t, g:g + k] = np.clip(cen + rng.normal(0.0, scatter, k), -1, 1)
                ps[t, g:g + k] = (lvl(s) if callable(lvl) else lvl) * rng.uniform(0.2, 1.0, k)
                g += k
        p[t, g:] = rng.uniform(-1, 1, G - g); ps[t, g:] = rng.uniform(0.0, 0.05, G - g)
        ok[t] = rng.uniform(size=G) < 0.8
        if silence and silence[0] <= s < silence[1]:
            ps[t] *= 1e-6
    p_slow = np.zeros((T, F)); e_slow = np.zeros((T, F))
    base = rng.uniform(-1, 1, F) * (rng.uniform(size=F) < 0.5)
    for t in range(T):
        p_slow[t] = np.clip(base + rng.normal(0.0, 0.02, F), -1, 1)
        e_slow[t] = rng.uniform(0.0, 3.0, F)
        if field and field[0] <= t * frame_s < field[1]:
            p_slow[t, :200] = field[2] + rng.normal(0.0, 0.02, 200); e_slow[t, :200] = 5.0; e_slow[t, 200:] *= 0.05
    return p, ps, ok, p_slow, e_slow, {int(r / frame_s) for r in resets}


def scenario_places(frame_s):
    """A +0.50 (0..), B -0.42 (0..9 s), B2 -0.72 (3..9 s, among the three loudest) with the membership energy
    piled between B and B2 (3..9 s): B2 reaches its dwell under B's kernel -> _shadow -> dying, again and
    again; C +0.85 (9 s..); D +0.62 (10 s..: within 0.15 of A -> merge path); E -0.80 (12 s..: with A, B
    (absent but admitted), C that is four admitted -> k_max fades the weakest); silence 15..16 s; reset at
    17 s (A, C come back: memory-boost dwell)."""
    places = [(0.50, 0.0, 99.0, 1.0), (-0.42, 0.0, 9.0, 0.9), (-0.72, 3.0, 9.0, 0.85), (0.85, 9.0, 99.0, 0.8),
              (0.62, 10.0, 99.0, 0.7), (-0.80, 12.0, 99.0, 0.6)]
    return synthetic(frame_s, places, 22.0, silence=(15.0, 16.0), resets=(17.0,), field=(3.0, 9.0, -0.57))


def scenario_overflow(frame_s):
    """Ten off-centre places on every second histogram bin centre (0.19 apart, tight scatter so the centroids
    stay 0.15+ apart and nothing merges; the centre is narrow so its tail does not swamp the +-0.19 bins).
    Four groups of places 0.25+ apart take turns being loud for 1 s each: shorter than the 1.5 s dwell, so
    every place becomes a 30 s ghost (gain 0, never admitted) and nine of them coexist: the Python list
    grows past SCENE_PLACES_MAX and the C has to evict."""
    b = lambda k: -0.952380952 + k * 0.0952380952
    groups = [{b(12), b(16), b(20)}, {b(8), b(4), b(0)}, {b(14), b(18), b(6)}, {b(2)}]
    pans = sorted(set().union(*groups))
    places = [(c, 0.0, 99.0, (lambda c: lambda s: 1.0 if c in groups[int(s / 1.0) % 4] else 0.1)(c)) for c in pans]
    return synthetic(frame_s, places, 40.0, k_per_place=6, scatter=0.02, centre_scatter=0.02, seed=5)


def run_oracle(lib, structs, cfg_dict, frame_s, name, data, places_max, overflow=False):
    cfg = scene_upmix.SceneConfig(**cfg_dict)
    ref = scene_upmix.StageClusters(cfg, frame_s)
    cc = CClusters(lib, structs, cfg_dict, frame_s)
    shadow_calls = [0, 0]
    ref_shadow = ref._shadow

    def counting_shadow(cand):
        r = ref_shadow(cand); shadow_calls[0] += 1; shadow_calls[1] += bool(r); return r
    ref._shadow = counting_shadow
    p, ps, ok, p_slow, e_slow, resets = data
    T = p.shape[0]
    keys_f = ("centre", "sigma", "mass", "seen_s", "absent_s", "gain"); keys_b = ("admitted", "present", "dying")
    rep = Report(name)
    max_n = 0; max_n_c = 0; admitted_max = 0; dying_frames = 0; boosted = 0; silent_frames = 0; n_live3 = 0
    t_over = None
    for t in range(T):
        if t in resets:
            ref.reset(); cc.reset()
        ref.update(p[t], ps[t], ok[t]); cc.update(p[t], ps[t], ok[t])
        rw, rq, ra = ref.membership(p_slow[t], e_slow[t]); rpc = ref.last_pc
        w, q, a, pc = cc.membership(p_slow[t], e_slow[t])
        pl = cc.places(); rl = ref.clusters
        n_live = sum(1 for c in rl if c["gain"] > 0)
        max_n = max(max_n, len(rl)); max_n_c = max(max_n_c, len(pl)); n_live3 += n_live >= 3
        admitted_max = max(admitted_max, sum(1 for c in rl if c["admitted"]))
        dying_frames += any(c["dying"] for c in rl); silent_frames += bool(ref.silent)
        boosted += any(not c["admitted"] and c["seen_s"] > cfg.admit_s * cfg.memory_boost for c in rl)
        if t_over is None and len(rl) > places_max:
            t_over = t
        if t_over is not None:                          # beyond the reference's reach: C invariants only
            assert len(pl) <= places_max and np.all(np.isfinite(w)) and w.min() >= 0 and w.max() <= 1, (t, len(pl))
            continue
        scale = max(float(np.abs(ref.hist).max()), 1e-30)
        e = float(np.abs(cc.hist - ref.hist).max()); rep.err("hist_rel", e / scale)
        if e / scale > 1e-4:
            rep.note(t, "hist", "rel %.3g" % (e / scale))
        if bool(cc.c.silent) != ref.silent:
            rep.note(t, "silent", "C %d ref %d" % (cc.c.silent, ref.silent))
        if cc.n_live() != n_live:
            why = vanishing_extra([c for c in pl if c["gain"] > 0], [c for c in rl if c["gain"] > 0])
            rep.note(t, "n_live", "C %d ref %d" % (cc.n_live(), n_live), why)
        if len(pl) != len(rl):
            rep.note(t, "n_clusters", "C %d ref %d\n      C   %s\n      ref %s" % (len(pl), len(rl), pl, rl), vanishing_extra(pl, rl))
        else:
            for i, (a_, b_) in enumerate(zip(pl, rl)):
                for k in keys_b:
                    if a_[k] != bool(b_[k]):
                        rep.note(t, "place%d.%s" % (i, k), "C %s ref %s" % (a_[k], b_[k]))
                for k in keys_f:
                    e = abs(a_[k] - b_[k]); rep.err(k, e)
                    if e > 1e-5:
                        rep.note(t, "place%d.%s" % (i, k), "C %.7g ref %.7g" % (a_[k], b_[k]))
        compare_membership(rep, t, [("w", w, rw), ("q", q, rq), ("a", a, ra), ("pc", pc, rpc)], p_slow[t], pl)
    lines = []
    if overflow:
        lines.append("reference exceeded %d places at frame %s (python max %d, C max %d): compared exactly before that, "
                     "C invariants (n <= %d, w finite in [0,1]) after" % (places_max, t_over, max_n, max_n_c, places_max))
        if t_over is None:
            rep.note(T, "overflow", "the scenario never exceeded %d places: eviction path not exercised" % places_max)
    return rep.finish("synthetic '%s': %d frames; python max clusters %d (C %d), max admitted %d, live>=3 frames %d, dying frames %d, "
                      "memory-boosted frames %d, silent frames %d, shadow calls %d (true %d), resets %s"
                      % (name, T, max_n, max_n_c, admitted_max, n_live3, dying_frames, boosted, silent_frames,
                         shadow_calls[0], shadow_calls[1], sorted(resets)), lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fixtures", default=FIXTURES)
    ap.add_argument("--lib", default=None, help="prebuilt libscene.so (default: build scene_clusters.c alone)")
    args = ap.parse_args()
    defs, structs = parse_header()
    lib = build_lib(args.lib)
    # layout sanity: the ctypes mirror must agree with the compiler on sizes and the cfg offset
    probe = os.path.join(BUILD, "layout_probe.c")
    open(probe, "w").write('#include <stdio.h>\n#include <stddef.h>\n#include "scene_engine.h"\nint main(void){printf("%zu %zu %zu\\n",'
                           'sizeof(struct scene_clusters),sizeof(struct scene_config),offsetof(struct scene_clusters,cfg));return 0;}\n')
    subprocess.check_call(["cc", "-std=gnu99", "-I" + HOOK, probe, "-o", probe[:-2]])
    got = tuple(int(v) for v in subprocess.check_output([probe[:-2]]).split())
    want = (ctypes.sizeof(structs["scene_clusters"]), ctypes.sizeof(structs["scene_config"]), structs["scene_clusters"].cfg.offset)
    print("layout: C %s ctypes %s %s" % (got, want, "OK" if got == want else "MISMATCH"))
    assert got == want
    f = np.load(os.path.join(args.fixtures, "clusters.npz"), allow_pickle=True)
    cfg_dict = json.loads(str(f["config"])); frame_s = float(f["frame_s"])
    results = [run_fixture(lib, structs, os.path.join(args.fixtures, "clusters.npz")),
               run_oracle(lib, structs, cfg_dict, frame_s, "places", scenario_places(frame_s), defs["SCENE_PLACES_MAX"]),
               run_oracle(lib, structs, cfg_dict, frame_s, "overflow", scenario_overflow(frame_s), defs["SCENE_PLACES_MAX"], overflow=True)]
    print("OVERALL", "PASS" if all(results) else "FAIL")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
