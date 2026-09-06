#!/usr/bin/env python3
"""Discover and download a licensed Archive.org surround-training pool.

Archive search metadata is noisy: the string ``5.1`` often means an episode
number, and file names do not prove the channel count.  This tool therefore:

1. searches audio-item metadata;
2. rejects licenses that forbid derivatives (and unknown licenses);
3. inspects each item's file metadata;
4. remotely ffprobes candidate audio files;
5. keeps only decodable six-channel programmes; and
6. downloads a bounded, resumable pool with checksums and provenance.

The resulting material is for a DEVELOPMENT checkpoint.  Archive.org license
metadata is uploader supplied and is not a substitute for legal provenance
review before weights are shipped in a commercial product.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import subprocess
import threading
import time
from pathlib import Path
from urllib.parse import quote

import requests


SEARCH_URL = "https://archive.org/advancedsearch.php"
METADATA_URL = "https://archive.org/metadata/{identifier}"
DOWNLOAD_URL = "https://archive.org/download/{identifier}/{name}"
USER_AGENT = "mhi2-hssr-surround-research/1.0"
SEARCH_QUERY = (
    "mediatype:audio AND ("
    "title:(surround) OR subject:(surround) OR description:(surround) OR "
    'title:("5.1") OR subject:("5.1") OR description:("5.1") OR '
    "title:(multichannel) OR subject:(multichannel) OR "
    "title:(DTS) OR subject:(DTS))"
)
AUDIO_SUFFIXES = {
    ".flac", ".wav", ".wave", ".w64", ".aif", ".aiff",
    ".dts", ".ac3", ".eac3", ".m4a", ".mka", ".mkv", ".mp4",
    ".ogg", ".opus",
}
SURROUND_HINT = re.compile(
    r"(5[._ -]?1|6ch|six.?channel|surround|dts|ac-?3|eac3)", re.IGNORECASE
)


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=root / "build/reference-audio/archive-surround-training-pool",
    )
    parser.add_argument("--search-pages", type=int, default=12)
    parser.add_argument("--rows-per-page", type=int, default=100)
    parser.add_argument("--probe-workers", type=int, default=8)
    parser.add_argument("--max-probe-files", type=int, default=600)
    parser.add_argument("--max-files", type=int, default=80)
    parser.add_argument("--max-files-per-item", type=int, default=4)
    parser.add_argument("--max-download-gib", type=float, default=8.0)
    parser.add_argument("--max-file-gib", type=float, default=2.5)
    parser.add_argument(
        "--include-noncommercial",
        action="store_true",
        help="include CC BY-NC/CC BY-NC-SA for local research only",
    )
    parser.add_argument(
        "--discover-only",
        action="store_true",
        help="write the verified remote-file catalog without downloading",
    )
    parser.add_argument(
        "--reuse-catalog",
        action="store_true",
        help="skip search/probing and download from OUTPUT/manifest.json",
    )
    parser.add_argument(
        "--allow-identifier",
        action="append",
        default=[],
        help=(
            "download only this reviewed Archive identifier; repeat for more. "
            "Discovery still records every technically valid candidate"
        ),
    )
    parser.add_argument(
        "--archive-identifier",
        action="append",
        default=[],
        help="probe only this known Archive identifier instead of searching",
    )
    parser.add_argument(
        "--exclude-manifest",
        type=Path,
        action="append",
        default=[
            root / "build/reference-audio/archive-surround-sources/manifest.json"
        ],
        help="manifest whose Archive identifiers stay evaluation-only",
    )
    return parser.parse_args()


def request_json(url: str, *, params: object | None = None) -> dict:
    response = requests.get(
        url,
        params=params,
        headers={"User-Agent": USER_AGENT},
        timeout=(10, 45),
    )
    response.raise_for_status()
    return response.json()


def license_class(url: object) -> str | None:
    value = str(url or "").lower().rstrip("/")
    if "/by-nd/" in value or "/by-nc-nd/" in value:
        return None
    if "creativecommons.org/publicdomain/zero" in value:
        return "cc0"
    if "creativecommons.org/licenses/by-nc-sa" in value:
        return "cc-by-nc-sa"
    if "creativecommons.org/licenses/by-nc" in value:
        return "cc-by-nc"
    if "creativecommons.org/licenses/by-sa" in value:
        return "cc-by-sa"
    if "creativecommons.org/licenses/by" in value:
        return "cc-by"
    return None


def license_allowed(kind: str | None, include_noncommercial: bool) -> bool:
    if kind in {"cc0", "cc-by", "cc-by-sa"}:
        return True
    return include_noncommercial and kind in {"cc-by-nc", "cc-by-nc-sa"}


def search_items(args: argparse.Namespace) -> list[dict]:
    by_identifier: dict[str, dict] = {}
    for page in range(1, args.search_pages + 1):
        data = request_json(
            SEARCH_URL,
            params=[
                ("q", SEARCH_QUERY),
                ("fl[]", "identifier"),
                ("fl[]", "title"),
                ("fl[]", "creator"),
                ("fl[]", "licenseurl"),
                ("fl[]", "downloads"),
                ("fl[]", "subject"),
                ("rows", str(args.rows_per_page)),
                ("page", str(page)),
                ("output", "json"),
                ("sort[]", "downloads desc"),
            ],
        )
        documents = data["response"]["docs"]
        for document in documents:
            kind = license_class(document.get("licenseurl"))
            if license_allowed(kind, args.include_noncommercial):
                document["license_class"] = kind
                by_identifier[document["identifier"]] = document
        print(
            f"search page={page} documents={len(documents)} "
            f"licensed_total={len(by_identifier)}",
            flush=True,
        )
        if len(documents) < args.rows_per_page:
            break
    return list(by_identifier.values())


def explicit_items(args: argparse.Namespace) -> list[dict]:
    documents = []
    for identifier in args.archive_identifier:
        data = request_json(
            METADATA_URL.format(identifier=quote(identifier, safe=""))
        )
        metadata = data.get("metadata", {})
        kind = license_class(metadata.get("licenseurl"))
        if not license_allowed(kind, args.include_noncommercial):
            print(f"license rejected {identifier}: {metadata.get('licenseurl')}")
            continue
        documents.append(
            {
                "identifier": identifier,
                "title": metadata.get("title"),
                "creator": metadata.get("creator"),
                "licenseurl": metadata.get("licenseurl"),
                "downloads": int(metadata.get("downloads") or 0),
                "subject": metadata.get("subject"),
                "license_class": kind,
            }
        )
    return documents


def excluded_identifiers(paths: list[Path]) -> set[str]:
    excluded: set[str] = set()
    for path in paths:
        if not path.is_file():
            continue
        data = json.loads(path.read_text(encoding="utf-8"))
        for item in data.get("items", []):
            source_url = str(item.get("source_url", ""))
            marker = "archive.org/details/"
            if marker in source_url:
                excluded.add(source_url.split(marker, 1)[1].split("?", 1)[0])
            if item.get("archive_identifier"):
                excluded.add(str(item["archive_identifier"]))
    return excluded


def candidate_files(document: dict) -> list[dict]:
    identifier = document["identifier"]
    metadata = request_json(
        METADATA_URL.format(identifier=quote(identifier, safe=""))
    )
    item_metadata = metadata.get("metadata", {})
    item_text = " ".join(
        str(item_metadata.get(field, ""))
        for field in ("title", "description", "subject")
    )
    candidates = []
    for entry in metadata.get("files", []):
        name = str(entry.get("name", ""))
        if Path(name).suffix.lower() not in AUDIO_SUFFIXES:
            continue
        if str(entry.get("private", "false")).lower() == "true":
            continue
        size = int(entry.get("size") or 0)
        if size < 1_000_000:
            continue
        source = str(entry.get("source", ""))
        format_name = str(entry.get("format", ""))
        hint = bool(
            SURROUND_HINT.search(name)
            or SURROUND_HINT.search(format_name)
            or SURROUND_HINT.search(item_text)
        )
        if not hint:
            continue
        candidates.append(
            {
                "archive_identifier": identifier,
                "item_title": document.get("title"),
                "creator": document.get("creator"),
                "subject": document.get("subject"),
                "downloads": int(document.get("downloads") or 0),
                "license_url": document.get("licenseurl"),
                "license_class": document["license_class"],
                "file_name": name,
                "file_format": format_name,
                "file_source": source,
                "bytes": size,
                "md5": entry.get("md5"),
                "sha1": entry.get("sha1"),
                "remote_url": DOWNLOAD_URL.format(
                    identifier=quote(identifier, safe=""),
                    name=quote(name, safe="/"),
                ),
                "metadata_surround_hint": True,
            }
        )
    # Original files and explicit filename hints are probed first.
    candidates.sort(
        key=lambda item: (
            item["file_source"] != "original",
            not bool(SURROUND_HINT.search(item["file_name"])),
            item["bytes"],
        )
    )
    return candidates


def probe_file(candidate: dict) -> dict | None:
    command = [
        "ffprobe", "-v", "error", "-rw_timeout", "20000000",
        "-select_streams", "a:0",
        "-show_entries",
        "stream=codec_name,channels,channel_layout,sample_rate,duration:"
        "format=format_name,duration",
        "-of", "json", candidate["remote_url"],
    ]
    try:
        completed = subprocess.run(
            command, check=True, capture_output=True, timeout=40
        )
        data = json.loads(completed.stdout)
        stream = data.get("streams", [])[0]
        if int(stream.get("channels") or 0) != 6:
            return None
        duration = stream.get("duration") or data.get("format", {}).get("duration")
        duration_seconds = float(duration) if duration is not None else None
        if duration_seconds is not None and duration_seconds < 20.0:
            return None
        result = dict(candidate)
        result.update(
            {
                "codec": stream.get("codec_name"),
                "channels": 6,
                "channel_layout": stream.get("channel_layout"),
                "sample_rate": int(stream.get("sample_rate") or 0),
                "duration_seconds": duration_seconds,
                "container": data.get("format", {}).get("format_name"),
            }
        )
        return result
    except (subprocess.SubprocessError, ValueError, KeyError, json.JSONDecodeError):
        return None


def safe_file_name(candidate: dict) -> str:
    digest = hashlib.sha1(candidate["file_name"].encode("utf-8")).hexdigest()[:10]
    base = re.sub(r"[^A-Za-z0-9._-]+", "_", Path(candidate["file_name"]).name)
    return f"{digest}-{base[-160:]}"


def download_file(output: Path, candidate: dict) -> dict:
    item_root = output / "files" / candidate["archive_identifier"]
    item_root.mkdir(parents=True, exist_ok=True)
    destination = item_root / safe_file_name(candidate)
    expected = int(candidate["bytes"])
    if destination.is_file() and destination.stat().st_size == expected:
        result = dict(candidate)
        result["local_path"] = str(destination.resolve())
        result["download_status"] = "reused"
        return result

    temporary = destination.with_suffix(destination.suffix + ".part")
    md5 = hashlib.md5()
    with requests.get(
        candidate["remote_url"],
        headers={"User-Agent": USER_AGENT},
        stream=True,
        timeout=(15, 120),
    ) as response:
        response.raise_for_status()
        with temporary.open("wb") as target:
            for block in response.iter_content(chunk_size=1024 * 1024):
                if block:
                    target.write(block)
                    md5.update(block)
    if temporary.stat().st_size != expected:
        raise RuntimeError(
            f"short download {candidate['remote_url']}: "
            f"{temporary.stat().st_size} != {expected}"
        )
    expected_md5 = candidate.get("md5")
    if expected_md5 and md5.hexdigest().lower() != str(expected_md5).lower():
        raise RuntimeError(f"MD5 mismatch for {candidate['remote_url']}")
    os.replace(temporary, destination)
    result = dict(candidate)
    result["local_path"] = str(destination.resolve())
    result["download_status"] = "downloaded"
    return result


def write_manifest(output: Path, args: argparse.Namespace,
                   verified: list[dict], downloaded: list[dict]) -> None:
    manifest = {
        "schema": 1,
        "kind": "archive-org-six-channel-training-pool",
        "generated_unix_seconds": time.time(),
        "development_only": True,
        "rights_warning": (
            "Archive.org license metadata is uploader supplied. Review every "
            "item's provenance before using resulting weights commercially."
        ),
        "query": SEARCH_QUERY,
        "include_noncommercial": args.include_noncommercial,
        "download_identifier_allowlist": args.allow_identifier,
        "selection": (
            "ffprobe-verified exactly six-channel, >=20 seconds, known CC "
            "license, no NoDerivatives license"
        ),
        "download_budget_bytes": int(args.max_download_gib * 1024**3),
        "verified_files": verified,
        "downloaded_files": downloaded,
        "verified_file_count": len(verified),
        "downloaded_file_count": len(downloaded),
        "downloaded_bytes": sum(int(item["bytes"]) for item in downloaded),
    }
    output.mkdir(parents=True, exist_ok=True)
    temporary = output / "manifest.json.tmp"
    temporary.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    os.replace(temporary, output / "manifest.json")


def download_verified(args: argparse.Namespace, verified: list[dict]) -> int:
    catalog_verified = verified
    verified = [
        item for item in verified
        if license_allowed(
            license_class(item.get("license_url")),
            args.include_noncommercial,
        )
    ]
    allowlist = set(args.allow_identifier)
    if allowlist:
        verified = [
            item for item in verified
            if item["archive_identifier"] in allowlist
        ]
    budget = int(args.max_download_gib * 1024**3)
    selected: list[dict] = []
    item_counts: dict[str, int] = {}
    used = 0
    # First pass maximizes item diversity; second pass fills remaining budget.
    for maximum_per_item in (1, max(1, args.max_files_per_item)):
        for item in verified:
            identifier = item["archive_identifier"]
            if item in selected or item_counts.get(identifier, 0) >= maximum_per_item:
                continue
            if len(selected) >= args.max_files or used + item["bytes"] > budget:
                continue
            selected.append(item)
            item_counts[identifier] = item_counts.get(identifier, 0) + 1
            used += int(item["bytes"])

    downloaded: list[dict] = []
    manifest_path = args.output / "manifest.json"
    if manifest_path.is_file():
        previous = json.loads(manifest_path.read_text(encoding="utf-8"))
        downloaded = list(previous.get("downloaded_files", []))
    downloaded_keys = {
        (item["archive_identifier"], item["file_name"]) for item in downloaded
    }
    for index, item in enumerate(selected, 1):
        key = (item["archive_identifier"], item["file_name"])
        if key in downloaded_keys:
            continue
        print(
            f"download {index}/{len(selected)} {item['archive_identifier']}/"
            f"{item['file_name']}",
            flush=True,
        )
        try:
            downloaded.append(download_file(args.output, item))
            downloaded_keys.add(key)
            write_manifest(args.output, args, catalog_verified, downloaded)
        except (requests.RequestException, OSError, RuntimeError) as error:
            print(f"download skip: {error}", flush=True)

    print(
        f"complete files={len(downloaded)} "
        f"bytes={sum(item['bytes'] for item in downloaded) / 1024**3:.2f} GiB",
        flush=True,
    )
    return 0


def main() -> int:
    args = parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if args.reuse_catalog:
        manifest_path = args.output / "manifest.json"
        if not manifest_path.is_file():
            raise SystemExit(f"catalog does not exist: {manifest_path}")
        catalog = json.loads(manifest_path.read_text(encoding="utf-8"))
        verified = list(catalog.get("verified_files", []))
        if not verified:
            raise SystemExit("catalog contains no verified_files")
        return download_verified(args, verified)

    excluded = excluded_identifiers(args.exclude_manifest)
    discovered = explicit_items(args) if args.archive_identifier else search_items(args)
    documents = [
        item for item in discovered if item["identifier"] not in excluded
    ]
    print(
        f"metadata candidates={len(documents)} excluded_eval_items={len(excluded)}",
        flush=True,
    )

    remote_candidates: list[dict] = []
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=max(1, args.probe_workers)
    ) as pool:
        futures = {pool.submit(candidate_files, item): item for item in documents}
        for index, future in enumerate(concurrent.futures.as_completed(futures), 1):
            try:
                remote_candidates.extend(future.result())
            except (requests.RequestException, ValueError, KeyError) as error:
                print(
                    f"metadata skip {futures[future]['identifier']}: {error}",
                    flush=True,
                )
            if index % 20 == 0:
                print(
                    f"metadata {index}/{len(documents)} "
                    f"audio_candidates={len(remote_candidates)}",
                    flush=True,
                )

    remote_candidates.sort(
        key=lambda item: (
            item["file_source"] != "original",
            not bool(SURROUND_HINT.search(item["file_name"])),
            -item["downloads"],
            item["bytes"],
        )
    )
    max_file_bytes = int(args.max_file_gib * 1024**3)
    remote_candidates = [
        item for item in remote_candidates if item["bytes"] <= max_file_bytes
    ][:args.max_probe_files]
    print(f"probing remote files={len(remote_candidates)}", flush=True)

    verified: list[dict] = []
    probe_lock = threading.Lock()
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=max(1, args.probe_workers)
    ) as pool:
        futures = [pool.submit(probe_file, item) for item in remote_candidates]
        for index, future in enumerate(concurrent.futures.as_completed(futures), 1):
            result = future.result()
            if result is not None:
                with probe_lock:
                    verified.append(result)
                    print(
                        f"verified 6ch {result['archive_identifier']}/"
                        f"{result['file_name']} ({result['bytes'] / 1024**2:.1f} MiB)",
                        flush=True,
                    )
            if index % 25 == 0:
                print(
                    f"probe {index}/{len(remote_candidates)} verified={len(verified)}",
                    flush=True,
                )

    # Prefer original files, diverse items, explicit surround names, and then
    # smaller programmes so the byte budget buys more musical examples.
    verified.sort(
        key=lambda item: (
            item["file_source"] != "original",
            not bool(SURROUND_HINT.search(item["file_name"])),
            item["bytes"],
        )
    )
    write_manifest(args.output, args, verified, [])
    if args.discover_only:
        print(f"discovery complete verified={len(verified)}", flush=True)
        return 0
    return download_verified(args, verified)


if __name__ == "__main__":
    raise SystemExit(main())
