#!/usr/bin/env python3
"""test_engine.py — hook/audio/scene_engine.c (the per-frame engine and the block driver of the scene renderer
port) against port_fixtures/stream_spatial.npz and stream_mask.npz.

Builds build/scene_port/libscene.so from scene_engine_math.c + scene_clusters.c + scene_engine.c, mirrors the
header's structs in ctypes (checked against the compiler's sizeof/offsetof), loads the Python phase tables
(functions.npz) through the test-only setter, then per fixture:

  1. frame level: the fixture's analysis frames XL[t]/XR[t] through scene_engine_frame(); subL/subR/rearL/rearR
     per frame within 1e-4 relative to the frame's max magnitude (max over the four spectra); n_clusters and
     the places (centre, sigma, mass, gain) from the clusters struct.
  2. block level: x in blocks of 768 (and 1000) through scene_engine_push() + flush; the [n,6] output against
     out6, error RMS below -60 dB of the output RMS (printed); the two block sizes must be bit-identical.
     For stream_mask.npz the mask rows are installed through scene_engine_set_mask() at the moments that
     reproduce the Python's inst_mask[(t*H)//512] exactly: row r after 512r-256 and before 512r pushed samples
     (blocks are split there; at frame level row min(t//2, rows-1) is installed before frame t).

    python3 test_engine.py [--fixtures DIR] [--lib PATH]
"""
import argparse
import ctypes
import json
import os
import re
import subprocess
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
HOOK = os.path.join(ROOT, "hook", "audio")
HEADER = os.path.join(HOOK, "scene_engine.h")
BUILD = os.path.join(ROOT, "build", "scene_port")
FIXTURES = ("/private/tmp/claude-501/-Users-luka-Desktop-AUDI-2-Patches-mhi2-carplay/"
            "68989c4d-7e7a-4c32-aa08-b7c2a6c66c89/scratchpad/port_fixtures")
SOURCES = ["scene_engine_math.c", "scene_clusters.c", "scene_engine.c"]
TOL_FRAME, TOL_DB, TOL_PLACE = 1e-4, -60.0, 1e-5
N, H, F, SR = 1024, 256, 513, 48000
MASK_HOP = 512
fails = []


# ----------------------------------------------------------------------------- header -> ctypes
def parse_header():
    """#defines and struct scene_config / scene_place / scene_memory / scene_clusters / scene_state as ctypes."""
    text = open(HEADER).read()
    defs = {}
    for m in re.finditer(r"^#define[ \t]+(SCENE_\w+)[ \t]+([^\n]+?)[ \t]*(?:/\*.*)?$", text, re.M):
        defs[m.group(1)] = int(eval(m.group(2), {}, dict(defs)))

    class scene_cpx(ctypes.Structure):
        _fields_ = [("re", ctypes.c_float), ("im", ctypes.c_float)]
    types = {"int": ctypes.c_int, "float": ctypes.c_float, "uint32_t": ctypes.c_uint32, "scene_cpx": scene_cpx}
    structs = {}
    for m in re.finditer(r"struct (\w+) \{(.*?)\};", text, re.S):
        body = re.sub(r"/\*.*?\*/", "", m.group(2), flags=re.S)
        fields = []
        for decl in body.split(";"):
            decl = " ".join(decl.split())
            if not decl:
                continue
            tname, rest = re.match(r"(struct \w+|\w+) (.*)", decl).groups()
            ct = structs[tname[7:]] if tname.startswith("struct ") else types[tname]
            for d in rest.split(","):
                md = re.match(r"(\w+)((?:\[[^\]]+\])*)$", d.strip())
                t = ct
                for n in reversed([int(eval(x, {}, dict(defs))) for x in re.findall(r"\[([^\]]+)\]", md.group(2))]):
                    t = t * n
                fields.append((md.group(1), t))
        structs[m.group(1)] = type(m.group(1), (ctypes.Structure,), {"_fields_": fields})
    return defs, structs


def check_layout(structs):
    """The ctypes mirror must agree with the compiler on sizes and a few offsets of struct scene_state."""
    names = ["freqs", "phL", "window", "PL", "e_inst", "clusters", "elig_row", "inbuf", "ola", "dline", "bline", "subL", "rearR", "fft_scratch"]
    probe = os.path.join(BUILD, "layout_probe_engine.c")
    open(probe, "w").write('#include <stdio.h>\n#include <stddef.h>\n#include "scene_engine.h"\nint main(void){printf("%zu %zu",'
                           'sizeof(struct scene_state),sizeof(struct scene_config));' +
                           "".join('printf(" %%zu",offsetof(struct scene_state,%s));' % n for n in names) + 'return 0;}\n')
    subprocess.check_call(["cc", "-std=gnu99", "-I" + HOOK, probe, "-o", probe[:-2]])
    got = tuple(int(v) for v in subprocess.check_output([probe[:-2]]).split())
    S = structs["scene_state"]
    want = (ctypes.sizeof(S), ctypes.sizeof(structs["scene_config"])) + tuple(getattr(S, n).offset for n in names)
    ok = got == want
    print("layout: sizeof(scene_state) C %d ctypes %d, %d offsets %s" % (got[0], want[0], len(names), "OK" if ok else "MISMATCH %s vs %s" % (got, want)))
    if not ok:
        fails.append("layout")
    return ok


def build(lib_path):
    os.makedirs(BUILD, exist_ok=True)
    cmd = ["cc", "-O2", "-std=gnu99", "-ffp-contract=off", "-Wall", "-Wextra", "-fPIC", "-shared", "-I" + HOOK] + \
          [os.path.join(HOOK, s) for s in SOURCES] + ["-lm", "-o", lib_path]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("build failed\n" + r.stderr)
        sys.exit(1)
    print("built", os.path.relpath(lib_path, ROOT), "from", SOURCES, "(%s)" % (r.stderr.strip() or "no warnings"))
    return ctypes.CDLL(lib_path)


class Engine:
    """One struct scene_state with the library's entry points bound."""

    def __init__(self, lib, structs, cfg_dict, phL, phR):
        self.lib = lib
        S = structs["scene_state"]; Cfg = structs["scene_config"]; P = ctypes.POINTER(S)
        F32 = np.ctypeslib.ndpointer(np.float32, flags="C_CONTIGUOUS")
        C64 = np.ctypeslib.ndpointer(np.complex64, flags="C_CONTIGUOUS")
        lib.scene_engine_init.argtypes = [P, ctypes.POINTER(Cfg)]; lib.scene_engine_init.restype = ctypes.c_int
        lib.scene_engine_test_set_phase.argtypes = [P, C64, C64]
        lib.scene_engine_frame.argtypes = [P, C64, C64, ctypes.c_int]
        lib.scene_engine_set_mask.argtypes = [P, F32, F32, ctypes.c_int]
        lib.scene_engine_clear_mask.argtypes = [P]
        lib.scene_engine_reset.argtypes = [P]
        lib.scene_engine_push.argtypes = [P, F32, ctypes.c_size_t, F32, ctypes.c_size_t]; lib.scene_engine_push.restype = ctypes.c_size_t
        lib.scene_engine_flush.argtypes = [P, F32, ctypes.c_size_t]; lib.scene_engine_flush.restype = ctypes.c_size_t
        lib.scene_clusters_n_live.argtypes = [ctypes.POINTER(structs["scene_clusters"])]; lib.scene_clusters_n_live.restype = ctypes.c_int
        self.s = S()
        self.cfg = Cfg()
        for name, _ in Cfg._fields_:
            setattr(self.cfg, name, cfg_dict["front_follows"] == "slow" if name == "front_follows_slow" else cfg_dict[name])
        rc = lib.scene_engine_init(ctypes.byref(self.s), ctypes.byref(self.cfg))
        assert rc == 0, rc
        lib.scene_engine_test_set_phase(ctypes.byref(self.s), np.ascontiguousarray(phL, np.complex64), np.ascontiguousarray(phR, np.complex64))
        self.spec = {k: np.frombuffer(getattr(self.s, k), np.complex64) for k in ("subL", "subR", "rearL", "rearR")}   # live views

    def frame(self, XL, XR, t):
        self.lib.scene_engine_frame(ctypes.byref(self.s), XL, XR, t)

    def set_mask(self, elig, prot):
        self.lib.scene_engine_set_mask(ctypes.byref(self.s), elig, prot, len(elig))

    def push(self, x32):
        out = np.zeros((len(x32) + H, 6), np.float32)
        n = self.lib.scene_engine_push(ctypes.byref(self.s), x32, len(x32), out, len(out))
        return out[:n]

    def flush(self):
        out = np.zeros((N + H, 6), np.float32)
        n = self.lib.scene_engine_flush(ctypes.byref(self.s), out, len(out))
        return out[:n]

    def n_live(self):
        return self.lib.scene_clusters_n_live(ctypes.byref(self.s.clusters))

    def places(self):
        return np.array([(c.centre, c.sigma, c.mass, c.gain) for c in self.s.clusters.c[:self.s.clusters.n]], np.float64).reshape(-1, 4)


def mask_rows(fx):
    if "inst_mask" not in fx.files:
        return None
    return np.ascontiguousarray(fx["inst_mask"], np.float32), np.ascontiguousarray(fx["prot_mask"], np.float32)


# ----------------------------------------------------------------------------- 1. frame level
def run_frames(lib, structs, fx, phL, phR, name):
    cfg = json.loads(str(fx["config"])); masks = mask_rows(fx)
    XL = np.ascontiguousarray(fx["XL"], np.complex64); XR = np.ascontiguousarray(fx["XR"], np.complex64)
    T = XL.shape[0]
    eng = Engine(lib, structs, cfg, phL, phR)
    keys = ("subL", "subR", "rearL", "rearR")
    refs = {k: fx[k] for k in keys}                      # decompress the npz arrays once, not per access
    n_clusters, places = fx["n_clusters"], fx["places"]
    worst = {k: (0.0, -1) for k in keys}; bad_frames = []; first_bad = None
    n_bad, places_bad, place_err = 0, 0, 0.0
    cur = -1
    t0 = time.time()
    for t in range(T):
        if masks is not None:
            r = min((t * H) // MASK_HOP, masks[0].shape[0] - 1)
            if r != cur:
                eng.set_mask(masks[0][r], masks[1][r]); cur = r
        eng.frame(XL[t], XR[t], t)
        scale = max(max(float(np.abs(refs[k][t]).max()) for k in keys), 1e-30)
        frame_bad = False
        for k in keys:
            rel = float(np.abs(eng.spec[k].astype(np.complex128) - refs[k][t]).max()) / scale
            if rel > worst[k][0]:
                worst[k] = (rel, t)
            if rel > TOL_FRAME:
                frame_bad = True
                if first_bad is None:
                    err = np.abs(eng.spec[k].astype(np.complex128) - refs[k][t]); b = int(err.argmax())
                    first_bad = (t, k, rel, b, eng.spec[k][b], refs[k][t][b], eng.n_live(), int(n_clusters[t]))
        if frame_bad:
            bad_frames.append(t)
        n_ref = int(n_clusters[t])
        if eng.n_live() != n_ref:
            n_bad += 1
        pref = places[t]; nref = int((~np.isnan(pref[:, 0])).sum()); pl = eng.places()
        if len(pl) != nref:
            places_bad += 1
        elif nref:
            e = float(np.abs(pl - pref[:nref]).max()); place_err = max(place_err, e)
            if e > TOL_PLACE:
                places_bad += 1
    dt = time.time() - t0
    print("frame level %s: %d frames (%.0f frames/s on the host)" % (name, T, T / dt))
    for k in keys:
        print("  %-6s worst rel=%.3e at frame %d" % (k, worst[k][0], worst[k][1]))
    print("  frames over %.0e: %d%s" % (TOL_FRAME, len(bad_frames), (" first %s ... last %d" % (bad_frames[:8], bad_frames[-1])) if bad_frames else ""))
    if first_bad:
        print("  first divergence: frame %d %s rel=%.3e bin %d C=%s ref=%s (n_clusters C %d ref %d)" % first_bad)
    print("  n_clusters mismatches: %d; places mismatches: %d (max abs err %.3e)" % (n_bad, places_bad, place_err))
    ok = not bad_frames and n_bad == 0 and places_bad == 0
    print("  " + ("PASS" if ok else "FAIL"))
    if not ok:
        fails.append("frames " + name)
    return ok


# ----------------------------------------------------------------------------- 2. block level
def stream(lib, structs, fx, phL, phR, block):
    """x through push() in blocks (split at the mask install points) + flush -> [n, 6]."""
    cfg = json.loads(str(fx["config"])); masks = mask_rows(fx)
    x = np.ascontiguousarray(fx["x"], np.float32); n = len(x)
    eng = Engine(lib, structs, cfg, phL, phR)
    rows = masks[0].shape[0] if masks is not None else 0
    install_at = lambda r: max(0, MASK_HOP * r - H)          # row r applies from frame 2r, computed once 512r samples are in
    outs = []; r = 0
    for b in range(0, n, block):
        pos, end = b, min(n, b + block)
        while pos < end:
            while r < rows and install_at(r) <= pos:
                eng.set_mask(masks[0][r], masks[1][r]); r += 1
            nxt = end if r >= rows else min(end, install_at(r))
            outs.append(eng.push(x[pos:nxt])); pos = nxt
    while r < rows and install_at(r) < n + N:                # rows for the tail frames of the flush
        eng.set_mask(masks[0][r], masks[1][r]); r += 1
    outs.append(eng.flush())
    y = np.concatenate(outs)
    assert len(y) >= n, (len(y), n)
    return y[:n].astype(np.float64)


def db(err, ref):
    return 20.0 * np.log10(max(np.sqrt((err ** 2).mean()), 1e-300) / np.sqrt((ref ** 2).mean()))


def run_blocks(lib, structs, fx, phL, phR, name):
    ref = fx["out6"]; n = len(ref)
    t0 = time.time(); y768 = stream(lib, structs, fx, phL, phR, 768); dt = time.time() - t0
    y1000 = stream(lib, structs, fx, phL, phR, 1000)
    ch = [0, 1, 4, 5]
    total = db(y768[:, ch] - ref[:, ch], ref[:, ch])
    per = [db(y768[:, c] - ref[:, c], ref[:, c]) for c in ch]
    zero = bool((y768[:, 2:4] == 0).all())
    same = float(np.abs(y768 - y1000).max())
    print("block level %s: %d samples, blocks of 768 (%.1fx realtime on the host)" % (name, n, n / SR / dt))
    print("  error vs out6: %.1f dB of the output RMS (L %.1f, R %.1f, Ls %.1f, Rs %.1f dB; tol %.0f dB); max abs %.3e; C/LFE zero: %s"
          % (total, per[0], per[1], per[2], per[3], TOL_DB, float(np.abs(y768[:, ch] - ref[:, ch]).max()), zero))
    print("  blocks of 1000 vs 768: max abs diff %.3e (%s)" % (same, "bit-identical" if same == 0 else "DIFFERENT"))
    ok = total <= TOL_DB and zero and same == 0
    print("  " + ("PASS" if ok else "FAIL"))
    if not ok:
        fails.append("blocks " + name)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fixtures", default=os.environ.get("SCENE_FIXTURES", FIXTURES))
    ap.add_argument("--lib", default=os.path.join(BUILD, "libscene.so"))
    args = ap.parse_args()
    defs, structs = parse_header()
    assert (defs["SCENE_NFFT"], defs["SCENE_HOP"], defs["SCENE_BINS"], defs["SCENE_MASK_HOP"]) == (N, H, F, MASK_HOP)
    lib = build(args.lib)
    check_layout(structs)
    fn = np.load(os.path.join(args.fixtures, "functions.npz"))
    phL, phR = fn["phL"], fn["phR"]
    for name in ("spatial", "mask"):
        fx = np.load(os.path.join(args.fixtures, "stream_%s.npz" % name), allow_pickle=True)
        run_frames(lib, structs, fx, phL, phR, name)
        run_blocks(lib, structs, fx, phL, phR, name)
    print("FAIL:", fails if fails else "none", "->", "ALL PASS" if not fails else "%d FAILED" % len(fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
