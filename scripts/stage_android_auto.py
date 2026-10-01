#!/usr/bin/env python3
"""Create a self-contained M.I.B. SD-card folder from locally built outputs."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def cksum(path: Path) -> tuple[int, int]:
    table = []
    for value in range(256):
        crc = value << 24
        for _ in range(8):
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if crc & 0x80000000 else (crc << 1) & 0xFFFFFFFF
        table.append(crc)
    crc = 0
    size = 0
    with path.open("rb") as fh:
        for block in iter(lambda: fh.read(1024 * 1024), b""):
            size += len(block)
            for byte in block:
                crc = ((crc << 8) & 0xFFFFFFFF) ^ table[((crc >> 24) ^ byte) & 0xFF]
    length = size
    while length:
        byte = length & 0xFF
        crc = ((crc << 8) & 0xFFFFFFFF) ^ table[((crc >> 24) ^ byte) & 0xFF]
        length >>= 8
    return (~crc) & 0xFFFFFFFF, size


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", type=Path, default=ROOT / "dist/android-auto-card")
    args = ap.parse_args()

    native = ROOT / "build/android_auto"
    inputs = {
        "libsq5_cluster_live.so": native / "libsq5_cluster_live.so",
        "cluster-player": native / "cluster-player",
        "config_tool": native / "config_tool",
        "mib2q_android_auto_cluster.jar": native / "mib2q_android_auto_cluster.jar",
        "maneuver_render": ROOT / "build/maneuver_render",
        "flag_atlas.rgba": ROOT / "maneuver_render/resources/flag_atlas.rgba",
        "gal_startup.sh": ROOT / "android_auto/deploy/gal_startup_standalone.sh",
        "sq5_luka_monitor.sh": ROOT / "android_auto/deploy/sq5_luka_monitor.sh",
        "sq5_luka_processes.sh": ROOT / "android_auto/deploy/sq5_luka_processes.sh",
        "sq5_luka_cleanup.sh": ROOT / "android_auto/deploy/sq5_luka_cleanup.sh",
    }
    missing = [str(path) for path in inputs.values() if not path.is_file()]
    if missing:
        raise SystemExit("Build is incomplete; missing:\n  " + "\n  ".join(missing))

    out = args.output.resolve()
    if out.exists():
        shutil.rmtree(out)
    mod = out / "install_AndroidAuto_MoreIncredibleBash/mod"
    payload = mod / "android-auto"
    payload.mkdir(parents=True)
    shutil.copy2(ROOT / "install_AndroidAuto_MoreIncredibleBash/mod/custom.sh", mod / "custom.sh")

    manifest = []
    for name, source in inputs.items():
        destination = payload / name
        shutil.copy2(source, destination)
        crc, size = cksum(destination)
        manifest.append(f"{crc} {size} {name}\n")
    (payload / "manifest.cksum").write_text("".join(manifest), encoding="ascii", newline="\n")
    (out / "README.txt").write_text(
        "Copy install_AndroidAuto_MoreIncredibleBash into the M.I.B. SD-card root.\n"
        "Run the custom script once with no marker for a read-only audit.\n"
        "Create mib2q_aa_install in the SD-card root and run again to install.\n"
        "Create mib2q_aa_uninstall instead to restore the saved configuration.\n",
        encoding="utf-8",
        newline="\n",
    )
    print(f"Staged {out}")
    for line in manifest:
        print("  " + line.rstrip())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
