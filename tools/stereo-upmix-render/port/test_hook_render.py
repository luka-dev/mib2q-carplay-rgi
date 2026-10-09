#!/usr/bin/env python3
"""test_hook_render.py — the SCENE backend of hook/audio/stereo_upmix.c, driven through
spa_stereo_upmix_process_s16() by tools/stereo-upmix-render/scene_render_hook.c in 768-frame calls,
against scene_upmix.py (same defaults, no mask) on the car-kit source album01.wav.

  1. builds build/scene_port/scene_render_hook with the project's host command (no FMA);
  2. renders album01.wav; the hook output lags by SPA_SCENE_LATENCY_FRAMES (1024): the first 1024 output
     frames are zero, then hook[1024 + i] vs reference[i] over L, R, Ls, Rs must be below -60 dB of the
     reference RMS (printed per channel too).  The reference is scene_upmix.stream_render in float64 with
     the C engine's decorrelation phase tables (scene_phase_table(), a different static uniform draw than
     numpy's: the side layer is phase-scrambled by design, so the raw CLI output differs on Ls/Rs by that
     table alone, which the printed CLI line shows); C and LFE are zero.  The C engine itself, run in
     float through scene_engine_push() (ctypes) on the same blocks, must agree with the hook's int16 output
     to 1 LSB: the backend adds nothing but the rounding (its own float32 error is the engine's);
  3. the output must not depend on the call size: --block 1024 and --block 100 (not a hop multiple, the
     FIFO bound's worst case) bit-identical to --block 768;
  4. --lfe assist: LFE non-zero, fronts = 31392/32768 x the reference (the assist's paired headroom).
  5. the mask net: --mask-blob (the SMSKNET1 blob exported from the fixtures' masknet_oracle.npz, or
     --blob PATH) with --mask-rows dumping the rows the backend installed at every 512-frame control
     boundary (256, 768, 1280, ... pushed frames; the renderer checks that cadence); scene_upmix.py driven
     by exactly those rows (a zero row for index 0 and for the priming boundaries, which is the engine's
     no-mask state exactly: e_inst and d_prot stay 0) must agree with the hook below -60 dB like the
     no-mask case, --block 100 must stay bit-identical, and --lfe assist with the mask keeps the fronts.
     With the mask the rear carries the instruments near full level, so on a few peak frames the
     cabin guard of stereo_upmix.c binds (the J525 Ls/Rs bus gain is 2.8) and scales the frame; the
     hook must equal the bare C engine driven by scene_engine_set_mask() at the 512r-256 rule
     (test_engine.py's) to the LSB everywhere else, and exactly its guard on those frames
     (spa_stereo_upmix_test_guard_s16 through the shared lib), which are excluded from the dB figures.

    python3 test_hook_render.py [--wav PATH] [--bin PATH] [--keep] [--fixtures DIR | --blob PATH | --no-mask]
"""
import argparse
import ctypes
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(TOOLS))
HOOK = os.path.join(ROOT, "hook", "audio")
BUILD = os.path.join(ROOT, "build", "scene_port")
WAV = "/Users/luka/Desktop/AUDI_2/upmix-data/scene_car_kit_2026-10-08/sources/album01.wav"
DEFAULT_FIX = "/Users/luka/Desktop/AUDI_2/upmix-data/scene_port_fixtures_2026-10-09"
LATENCY, TOL_DB, FRONT_Q15 = 1024, -60.0, 31392
MASK_HOP, MASK_BINS, MASK_PREFILL = 512, 257, 256
LIB_SOURCES = ["stereo_upmix.c", "scene_engine_math.c",
               "scene_clusters.c", "scene_engine.c", "scene_mask.c"]   # the backend too: its test guard
SOURCES = ["stereo_upmix.c",
           "soundcheck.c", "scene_engine_math.c", "scene_clusters.c", "scene_engine.c", "scene_mask.c"]
sys.path.insert(0, TOOLS)
sys.path.insert(0, HERE)
import scene_upmix as su  # noqa: E402

fails = []
LIB = None


def check(ok, what):
    print(("OK   " if ok else "FAIL ") + what)
    if not ok:
        fails.append(what)


def build(binary):
    os.makedirs(BUILD, exist_ok=True)
    cmd = ["cc", "-std=gnu99", "-O2", "-ffp-contract=off", "-Wall", "-Wextra", "-Werror",
           "-DSPA_STEREO_UPMIX_TESTING",       # --mask-rows (the test observer of stereo_upmix.h)
           "-I" + os.path.join(ROOT, "hook"), "-I" + HOOK,
           os.path.join(TOOLS, "scene_render_hook.c")] + [os.path.join(HOOK, s) for s in SOURCES] + \
          ["-lm", "-lpthread", "-o", binary]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        print("build failed\n" + r.stderr)
        sys.exit(1)
    print("built", os.path.relpath(binary, ROOT), "(%s)" % (r.stderr.strip() or "no warnings"))


def c_phase_tables():
    """phL/phR of the C engine (scene_phase_table of scene_engine_math.c) through a host shared lib."""
    global LIB
    lib = os.path.join(BUILD, "libscene_hook_test.so")
    cmd = ["cc", "-O2", "-std=gnu99", "-ffp-contract=off", "-fPIC", "-shared", "-DSPA_STEREO_UPMIX_TESTING",
           "-I" + HOOK] + [os.path.join(HOOK, s) for s in LIB_SOURCES] + ["-lm", "-lpthread", "-o", lib]
    subprocess.run(cmd, check=True)
    c = LIB = ctypes.CDLL(lib)
    cfg = su.SceneConfig()
    F = cfg.n_fft // 2 + 1
    tables = {}
    for sign in (+1.0, -1.0):
        out = (ctypes.c_float * (2 * F))()
        c.scene_phase_table(ctypes.c_int(F), ctypes.c_uint32(cfg.seed), ctypes.c_float(sign), out)
        v = np.frombuffer(out, np.float32).reshape(F, 2).astype(np.float64)
        tables[sign] = v[:, 0] + 1j * v[:, 1]
    return tables


def c_engine(lib, x, block=768, rows=None):
    """scene_engine_push() in float on x[n, 2] in blocks -> [n, 6] aligned like stream_render (no flush).
    rows: the renderer's --mask-rows records (one per 512-frame boundary, all zero = nothing installed):
    record m goes in through scene_engine_set_mask() once 256 + 512 m frames are in and before any more
    are pushed, the 512r-256 rule of scene_engine.c that test_engine.py proves against the fixtures."""
    F32 = np.ctypeslib.ndpointer(np.float32, flags="C")
    cfg = ctypes.create_string_buffer(4096)             # >= sizeof(struct scene_config)
    state = ctypes.create_string_buffer(1 << 20)        # >= sizeof(struct scene_state) (290 KB)
    lib.scene_config_defaults(cfg)
    assert lib.scene_engine_init(state, cfg) == 0
    lib.scene_engine_push.restype = ctypes.c_size_t
    lib.scene_engine_set_mask.argtypes = [ctypes.c_void_p, F32, F32, ctypes.c_int]
    out = np.zeros((len(x) + 2048, 6), np.float32)
    xin = np.ascontiguousarray(x, np.float32)
    got = m = 0
    for i in range(0, len(x), block):
        pos, end = i, min(len(x), i + block)
        while pos < end:
            nxt = end
            if rows is not None:
                while m < len(rows) and MASK_PREFILL + MASK_HOP * m <= pos:
                    if rows[m].any():
                        lib.scene_engine_set_mask(state, np.ascontiguousarray(rows[m, 0], np.float32),
                                                  np.ascontiguousarray(rows[m, 1], np.float32), MASK_BINS)
                    m += 1
                if m < len(rows):
                    nxt = min(end, MASK_PREFILL + MASK_HOP * m)
            blk = np.ascontiguousarray(xin[pos:nxt])
            cap = len(out) - got
            got += lib.scene_engine_push(state, blk.ctypes.data_as(ctypes.c_void_p), ctypes.c_size_t(len(blk)),
                                         out[got:].ctypes.data_as(ctypes.c_void_p), ctypes.c_size_t(cap))
            pos = nxt
    return out[:got].astype(np.float64)


def guard_frames(lib, hook_i16, eng, lfe):
    """Frames where the hook's int16 output is not the rounded engine: they must be exactly the
    cabin guard (spa_stereo_upmix_test_guard_s16) of the rounded engine frame.  Returns the mask."""
    I16 = np.ctypeslib.ndpointer(np.int16, flags="C")
    lib.spa_stereo_upmix_test_guard_s16.argtypes = [I16, ctypes.c_int]
    rounded = np.clip(np.rint(eng * 32768.0), -32768, 32767).astype(np.int16)
    guard = np.abs(hook_i16.astype(np.int32) - rounded.astype(np.int32)).max(1) > 1
    exact = 0
    for idx in np.flatnonzero(guard):
        frame = np.ascontiguousarray(rounded[idx])
        lib.spa_stereo_upmix_test_guard_s16(frame, lfe)
        exact += np.abs(frame.astype(np.int32) - hook_i16[idx].astype(np.int32)).max() <= 1
    check(exact == guard.sum(), "frames off the rounded engine are its cabin guard, to 1 LSB: %d of %d (%.3f %% of the stream)"
          % (exact, guard.sum(), 100.0 * guard.mean()))
    return guard


def render(binary, wav, out, *flags):
    r = subprocess.run([binary, *flags, wav, out], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr)
        sys.exit(1)
    with wave.open(out, "rb") as w:
        assert w.getnchannels() == 6 and w.getframerate() == 48000
        return np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, 6)


def db(err, ref):
    return 20.0 * np.log10(np.sqrt(np.mean(err ** 2)) / np.sqrt(np.mean(ref ** 2)) + 1e-30)


def mask_blob(a):
    """The SMSKNET1 blob: --blob as given, else exported from the fixtures' oracle; None = skip."""
    if a.no_mask:
        return None
    if a.blob:
        return a.blob
    npz = os.path.join(a.fixtures, "masknet_oracle.npz")
    if not os.path.exists(npz):
        print("SKIP mask net: no --blob and no %s" % npz)
        return None
    import export_masknet_weights as exp
    out = os.path.join(BUILD, "masknet_hook_test.bin")
    with open(out, "wb") as fh:
        fh.write(exp.blob(np.load(npz)))
    return out


def mask_section(a, binary, tmp, x, n, hook_nomask):
    blob = mask_blob(a)
    if not blob:
        return
    rows_path = os.path.join(tmp, "rows.f32")
    hook = render(binary, a.wav, os.path.join(tmp, "hook_mask.wav"), "--block", "768",
                  "--mask-blob", blob, "--mask-rows", rows_path) / 32768.0
    rows = np.fromfile(rows_path, np.float32).reshape(-1, 2, MASK_BINS).astype(np.float64)
    boundaries = (n - MASK_PREFILL) // MASK_HOP + 1
    check(rows.shape[0] == boundaries, "renderer wrote one record per control boundary (%d, cadence checked by it)"
          % boundaries)
    first = int(np.argmax(rows.reshape(len(rows), -1).any(1)))
    check(rows[:first].size == 0 or not rows[:first].any(), "no row before the net's first control")
    check(first == 2 and rows[2:].any(1).any(1).all(),
          "net rows from the third boundary on (control 0 = Python row 3), %d rows" % (len(rows) - first))
    check(not np.array_equal(hook, hook_nomask), "the mask changes the render")
    # the hook = the bare engine fed the same rows at the 512r-256 rule, rounded, plus the cabin guard
    eng = c_engine(LIB, x, rows=rows.astype(np.float32))[:n - LATENCY]
    h = hook[LATENCY:n]
    guard = guard_frames(LIB, np.rint(h * 32768.0).astype(np.int16), eng, 0)
    keep = ~guard
    # Python row r (frames 2r, 2r+1) = the record installed after 512 r - 256 frames = record r - 1
    inst = np.concatenate([np.zeros((1, MASK_BINS)), rows[:, 0]])
    prot = np.concatenate([np.zeros((1, MASK_BINS)), rows[:, 1]])
    print("reference: scene_upmix.stream_render driven by the %d rows the C runtime installed" % len(rows))
    r = su.stream_render(x, su.SceneConfig(), block=768, inst_mask=inst, prot_mask=prot)[:n - LATENCY]
    worst = -999.0
    for c, name in zip((0, 1, 4, 5), ("L", "R", "Ls", "Rs")):
        d = db(h[keep, c] - r[keep, c], r[keep, c])
        worst = max(worst, d)
        print("     %-2s error %.1f dB re its RMS (ref RMS %.1f dBFS; bare engine %.1f dB)"
              % (name, d, 20 * np.log10(np.sqrt(np.mean(r[:, c] ** 2))), db(eng[:, c] - r[:, c], r[:, c])))
    overall = db((h - r)[keep][:, [0, 1, 4, 5]], r[keep][:, [0, 1, 4, 5]])
    check(overall <= TOL_DB,
          "mask: hook vs stream_render on the same rows after the latency, guard frames aside: %.1f dB overall, "
          "worst channel %.1f dB (limit %.0f; bare engine %.1f dB)"
          % (overall, worst, TOL_DB, db((eng - r)[:, [0, 1, 4, 5]], r[:, [0, 1, 4, 5]])))
    other = render(binary, a.wav, os.path.join(tmp, "hook_mask100.wav"), "--block", "100", "--mask-blob", blob) / 32768.0
    check(np.array_equal(other, hook), "mask: --block 100 bit-identical to --block 768")
    lfe = render(binary, a.wav, os.path.join(tmp, "hook_mask_lfe.wav"), "--lfe", "assist", "--mask-blob", blob) / 32768.0
    check(np.abs(lfe[:, 3]).max() > 0.0 and not lfe[:, 2].any(), "mask + --lfe assist writes LFE, C stays zero")
    lf = lfe[LATENCY:n]
    guard_lfe = np.abs(np.rint(lf[:, :2] * 32768.0) - np.rint(np.rint(eng[:, :2] * 32768.0) * FRONT_Q15 / 32768.0)).max(1) > 1
    keep2 = keep & ~guard_lfe
    front = db(lf[keep2, :2] - r[keep2, :2] * (FRONT_Q15 / 32768.0), r[keep2, :2])
    check(front <= TOL_DB, "mask + --lfe assist fronts = %d/32768 x reference, guard frames aside (%d): %.1f dB"
          % (FRONT_Q15, guard_lfe.sum(), front))
    moved = np.flatnonzero(np.abs(lf[:, 4:] - h[:, 4:]).max(1) > 0)
    check(np.all(guard[moved] | guard_lfe[moved]) and len(moved) < 1e-3 * n,
          "mask + --lfe assist moves Ls/Rs only where a guard binds (%d frames)" % len(moved))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wav", default=os.environ.get("SCENE_HOOK_WAV", WAV))
    ap.add_argument("--bin", default=os.path.join(BUILD, "scene_render_hook"))
    ap.add_argument("--keep", action="store_true", help="keep the rendered WAVs in build/scene_port")
    ap.add_argument("--fixtures", default=os.environ.get("SCENE_PORT_FIXTURES", DEFAULT_FIX),
                    help="port fixtures dir with masknet_oracle.npz (the blob is exported from it)")
    ap.add_argument("--blob", help="an SMSKNET1 mask blob to use instead of exporting one")
    ap.add_argument("--no-mask", action="store_true", help="skip the mask net section")
    a = ap.parse_args()
    build(a.bin)
    tmp = BUILD if a.keep else tempfile.mkdtemp(prefix="scene_hook_")
    x = su.read_wav(a.wav)
    n = len(x)
    tables = c_phase_tables()
    su.phase_table = lambda n_bins, seed, sign: tables[sign][:n_bins]   # the C engine's draw, same override
    print("reference: scene_upmix.stream_render with the C phase tables on %s (%.1f s)"
          % (os.path.basename(a.wav), n / 48000.0))
    ref = su.stream_render(x, su.SceneConfig(), block=768)

    hook = render(a.bin, a.wav, os.path.join(tmp, "hook768.wav"), "--block", "768") / 32768.0
    check(hook.shape[0] == n, "output has the input's %d frames" % n)
    check(not hook[:LATENCY].any(), "first %d output frames are zero (the primed latency)" % LATENCY)
    check(not hook[:, 2].any() and not hook[:, 3].any(), "C and LFE are zero (LFE off)")
    h, r = hook[LATENCY:n], ref[:n - LATENCY]
    worst = -999.0
    for c, name in zip((0, 1, 4, 5), ("L", "R", "Ls", "Rs")):
        d = db(h[:, c] - r[:, c], r[:, c])
        worst = max(worst, d)
        print("     %-2s error %.1f dB re its RMS (ref RMS %.1f dBFS)" % (name, d, 20 * np.log10(np.sqrt(np.mean(r[:, c] ** 2)))))
    overall = db((h - r)[:, [0, 1, 4, 5]], r[:, [0, 1, 4, 5]])
    check(overall <= TOL_DB,
          "hook vs stream_render after the %d-frame latency: %.1f dB overall, worst channel %.1f dB (limit %.0f)"
          % (LATENCY, overall, worst, TOL_DB))
    eng = c_engine(LIB, x)
    check(eng.shape[0] >= n - LATENCY, "direct C engine emitted %d frames" % eng.shape[0])
    e = eng[:n - LATENCY]
    lsb = np.abs(np.rint(h * 32768.0) - np.rint(e * 32768.0)).max()
    print("     direct C engine (float) vs stream_render: %.1f dB overall, Ls %.1f dB, Rs %.1f dB"
          % (db((e - r)[:, [0, 1, 4, 5]], r[:, [0, 1, 4, 5]]), db(e[:, 4] - r[:, 4], r[:, 4]), db(e[:, 5] - r[:, 5], r[:, 5])))
    check(lsb <= 1.0, "hook int16 output = the direct C engine rounded (max %.0f LSB apart)" % lsb)

    cli = os.path.join(tmp, "cli.wav")
    subprocess.run([sys.executable, os.path.join(TOOLS, "scene_upmix.py"), a.wav, cli], check=True)
    with wave.open(cli, "rb") as w:
        y = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, 6) / 32768.0
    yc = y[:n - LATENCY]
    print("     vs the CLI's own WAV (python3 scene_upmix.py: numpy phase tables, int16 truncation): "
          "fronts %.1f dB, rears %.1f dB (rears differ by the phase table alone)"
          % (db((h - yc)[:, :2], yc[:, :2]), db((h - yc)[:, 4:], yc[:, 4:])))

    for block in (1024, 100):
        other = render(a.bin, a.wav, os.path.join(tmp, "hook%d.wav" % block), "--block", str(block)) / 32768.0
        check(np.array_equal(other, hook), "--block %d bit-identical to --block 768" % block)

    lfe = render(a.bin, a.wav, os.path.join(tmp, "hook_lfe.wav"), "--lfe", "assist") / 32768.0
    check(np.abs(lfe[:, 3]).max() > 0.0 and not lfe[:, 2].any(), "--lfe assist writes LFE, C stays zero")
    front = db(lfe[LATENCY:n, :2] - r[:, :2] * (FRONT_Q15 / 32768.0), r[:, :2])
    check(front <= TOL_DB, "--lfe assist fronts = %d/32768 x reference: %.1f dB" % (FRONT_Q15, front))
    check(np.array_equal(lfe[:, 4:], hook[:, 4:]), "--lfe assist leaves Ls/Rs unchanged")

    mask_section(a, a.bin, tmp, x, n, hook)

    if fails:
        print("\n%d FAILED:\n  " % len(fails) + "\n  ".join(fails))
        sys.exit(1)
    print("\nall OK")


if __name__ == "__main__":
    main()
