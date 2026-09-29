#!/usr/bin/env python3
"""Privileged decoder-state diagnostic on the finite TRAIN fit manifest.

Optimize the existing decoder's local h and shared g without an encoder.
Two sequential arms: freeze decoder weights, or fit weights as well. No
causal/deployable claim; h/g retain their architecture's amplitude bounds.
"""
import argparse
import json
import time
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN
from separator_fit_probe import objective, sha, write_json


def run(args):
    if args.out.exists():
        raise FileExistsError("new output required")
    fit = json.loads((args.fit / "fit_complete.json").read_text())
    manifest = json.loads((args.fit / "manifest.json").read_text())
    if sha(args.cache / "split.json") != manifest["split_sha256"]:
        raise ValueError("cache split changed")
    arrays = []
    for item in manifest["windows"]:
        raw = MN.expand_slots(MN.read_window(str(args.cache / (item["song"] + ".npy")),
                                           item["start_sample"], item["samples"]))
        import hashlib
        if hashlib.sha256(raw.tobytes()).hexdigest() != item["selected_int16_sha256"]:
            raise ValueError("frozen TRAIN window changed")
        arrays.append(raw.astype(np.float32) / 32768)
    args.out.mkdir(parents=True)
    result = dict(scope="privileged TRAIN decoder-state relaxation; not a student",
        fit_status=fit["status"], fit_checkpoint_sha256=sha(args.fit / "fit.pt"),
        script_sha256=sha(Path(__file__)), steps_per_arm=args.steps, arms=[])
    for learned in (False, True):
        torch, net, device = SN.load(str(args.fit / "fit.pt"))
        if net.mask_mode != "complex2" or net.dec_skip or net.nbrnn:
            raise ValueError("probe requires the frozen baseline decoder topology")
        w = torch.as_tensor(np.stack(arrays), device=device)
        mix, eligible = w.sum(1), w[:, 6]
        n = mix.shape[-1]
        X, E = SN.stft(torch, mix), SN.stft(torch, eligible)
        captured = {}
        handles = [d.register_forward_pre_hook(
            lambda module, inputs, index=i: captured.__setitem__(index, inputs[0].detach().clone()))
            for i, d in enumerate(net.dec)]
        with torch.no_grad():
            baseline = net.separate_stft(X)
        for handle in handles:
            handle.remove()
        g0 = captured[0][..., net.h2:]
        if any(float((z[..., net.h2:] - g0).abs().max()) > 1e-7 for z in captured.values()):
            raise ValueError("global state is not shared across bands")
        h = torch.nn.Parameter(torch.stack([captured[i][..., :net.h2]
                                           for i in range(len(net.bands))], 2))
        g = torch.nn.Parameter(g0.clone())
        for p in net.parameters():
            p.requires_grad_(False)
        groups = [dict(params=[h, g], lr=.03)]
        if learned:
            for p in net.dec.parameters():
                p.requires_grad_(True)
            groups.append(dict(params=list(net.dec.parameters()), lr=.001))
        optimizer = torch.optim.Adam(groups)

        def estimate():
            pieces = [d(torch.cat([h[:, :, i], g], -1)).reshape(X.shape[0], X.shape[1],
                        hi - lo, 4) for i, (d, (lo, hi)) in enumerate(zip(net.dec, net.bands))]
            m = torch.cat(pieces, 2)
            mask = torch.complex(m[..., 0::2], m[..., 1::2])
            return X * mask

        with torch.no_grad():
            parity = float((estimate() - baseline).abs().square().sum() /
                           baseline.abs().square().sum())
            initial = float(objective(torch, baseline, E, eligible, n))
        if parity > 1e-10:
            raise ValueError("decoder-state replay does not reproduce the checkpoint")
        history = []
        clock = time.monotonic()
        for step in range(args.steps + 1):
            if step:
                optimizer.zero_grad(set_to_none=True)
                loss = objective(torch, estimate(), E, eligible, n)
                if not bool(torch.isfinite(loss)):
                    raise ValueError("nonfinite decoder diagnostic loss")
                loss.backward()
                optimizer.step()
                with torch.no_grad():
                    h.clamp_(-3, 3)
                    g.clamp_(-1, 1)
            if step % 250 == 0 or step == args.steps:
                with torch.no_grad():
                    y = estimate()
                    value = float(objective(torch, y, E, eligible, n))
                    row = dict(step=step, loss=value,
                        fraction_of_checkpoint_loss=value / initial,
                        sdr_db=SN.si_sdr(torch, SN.istft(torch, y, n), eligible).mean(-1).tolist(),
                        seconds=time.monotonic() - clock)
                history.append(row)
                print(f"{'free_weights' if learned else 'fixed_weights'} step {step} "
                      f"loss {value:.6f} fraction {value/initial:.4f} "
                      f"median_sdr {np.median(row['sdr_db']):.2f} "
                      f"elapsed {row['seconds']:.0f}s", flush=True)
        result["arms"].append(dict(decoder_weights_learned=learned, replay_nse=parity,
            initial_loss=initial, best_loss=min(r["loss"] for r in history), rows=history))
        write_json(args.out / "decoder_result.json", result)
    write_json(args.out / "complete.json", dict(status="complete", scope=result["scope"]))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cache", type=Path, required=True)
    p.add_argument("--fit", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--steps", type=int, default=2000)
    args = p.parse_args()
    if args.steps < 1:
        p.error("positive steps required")
    run(args)


if __name__ == "__main__":
    main()
