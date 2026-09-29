#!/usr/bin/env python3
"""CPU-only global-polarity equivariance check on frozen TRAIN windows.

Negating mixture and target must not change a diagonal source ratio. This is
a representation stress test, not a generalization or product promotion gate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path

os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
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
    for item in manifest["windows"]:
        raw = MN.expand_slots(MN.read_window(str(a.cache / (item["song"] + ".npy")),
                                           item["start_sample"], item["samples"]))
        if hashlib.sha256(raw.tobytes()).hexdigest() != item["selected_int16_sha256"]:
            raise ValueError("TRAIN crop changed")
        arrays.append(raw.astype(np.float32) / 32768)
    result = dict(scope="four frozen TRAIN windows; physical global polarity flip", models=[])
    for checkpoint in a.checkpoint:
        # Explicit mapping avoids availability/initialization disagreements on
        # Windows when CUDA is hidden. These checkpoints are owned by this run.
        torch = SN._torch()
        ck = torch.load(str(checkpoint), map_location="cpu")
        if ck.get("arch") != "band":
            raise ValueError("BandSepNet checkpoint required")
        net = SN.BandSepNet(torch, feat=ck.get("feat", 48), h1=ck.get("h1", 32),
            h2=ck.get("h2", 32), hg=ck.get("bhg", 32), brnn=ck.get("brnn", 0),
            lookahead=ck.get("lookahead", 0), bidir=ck.get("bidir", False),
            dec_skip=ck.get("dec_skip", False), norm=ck.get("norm", "window"),
            mask=ck.get("mask", "complex2")).to("cpu")
        net.modules.load_state_dict(ck["state"])
        net.modules.eval()
        SN.STATS.update(ck["stats"])
        device = "cpu"
        torch.set_num_threads(4)
        w = torch.as_tensor(np.stack(arrays), device=device)
        mix, target = w.sum(1), w[:, 6]
        n = mix.shape[-1]
        with torch.no_grad():
            X, E = SN.stft(torch, mix), SN.stft(torch, target)
            positive = net.separate_stft(X)
            negative = -net.separate_stft(-X)
            row = dict(checkpoint=str(checkpoint), checkpoint_sha256=sha(checkpoint),
                norm=net.norm, positive_loss=float(objective(torch, positive, E, target, n)),
                inverted_loss=float(objective(torch, negative, E, target, n)),
                positive_sdr_db=SN.si_sdr(torch, SN.istft(torch, positive, n), target).mean(-1).tolist(),
                inverted_sdr_db=SN.si_sdr(torch, SN.istft(torch, negative, n), target).mean(-1).tolist(),
                polarity_output_difference_nse=((negative-positive).abs().square().sum(dim=(1,2,3)) /
                    positive.abs().square().sum(dim=(1,2,3)).clamp_min(1e-12)).tolist())
        result["models"].append(row)
        print(json.dumps(row), flush=True)
    result["script_sha256"] = sha(Path(__file__))
    write_json(a.out, result)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cache", type=Path, required=True)
    p.add_argument("--fit", type=Path, required=True)
    p.add_argument("--checkpoint", type=Path, action="append", required=True)
    p.add_argument("--out", type=Path, required=True)
    run(p.parse_args())
