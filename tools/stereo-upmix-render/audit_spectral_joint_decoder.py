#!/usr/bin/env python3
"""Privileged fixed24-decoder diagnostic on the original four TRAIN8s crops.

Compare saved features with independent per-frame unconstrained and box-feasible
features. Joint weighted squared spectral error is a diagnostic surrogate, not
the training spectral/multires objective. No model updates or audio promotion.
"""
import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import torch
import train_spectral_context_fit as F


CHECKPOINT_SHA = "ff072970aef01749f44afc0c8bc010c542eb7e2384cdf88d8b7473b6a3854da2"
CYCLES = 64
CHUNK = 256


def nse(value, reference):
    energy = reference.abs().double().square().sum()
    return float((value-reference).abs().double().square().sum()/energy) if energy > 1e-12 else None


def system(x, weight, bias, lo, hi):
    """[frame,bin,source,channel,real/imag,latent] physical affine map."""
    xr, xi = x.real[:, :, None, :, None], x.imag[:, :, None, :, None]
    ar = xr*weight[None, :, :, :, 0]-xi*weight[None, :, :, :, 1]
    ai = xi*weight[None, :, :, :, 0]+xr*weight[None, :, :, :, 1]
    br = x.real[:, :, None, :]*bias[None, :, :, :, 0]-x.imag[:, :, None, :]*bias[None, :, :, :, 1]
    bi = x.imag[:, :, None, :]*bias[None, :, :, :, 0]+x.real[:, :, None, :]*bias[None, :, :, :, 1]
    if lo == 0:
        ai[:, 0] = 0; bi[:, 0] = 0
    if hi == F.T.SN.N_BINS:
        ai[:, -1] = 0; bi[:, -1] = 0
    return torch.stack([ar, ai], -2), torch.stack([br, bi], -1)


def solve(matrix, base, truth, weights, initial):
    frames, dim = matrix.shape[0], matrix.shape[-1]
    scale = weights.sqrt().reshape(1, -1)
    a = matrix.reshape(frames, -1, dim)*scale[:, :, None]
    y = (truth-base).reshape(frames, -1)*scale
    gram = a.transpose(1, 2)@a
    rhs = (a.transpose(1, 2)@y[:, :, None])[:, :, 0]
    ridge = gram.diagonal(dim1=-2, dim2=-1).mean(-1).clamp_min(1e-12)*1e-10
    unconstrained = torch.linalg.solve(gram+ridge[:, None, None]*torch.eye(dim, dtype=torch.float64), rhs[:, :, None])[:, :, 0]
    feasible = initial.clone()
    gradient = (gram@feasible[:, :, None])[:, :, 0]-rhs
    diagonal = gram.diagonal(dim1=-2, dim2=-1).clamp_min(1e-20)
    for _ in range(CYCLES):
        for j in range(dim):
            delta = (feasible[:, j]-gradient[:, j]/diagonal[:, j]).clamp(-1, 1)-feasible[:, j]
            feasible[:, j] += delta
            gradient += delta[:, None]*gram[:, :, j]
    residual = ((feasible-gradient/diagonal).clamp(-1, 1)-feasible).abs()
    old = ((a@initial[:, :, None])[:, :, 0]-y).square().sum()
    unbounded_error = ((a@unconstrained[:, :, None])[:, :, 0]-y).square().sum()
    bounded_error = ((a@feasible[:, :, None])[:, :, 0]-y).square().sum()
    if not torch.isfinite(unconstrained).all() or not torch.isfinite(feasible).all():
        raise ValueError("Nonfinite projection")
    if unbounded_error > old+1e-6*old.clamp_min(1) or bounded_error > old+1e-6*old.clamp_min(1):
        raise ValueError("Projection increases its declared quadratic")
    if feasible.abs().max() > 1:
        raise ValueError("Box constraint violated")
    return unconstrained, feasible, {
        "saved_weighted_squared_error": float(old),
        "unbounded_weighted_squared_error": float(unbounded_error),
        "bounded_weighted_squared_error": float(bounded_error),
        "unbounded_latent_max": float(unconstrained.abs().max()),
        "outside_tanh_coordinates": int((unconstrained.abs() > 1).sum()),
        "coordinates": unconstrained.numel(),
        "bounded_projected_step_max": float(residual.max()),
    }


def main(args):
    if args.out.exists():
        raise FileExistsError("Preserve the prior attempt")
    F.T.configure("cpu"); torch.set_num_threads(2)
    checkpoint = args.fit/"spectral/step20000.pt"
    complete = json.loads((args.fit/"complete.json").read_bytes())
    protocol = json.loads((args.fit/"protocol.json").read_bytes())
    if not complete["audit_verified"] or not complete["fitting_gate_pass"] or complete["smoke"]:
        raise ValueError("Verified non-smoke fitting terminal required")
    if F.T.F.sha(checkpoint) != CHECKPOINT_SHA or complete["arms"]["spectral"]["checkpoint_sha256"] != CHECKPOINT_SHA:
        raise ValueError("Predeclared checkpoint differs")
    if complete["protocol_sha256"] != F.T.F.sha(args.fit/"protocol.json"):
        raise ValueError("Protocol differs")
    if protocol["sources"] != F.hashes():
        raise ValueError("Frozen fitting helpers differ")
    net, ck = F.load(checkpoint, "cpu"); net.eval()
    if ck["step"] != 20000 or ck["mode"] != "spectral" or net.config["decoder"] != 24:
        raise ValueError("Wrong model")
    if F.T.P.tensors_sha(net.dec) != protocol["fixed_decoder_sha256"]:
        raise ValueError("Decoder differs")
    if F.T.F.sha(args.cache/"split.json") != protocol["split_sha256"] or len(protocol["windows"]) != 4:
        raise ValueError("TRAIN split or four-window fixture differs")
    train_ids = set(F.T.MN.load_split(str(args.cache))["train"])
    raw = []
    for window in protocol["windows"]:
        if window["song"] not in train_ids:
            raise ValueError("Non-TRAIN window")
        data = F.T.MN.read_window(str(args.cache/(window["song"]+".npy")), window["start_sample"], window["samples"])
        if hashlib.sha256(data.tobytes()).hexdigest() != window["selected_int16_sha256"]:
            raise ValueError("Original TRAIN crop differs")
        raw.append(data.astype(np.float32)/32768)
    w = torch.from_numpy(np.stack(raw))
    roles = torch.stack([w[:, 6], w[:, 0]+w[:, 1], w[:, 2:5].sum(1), w[:, 5]], 1)
    if hashlib.sha256(roles.numpy().tobytes()).hexdigest() != protocol["role_tensor_sha256"]:
        raise ValueError("Physical roles differ")
    args.out.mkdir(parents=True)
    coefficients = [1.]+[protocol["auxiliary_weight"]*protocol["normalizers"][i]**2/3 for i in range(1, 4)]
    frozen = {
        "version": "spectral-joint-fixed24-decoder-v1", "checkpoint_sha256": CHECKPOINT_SHA,
        "fit_protocol_sha256": F.T.F.sha(args.fit/"protocol.json"),
        "role_tensor_sha256": protocol["role_tensor_sha256"], "windows": protocol["windows"],
        "script_hashes": F.hashes()|{Path(__file__).name: F.T.F.sha(Path(__file__))},
        "joint_coefficients": coefficients, "cycles": CYCLES, "chunk": CHUNK,
        "projection": "float64 squared physical spectral error, unbounded trace-relative1e-10 ridge; bounded64cycle coordinate descent from actual tanh features",
        "scope": "Four original TRAIN8s cuts, fixed trained24 decoder, privileged independent per-frame features; no training/DEV/teacher/C/device/listening",
        "limitations": "Quadratic surrogate differs from original spectral+multires loss. Features use unavailable truth and bypass upstream decoder layers, encoder and temporal constraints. Bounded result is feasible but not certified optimum. Unbounded result may violate tanh. No universal architecture ceiling or product claim.",
    }
    F.write(args.out/"protocol.json", frozen)
    begin = time.monotonic()
    mixture = F.S.analyze(roles.sum(1))
    target = F.B.H.real_endpoints(F.B.spectra(roles))
    captured = {}
    handles = []
    for index, dec in enumerate(net.dec):
        def capture(module, inputs, output, index=index):
            captured[index] = inputs[0].detach().clone()
        handles.append(dec[-1].register_forward_hook(capture))
    with torch.inference_mode():
        prediction, _ = net.forward_stft(mixture, with_scene=False)
    for handle in handles:
        handle.remove()
    saved = F.B.H.real_endpoints(prediction["source_stft"])
    variants = {"saved_encoder": saved, "unbounded_joint": torch.zeros_like(saved),
                "bounded_joint": torch.zeros_like(saved), "unbounded_eligible_only": torch.zeros_like(saved)}
    bands = []
    for index, (lo, hi) in enumerate(net.bands):
        last = net.dec[index][-1]; dim = last.weight.shape[1]
        weight = last.weight.detach().double().reshape(hi-lo, 4, 2, 2, dim)
        bias = last.bias.detach().double().reshape(hi-lo, 4, 2, 2)
        x = mixture[:, :, lo:hi].reshape(-1, hi-lo, 2).to(torch.complex128)
        y = target[:, :, :, lo:hi].permute(0, 1, 3, 2, 4).reshape(-1, hi-lo, 4, 2).to(torch.complex128)
        initial = captured[index].reshape(x.shape[0], dim).double()
        if initial.abs().max() > 1:
            raise ValueError("Actual decoder latent is outside tanh")
        joint_weights = torch.tensor(coefficients, dtype=torch.float64)[None, :, None, None].expand(hi-lo, 4, 2, 2)
        results = {label: [] for label in variants if label != "saved_encoder"}
        records = []
        closure_max, closure_nse_max = 0., 0.
        for start in range(0, len(x), CHUNK):
            end = min(start+CHUNK, len(x))
            matrix, base = system(x[start:end], weight, bias, lo, hi)
            truth = torch.stack([y[start:end].real, y[start:end].imag], -1)
            native = torch.einsum("fbscqd,fd->fbscq", matrix, initial[start:end])+base
            reference = saved[:, :, :, lo:hi].permute(0, 1, 3, 2, 4).reshape_as(y)[start:end]
            native_complex = torch.complex(native[..., 0], native[..., 1])
            closure_max = max(closure_max, float((native_complex-reference).abs().max()))
            closure = nse(native_complex, reference)
            if closure is not None:
                closure_nse_max = max(closure_nse_max, closure)
                if closure > 1e-9:
                    raise ValueError("Physical affine map does not reproduce actual decoder")
            unbounded, bounded, record = solve(matrix, base, truth, joint_weights, initial[start:end])
            # Eligible-only lower residual ignores protected objectives. It is
            # reported separately and is never called a joint-source bound.
            ma = matrix[:, :, 0:1]; ba = base[:, :, 0:1]; tr = truth[:, :, 0:1]
            am = ma.reshape(end-start, -1, dim)
            gram = am.transpose(1, 2)@am
            rhs = am.transpose(1, 2)@(tr-ba).reshape(end-start, -1, 1)
            ridge = gram.diagonal(dim1=-2, dim2=-1).mean(-1).clamp_min(1e-12)*1e-10
            eligible = torch.linalg.solve(gram+ridge[:, None, None]*torch.eye(dim, dtype=torch.float64), rhs)[:, :, 0]
            if not torch.isfinite(eligible).all():
                raise ValueError("Nonfinite eligible projection")
            for label, latent in [("unbounded_joint", unbounded), ("bounded_joint", bounded), ("unbounded_eligible_only", eligible)]:
                value = torch.einsum("fbscqd,fd->fbscq", matrix, latent)+base
                results[label].append(torch.complex(value[..., 0], value[..., 1]).to(saved.dtype))
            records.append(record)
        for label, pieces in results.items():
            value = torch.cat(pieces).reshape(*saved.shape[:2], hi-lo, 4, 2).permute(0, 1, 3, 2, 4)
            variants[label][:, :, :, lo:hi] = value
        energy = saved[:, :, :, lo:hi].abs().double().square().sum()
        record = {
            "index": index, "lo_bin": lo, "hi_bin": hi, "dimensions": dim,
            "joint_real_outputs": (hi-lo)*16, "eligible_real_outputs": (hi-lo)*4,
            "decoder_all_source_weight_rank": int(torch.linalg.matrix_rank(weight.reshape(-1, dim))),
            "affine_maxabs": closure_max,
            "affine_chunk_nse_max": closure_nse_max,
            "saved_spectrum_energy": float(energy),
            "weighted_errors": {key: sum(r[key] for r in records) for key in ("saved_weighted_squared_error", "unbounded_weighted_squared_error", "bounded_weighted_squared_error")},
            "unbounded_latent_max": max(r["unbounded_latent_max"] for r in records),
            "outside_tanh_fraction": sum(r["outside_tanh_coordinates"] for r in records)/sum(r["coordinates"] for r in records),
            "bounded_projected_step_max": max(r["bounded_projected_step_max"] for r in records),
        }
        bands.append(record)
        print("JOINT_DECODER_BAND", index+1, len(net.bands), json.dumps(record["weighted_errors"]), flush=True)
    summaries = {}
    frequencies = torch.arange(F.T.SN.N_BINS)*48000/2048
    for label, spectrum in variants.items():
        wave = F.B.waves(spectrum, roles.shape[-1])
        source = {}
        for i, name in enumerate(F.T.U.SOURCES):
            present = roles[:, i].double().square().sum((1, 2)) > 1e-8
            sd = F.T.SN.si_sdr(torch, wave[:, i], roles[:, i]).mean(-1)
            source[name] = {
                "sdr_db": [float(v) if bool(ok) else None for v, ok in zip(sd, present)],
                "waveform_nse": [nse(v, r) for v, r in zip(wave[:, i], roles[:, i])],
                "spectral_nse": nse(spectrum[:, :, i], target[:, :, i]),
                "bands": {f"{lo}-{hi}Hz": nse(spectrum[:, :, i, (frequencies >= lo) & (frequencies < hi)], target[:, :, i, (frequencies >= lo) & (frequencies < hi)])
                          for lo, hi in [(0, 300), (300, 8000), (8000, 14000), (14000, 24000)]},
            }
        summaries[label] = source
    if F.T.F.sha(checkpoint) != CHECKPOINT_SHA:
        raise ValueError("Checkpoint changed")
    F.write(args.out/"complete.json", {
        "complete": True, "protocol_sha256": F.T.F.sha(args.out/"protocol.json"),
        "checkpoint_sha256": CHECKPOINT_SHA, "quality": summaries, "bands": bands,
        "seconds": time.monotonic()-begin, "product_pass": False,
        "scope": frozen["scope"], "limitations": frozen["limitations"],
    })
    print("SPECTRAL_JOINT_DECODER_COMPLETE", json.dumps(summaries), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fit", "cache", "out"):
        parser.add_argument("--"+name, type=Path, required=True)
    main(parser.parse_args())
