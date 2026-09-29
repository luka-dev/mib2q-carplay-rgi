#!/usr/bin/env python3
"""One input-representation intervention on the frozen four TRAIN windows.

Same trunk, decoder, loss and initialization as separator_fit_probe. Fixed
10k horizon, no early gate stopping, no augmentation or new data. All per-window
results include physical polarity; frozen source contributions are descriptive.
"""
import argparse
import hashlib
import json
import time
from pathlib import Path

import numpy as np
import mask_net as MN
import sep_net as SN
import separator_invariant_features as IF
from separator_fit_probe import objective, transmissions, sha, write_json


def run(a):
    if a.out.exists():
        raise FileExistsError("choose a new output; preserve completed work")
    manifest = json.loads((a.control / "manifest.json").read_text())
    control = json.loads((a.control / "fit_complete.json").read_text())
    if control["step"] != 10000 or manifest["lr"] != 3e-4:
        raise ValueError("frozen 10k control required")
    if sha(a.cache / "split.json") != manifest["split_sha256"]:
        raise ValueError("frozen split changed")
    if sha(Path(SN.__file__)) != manifest["separator_sha256"]:
        raise ValueError("separator code changed")
    arrays = []
    for r in manifest["windows"]:
        raw = MN.expand_slots(MN.read_window(str(a.cache / (r["song"] + ".npy")),
                                           r["start_sample"], r["samples"]))
        if hashlib.sha256(raw.tobytes()).hexdigest() != r["selected_int16_sha256"]:
            raise ValueError("frozen TRAIN window changed")
        arrays.append(raw.astype(np.float32) / 32768)
    torch = SN._torch()
    torch.manual_seed(1)
    device = "cuda" if torch.cuda.is_available() else "cpu"
    w = torch.as_tensor(np.stack(arrays), device=device)
    mix, eligible = w.sum(1), w[:, 6]
    n = mix.shape[-1]
    X, E = SN.stft(torch, mix), SN.stft(torch, eligible)
    sources = dict(eligible=E, vocal=SN.stft(torch, w[:, 0]+w[:, 1]),
                   drums=SN.stft(torch, w[:, 2]+w[:, 3]+w[:, 4]), bass=SN.stft(torch, w[:, 5]))
    with torch.no_grad():
        lm = torch.log(X.abs()+1e-5)
        SN.STATS.update(mean=float(lm.mean()), std=float(lm.std()+1e-3))
    net = IF.make(torch, a.mode).to(device)
    optimizer = torch.optim.AdamW(net.parameters(), lr=3e-4, weight_decay=1e-4)
    a.out.mkdir(parents=True)
    write_json(a.out / "manifest.json", dict(scope="fixed TRAIN representation comparison",
        feature_mode=a.mode, control_sha256=sha(a.control / "fit_complete.json"),
        control_checkpoint_sha256=sha(a.control / "fit.pt"), windows=manifest["windows"],
        split_sha256=manifest["split_sha256"], separator_sha256=sha(Path(SN.__file__)),
        script_sha256=sha(Path(__file__)), feature_sha256=sha(Path(IF.__file__)),
        parameters=sum(p.numel() for p in net.parameters()), estimated_neural_macs_s=net.macs*SN.FPS,
        steps=a.steps, seed=1, lr=3e-4, weight_decay=1e-4, norm="stream",
        objective="unchanged linear magnitude/complex + multiresolution magnitude",
        promotion="10k loss <= .9 control, median SI-SDR >= control+1dB, >=3/4 nonworse songs, no song worse by >1dB; polarity NSE<=1e-8"))
    rows, start, last_grad = [], time.monotonic(), 0.
    for step in range(a.steps+1):
        if step:
            optimizer.zero_grad(set_to_none=True)
            loss = objective(torch, net.separate_stft(X), E, eligible, n)
            if not bool(torch.isfinite(loss)):
                raise ValueError("nonfinite loss")
            loss.backward()
            last_grad = float(torch.nn.utils.clip_grad_norm_(net.parameters(), 5.))
            if not np.isfinite(last_grad):
                raise ValueError("nonfinite gradient")
            optimizer.step()
        if step % 250 == 0 or step == a.steps:
            with torch.no_grad():
                estimate = net.separate_stft(X)
                normal_mask = net.last_mask.clone()
                negative = -net.separate_stft(-X)
                neg_mask = net.last_mask
                waveform = SN.istft(torch, estimate, n)
                inverted = SN.istft(torch, negative, n)
                row = dict(step=step, loss=float(objective(torch, estimate, E, eligible, n)),
                    inverted_loss=float(objective(torch, negative, E, eligible, n)),
                    sdr_db=SN.si_sdr(torch, waveform, eligible).mean(-1).tolist(),
                    inverted_sdr_db=SN.si_sdr(torch, inverted, eligible).mean(-1).tolist(),
                    polarity_mask_nse=float((normal_mask-neg_mask).abs().square().sum()/
                        normal_mask.abs().square().sum().clamp_min(1e-12)),
                    polarity_output_nse=float((estimate-negative).abs().square().sum()/
                        estimate.abs().square().sum().clamp_min(1e-12)),
                    reconstruction_nse=((waveform-eligible).square().sum(dim=(1,2))/
                        eligible.square().sum(dim=(1,2))).tolist(),
                    source_tx=transmissions(torch, normal_mask, sources, n),
                    grad_norm_before_clip=last_grad, seconds=time.monotonic()-start)
            rows.append(row)
            write_json(a.out / "progress.json", dict(rows=rows))
            torch.save(dict(state=net.modules.state_dict(), stats=dict(SN.STATS), step=step,
                            arch="band", norm="stream", mask="complex2", feature_mode=a.mode),
                       a.out / "fit.pt")
            print(f"{a.mode} step {step} loss {row['loss']:.6f} "
                  f"median_sdr {np.median(row['sdr_db']):.2f} polarity_nse {row['polarity_output_nse']:.3g} "
                  f"elapsed {row['seconds']:.0f}s", flush=True)
    write_json(a.out / "complete.json", dict(status="complete", scope="TRAIN only", final=rows[-1]))


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("cache", "control", "out"):
        p.add_argument("--"+name, type=Path, required=True)
    p.add_argument("--mode", choices=("absolute6", "invariant6"), required=True)
    p.add_argument("--steps", type=int, default=10000)
    a = p.parse_args()
    if a.steps < 1:
        p.error("positive steps required")
    run(a)
