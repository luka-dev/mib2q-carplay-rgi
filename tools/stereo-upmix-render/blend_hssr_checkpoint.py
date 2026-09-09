#!/usr/bin/env python3
"""Conservative warm-start weight interpolation; never averages output audio.

Only use a checkpoint fine-tuned from this base with identical normalization
and neuron ordering. One exported model has unchanged target runtime cost.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import torch
from train_hssr import export_model, import_model


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-model", type=Path, required=True)
    parser.add_argument("--candidate-checkpoint", type=Path, required=True)
    parser.add_argument("--weight", type=float, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 0 < args.weight <= 1:
        raise RuntimeError("weight must be in (0, 1]")
    model, mean, inv = import_model(args.base_model)
    checkpoint = torch.load(args.candidate_checkpoint, map_location="cpu", weights_only=False)
    if not np.array_equal(mean, checkpoint["mean"]) or not np.array_equal(inv, checkpoint["inv_std"]):
        raise RuntimeError("normalization differs; interpolation not valid")
    base_state = model.state_dict()
    if set(base_state) != set(checkpoint["state_dict"]):
        raise RuntimeError("network layouts differ")
    for name, value in base_state.items():
        fine = checkpoint["state_dict"][name]
        if fine.shape != value.shape or not torch.isfinite(fine).all():
            raise RuntimeError(f"invalid candidate parameter: {name}")
        base_state[name] = value + args.weight * (fine - value)
    model.load_state_dict(base_state)
    args.output.mkdir(parents=True, exist_ok=False)
    torch.save({"state_dict": model.state_dict(), "mean": mean, "inv_std": inv},
               args.output / "hssr_development.pt")
    export = export_model(model, mean, inv, args.output / "hssr_model.bin")
    manifest = {"development_only": True, "kind": "warm-start-weight-interpolation",
        "base_model": str(args.base_model.resolve()),
        "base_sha256": hashlib.sha256(args.base_model.read_bytes()).hexdigest(),
        "fine_tuned_checkpoint": str(args.candidate_checkpoint.resolve()),
        "checkpoint_sha256": hashlib.sha256(args.candidate_checkpoint.read_bytes()).hexdigest(),
        "fine_tuned_weight": args.weight, "export": export,
        "model_sha256": hashlib.sha256((args.output / "hssr_model.bin").read_bytes()).hexdigest(),
        "normalization": "identical to base", "vehicle_approved": False,
        "note": "Single network; reduces fine-tuning drift, not acoustic calibration or automatic promotion."}
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
