#!/usr/bin/env python3
"""Goldens for the C runtime of the fine mask net (ConvNet / conv2048, spec §18 item 2). Runs on the
training PC (needs torch + mask_net.py next to it):

  python scene_mask_fixtures.py --model D:\\upmix\\runs\\masknet-fine2048-c\\a\\mask_net.pt --wav X.wav --out X.masknet.npz

Saves: the model's weights (every state_dict tensor as float32), the standardize constants, the fine
STFT of the first controls, the 4 features per bin per control (before and after standardize/clamp),
every layer's activations for the first 64 controls (c1, c2, c3 outputs, GRU outputs, logits), the
output posteriors m [Tc, 257, n_out] for the whole file, the rear eligibility and the protect mask
exactly as protect_mask() returns it, and the control timing (hop 512, frame k+1 of the centred STFT)."""
import argparse, os, sys, wave, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mask_net as MN

ap = argparse.ArgumentParser(); ap.add_argument("--model", required=True); ap.add_argument("--wav", required=True); ap.add_argument("--out", required=True)
ap.add_argument("--controls", type=int, default=64, help="controls with full activations saved")
a = ap.parse_args()
torch, net, roles, dev = MN.load_model(a.model)
with wave.open(a.wav, "rb") as w:
    x = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, 2).T.astype(np.float32) / 32768
out = {}
sd = {k: v.detach().cpu().numpy().astype(np.float32) for k, v in net.modules.state_dict().items()}
for k, v in sd.items():
    out["w__" + k.replace(".", "__")] = v
out["weight_names"] = np.array(list(sd.keys()))
out["arch"] = np.array([net.ch, net.hidden, net.n_out])
out["roles_names"] = np.array(list(roles.names)); out["roles_protect"] = np.array(roles.protect); out["roles_eligible"] = np.array(roles.eligible)
with torch.no_grad():
    xt = torch.as_tensor(x[None], device=dev, dtype=torch.float32)
    Xf = MN.stft_fine(torch, xt)                                  # [1, 2, Tf, 1025]
    f_raw = MN.features(torch, Xf[:, 0], Xf[:, 1])[:, 1:]         # [1, Tc, 1025, 4]
    f = MN.standardize(torch, f_raw).clamp(-6, 6)
    B, T = f.shape[:2]
    out["x"] = x; out["Tc"] = T
    out["stft_fine_first"] = Xf[0, :, :a.controls + 2].cpu().numpy()          # [2, k, 1025] complex
    out["features_raw_first"] = f_raw[0, :a.controls].cpu().numpy()
    out["features_std_first"] = f[0, :a.controls].cpu().numpy()
    xx = f.reshape(B * T, MN.N_BINS_FINE, 4).transpose(1, 2)
    h1 = torch.tanh(net.c1(xx))[:, :, :MN.N_BINS]
    h2 = torch.tanh(net.c2(h1) + h1)
    h3 = torch.tanh(net.c3(h2) + h2)
    out["c1_first"] = h1[:a.controls].cpu().numpy(); out["c2_first"] = h2[:a.controls].cpu().numpy(); out["c3_first"] = h3[:a.controls].cpu().numpy()
    xr = h3.transpose(1, 2).reshape(B, T, MN.N_BINS, net.ch)
    tm, _ = net.time(xr.permute(0, 2, 1, 3).reshape(B * MN.N_BINS, T, net.ch))
    tm = tm.reshape(B, MN.N_BINS, T, net.hidden).permute(0, 2, 1, 3)
    out["gru_first"] = tm[0, :a.controls].cpu().numpy()
    logits = net.head(torch.cat([xr, tm], -1))
    out["logits_first"] = logits[0, :a.controls].cpu().numpy()
    m = net.forward(f)[0]
    out["posteriors"] = m.cpu().numpy().astype(np.float32)
    elig = MN.rear_eligibility(m, roles)
    out["elig_before_calib"] = elig.cpu().numpy().astype(np.float32)
    out["calib"] = np.array([float(net.calib["a"]), float(net.calib["b"])]) if getattr(net, "calib", None) else np.array([np.nan, np.nan])
    out["protect"] = MN.protect_mask(torch, net, roles, dev, x)
# standardize constants: mask_net.FEAT_STATS (restored by load_model)
st = MN.FEAT_STATS
out["feat_mean"] = np.asarray(st["mean"], dtype=np.float32) if st.get("mean") is not None else np.array([])
out["feat_std"] = np.asarray(st["std"], dtype=np.float32) if st.get("std") is not None else np.array([])
out["loss_mode"] = np.array(str(getattr(MN, "LOSS_MODE", "")))
out["softmax_out"] = np.array(int(getattr(net, "softmax_out", 0)))
np.savez_compressed(a.out, **out)
print(a.out, "controls", T, "posteriors", out["posteriors"].shape, "weights", len(sd), "softmax_out", out["softmax_out"])
