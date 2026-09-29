#!/usr/bin/env python3
"""Apply the frozen finite-TRAIN feature gate and verify checkpoint semantics.

CPU only. Refuse incomplete/mismatched experiments. The numerical gate is for
the finite fitting diagnostic, never product promotion or device certification.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import mask_net as MN
import sep_net as SN
import separator_invariant_features as IF
from separator_fit_probe import sha, write_json, objective


def run(a):
    if a.out.exists():
        raise FileExistsError("preserve earlier result audit")
    control = json.loads((a.control / "fit_complete.json").read_text())
    candidate = json.loads((a.candidate / "complete.json").read_text())
    cm = json.loads((a.control / "manifest.json").read_text())
    nm = json.loads((a.candidate / "manifest.json").read_text())
    if nm["control_sha256"] != sha(a.control / "fit_complete.json"):
        raise ValueError("frozen control result changed")
    if nm["control_checkpoint_sha256"] != sha(a.control / "fit.pt"):
        raise ValueError("frozen control checkpoint changed")
    if nm["windows"] != cm["windows"] or nm["feature_mode"] != "invariant6":
        raise ValueError("window/feature intervention changed")
    if nm["feature_sha256"] != sha(Path(IF.__file__)):
        raise ValueError("feature implementation changed")
    if control["step"] != 10000 or candidate["final"]["step"] != 10000:
        raise ValueError("matched full horizon required")
    if sha(a.cache / "split.json") != nm["split_sha256"]:
        raise ValueError("cache split changed")
    arrays = []
    for r in nm["windows"]:
        raw = MN.expand_slots(MN.read_window(str(a.cache / (r["song"] + ".npy")),
                                           r["start_sample"], r["samples"]))
        if hashlib.sha256(raw.tobytes()).hexdigest() != r["selected_int16_sha256"]:
            raise ValueError("frozen crop changed")
        arrays.append(raw.astype(np.float32) / 32768)
    torch, net, device = IF.load(str(a.candidate / "fit.pt"), device="cpu")
    torch.set_num_threads(4)
    w = torch.as_tensor(np.stack(arrays), device=device)
    mix, eligible = w.sum(1), w[:, 6]
    X, E = SN.stft(torch, mix), SN.stft(torch, eligible)
    captured = {}
    # Registered after the replacement hook: capture the actual encoded input.
    handles = [enc.register_forward_pre_hook(
        lambda module, inputs, index=i: captured.__setitem__(index, inputs[0].detach().clone()))
               for i, enc in enumerate(net.enc)]
    with torch.no_grad():
        estimate = net.separate_stft(X)
        scale = net.running_level(X.abs().mean((2, 3)))[:, :, None, None].clamp_min(1e-6)
        normalized = X / scale
        cross = normalized[..., 0] * normalized[..., 1].conj()
        den = (normalized[..., 0].abs()*normalized[..., 1].abs()).clamp_min(1e-8)
        log = ((torch.log(X.abs()+1e-5)-SN.STATS["mean"])/SN.STATS["std"]).clamp(-6, 6)
        expected = torch.cat((torch.log1p(normalized.abs()), log,
                              (cross.real/den)[..., None], (cross.imag/den)[..., None]), -1)
        feature_error = max(float((captured[i]-expected[:, :, lo:hi].reshape_as(captured[i])).abs().max())
                            for i, (lo, hi) in enumerate(net.bands))
        mask = net.last_mask.clone()
        negative = -net.separate_stft(-X)
        n = mix.shape[-1]
        parity_loss = float(objective(torch, estimate, E, eligible, n))
        parity_sdr = SN.si_sdr(torch, SN.istft(torch, estimate, n), eligible).mean(-1).numpy()
        polarity = float((estimate-negative).abs().square().sum()/estimate.abs().square().sum())
        # Reanalysis can have different floating-point arithmetic on CPU/GPU.
        # Tight absolute tolerances detect wrong feature/checkpoint semantics.
        if feature_error > 2e-5 or abs(parity_loss-candidate["final"]["loss"]) > 1e-4:
            raise ValueError("feature formula/checkpoint parity failed")
        if np.max(np.abs(parity_sdr-np.array(candidate["final"]["sdr_db"]))) > .01:
            raise ValueError("checkpoint waveform parity failed")
        # Contrast with the wrong generic loader, without modifying its source.
        wrong = SN.BandSepNet(torch, norm="stream", mask="complex2").to("cpu")
        wrong.modules.load_state_dict(net.modules.state_dict())
        wrong_estimate = wrong.separate_stft(X)
        wrong_loss = float(objective(torch, wrong_estimate, E, eligible, n))
    for handle in handles:
        handle.remove()
    base, new = control["final"], candidate["final"]
    delta = np.array(new["sdr_db"])-np.array(base["sdr_db"])
    gates = dict(loss=new["loss"] <= .9*base["loss"],
                 median_sdr=bool(np.median(new["sdr_db"]) >= np.median(base["sdr_db"])+1),
                 per_window=int((delta >= 0).sum()) >= 3 and float(delta.min()) >= -1,
                 polarity=new["polarity_mask_nse"] <= 1e-8 and new["polarity_output_nse"] <= 1e-8)
    result = dict(scope="single-seed four-window TRAIN feature comparison, not product acceptance",
        control_loss=base["loss"], candidate_loss=new["loss"], loss_ratio=new["loss"]/base["loss"],
        control_median_sdr_db=float(np.median(base["sdr_db"])),
        candidate_median_sdr_db=float(np.median(new["sdr_db"])), per_window_sdr_delta_db=delta.tolist(),
        control_waveform_nse=base["reconstruction_nse"], candidate_waveform_nse=new["reconstruction_nse"],
        control_grad_norm=base["grad_norm_before_clip"], candidate_grad_norm=new["grad_norm_before_clip"],
        control_seconds=base["seconds"], candidate_seconds=new["seconds"], gates=gates,
        diagnostic_success=all(gates.values()), cpu_feature_max_error=feature_error,
        cpu_checkpoint_loss=parity_loss, cpu_polarity_output_nse=polarity,
        wrong_generic_loader_loss=wrong_loss, candidate_checkpoint_sha256=sha(a.candidate / "fit.pt"),
        script_sha256=sha(Path(__file__)))
    write_json(a.out, result)
    print(json.dumps(result, indent=2), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("cache", "control", "candidate", "out"):
        p.add_argument("--"+name, type=Path, required=True)
    run(p.parse_args())
