#!/usr/bin/env python3
"""Run the exact diag_transitions event detector on selected teacher vocal stems."""
import argparse
import json
from pathlib import Path

import numpy as np

import diag_transitions as transitions


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("ids", nargs="+")
    a = p.parse_args()
    for tag in a.ids:
        meta = json.loads((a.cache / (tag + ".json")).read_text(encoding="utf-8"))
        w = np.load(a.cache / (tag + ".npy"), mmap_mode="r")
        if w.shape[:2] != (4, 2):
            raise ValueError(f"{tag}: not a four-stem cache")
        vocal = np.asarray(w[0], dtype=np.float32) / 32768.0
        events = [(kind, round(sample / transitions.SR, 2)) for kind, sample in transitions.events(vocal)]
        print(json.dumps(dict(id=tag, source=meta["source"], events=events), ensure_ascii=False), flush=True)


if __name__ == "__main__":
    main()
