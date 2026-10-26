#!/usr/bin/env python3
"""Create a self-contained, profile-aware M.I.B. SD-card folder."""

from __future__ import annotations

import argparse
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROFILE_DIR = ROOT / "firmware/profiles"
JAR_NAME = "mib2q_android_auto_cluster.jar"


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


def parse_profile_artifact(value: str) -> tuple[str, Path]:
    if "=" not in value:
        raise argparse.ArgumentTypeError("expected PROFILE_ID=PATH")
    profile_id, raw_path = value.split("=", 1)
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", profile_id):
        raise argparse.ArgumentTypeError("invalid profile id")
    return profile_id, Path(raw_path)


def profile_field(profile: dict, name: str, field: str) -> str:
    value = profile.get("files", {}).get(name, {}).get(field, "")
    return str(value) if value != "" else ""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", type=Path, default=ROOT / "dist/android-auto-card")
    ap.add_argument(
        "--profile-jar", action="append", default=[], type=parse_profile_artifact,
        metavar="PROFILE_ID=PATH", help="stage a firmware-specific Java overlay (repeatable)",
    )
    args = ap.parse_args()

    profiles = {}
    for path in sorted(PROFILE_DIR.glob("*.json")):
        profile = json.loads(path.read_text(encoding="utf-8"))
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", profile.get("id", "")):
            raise SystemExit(f"Invalid profile id in {path}")
        if path.stem != profile["id"]:
            raise SystemExit(f"Profile filename/id mismatch: {path.name} != {profile['id']}.json")
        profiles[profile["id"]] = profile
    if not profiles:
        raise SystemExit("No firmware profiles found")

    artifacts = dict(args.profile_jar)
    if len(artifacts) != len(args.profile_jar):
        raise SystemExit("Duplicate --profile-jar id")
    if not artifacts:
        profile_id = "mhi2q_us_aug22_p3639_mu0918"
        preferred = ROOT / f"build/android_auto/profiles/{profile_id}/{JAR_NAME}"
        artifacts[profile_id] = preferred if preferred.is_file() else ROOT / f"build/android_auto/{JAR_NAME}"
    unknown = sorted(set(artifacts) - set(profiles))
    if unknown:
        raise SystemExit("Unknown profile id(s): " + ", ".join(unknown))

    native = ROOT / "build/android_auto"
    common_inputs = {
        "libsq5_cluster_live.so": native / "libsq5_cluster_live.so",
        "cluster-player": native / "cluster-player",
        "config_tool": native / "config_tool",
        "maneuver_render": ROOT / "build/maneuver_render",
        "flag_atlas.rgba": ROOT / "maneuver_render/resources/flag_atlas.rgba",
        "gal_startup.sh": ROOT / "android_auto/deploy/gal_startup_standalone.sh",
        "sq5_luka_monitor.sh": ROOT / "android_auto/deploy/sq5_luka_monitor.sh",
        "sq5_luka_processes.sh": ROOT / "android_auto/deploy/sq5_luka_processes.sh",
        "sq5_luka_cleanup.sh": ROOT / "android_auto/deploy/sq5_luka_cleanup.sh",
    }
    all_inputs = list(common_inputs.values()) + list(artifacts.values())
    missing = [str(path) for path in all_inputs if not path.is_file()]
    if missing:
        raise SystemExit("Build is incomplete; missing:\n  " + "\n  ".join(missing))

    out = args.output.resolve()
    if out.exists():
        shutil.rmtree(out)
    mod = out / "install_AndroidAuto_MoreIncredibleBash/mod"
    payload = mod / "android-auto"
    payload.mkdir(parents=True)
    shutil.copy2(ROOT / "install_AndroidAuto_MoreIncredibleBash/mod/custom.sh", mod / "custom.sh")

    staged: dict[str, Path] = {}
    for name, source in common_inputs.items():
        destination = payload / name
        shutil.copy2(source, destination)
        staged[name] = destination

    jar_paths = {}
    for profile_id, source in sorted(artifacts.items()):
        relative = f"profiles/{profile_id}/{JAR_NAME}"
        destination = payload / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        staged[relative] = destination
        jar_paths[profile_id] = relative

    profile_lines = []
    for profile_id, profile in sorted(profiles.items()):
        files = profile.get("files", {})
        installable = profile.get("installable", profile.get("status") == "validated-on-car")
        gal_size = profile_field(profile, "gal", "size") or str(profile.get("detection", {}).get("gal_size_observed", ""))
        fields = (
            profile_id,
            profile["label"],
            "installable" if installable else profile.get("status", "pending"),
            profile_field(profile, "gal", "posix_cksum"),
            gal_size,
            profile_field(profile, "libautoreceiver.so", "posix_cksum"),
            profile_field(profile, "libautoreceiver.so", "size"),
            profile_field(profile, "lsd.jxe", "posix_cksum"),
            profile_field(profile, "lsd.jxe", "size"),
            jar_paths.get(profile_id, ""),
        )
        if any("|" in str(field) or "\n" in str(field) for field in fields):
            raise SystemExit(f"Invalid profile manifest field in {profile_id}")
        profile_lines.append("|".join(str(field) for field in fields) + "\n")
    profiles_list = payload / "profiles.list"
    profiles_list.write_text("".join(profile_lines), encoding="utf-8", newline="\n")
    staged["profiles.list"] = profiles_list

    manifest = []
    for name, destination in sorted(staged.items()):
        crc, size = cksum(destination)
        manifest.append(f"{crc} {size} {name}\n")
    (payload / "manifest.cksum").write_text("".join(manifest), encoding="ascii", newline="\n")
    (out / "README.txt").write_text(
        "Copy install_AndroidAuto_MoreIncredibleBash into the M.I.B. SD-card root.\n"
        "Run the custom script once with no marker for a read-only firmware/profile audit.\n"
        "Create mib2q_aa_install in the SD-card root and run again to install.\n"
        "Unknown, incomplete and pending-native profiles are refused without modifying the unit.\n"
        "Create mib2q_aa_uninstall instead to restore the saved configuration.\n",
        encoding="utf-8", newline="\n",
    )
    print(f"Staged {out}")
    for line in manifest:
        print("  " + line.rstrip())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
