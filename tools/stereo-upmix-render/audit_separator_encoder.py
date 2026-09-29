#!/usr/bin/env python3
"""CPU activation/block-gradient audit on the fixed TRAIN fitting batch.

Descriptive measurements, not tuned thresholds or a diagnosis from saturation
alone. Compare the fitted baseline with its seed-1 initial state and same stats.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import mask_net as MN
import sep_net as SN
from separator_fit_probe import objective, sha, write_json


def run(a):
    if a.out.exists():
        raise FileExistsError("preserve previous audit")
    manifest = json.loads((a.fit / "manifest.json").read_text())
    if sha(a.cache / "split.json") != manifest["split_sha256"]:
        raise ValueError("TRAIN split changed")
    arrays = []
    for r in manifest["windows"]:
        raw = MN.expand_slots(MN.read_window(str(a.cache / (r["song"] + ".npy")),
                                           r["start_sample"], r["samples"]))
        if hashlib.sha256(raw.tobytes()).hexdigest() != r["selected_int16_sha256"]:
            raise ValueError("TRAIN crop changed")
        arrays.append(raw.astype(np.float32) / 32768)
    torch = SN._torch()
    torch.set_num_threads(4)
    ck = torch.load(a.fit / "fit.pt", map_location="cpu")
    if ck.get("arch") != "band" or ck.get("norm") != "stream" or ck.get("mask") != "complex2":
        raise ValueError("fixed-fit baseline required")
    SN.STATS.update(ck["stats"])
    w = torch.as_tensor(np.stack(arrays), device="cpu")
    target, mix = w[:, 6], w.sum(1)
    X, E = SN.stft(torch, mix), SN.stft(torch, target)
    result = dict(scope="four frozen TRAIN windows; CPU activation/gradient measurements",
                  checkpoint_sha256=sha(a.fit / "fit.pt"), script_sha256=sha(Path(__file__)), arms=[])
    for fitted in (False, True):
        torch.manual_seed(1)
        net = SN.BandSepNet(torch, norm="stream", mask="complex2").to("cpu")
        if fitted:
            net.modules.load_state_dict(ck["state"])
        captured = {}
        handles = [enc.register_forward_hook(
            lambda module, inputs, output, i=i: captured.__setitem__(i, output.detach()))
                   for i, enc in enumerate(net.enc)]
        estimate = net.separate_stft(X)
        loss = objective(torch, estimate, E, target, mix.shape[-1])
        loss.backward()
        for handle in handles:
            handle.remove()
        bands = []
        for i, (lo, hi) in enumerate(net.bands):
            activation = captured[i].tanh()
            bands.append(dict(band=i, bins=[lo, hi],
                tanh_abs_gt_095=float((activation.abs() > .95).float().mean()),
                tanh_abs_gt_099=float((activation.abs() > .99).float().mean()),
                mean_tanh_derivative=float((1 - activation.square()).mean())))
        gradients = {}
        for name in ("enc", "gru1", "gru2", "mix1", "mix2", "ggru", "dec"):
            parameters = list(getattr(net, name).parameters())
            grad = float(sum(p.grad.square().sum() for p in parameters if p.grad is not None).sqrt())
            weight = float(sum(p.detach().square().sum() for p in parameters).sqrt())
            gradients[name] = dict(gradient_l2=grad, parameter_l2=weight,
                                   gradient_per_parameter_norm=grad / max(weight, 1e-12))
        activation = torch.stack([captured[i].tanh() for i in range(len(net.bands))], 2)
        row = dict(fitted=fitted, loss=float(loss.detach()),
                   tanh_abs_gt_095=float((activation.abs() > .95).float().mean()),
                   tanh_abs_gt_099=float((activation.abs() > .99).float().mean()),
                   mean_tanh_derivative=float((1 - activation.square()).mean()),
                   bands=bands, gradients=gradients)
        result["arms"].append(row)
        print(json.dumps({k: v for k, v in row.items() if k != "bands"}), flush=True)
    write_json(a.out, result)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("cache", "fit", "out"):
        p.add_argument("--" + name, type=Path, required=True)
    run(p.parse_args())
