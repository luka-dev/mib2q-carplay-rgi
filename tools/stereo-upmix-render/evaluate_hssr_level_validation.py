#!/usr/bin/env python3
"""Compare exported models on source-disjoint fine-tuning validation caches.

Row-dequantized Torch inference is a training diagnostic, not bit-exact C
inference and not an acoustic quality measurement. No heldout audio is used.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import torch
from torch.utils.data import DataLoader
from train_hssr import SequenceDataset, evaluate, import_model


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache-root", type=Path, required=True)
    parser.add_argument("--models", type=Path, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    grouped = {}
    validation_ids = set()
    for path in sorted((args.cache_root / "validation").glob("*.npz")):
        with np.load(path) as cache:
            level = float(cache["input_gain_db"])
            validation_ids.add(str(cache["source_sha256"].item()))
        grouped.setdefault(level, []).append(path)
    if not grouped:
        raise RuntimeError("no validation caches")
    for path in (args.cache_root / "train").glob("*.npz"):
        with np.load(path) as cache:
            if str(cache["source_sha256"].item()) in validation_ids:
                raise RuntimeError("train/validation source leakage")
    device = torch.device("mps" if torch.backends.mps.is_available() else "cpu")
    rows = []
    for path in args.models:
        model, mean, inv = import_model(path)
        model.to(device)
        for level, paths in sorted(grouped.items()):
            dataset = SequenceDataset(paths, mean, inv, routing_target="universal-v5")
            metrics = evaluate(model, DataLoader(dataset, batch_size=32), device, "balanced")
            row = {"model": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                   "input_gain_db": level, "caches": len(paths), "metrics": metrics}
            rows.append(row)
            print(json.dumps(row), flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({"rows": rows, "validation_sources": len(validation_ids),
        "limitations": ["Fine-tune validation sources may have been seen during original V11 training.",
                        "Dequantized Torch diagnostic; C renders remain required."]}, indent=2) + "\n")


if __name__ == "__main__":
    main()
