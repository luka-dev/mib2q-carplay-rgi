#!/usr/bin/env python3
"""test_mask.py — hook/audio/scene_mask.c (the streaming C runtime of the fine mask net, ConvNet / conv2048)
against port_fixtures/masknet_oracle.npz (scene_mask_fixtures.py) and mask_net.py run with torch.

Builds build/scene_port/libscene_mask.so from scene_mask.c alone, turns the fixture's w__ keys into the
SMSKNET1 blob with export_masknet_weights.py, streams the fixture's signal through scene_mask_control() in
512-sample calls and checks, printing the max error and PASS/FAIL per line (exit 1 on any FAIL):

  A. the fine STFT against the fixture's torch.stft (1e-4 relative to the frame's max magnitude);
  B. the port's arithmetic on its own inputs, as test_math.py does for the ill-conditioned cells: the
     standardized features against float64 numpy from the C spectra (1e-4 relative), and c1 / c2 / c3 /
     GRU / logits (first 64 controls) and the posteriors, eligibility and protect mask (all controls)
     against mask_net.ConvNet's layers run by torch ON THE C FEATURES (1e-3 absolute);
  C. end to end against the stored goldens (features_std_first, c*_first, gru_first, logits_first, protect)
     and against ConvNet.forward() rerun by torch on torch's own STFT, gated at 5e-2 absolute for the hidden
     layers and 1e-2 for the outputs: the cos/sin IPD features of fine bins whose power is ~1e-13 of the frame's
     maximum (e.g. bin 956 = 22 kHz) are the phase of float32 FFT rounding noise, in torch as much as in C, so
     these cells differ by O(0.1) in the features and by a few 1e-3 in the posteriors between any two float32
     implementations. Measured for the reference itself, torch CPU on this machine vs the training PC's
     fixture: features 3.5e-2, c1 7.4e-3, c2 9.7e-3, c3 4.4e-3, gru 1.4e-3, logits 3.8e-3, protect 4.2e-3;
     the port lands within 2x of that at every layer. A real layer bug shows as O(0.1..1) here.

Timing: call j returns control j-2 (frame j-1 of the centred STFT). The first control is bit-for-bit torch's
(the reflect prefix x[-i] = x[i] is rebuilt inside the runtime after call 1); the last two controls need
torch's reflected tail, which the test feeds as two extra calls (x[n-2], x[n-3], ..., x[n-1025]).

Fixture caveat: its `posteriors` / `elig_before_calib` keys were produced by net.forward() on ALREADY
standardized features (scene_mask_fixtures.py standardizes once for the per-layer dumps and ConvNet._logits()
standardizes again), so they disagree with `logits_first` and with `protect` (protect_mask() works from raw
features) by up to 0.98; they are not used.

  python3 test_mask.py [--fixtures DIR] [--lib PATH]
"""
import argparse
import ctypes
import os
import subprocess
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
HOOK = os.path.join(ROOT, "hook", "audio")
DEFAULT_FIX = "/Users/luka/Desktop/AUDI_2/upmix-data/scene_port_fixtures_2026-10-09"
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, ".."))
import export_masknet_weights as EXP   # noqa: E402

CH, HID, OUT, FBINS, BINS, HOP, NFFT = EXP.CH, EXP.HID, EXP.OUT, 1025, 257, 512, 2048
N_FIRST = 64
TOL_STFT, TOL_FEAT, TOL_ACT, TOL_E2E, TOL_E2E_HID = 1e-4, 1e-4, 1e-3, 1e-2, 5e-2
DBG_L, DBG_R, DBG_FEAT, DBG_H1, DBG_H2, DBG_H3, DBG_GRU, DBG_LOGITS, DBG_POST, DBG_ELIG, DBG_PROT = range(11)
fails = []


def build(lib):
    os.makedirs(os.path.dirname(lib), exist_ok=True)
    cmd = ["cc", "-O2", "-std=gnu99", "-ffp-contract=off", "-Wall", "-Wextra", "-fPIC", "-shared", "-I" + HOOK,
           os.path.join(HOOK, "scene_mask.c"), "-lm", "-o", lib]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("build failed:\n" + r.stderr)
        sys.exit(1)
    print("built", os.path.relpath(lib, ROOT), (r.stderr.strip() or "no warnings"))


def report(name, err, tol, kind="abs"):
    ok = err <= tol
    print(f"  {name:52s} max_{kind}={err:.3e} tol={tol:.0e} {'PASS' if ok else 'FAIL'}")
    if not ok:
        fails.append(name)


def maxabs(a, b):
    return float(np.abs(np.asarray(a, np.float64) - np.asarray(b, np.float64)).max())


def features64(L, R, mean, std):
    """features() + standardize() + clamp in float64 from complex spectra [T, 1025] -> [T, 1025, 4]."""
    L = L.astype(np.complex128); R = R.astype(np.complex128)
    pl = L.real ** 2 + L.imag ** 2; pr = R.real ** 2 + R.imag ** 2
    cross = L * np.conj(R); mag = np.sqrt(pl * pr) + 1e-9
    f = np.stack([np.log(pl + 1e-7), np.log(pr + 1e-7), cross.real / mag, cross.imag / mag], -1)
    return np.clip((f - mean) / std, -6, 6)


def load_net(z):
    import torch
    import mask_net as MN
    net = MN.ConvNet(torch, MN.bin_band(), ch=CH, hidden=HID, n_out=OUT)
    sd = {str(n): torch.from_numpy(np.array(z["w__" + str(n).replace(".", "__")])) for n in z["weight_names"]}
    net.modules.load_state_dict(sd)
    net.modules.eval()
    net.softmax_out = False
    MN.FEAT_STATS["mean"] = z["feat_mean"].tolist()
    MN.FEAT_STATS["std"] = z["feat_std"].tolist()
    return torch, MN, net


def torch_layers(torch, MN, net, f_std, keep):
    """ConvNet's layers on standardized features f_std [T, 1025, 4] (the fixture script's own code, no second
    standardize): per-layer activations for the first `keep` controls and the posteriors for all."""
    with torch.no_grad():
        f = torch.from_numpy(np.ascontiguousarray(f_std, dtype=np.float32))
        T = f.shape[0]
        xx = f.reshape(T, MN.N_BINS_FINE, 4).transpose(1, 2)
        h1 = torch.tanh(net.c1(xx))[:, :, :MN.N_BINS]
        h2 = torch.tanh(net.c2(h1) + h1)
        h3 = torch.tanh(net.c3(h2) + h2)
        xr = h3.transpose(1, 2).reshape(1, T, MN.N_BINS, net.ch)
        tm, _ = net.time(xr.permute(0, 2, 1, 3).reshape(MN.N_BINS, T, net.ch))
        tm = tm.reshape(1, MN.N_BINS, T, net.hidden).permute(0, 2, 1, 3)
        logits = net.head(torch.cat([xr, tm], -1))[0]
        post = torch.sigmoid(logits)
        return dict(c1=h1[:keep].numpy(), c2=h2[:keep].numpy(), c3=h3[:keep].numpy(), gru=tm[0, :keep].numpy(),
                    logits=logits[:keep].numpy(), post=post.numpy())


def torch_end_to_end(torch, MN, net, x):
    """protect_mask()'s path: torch's own STFT and features, ConvNet.forward() (one standardize) -> [Tc, 257, 4]."""
    with torch.no_grad():
        xt = torch.from_numpy(np.array(x))[None]
        Xf = MN.stft_fine(torch, xt)
        f_raw = MN.features(torch, Xf[:, 0], Xf[:, 1])[:, 1:]
        return net.forward(f_raw)[0].numpy()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fixtures", default=os.environ.get("SCENE_FIXTURES", DEFAULT_FIX))
    ap.add_argument("--lib", default=os.path.join(ROOT, "build", "scene_port", "libscene_mask.so"))
    args = ap.parse_args()
    build(args.lib)
    z = np.load(os.path.join(args.fixtures, "masknet_oracle.npz"))
    blob = EXP.blob(z)
    with open(os.path.join(os.path.dirname(args.lib), "masknet_c.bin"), "wb") as fh:
        fh.write(blob)
    elig_idx = np.asarray(z["roles_eligible"], int)
    mean, std = z["feat_mean"].astype(np.float64), z["feat_std"].astype(np.float64)

    lib = ctypes.CDLL(args.lib)
    F32 = np.ctypeslib.ndpointer(np.float32, flags="C_CONTIGUOUS")
    lib.scene_mask_size.restype = ctypes.c_size_t
    lib.scene_mask_init.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.scene_mask_init.restype = ctypes.c_int
    lib.scene_mask_reset.argtypes = [ctypes.c_void_p]
    lib.scene_mask_control.argtypes = [ctypes.c_void_p, F32, F32, F32]
    lib.scene_mask_control.restype = ctypes.c_int
    lib.scene_mask_debug.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.scene_mask_debug.restype = ctypes.POINTER(ctypes.c_float)
    state = ctypes.create_string_buffer(lib.scene_mask_size())
    print(f"  sizeof(struct scene_mask) = {lib.scene_mask_size()} bytes, blob {len(blob)} bytes")
    bad = bytearray(blob); bad[0] ^= 1
    report("init rejects a bad magic", 0.0 if lib.scene_mask_init(state, bytes(bad)) == -1 else 1.0, 0.0, "flag")
    report("init accepts the blob", 0.0 if lib.scene_mask_init(state, blob) == 0 else 1.0, 0.0, "flag")

    def dbg(what, n):
        return np.ctypeslib.as_array(lib.scene_mask_debug(state, what), shape=(n,)).copy()

    # ------------------------------------------------------------------ stream the signal through the C runtime
    x = z["x"].astype(np.float32)
    n = x.shape[1]
    Tc = int(z["Tc"])
    tail = x[:, n - 2:n - 2 - NFFT // 2:-1]                    # torch's reflect pad at the end: x[n-2] .. x[n-1025]
    xs = np.ascontiguousarray(np.concatenate([x, tail], 1).T)    # [n + 1024, 2]
    calls = xs.shape[0] // HOP
    L_c = np.zeros((N_FIRST, FBINS), np.complex64); R_c = np.zeros((N_FIRST, FBINS), np.complex64)
    feat_c = np.zeros((Tc, FBINS, 4), np.float32)
    act_c = dict(c1=np.zeros((N_FIRST, CH, BINS), np.float32), c2=np.zeros((N_FIRST, CH, BINS), np.float32),
                 c3=np.zeros((N_FIRST, CH, BINS), np.float32), gru=np.zeros((N_FIRST, BINS, HID), np.float32),
                 logits=np.zeros((N_FIRST, BINS, OUT), np.float32))
    post_c = np.zeros((Tc, BINS, OUT), np.float32)
    elig_c = np.zeros((Tc, BINS), np.float32); prot_c = np.zeros((Tc, BINS), np.float32)
    e_row = np.zeros(BINS, np.float32); p_row = np.zeros(BINS, np.float32)
    k = 0
    t0 = time.perf_counter()
    for j in range(calls):
        blk = np.ascontiguousarray(xs[j * HOP:(j + 1) * HOP])
        got = lib.scene_mask_control(state, blk, e_row, p_row)
        if j < 2:
            report(f"call {j} primes (returns 0)", float(got), 0.0, "flag")
            continue
        assert got == 1 and k == j - 2
        if k < N_FIRST:
            L_c[k] = dbg(DBG_L, 2 * FBINS).view(np.complex64); R_c[k] = dbg(DBG_R, 2 * FBINS).view(np.complex64)
            act_c["c1"][k] = dbg(DBG_H1, CH * BINS).reshape(CH, BINS)
            act_c["c2"][k] = dbg(DBG_H2, CH * BINS).reshape(CH, BINS)
            act_c["c3"][k] = dbg(DBG_H3, CH * BINS).reshape(CH, BINS)
            act_c["gru"][k] = dbg(DBG_GRU, HID * BINS).reshape(HID, BINS).T
            act_c["logits"][k] = dbg(DBG_LOGITS, OUT * BINS).reshape(OUT, BINS).T
        feat_c[k] = dbg(DBG_FEAT, 4 * FBINS).reshape(4, FBINS).T
        post_c[k] = dbg(DBG_POST, OUT * BINS).reshape(OUT, BINS).T
        elig_c[k] = e_row; prot_c[k] = p_row
        k += 1
    dt = time.perf_counter() - t0
    assert k == Tc, (k, Tc)
    print(f"  streamed {calls} calls -> {k} controls in {dt:.2f} s ({1e3 * dt / calls:.3f} ms per control, host)")

    # ------------------------------------------------------------------ A. the STFT against torch.stft
    print("A. fine analysis vs the fixture's torch.stft (frame k+1 per control k):")
    ref = z["stft_fine_first"][:, 1:N_FIRST + 1]                     # [2, 64, 1025]
    report("stft_fine", float(np.abs(np.stack([L_c, R_c]) - ref).max() / np.abs(ref).max()), TOL_STFT, "rel")

    # ------------------------------------------------------------------ B. the port's arithmetic on its own inputs
    print("B. the net on the C spectra / C features (torch run on the port's inputs):")
    f64 = features64(L_c, R_c, mean, std)
    report("features vs float64 numpy of the C spectra", float(np.abs(feat_c[:N_FIRST] - f64).max() / np.abs(f64).max()), TOL_FEAT, "rel")
    torch, MN, net = load_net(z)
    own = torch_layers(torch, MN, net, feat_c, N_FIRST)
    for name in ("c1", "c2", "c3", "gru", "logits"):
        report(name + " vs torch on the C features", maxabs(act_c[name], own[name]), TOL_ACT)
    own_elig = np.minimum(1.0, own["post"][..., elig_idx].sum(-1))
    report("posteriors vs torch on the C features (all)", maxabs(post_c, own["post"]), TOL_ACT)
    report("elig vs torch on the C features (all)", maxabs(elig_c, own_elig), TOL_ACT)
    report("protect vs torch on the C features (all)", maxabs(prot_c, 1 - own_elig), TOL_ACT)
    report("elig + protect == 1", maxabs(elig_c + prot_c, 1.0), 1e-6)

    # ------------------------------------------------------------------ C. end to end against the goldens
    print("C. end to end vs the stored goldens and torch's own STFT (noise-phase cells, see the docstring):")
    report("features vs features_std_first", float(np.abs(feat_c[:N_FIRST] - z["features_std_first"]).max()), 0.5)
    for name, key in (("c1", "c1_first"), ("c2", "c2_first"), ("c3", "c3_first"), ("gru", "gru_first"), ("logits", "logits_first")):
        report(name + " vs " + key, maxabs(act_c[name], z[key]), TOL_E2E_HID)
    e2e = torch_end_to_end(torch, MN, net, x)
    e2e_elig = np.minimum(1.0, e2e[..., elig_idx].sum(-1))
    print(f"  (reference noise: torch CPU rerun vs the fixture's protect = {maxabs(1 - e2e_elig, z['protect']):.2e};"
          f" the fixture's double-standardized 'posteriors' key vs the rerun = {maxabs(z['posteriors'], e2e):.2e})")
    report("posteriors vs ConvNet.forward() (torch STFT, all)", maxabs(post_c, e2e), TOL_E2E)
    report("elig vs 1 - protect (fixture, all)", maxabs(elig_c, 1 - z["protect"]), TOL_E2E)
    report("protect vs protect (fixture, all)", maxabs(prot_c, z["protect"]), TOL_E2E)

    print("FAIL:", fails) if fails else print("ALL PASS")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
