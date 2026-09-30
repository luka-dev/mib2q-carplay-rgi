#!/usr/bin/env python3
"""Frozen TRAIN-only eligible versus four-source fitting; one GPU owner.

Scene heads are dormant in both arms. This tests shared audio supervision before
scene training, broad data, teacher distillation or device claims.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import time

import numpy as np
import torch

import mask_net as MN
import sep_net as SN
import separator_causal_reference_probe as P
import separator_fit_probe as F
import unified_upmix_net as U

MODES = ("eligible_only", "joint_audio")
STEPS = 10000


def hashes():
    return {Path(m.__file__).name:F.sha(Path(m.__file__)) for m in (MN, SN, P, F, U)} | {
        Path(__file__).name:F.sha(Path(__file__))}


def configure(device):
    torch.set_num_threads(4)
    torch.manual_seed(1)
    torch.backends.cuda.matmul.allow_tf32 = False
    torch.backends.cudnn.allow_tf32 = False
    if device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA required")


def fixtures(a, device):
    arrays, manifest = P.data(a.cache, a.initial)
    w = torch.from_numpy(arrays).to(device)
    roles = torch.stack([w[:, 6], w[:, 0] + w[:, 1], w[:, 2:5].sum(1), w[:, 5]], 1)
    refs = torch.stack([SN.stft(torch, roles[:, i]) for i in range(4)], 2)
    return roles, refs, refs.sum(2), manifest


def role_loss(prediction, refs, roles, i, normalize=1.):
    n = roles.shape[-1]
    return F.objective(torch, prediction[:, :, i] * normalize,
                       refs[:, :, i] * normalize, roles[:, i] * normalize, n)


def prepare(a):
    if a.out.exists():
        raise FileExistsError("Preserve prior attempt; new root required")
    configure(a.device)
    a.out.mkdir(parents=True)
    # Manifest precedes GPU initialization/calibration. Derived coefficient is
    # determined exclusively by these exact TRAIN targets, before any update.
    input_manifest = json.loads((a.initial / "manifest.json").read_text())
    protocol = dict(architecture=U.ARCHITECTURE, scope="four exact frozen TRAIN8s crops only",
                    source_hashes=hashes(), input_manifest_sha256=F.sha(a.initial / "manifest.json"),
                    split_sha256=F.sha(a.cache / "split.json"), windows=input_manifest["windows"],
                    steps=STEPS, seed=1, lr=3e-4, weight_decay=1e-4, clip=5,
                    tf32=False, scene="dormant, frozen in both arms", sources=list(U.SOURCES),
                    calibration="0.20 * eligible/shared-trunk gradient norm divided by normalized protected mean/shared-trunk gradient norm; exact TRAIN fixtures",
                    selection="terminal10k both; no DEV, early acoustic stop, teacher or new data",
                    gates="joint eligible loss <=1.05 control; median SDR >=control-.3 and >=7dB; no crop delta below-1dB; each protected source median SDR>=0dB",
                    stats=input_manifest["stats"])
    F.write_json(a.out / "protocol.json", protocol)
    roles, refs, mixture, _ = fixtures(a, a.device)
    torch.manual_seed(1)
    net = U.UnifiedUpmixNet(log_mean=protocol["stats"]["mean"], log_std=protocol["stats"]["std"]).to(a.device)
    for p in net.scene.parameters():
        p.requires_grad_(False)
    prediction, _ = net.forward_stft(mixture, with_scene=False)
    prediction = prediction["source_stft"]
    rms = roles.square().mean((0, 2, 3)).sqrt()
    normalizers = (rms[0] / rms.clamp_min(1e-5)).tolist()
    shared = [p for module in [net.enc, net.blocks, net.interface, net.global_gru]
              for p in module.parameters()]
    base = role_loss(prediction, refs, roles, 0)
    aux = sum(role_loss(prediction, refs, roles, i, normalizers[i]) for i in range(1, 4)) / 3
    grad_base = torch.autograd.grad(base, shared, retain_graph=True)
    grad_aux = torch.autograd.grad(aux, shared)
    norm_base = math.sqrt(sum(float(g.detach().square().sum()) for g in grad_base))
    norm_aux = math.sqrt(sum(float(g.detach().square().sum()) for g in grad_aux))
    if min(norm_base, norm_aux) <= 0 or not np.isfinite([norm_base, norm_aux]).all():
        raise ValueError("Invalid TRAIN gradient calibration")
    coefficient = .20 * norm_base / norm_aux
    torch.save(dict(architecture=U.ARCHITECTURE, config=net.config,
                    state={k:v.detach().cpu() for k,v in net.state_dict().items()},
                    protocol_sha256=F.sha(a.out / "protocol.json")), a.out / "initial.pt")
    F.write_json(a.out / "prepared.json", dict(complete=True, protocol_sha256=F.sha(a.out / "protocol.json"),
                 initial_sha256=F.sha(a.out / "initial.pt"), normalizers=normalizers,
                 shared_gradient_norms=dict(eligible=norm_base, protected=norm_aux),
                 auxiliary_weight=coefficient, cost=net.cost()))
    print("UNIFIED_FIT_PREPARED", coefficient, net.cost(), flush=True)


def check(a):
    protocol = json.loads((a.out / "protocol.json").read_text())
    prepared = json.loads((a.out / "prepared.json").read_text())
    if protocol["source_hashes"] != hashes() or protocol["split_sha256"] != F.sha(a.cache / "split.json"):
        raise ValueError("Frozen sources or split changed")
    if protocol["input_manifest_sha256"] != F.sha(a.initial / "manifest.json"):
        raise ValueError("Frozen fitting fixtures changed")
    if prepared["protocol_sha256"] != F.sha(a.out / "protocol.json") or prepared["initial_sha256"] != F.sha(a.out / "initial.pt"):
        raise ValueError("Preparation/checkpoint changed")
    return protocol, prepared


def evaluate(net, mixture, refs, roles):
    net.eval()
    with torch.no_grad():
        pred, _ = net.forward_stft(mixture, with_scene=False)
        losses, sdr = {}, {}
        for i, source in enumerate(U.SOURCES):
            losses[source] = float(role_loss(pred["source_stft"], refs, roles, i))
            wav = SN.istft(torch, pred["source_stft"][:, :, i], roles.shape[-1])
            sdr[source] = SN.si_sdr(torch, wav, roles[:, i]).mean(-1).tolist()
        mask = pred["source_masks"][:, :, 0]
        tx = F.transmissions(torch, mask, {source:refs[:, :, i] for i, source in enumerate(U.SOURCES)}, roles.shape[-1])
    net.train()
    if not np.isfinite(list(losses.values())).all() or not np.isfinite(list(sdr.values())).all():
        raise RuntimeError("Nonfinite acoustic measurement")
    power = roles.square().sum((2, 3))
    present = power > power.sum(1, keepdim=True).clamp_min(1e-12) * 1e-6
    return dict(loss=losses, sdr_db=sdr, eligible_source_tx=tx,
                source_present={s:present[:, i].tolist() for i, s in enumerate(U.SOURCES)})


def temporal_audit(net, mixture):
    """Saved trained weights, real spectra; waveform latency is a separate gate."""
    net.eval()
    x = mixture[:1, :145]
    future = x.clone()
    future[:, 65:] *= complex(-2, .7)
    with torch.no_grad():
        full, _ = net.forward_stft(x, with_scene=False)
        changed, _ = net.forward_stft(future, with_scene=False)
        pieces, state = [], None
        for lo, hi in ((0, 1), (1, 64), (64, 65), (65, 145)):
            piece, state = net.forward_stft(x[:, lo:hi], state, with_scene=False)
            pieces.append(piece["source_stft"])
        y = full["source_stft"]
        prefix = float((y[:, :65]-changed["source_stft"][:, :65]).abs().square().sum()
                       / y[:, :65].abs().square().sum().clamp_min(1e-12))
        chunk = float((y-torch.cat(pieces, 1)).abs().square().sum()
                      / y.abs().square().sum().clamp_min(1e-12))
        suffix = float((y[:, 65:]-changed["source_stft"][:, 65:]).abs().square().sum()
                       / y[:, 65:].abs().square().sum().clamp_min(1e-12))
    if prefix > 1e-10 or chunk > 1e-10 or suffix <= 1e-9 or not np.isfinite([prefix, chunk, suffix]).all():
        raise ValueError("Saved trained audio causality/chunk audit failed")
    return dict(future_prefix_nse=prefix, chunk_nse=chunk, perturbed_suffix_nse=suffix,
                scope="neural STFT interface only; scene dormant; centered waveform transform not certified")


def load(path, device):
    ck = torch.load(path, map_location=device, weights_only=True)
    if ck.get("architecture") != U.ARCHITECTURE:
        raise ValueError("Wrong checkpoint architecture")
    net = U.UnifiedUpmixNet(**ck["config"]).to(device)
    net.load_state_dict(ck["state"], strict=True)
    return net, ck


def train(a):
    configure(a.device)
    protocol, prepared = check(a)
    if a.mode not in MODES:
        raise ValueError("Unknown fitting mode")
    folder = a.out / a.mode
    if folder.exists() and not a.resume:
        raise FileExistsError("Partial run requires explicit resume")
    folder.mkdir(exist_ok=True)
    net, ck = load(folder / "last.pt" if a.resume else a.out / "initial.pt", a.device)
    for p in net.scene.parameters():
        p.requires_grad_(False)
    scene_sha = P.tensors_sha(net.scene)
    roles, refs, mixture, _ = fixtures(a, a.device)
    optimizer = torch.optim.AdamW([p for p in net.parameters() if p.requires_grad], lr=3e-4, weight_decay=1e-4)
    start = 0
    rows = []
    elapsed = 0.
    if a.resume:
        if ck["protocol_sha256"] != F.sha(a.out / "protocol.json") or ck["mode"] != a.mode:
            raise ValueError("Resume semantics differ")
        optimizer.load_state_dict(ck["optimizer"])
        start, rows, elapsed = ck["step"], ck["rows"], ck["seconds"]
    begin = time.monotonic()
    for step in range(start, STEPS + 1):
        if step > start:
            optimizer.zero_grad(set_to_none=True)
            pred, _ = net.forward_stft(mixture, with_scene=False)
            loss = role_loss(pred["source_stft"], refs, roles, 0)
            if a.mode == "joint_audio":
                aux = sum(role_loss(pred["source_stft"], refs, roles, i, prepared["normalizers"][i]) for i in range(1, 4)) / 3
                loss = loss + prepared["auxiliary_weight"] * aux
            if not bool(torch.isfinite(loss)):
                raise RuntimeError("Nonfinite training loss")
            loss.backward()
            grad = float(torch.nn.utils.clip_grad_norm_([p for p in net.parameters() if p.requires_grad], 5))
            if not np.isfinite(grad):
                raise RuntimeError("Nonfinite gradients")
            optimizer.step()
        if step % 250 == 0:
            row = evaluate(net, mixture, refs, roles)
            row.update(step=step, seconds=elapsed + time.monotonic()-begin)
            if not rows or rows[-1]["step"] != step:
                rows.append(row)
            if P.tensors_sha(net.scene) != scene_sha:
                raise RuntimeError("Dormant scene parameters changed")
            F.write_json(folder / "progress.json", dict(mode=a.mode, rows=rows))
            temporary = folder / "last.pt.new"
            torch.save(dict(architecture=U.ARCHITECTURE, config=net.config, state=net.state_dict(),
                            protocol_sha256=F.sha(a.out / "protocol.json"), mode=a.mode, step=step,
                            optimizer=optimizer.state_dict(), rows=rows, seconds=row["seconds"]), temporary)
            temporary.replace(folder / "last.pt")
            print(a.mode, step, "loss", row["loss"]["eligible"], "eligibleSDR", float(np.median(row["sdr_db"]["eligible"])), "seconds", row["seconds"], flush=True)
    F.write_json(folder / "complete.json", dict(complete=True, step=STEPS, mode=a.mode,
                 checkpoint_sha256=F.sha(folder / "last.pt"), final=rows[-1], scene_frozen_sha256=scene_sha))


def audit(a):
    configure("cpu")
    protocol, prepared = check(a)
    roles, refs, mixture, _ = fixtures(a, "cpu")
    result = {}
    for mode in MODES:
        marker = json.loads((a.out / mode / "complete.json").read_text())
        path = a.out / mode / "last.pt"
        if not marker["complete"] or marker["step"] != STEPS or marker["checkpoint_sha256"] != F.sha(path):
            raise ValueError("Incomplete final checkpoint")
        net, ck = load(path, "cpu")
        if ck["mode"] != mode or ck["step"] != STEPS or ck["protocol_sha256"] != F.sha(a.out / "protocol.json"):
            raise ValueError("Final checkpoint semantics differ")
        cpu = evaluate(net, mixture, refs, roles)
        loss_error = max(abs(cpu["loss"][s] - marker["final"]["loss"][s]) for s in U.SOURCES)
        sdr_error = float(np.max(np.abs(np.asarray(list(cpu["sdr_db"].values())) - np.asarray(list(marker["final"]["sdr_db"].values())))))
        if loss_error > 1e-4 or sdr_error > .05:
            raise ValueError("Final CPU replay failed")
        if P.tensors_sha(net.scene) != marker["scene_frozen_sha256"]:
            raise ValueError("Frozen scene checkpoint differs")
        result[mode] = dict(checkpoint_sha256=F.sha(path), cpu=cpu, loss_error=loss_error, sdr_error=sdr_error,
                            temporal_audit=temporal_audit(net, mixture))
    old, new = (result[mode]["cpu"] for mode in MODES)
    delta = np.asarray(new["sdr_db"]["eligible"]) - np.asarray(old["sdr_db"]["eligible"])
    gates = dict(eligible_loss_nonworse_5pct=new["loss"]["eligible"] <= 1.05 * old["loss"]["eligible"],
                 eligible_median_no_more_than_03db_regression=np.median(new["sdr_db"]["eligible"]) >= np.median(old["sdr_db"]["eligible"])-.3,
                 eligible_median_at_least_7db=np.median(new["sdr_db"]["eligible"]) >= 7,
                 no_eligible_crop_regression_over_1db=bool((delta >= -1).all()))
    for s in U.SOURCES[1:]:
        valid = np.asarray(new["source_present"][s])
        gates[s+"_median_sdr_nonnegative"] = bool(valid.any() and np.median(np.asarray(new["sdr_db"][s])[valid]) >= 0)
    F.write_json(a.out / "complete.json", dict(complete=True, audit_verified=True, arms=result,
                 gates={k:bool(v) for k,v in gates.items()}, fitting_gate_pass=bool(all(gates.values())),
                 protocol_sha256=F.sha(a.out / "protocol.json"), scope="TRAIN-only; no scene, DEV, C, device or owner verdict"))
    print("UNIFIED_FIT_PAIR_COMPLETE", gates, flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("action", choices=("prepare", "train", "audit"))
    p.add_argument("--cache", type=Path, required=True)
    p.add_argument("--initial", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--device", default="cuda", choices=("cpu", "cuda"))
    p.add_argument("--mode", choices=MODES)
    p.add_argument("--resume", action="store_true")
    args = p.parse_args()
    {"prepare":prepare, "train":train, "audit":audit}[args.action](args)
