#!/usr/bin/env python3
"""Build an exec-only QNX SI launcher and matched review payload; never install.

The SI candidate and adapter are copied unchanged from the selected stage.
No rollback payload or automatic recovery is generated.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", required=True, help="absolute installed candidate directory")
    parser.add_argument("--stage2", action="store_true")
    parser.add_argument("--outdir", type=Path, default=ROOT / "build/si-boot-fix")
    args = parser.parse_args()
    directory = PurePosixPath(args.directory)
    if not directory.is_absolute() or ".." in directory.parts or any(ord(c) < 32 or ord(c) == 127 for c in args.directory):
        parser.error("--directory must be an absolute HU directory without traversal or control characters")
    out = args.outdir.resolve()
    out.relative_to(ROOT / "build")
    out.mkdir(parents=True, exist_ok=True)
    (out / "manifest.json").unlink(missing_ok=True)
    stage = ROOT / "build" / ("si-wireless-stage2" if args.stage2 else "si-wireless-stage1")
    receipt = json.loads((stage / "libsi_adapter.so.receipt.json").read_text())
    candidate = stage / "smartphone_integrator.si_wireless_stage1"
    adapter = stage / "libsi_adapter.so"
    if receipt["candidate_sha256"] != digest(candidate) or receipt["sha256"] != digest(adapter):
        raise ValueError("candidate/adapter differ from their build receipt")
    if receipt["diagnostic"]:
        raise ValueError("HU review requires production adapter")
    header = out / "si_startup_config.h"
    header.write_text(f'#define SI_STARTUP_DIR {json.dumps(str(directory))}\n'
                      f'#define SI_STARTUP_STAGE2 {int(args.stage2)}\n')
    source = ROOT / "wireless/si/si_startup.c"
    subprocess.run([
        "docker", "run", "--rm", "--platform=linux/amd64", "-v", f"{ROOT}:/src",
        "-w", "/src", "qnx65-armv7-toolchain:latest",
        "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c99", "-Os", "-Wall", "-Wextra", "-Werror",
        "-include", "/src/" + str(header.relative_to(ROOT)),
        "wireless/si/si_startup.c", "-o", "/src/" + str((out / "si_startup").relative_to(ROOT)),
    ], check=True)
    shutil.copyfile(candidate, out / "smartphone_integrator")
    shutil.copyfile(adapter, out / "libsi_adapter.so")
    shutil.copyfile(ROOT / "wireless/si/si_hu_diagnose.sh", out / "si_hu_diagnose.sh")
    manifest = {
        "profile": "si-exec-launcher-review", "abi": 2 if args.stage2 else 1,
        "installed_directory": str(directory), "hu_deployed": False,
        "automatic_rollback": False, "fallback_image": None,
        "preserves_candidate_and_adapter": True,
        "launch_entry": {"target": "/mnt/app/eso/bin/apps/smartphone_integrator",
                         "payload": "si_startup", "exec_directory": str(directory)},
        "review_only": True,
        "source_sha256": digest(source), "config_sha256": digest(header),
        "files": {name: {"sha256": digest(out / name), "size": (out / name).stat().st_size}
                  for name in ("si_startup", "smartphone_integrator", "libsi_adapter.so", "si_hu_diagnose.sh")},
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
