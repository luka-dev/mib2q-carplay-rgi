#!/usr/bin/env python3
"""Install the pinned notarized Launchpad without modifying host security."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import urllib.request

if __package__:
    from .lab import CLI, OUT, profile
else:
    from lab import CLI, OUT, profile

LIMIT = 128 * 1024 * 1024


def download(name: str, digest: str, version: str, destination: Path) -> Path:
    if destination.exists():
        if destination.stat().st_size > LIMIT:
            raise ValueError("cached release exceeds the download bound")
        if hashlib.sha256(destination.read_bytes()).hexdigest() != digest:
            raise ValueError("cached release SHA-256 mismatch; retain it for inspection")
        return destination
    url = f"https://github.com/Lakr233/vphone-cli/releases/download/{version}/{name}"
    request = urllib.request.Request(url, headers={"User-Agent": "mhi2-vphone-lab"})
    temporary = destination.with_suffix(".download")
    if temporary.exists():
        raise FileExistsError("a partial release download already exists; inspect it before retrying")
    created = False
    try:
        hasher = hashlib.sha256()
        size = 0
        with urllib.request.urlopen(request, timeout=30) as response, temporary.open("xb") as output:
            created = True
            while block := response.read(1024 * 1024):
                size += len(block)
                if size > LIMIT:
                    raise ValueError("release exceeds the download bound")
                hasher.update(block)
                output.write(block)
        if hasher.hexdigest() != digest:
            raise ValueError("downloaded release SHA-256 mismatch")
        temporary.replace(destination)
    finally:
        # Only remove this invocation's partial download, never a cached release.
        if created and temporary.exists():
            temporary.unlink()
    return destination


def install_launchpad(archive: Path, directory: Path, version: str) -> None:
    # A second persistent .app with the same bundle ID makes LaunchServices
    # and computer-use app lookup ambiguous. Keep only the installed copy.
    with tempfile.TemporaryDirectory(dir=directory, prefix=".launchpad-extract-") as unpacked:
        subprocess.run(["ditto", "-x", "-k", str(archive), unpacked], check=True)
        source = Path(unpacked) / "vphone-launchpad.app"
        subprocess.run(["codesign", "--verify", "--deep", "--strict", str(source)], check=True)
        subprocess.run(["spctl", "--assess", "--type", "execute", str(source)], check=True)
        target = CLI.parents[2]
        if target.exists():
            installed = plistlib.loads((target / "Contents/Info.plist").read_bytes())
            if installed["CFBundleShortVersionString"] != version:
                raise RuntimeError("another Launchpad version is installed; replacement requires review")
            subprocess.run(["codesign", "--verify", "--deep", "--strict", str(target)], check=True)
            for relative in ("Contents/MacOS/vphone-launchpad", "Contents/MacOS/vphone-launchpad-cli",
                             "Contents/Library/LaunchServices/com.vphone.launchpad.helper"):
                if hashlib.sha256((target / relative).read_bytes()).digest() != \
                        hashlib.sha256((source / relative).read_bytes()).digest():
                    raise RuntimeError("installed Launchpad executables differ from the verified release")
        else:
            subprocess.run(["ditto", str(source), str(target)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--download-only", action="store_true")
    args = parser.parse_args()
    settings = profile()
    base = OUT / "bootstrap"
    base.mkdir(parents=True, exist_ok=True)
    version = settings["bundle"]
    archives = []
    for name, digest in ((f"vphone-launchpad-{version}-notarized.zip", settings["launchpad_sha256"]),
                         (f"VPhone-{version}.zip", settings["bundle_sha256"])):
        archives.append(download(name, digest, version, base / name))
    if not args.download_only:
        install_launchpad(archives[0], base, version)
        directory = Path.home() / ".local/bin"
        directory.mkdir(parents=True, exist_ok=True)
        link = directory / "vphone-launchpad-cli"
        if not link.exists() and not link.is_symlink():
            link.symlink_to(CLI)
        elif link.resolve() != CLI.resolve():
            raise RuntimeError("existing CLI link belongs to another installation")
    print(json.dumps({"version": version, "verified_archives": [str(p) for p in archives],
                      "cli": str(CLI), "host_security_changed": False}, indent=2))


if __name__ == "__main__":
    main()
