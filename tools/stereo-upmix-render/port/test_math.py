#!/usr/bin/env python3
"""Golden test of hook/audio/scene_engine_math.c (the pure helpers, tables, FFT and config defaults of the
scene renderer port) against port_fixtures/functions.npz and the config JSON of stream_spatial.npz.

Builds build/scene_port/libscene.so from hook/audio/scene_engine*.c (the math module alone if the engine
module is absent or does not compile yet), loads it with ctypes and prints per check the max absolute error,
the error relative to the array's max magnitude ("rel") and PASS/FAIL; exits 1 on any FAIL.

Tolerances: helpers/tables 1e-6, FFT 1e-5 (both relative to the max magnitude of the compared array; the
scalar helpers per value). decompose_frame / centre_removed_pan are ill-conditioned per cell (P_N is a
difference of near-equal numbers, p the ratio of two tiny squares), so the 1e-6 gate is applied against the
float64 reference evaluated on the SAME float32-rounded inputs, and the comparison against the stored
float64-input fixture is gated at the engine statistics tolerance 1e-4.

  python3 test_math.py [--fixtures DIR] [--lib PATH]
"""
import argparse, ctypes, glob, json, os, re, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
HOOK = os.path.join(ROOT, "hook", "audio")
HEADER = os.path.join(HOOK, "scene_engine.h")
import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '../../..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
DEFAULT_FIX = _WS + "/upmix-data/scene_port_fixtures_2026-10-17"
sys.path.insert(0, os.path.join(HERE, ".."))
import scene_upmix as su   # the reference (read only)

TOL_HELP, TOL_FFT, TOL_STAT = 1e-6, 1e-5, 1e-4
N, H, F, SR = 1024, 256, 513, 48000
fails = []


def build(lib):
    os.makedirs(os.path.dirname(lib), exist_ok=True)
    srcs = sorted(glob.glob(os.path.join(HOOK, "scene_engine*.c"))) + [os.path.join(HOOK, "scene_clusters.c")]
    for attempt in (srcs, [os.path.join(HOOK, "scene_engine_math.c")]):
        cmd = ["cc", "-O2", "-std=gnu99", "-ffp-contract=off", "-Wall", "-Wextra", "-fPIC", "-shared", "-I" + HOOK] + attempt + ["-lm", "-o", lib]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode == 0:
            print("built", os.path.relpath(lib, ROOT), "from", [os.path.basename(s) for s in attempt], (r.stderr.strip() or "no warnings"))
            return
        print("build failed for", [os.path.basename(s) for s in attempt], "\n" + r.stderr)
    sys.exit(1)


def check(name, got, ref, tol, extra=""):
    dt = np.complex128 if np.iscomplexobj(got) or np.iscomplexobj(ref) else np.float64
    got = np.asarray(got, dt); ref = np.asarray(ref, dt)
    err = np.abs(got - ref)
    scale = max(float(np.abs(ref).max()), 1e-300)
    rel = float(err.max()) / scale
    ok = rel <= tol
    print(f"  {name:46s} max_abs={err.max():.3e} rel={rel:.3e} tol={tol:.0e} {'PASS' if ok else 'FAIL'}{extra}")
    if not ok:
        fails.append(name)
    return rel


def check_flag(name, ok, extra=""):
    print(f"  {name:46s} {'PASS' if ok else 'FAIL'}{extra}")
    if not ok:
        fails.append(name)


def rel_per_value(got, ref):
    got = np.asarray(got, np.float64); ref = np.asarray(ref, np.float64)
    return float((np.abs(got - ref) / np.maximum(np.abs(ref), 1e-300)).max())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fixtures", default=os.environ.get("SCENE_FIXTURES", DEFAULT_FIX))
    ap.add_argument("--lib", default=os.path.join(ROOT, "build", "scene_port", "libscene.so"))
    args = ap.parse_args()
    build(args.lib)
    lib = ctypes.CDLL(args.lib)
    F32 = np.ctypeslib.ndpointer(np.float32, flags="C_CONTIGUOUS")
    C64 = np.ctypeslib.ndpointer(np.complex64, flags="C_CONTIGUOUS")   # scene_cpx[] = interleaved re, im
    I32 = np.ctypeslib.ndpointer(np.int32, flags="C_CONTIGUOUS")
    c_int, c_float = ctypes.c_int, ctypes.c_float
    lib.scene_smoothstep.argtypes = [c_float, c_float, c_float]; lib.scene_smoothstep.restype = c_float
    lib.scene_ema_alpha.argtypes = [c_float, c_int]; lib.scene_ema_alpha.restype = c_float
    lib.scene_decompose_frame.argtypes = [F32, F32, C64, c_int] + [F32] * 7
    lib.scene_centre_removed_pan.argtypes = [F32, F32, C64, c_int, c_float, F32, F32, F32, I32]
    lib.scene_erb_groups.argtypes = [c_int, c_int, I32, F32, I32]; lib.scene_erb_groups.restype = c_int
    lib.scene_phase_table.argtypes = [c_int, ctypes.c_uint32, c_float, C64]
    lib.scene_rfft1024.argtypes = [F32, C64, F32]
    lib.scene_irfft1024.argtypes = [C64, F32, F32]

    fx = np.load(os.path.join(args.fixtures, "functions.npz"))
    cfg_json = json.loads(str(np.load(os.path.join(args.fixtures, "stream_spatial.npz"))["config"]))

    # ------------------------------------------------------------------ config defaults (header comments + fixture JSON)
    print("scene_config_defaults")
    src = open(HEADER).read()
    body = src[src.index("struct scene_config {"):]; body = body[:body.index("};")]
    fields = re.findall(r"^\s*(int|float)\s+(\w+)\s*;\s*/\*\s*default\s+([-\w.]+)", body, re.M)

    class Cfg(ctypes.Structure):
        _fields_ = [(n, c_int if t == "int" else c_float) for t, n, _ in fields]
    lib.scene_config_defaults.argtypes = [ctypes.POINTER(Cfg)]
    cfg = Cfg(); lib.scene_config_defaults(ctypes.byref(cfg))
    check_flag(f"struct layout: {len(fields)} fields, {ctypes.sizeof(Cfg)} bytes", ctypes.sizeof(Cfg) == 4 * len(fields) and len(fields) == len(cfg_json))
    bad_hdr, bad_json = [], []
    for t, n, d in fields:
        cv = getattr(cfg, n)
        hv = int(d) if t == "int" else float(d.rstrip("f"))
        if t == "int" and cv != hv or t == "float" and np.float32(cv) != np.float32(hv):
            bad_hdr.append((n, cv, hv))
        jv = (cfg_json["front_follows"] == "slow") if n == "front_follows_slow" else cfg_json[n]
        if jv is None or (t == "int" and cv != int(jv)) or (t == "float" and np.float32(cv) != np.float32(jv)):
            bad_json.append((n, cv, jv))
    check_flag("defaults == header comments", not bad_hdr, f" {bad_hdr}" if bad_hdr else "")
    check_flag("defaults == stream_spatial config JSON", not bad_json, f" {bad_json}" if bad_json else "")

    # ------------------------------------------------------------------ scalar helpers
    print("scene_smoothstep / scene_ema_alpha")
    x32 = fx["ss_x"].astype(np.float32); lo, hi = float(fx["ss_lo"]), float(fx["ss_hi"])
    ss = np.array([lib.scene_smoothstep(v, lo, hi) for v in x32], np.float32)
    check("smoothstep vs fixture", ss, fx["ss_y"], TOL_HELP)
    check("smoothstep vs reference on float32 inputs", ss, su.smoothstep(x32.astype(np.float64), lo, hi), TOL_HELP,
          f" per-value rel={rel_per_value(ss, su.smoothstep(x32.astype(np.float64), lo, hi)):.3e}")
    check_flag("smoothstep clips: ss(lo-1)=0, ss(hi+1)=1, ss(mid)=0.5",
               lib.scene_smoothstep(lo - 1, lo, hi) == 0 and lib.scene_smoothstep(hi + 1, lo, hi) == 1 and abs(lib.scene_smoothstep((lo + hi) / 2, lo, hi) - 0.5) < 1e-7)
    ema = np.array([lib.scene_ema_alpha(float(t), H) for t in fx["ema_tau_ms"]])
    r = rel_per_value(ema, fx["ema_alpha"])
    check_flag(f"ema_alpha per-value rel={r:.3e} tol={TOL_HELP:.0e}", r <= TOL_HELP, f" taus={fx['ema_tau_ms'].tolist()}")
    check_flag("ema_alpha(tau<=0) == 1", lib.scene_ema_alpha(0.0, H) == 1.0 and lib.scene_ema_alpha(-5.0, H) == 1.0)

    # ------------------------------------------------------------------ decompose_frame
    print("scene_decompose_frame")
    PL, PR, C = fx["PL"].astype(np.float32), fx["PR"].astype(np.float32), fx["C"].astype(np.complex64)
    n = len(PL); outs = [np.empty(n, np.float32) for _ in range(7)]
    lib.scene_decompose_frame(PL, PR, C, n, *outs)
    ref32 = su.decompose_frame(PL.astype(np.float64), PR.astype(np.float64), C.astype(np.complex128))
    for k, name in enumerate(("PN", "PS", "a", "b", "theta", "Pa", "Pb")):
        check(f"{name} vs fixture", outs[k], fx[name], TOL_STAT)
        check(f"{name} vs reference on float32 inputs", outs[k], ref32[k], TOL_HELP)
    # NULL outputs and aliasing (the engine discards most outputs and may write in place)
    lib.scene_decompose_frame.argtypes = [F32, F32, C64, c_int] + [ctypes.c_void_p] * 7
    pn_only = np.empty(n, np.float32)
    lib.scene_decompose_frame(PL, PR, C, n, pn_only.ctypes.data, None, None, None, None, None, None)
    check("PN with the other outputs NULL", pn_only, outs[0], 0.0)
    lib.scene_decompose_frame.argtypes = [F32, F32, C64, c_int] + [F32] * 7

    # ------------------------------------------------------------------ centre_removed_pan
    print("scene_centre_removed_pan")
    eps = float(fx["eps"]); PN32 = outs[0]
    p, e = np.empty(n, np.float32), np.empty(n, np.float32); v = np.empty(n, np.int32)
    lib.scene_centre_removed_pan(PL, PR, C, n, eps, PN32, p, e, v)
    p_ref, e_ref, v_ref = su.centre_removed_pan(PL.astype(np.float64), PR.astype(np.float64), C.astype(np.complex128), np.float32(eps), PN32.astype(np.float64))
    check_flag(f"valid vs reference on float32 inputs: {int((v.astype(bool) != v_ref).sum())} mismatches", bool((v.astype(bool) == v_ref).all()))
    nm = int((v.astype(bool) != fx["v_cr"]).sum())
    check_flag(f"valid vs fixture: {nm} mismatches of {n} ({int(fx['v_cr'].sum())} valid)", nm <= 2, " (input quantization can flip a cell at the eps edge)" if nm else "")
    check("p vs fixture", p, fx["p_cr"], TOL_STAT)
    check("p vs reference on float32 inputs", p, p_ref, TOL_HELP)
    check("e vs fixture", e, fx["e_cr"], TOL_STAT)
    check("e vs reference on float32 inputs", e, e_ref, TOL_HELP)
    p0, e0, v0 = np.empty(n, np.float32), np.empty(n, np.float32), np.empty(n, np.int32)
    lib.scene_centre_removed_pan.argtypes = [F32, F32, C64, c_int, c_float, ctypes.c_void_p, F32, F32, I32]
    lib.scene_centre_removed_pan(PL, PR, C, n, eps, None, p0, e0, v0)
    p0_ref, _, v0_ref = su.centre_removed_pan(PL.astype(np.float64), PR.astype(np.float64), C.astype(np.complex128), np.float32(eps), None)
    check("p with PN=NULL (tol 0) vs reference", p0, p0_ref, TOL_HELP, f" valid mismatches={int((v0.astype(bool) != v0_ref).sum())}")

    # ------------------------------------------------------------------ erb_groups / freqs / window tables
    print("scene_erb_groups / tables")
    group = np.zeros(F, np.int32); gfreq = np.zeros(160, np.float32); gcount = np.zeros(160, np.int32)
    G = lib.scene_erb_groups(F, N, group, gfreq, gcount)
    check_flag(f"G == {int(fx['G'])} (got {G})", G == int(fx["G"]))
    check_flag("group[] identical to fixture", bool((group == fx["group"]).all()))
    counts = np.bincount(fx["group"], minlength=G)
    check_flag("group_count == bincount(group)", bool((gcount[:G] == counts).all()) and (gcount[G:] == 0).all())
    fg_ref = np.bincount(fx["group"], weights=fx["freqs"], minlength=G) / np.maximum(counts, 1)
    r = rel_per_value(gfreq[:G], fg_ref)
    check_flag(f"group_freq per-value rel={r:.3e} tol={TOL_HELP:.0e}", r <= TOL_HELP)
    freqs32 = (np.arange(F, dtype=np.float32) * np.float32(SR)) / np.float32(N)
    check("freqs formula (float32 k*SR/N) vs fixture", freqs32, fx["freqs"], 0.0)
    win32 = np.sin((np.float32(np.pi) * (np.arange(N, dtype=np.float32) + np.float32(0.5))) / np.float32(N)).astype(np.float32)
    check("sine window formula in float32 vs fixture", win32, fx["window"], TOL_HELP)

    # ------------------------------------------------------------------ phase_table (structure only: a different stream than numpy's PCG64)
    print("scene_phase_table")
    phL = np.empty(F, np.complex64); phR = np.empty(F, np.complex64); ph2 = np.empty(F, np.complex64); ph9 = np.empty(F, np.complex64)
    lib.scene_phase_table(F, 7, +1.0, phL); lib.scene_phase_table(F, 7, -1.0, phR); lib.scene_phase_table(F, 7, +1.0, ph2); lib.scene_phase_table(F, 9, +1.0, ph9)
    check("|phL| == 1", np.abs(phL.astype(np.complex128)), np.ones(F), TOL_HELP)
    check("phR == conj(phL) (sign mirror)", phR, np.conj(phL), TOL_HELP)
    check_flag("deterministic for a seed, different across seeds", bool((phL == ph2).all()) and np.abs(phL - ph9).max() > 0.5)
    check_flag(f"phases spread: |mean phL|={abs(phL.mean()):.3f} (< 0.2), |mean phase|/pi={abs(np.angle(phL).mean()) / np.pi:.3f}", abs(phL.mean()) < 0.2)
    check("fixture phL unit magnitude (structure the engine test loads)", np.abs(fx["phL"]), np.ones(F), 1e-12)
    check("fixture phR == conj(fixture phL)", fx["phR"], np.conj(fx["phL"]), 1e-12)

    # ------------------------------------------------------------------ FFT: rfft of every stft() frame, irfft of every frame, istft round trip
    print("scene_rfft1024 / scene_irfft1024")
    sig, frames_ref, istft_ref = fx["stft_in"], fx["stft_frames"], fx["istft_out"]
    win = fx["window"]; pad = np.concatenate([np.zeros(N), sig, np.zeros(N)]); T = frames_ref.shape[0]
    check_flag(f"frame count: fixture {T}, stft() of {len(sig)} samples -> {(len(pad) - N) // H + 1}", (len(pad) - N) // H + 1 == T)
    scratch = np.empty(4 * N, np.float32); X = np.empty(F, np.complex64); x = np.empty(N, np.float32)
    worst = {"rfft": 0.0, "rfft(float32 windowing)": 0.0, "irfft": 0.0, "round trip": 0.0}
    frames_c = np.zeros((T, N))
    for t in range(T):
        frame = (pad[t * H:t * H + N] * win).astype(np.float32)
        lib.scene_rfft1024(frame, X, scratch)
        scale = max(np.abs(frames_ref[t]).max(), 1e-300)            # the first frames lie in the zero pad: exact zeros on both sides
        worst["rfft"] = max(worst["rfft"], np.abs(X - frames_ref[t]).max() / scale)
        frame32 = (pad[t * H:t * H + N].astype(np.float32) * win.astype(np.float32)).astype(np.float32)   # what the engine will do
        lib.scene_rfft1024(frame32, X, scratch)
        worst["rfft(float32 windowing)"] = max(worst["rfft(float32 windowing)"], np.abs(X - frames_ref[t]).max() / scale)
        lib.scene_irfft1024(frames_ref[t].astype(np.complex64), x, scratch)
        ref = np.fft.irfft(frames_ref[t], n=N)                       # the per-frame irfft istft() overlap-adds
        worst["irfft"] = max(worst["irfft"], np.abs(x - ref).max() / max(np.abs(ref).max(), 1e-300))
        frames_c[t] = x
        lib.scene_rfft1024(frame, X, scratch); lib.scene_irfft1024(X, x, scratch)
        worst["round trip"] = max(worst["round trip"], float(np.abs(x - frame).max()) / max(float(np.abs(frame).max()), 1e-300))
    for k, r in worst.items():
        check_flag(f"{k}: worst frame rel={r:.3e} tol={TOL_FFT:.0e} over {T} frames", r <= TOL_FFT)
    # istft() overlap-add of the C frames (float64 OLA, exactly the Python loop) against the fixture
    out = np.zeros(N * 2 + len(sig) + N); norm = np.zeros_like(out)
    for t in range(T):
        i = t * H
        if i + N > len(out):
            break
        out[i:i + N] += frames_c[t] * win; norm[i:i + N] += win * win
    check("istft() of the C irfft frames vs istft_out", (out / np.maximum(norm, 1e-6))[N:N + len(sig)], istft_ref, TOL_FFT)
    check("istft_out round trip vs stft_in (fixture self-check)", istft_ref, sig, 1e-9)
    # imaginary parts of DC and Nyquist are ignored like numpy's irfft
    Xd = frames_ref[20].astype(np.complex64).copy(); Xd[0] += 3j; Xd[-1] -= 2j     # frame 20 holds signal (0..3 are in the pad)
    lib.scene_irfft1024(Xd, x, scratch)
    check("irfft ignores imag of bins 0 and 512 (numpy c2r)", x, np.fft.irfft(frames_ref[20], n=N), TOL_FFT)
    # pure tones: bin-exact magnitude and the leakage floor of the float32 FFT
    tone = np.cos(2 * np.pi * 100 * np.arange(N) / N).astype(np.float32)
    lib.scene_rfft1024(tone, X, scratch)
    check("rfft of cos(2*pi*100 n/N): bin 100 = 512, others 0", X, np.fft.rfft(tone.astype(np.float64)), TOL_FFT)

    print("FAIL:", fails if fails else "none", "->", "ALL PASS" if not fails else f"{len(fails)} FAILED")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
