#!/usr/bin/env python3
"""Inspect DSD100 archive structure and possible song overlap without extracting audio.

This is an inventory, not an independence certificate: spelling, aliases,
covers, and shared recordings still need a manual metadata/audio audit.
Run --crc only after the full archive is downloaded and GPU jobs are idle.
"""

import argparse
import difflib
import json
import re
import zipfile
from pathlib import Path


ROLES = frozenset(("vocals", "drums", "bass", "other"))
OFFICIAL_BYTES = 15989815305
SOURCE = re.compile(r"(?:^|/)sources/(dev|train|test)/([^/]+)/([^/]+)\.wav$", re.I)
MIXTURE = re.compile(r"(?:^|/)mixtures/(dev|train|test)/([^/]+)/mixture\.wav$", re.I)


def normalized(value):
    return " ".join(re.findall(r"[\w]+", value.casefold().replace("_", " ")))


def variants(value):
    result = {normalized(value)}
    if " - " in value:
        left, right = value.split(" - ", 1)
        result.add(normalized(right + " " + left))
    return result - {""}


def known_songs(moises_root, teacher_cache):
    known = []
    if moises_root:
        for path in moises_root.glob("*/data.json"):
            data = json.loads(path.read_text(encoding="utf-8-sig"))
            artist, song = data.get("artist", ""), data.get("song", "")
            if artist and song:
                known.append(("moisesdb", path.parent.name, f"{artist} - {song}"))
    if teacher_cache:
        for path in teacher_cache.glob("*.json"):
            data = json.loads(path.read_text(encoding="utf-8-sig"))
            source = data.get("source", "")
            if source:
                known.append(("teacher_cache4", path.stem, Path(source.replace("\\", "/")).stem))
    return [(corpus, key, name, variants(name)) for corpus, key, name in known]


def matches(name, known, cutoff):
    query = variants(name)
    found = []
    for corpus, key, other, forms in known:
        score = max(difflib.SequenceMatcher(None, a, b).ratio()
                    for a in query for b in forms)
        if score >= cutoff:
            found.append(dict(corpus=corpus, key=key, title=other,
                              similarity=round(score, 3)))
    return sorted(found, key=lambda item: (-item["similarity"], item["corpus"], item["key"]))[:5]


def inspect(archive, known, cutoff, crc):
    tracks = {}
    with zipfile.ZipFile(archive) as zf:
        for item in zf.infolist():
            if item.is_dir():
                continue
            path = item.filename.replace("\\", "/")
            source = SOURCE.search(path)
            mixture = MIXTURE.search(path)
            if source:
                split, title, role = source.groups()
                key = (split.lower(), title)
                files = tracks.setdefault(key, dict(sources={}, mixture=None))
                role = role.lower()
                if role in files["sources"]:
                    raise ValueError(f"duplicate source {key} {role}")
                files["sources"][role] = path
            elif mixture:
                split, title = mixture.groups()
                key = (split.lower(), title)
                files = tracks.setdefault(key, dict(sources={}, mixture=None))
                if files["mixture"]:
                    raise ValueError(f"duplicate mixture {key}")
                files["mixture"] = path
        if crc:
            bad = zf.testzip()
            if bad:
                raise ValueError(f"ZIP CRC failed for {bad}")

    if not tracks:
        raise ValueError("no DSD100 source or mixture tracks recognized in archive")
    details = []
    for (split, title), files in sorted(tracks.items()):
        roles = set(files["sources"])
        details.append(dict(split=split, title=title,
                            roles=sorted(roles), missing_roles=sorted(ROLES - roles),
                            mixture=bool(files["mixture"]),
                            possible_overlap=matches(re.sub(r"^\d{3}\s*-\s*", "", title),
                                                     known, cutoff)))
    counts = {split: sum(item["split"] == split for item in details)
              for split in sorted({item["split"] for item in details})}
    return dict(archive=str(archive), bytes=archive.stat().st_size,
                zip_crc="passed" if crc else "not_run", tracks=counts,
                incomplete=[item["title"] for item in details
                            if item["missing_roles"] or not item["mixture"]],
                possible_overlap_count=sum(bool(item["possible_overlap"]) for item in details),
                songs=details)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--moises-root", type=Path)
    parser.add_argument("--teacher-cache", type=Path)
    parser.add_argument("--similarity", type=float, default=0.84)
    parser.add_argument("--expected-bytes", type=int, default=OFFICIAL_BYTES)
    parser.add_argument("--crc", action="store_true", help="decompress every ZIP member for CRC validation")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if not 0 < args.similarity <= 1:
        parser.error("--similarity must be in (0, 1]")
    if args.archive.stat().st_size != args.expected_bytes:
        parser.error("archive length differs from --expected-bytes")
    report = inspect(args.archive, known_songs(args.moises_root, args.teacher_cache),
                     args.similarity, args.crc)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"tracks {report['tracks']}; incomplete {len(report['incomplete'])}; "
          f"possible overlaps {report['possible_overlap_count']}; CRC {report['zip_crc']}")


if __name__ == "__main__":
    main()
