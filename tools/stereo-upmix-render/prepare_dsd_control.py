#!/usr/bin/env python3
"""Make a 50-song MoisesDB TRAIN-only extra-truth control for a DSD100 data pair.

The extra pool holds the same number of songs and window probability as the
DSD100 candidate. It contains no DEV/TEST song and changes no source cache.
"""

import argparse
import hashlib
import json
import os
import shutil
from pathlib import Path

from mask_net import GROUPS, SR, load_split


def same_contents(left, right):
    if left.stat().st_size != right.stat().st_size:
        return False
    with left.open("rb") as a, right.open("rb") as b:
        while True:
            x, y = a.read(1024 * 1024), b.read(1024 * 1024)
            if x != y:
                return False
            if not x:
                return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--count", type=int, default=50)
    args = parser.parse_args()
    if args.count <= 0:
        parser.error("--count must be positive")
    songs = load_split(str(args.source))["train"]
    if len(songs) < args.count:
        raise ValueError(f"only {len(songs)} present TRAIN songs")
    chosen = sorted(songs, key=lambda name: (hashlib.sha256(name.encode()).digest(), name))[:args.count]
    args.out.mkdir(parents=True, exist_ok=True)
    expected = {"split": {name: "train" for name in chosen}, "groups": list(GROUPS), "sr": SR,
                "source": str(args.source.resolve()), "selection": "lowest SHA-256 of TRAIN id",
                "count": args.count}
    split_path = args.out / "split.json"
    if split_path.exists() and json.loads(split_path.read_text(encoding="utf-8")) != expected:
        raise ValueError("existing control split.json differs")
    for name in chosen:
        for suffix in (".npy", ".json"):
            source, target = args.source / (name + suffix), args.out / (name + suffix)
            if not source.is_file():
                raise FileNotFoundError(source)
            if target.exists():
                if not os.path.samefile(source, target) and not same_contents(source, target):
                    raise ValueError(f"existing control file differs: {target}")
                continue
            try:
                os.link(source, target)
            except OSError:
                shutil.copy2(source, target)
    new = args.out / "split.json.new"
    new.write_text(json.dumps(expected, indent=2) + "\n", encoding="utf-8")
    os.replace(new, split_path)
    print(f"DONE {len(chosen)} MoisesDB TRAIN-only extra-truth controls; no DEV/TEST songs")


if __name__ == "__main__":
    main()
