#!/usr/bin/env python3
"""Freeze 20 separate MoisesDB test-split songs for oracle validation."""

import argparse
import json
import random
import re
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--moisesdb", type=Path, required=True)
    p.add_argument("--split-file", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--songs", type=int, default=20)
    p.add_argument("--seed", type=int, default=1610)
    a = p.parse_args()
    split = json.loads(a.split_file.read_text(encoding="utf-8"))["split"]
    candidates = sorted(track_id for track_id, group in split.items() if group == "test")
    if a.songs != 20 or len(candidates) < a.songs:
        p.error(f"expected 20 requested songs from at least 20 test tracks; found {len(candidates)}")
    chosen = sorted(random.Random(a.seed).sample(candidates, a.songs))
    song_ids = {}
    for track_id in chosen:
        meta = json.loads((a.moisesdb / track_id / "data.json").read_text(encoding="utf-8"))
        title = re.sub(r'[\\/:*?"<>|]', "_", f"{meta.get('artist', '?')} - {meta.get('song', '?')}")[:60]
        if title in song_ids:
            raise ValueError(f"duplicate title after renderer truncation: {title}")
        song_ids[title] = track_id
    a.out.mkdir(parents=True, exist_ok=True)
    manifest = dict(scope="20 fixed songs from separate MoisesDB test split; no original oracle render",
                    seed=a.seed, available_test_songs=len(candidates), song_track_ids=song_ids)
    (a.out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Frozen {len(song_ids)} songs from {len(candidates)} test-split tracks")


if __name__ == "__main__":
    main()
