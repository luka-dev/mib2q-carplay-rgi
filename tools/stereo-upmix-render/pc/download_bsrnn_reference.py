#!/usr/bin/env python3
"""Fetch pinned upstream BSRNN code and two public checkpoint bundles.

Standard library only. No package installation, torch import, inference or GPU
lease. Files are checked before extraction; checkpoints are never unpickled.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import time
import urllib.error
import urllib.request
import zipfile


REPO = "https://github.com/magronp/bsrnn.git"
REV = "94adc8c184ac9b1d2e496fcb6e78cfe3fb78fc9c"
RECORD = "https://zenodo.org/records/17516442"
BUNDLES = (
    ("simo-bsrnn-opt", 1213413202, "817e8ce4bcd17c38bd3812167e97db58"),
    ("bsrnn-large", 1627580581, "0d1e298e725bf45577a4f61e0a30c2b1"),
)


def say(message):
    print(time.strftime("%Y-%m-%d %H:%M:%S"), message, flush=True)


def hashes(path):
    md5, sha = hashlib.md5(), hashlib.sha256()
    with path.open("rb") as f:
        while block := f.read(8 * 1024 * 1024):
            md5.update(block)
            sha.update(block)
    return {"bytes": path.stat().st_size, "md5": md5.hexdigest(),
            "sha256": sha.hexdigest()}


def write_json(path, value):
    partial = path.with_suffix(path.suffix + ".new")
    partial.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    os.replace(partial, path)


def git(*args):
    return subprocess.check_output(["git", *args], text=True).strip()


def lock_download(root):
    handle = (root / "DOWNLOAD.lock").open("a+b")
    if handle.seek(0, os.SEEK_END) == 0:
        handle.write(b"0")
        handle.flush()
    handle.seek(0)
    try:
        if os.name == "nt":
            import msvcrt
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        handle.close()
        raise RuntimeError("Another downloader owns this root") from None
    return handle  # Caller retains the handle until completion; OS releases on exit.


def fetch(url, target, expected_size, expected_md5):
    partial = target.with_suffix(target.suffix + ".part")
    if not target.exists():
        parts = target.with_suffix(target.suffix + ".parts")
        width = 32 * 1024 * 1024
        count = (expected_size + width - 1) // width
        if not parts.exists():
            parts.mkdir()
            # Preserve and reuse bytes already fetched by the serial downloader.
            if partial.exists():
                if partial.stat().st_size > expected_size:
                    raise RuntimeError("Oversized previous partial; preserve it")
                with partial.open("rb") as src:
                    for index in range(count):
                        remaining = min(width, expected_size - index * width)
                        first = src.read(min(8 * 1024 * 1024, remaining))
                        if not first:
                            break
                        with (parts / f"{index:04d}.part").open("wb") as dst:
                            dst.write(first)
                            remaining -= len(first)
                            while remaining:
                                block = src.read(min(8 * 1024 * 1024, remaining))
                                if not block:
                                    break
                                dst.write(block)
                                remaining -= len(block)
        def segment(index):
            path = parts / f"{index:04d}.part"
            lo = index * width
            hi = min(expected_size, lo + width) - 1
            for attempt in range(4):
                offset = path.stat().st_size if path.exists() else 0
                if offset == hi - lo + 1:
                    return
                if offset > hi - lo + 1:
                    raise RuntimeError("Oversized segment; preserve it")
                try:
                    headers = {"User-Agent": "Audi-Upmix-Research/1.0",
                               "Range": f"bytes={lo + offset}-{hi}"}
                    request = urllib.request.Request(url, headers=headers)
                    with urllib.request.urlopen(request, timeout=60) as response:
                        expected_range = f"bytes {lo + offset}-{hi}/{expected_size}"
                        if response.status != 206 or response.headers.get("Content-Range") != expected_range:
                            raise RuntimeError("Server did not honor exact requested range")
                        with path.open("ab") as f:
                            while block := response.read(1024 * 1024):
                                f.write(block)
                    if path.stat().st_size == hi - lo + 1:
                        return
                except (OSError, urllib.error.URLError) as exc:
                    say(f"Segment {index} retry {attempt + 1}/4: {type(exc).__name__}")
                    if attempt == 3:
                        raise
                    time.sleep(3)
            raise RuntimeError("Incomplete range segment")
        start = time.monotonic()
        initial = sum(p.stat().st_size for p in parts.glob("*.part"))
        with ThreadPoolExecutor(max_workers=8) as executor:
            futures = [executor.submit(segment, i) for i in range(count)]
            while not all(f.done() for f in futures):
                for f in futures:
                    if f.done() and f.exception():
                        raise f.exception()
                time.sleep(10)
                downloaded = sum(p.stat().st_size for p in parts.glob("*.part"))
                speed = (downloaded - initial) / max(time.monotonic() - start, .001) / 1e6
                say(f"{target.name}: {downloaded}/{expected_size} bytes "
                    f"({100 * downloaded / expected_size:.1f}%), {speed:.2f} MB/s, 8 ranges")
            for f in futures:
                f.result()
        with partial.open("wb") as dst:
            for i in range(count):
                with (parts / f"{i:04d}.part").open("rb") as src:
                    shutil.copyfileobj(src, dst, 8 * 1024 * 1024)
        check = hashes(partial)
        if check["bytes"] != expected_size or check["md5"] != expected_md5:
            raise RuntimeError("Published size/checksum mismatch; partial preserved")
        os.replace(partial, target)
        shutil.rmtree(parts)
    check = hashes(target)
    if check["bytes"] != expected_size or check["md5"] != expected_md5:
        raise RuntimeError("Existing archive does not match published checksum")
    say(f"Verified {target.name}: published MD5 and {check['bytes']} bytes")
    return check


def extract(archive, destination):
    marker = destination / "EXTRACTION_COMPLETE.json"
    if marker.exists():
        saved = json.loads(marker.read_text(encoding="utf-8"))
        for row in saved["checkpoints"]:
            if hashes(destination / row["path"]) != row["hashes"]:
                raise RuntimeError("Extracted checkpoint changed: " + row["path"])
        return saved
    if destination.exists():
        raise RuntimeError("Unmarked destination exists; preserve it: " + str(destination))
    staging = destination.with_name(destination.name + ".extracting")
    if staging.exists():
        raise RuntimeError("Previous extraction staging exists; preserve it: " + str(staging))
    with zipfile.ZipFile(archive) as z:
        entries = z.infolist()
        for entry in entries:
            name = PurePosixPath(entry.filename)
            if name.is_absolute() or ".." in name.parts or "\\" in entry.filename or ":" in entry.filename:
                raise RuntimeError("Unsafe archive member")
            if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise RuntimeError("Symlink archive member")
        bad = z.testzip()
        if bad:
            raise RuntimeError("ZIP CRC failure: " + bad)
        say(f"Full ZIP CRC passed: {archive.name}, {len(entries)} members")
        staging.mkdir(parents=True)
        z.extractall(staging)
    checkpoints = []
    for path in sorted(staging.rglob("*.ckpt")):
        checkpoints.append({"path": path.relative_to(staging).as_posix(), "hashes": hashes(path)})
    expected = 1 if archive.name.startswith("simo-") else 4
    if len(checkpoints) != expected:
        raise RuntimeError(f"Expected {expected} checkpoints; found {len(checkpoints)}")
    result = {"zip_crc": "all_members_passed", "checkpoints": checkpoints,
              "checkpoint_deserialization": "not_performed"}
    write_json(staging / marker.name, result)
    staging.rename(destination)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    download_lock = lock_download(root)
    if shutil.disk_usage(root).free < 12 * 1024 ** 3:
        raise RuntimeError("Need at least 12 GiB free for archives and extracted checkpoints")
    source = root / "code"
    if not source.exists():
        subprocess.run(["git", "init", str(source)], check=True, capture_output=True)
        subprocess.run(["git", "-C", str(source), "remote", "add", "origin", REPO], check=True)
        subprocess.run(["git", "-C", str(source), "fetch", "--depth=1", "origin", REV], check=True)
        subprocess.run(["git", "-C", str(source), "checkout", "--detach", REV], check=True)
    if git("-C", str(source), "rev-parse", "HEAD") != REV:
        raise RuntimeError("Existing source revision differs; preserve it")
    if git("-C", str(source), "status", "--porcelain"):
        raise RuntimeError("Existing upstream source is dirty; preserve it")
    if git("-C", str(source), "remote", "get-url", "origin") != REPO:
        raise RuntimeError("Unexpected source provenance")
    required = ("train.py", "separate.py", "requirements.txt", "LICENSE.txt",
                "models/bs.py", "models/bsrnn.py", "conf/config.yaml")
    for name in required:
        if not (source / name).is_file():
            raise RuntimeError("Missing full pipeline file: " + name)
    say("Pinned full upstream code: " + REV)
    archive_dir = root / "archives"
    archive_dir.mkdir(exist_ok=True)
    manifest = {"source": {"repository": REPO, "commit": REV, "code_license": "MIT"},
                "record": RECORD, "weights_license": "CC-BY-4.0",
                "training_corpus_as_published": "MUSDB18-HQ",
                "purpose": "Unmodified noncausal PC reference; not an Audi candidate",
                "training_or_inference_started": False, "bundles": []}
    for name, size, md5 in BUNDLES:
        archive = archive_dir / (name + ".zip")
        url = f"https://zenodo.org/api/records/17516442/files/{name}.zip/content"
        check = fetch(url, archive, size, md5)
        extracted = extract(archive, root / "weights" / name)
        manifest["bundles"].append({"name": name, "url": url,
                                    "archive": check, "extracted": extracted})
        write_json(root / "INTAKE_PROGRESS.json", manifest)
    write_json(root / "INTAKE_COMPLETE.json", manifest)
    say("COMPLETE: full code and both verified checkpoint bundles; no models executed")
    download_lock.close()


if __name__ == "__main__":
    main()
