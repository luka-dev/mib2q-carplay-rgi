#!/usr/bin/env python3
"""Build the offline QNX dio-owner handoff candidate; never install it."""

from __future__ import annotations

from hashlib import sha256
from pathlib import Path
import argparse
import shutil
import struct
import subprocess


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
AUDI = HERE.parents[3]
OUT = HERE / "out"
IMAGE = "qnx65-armv7-toolchain:latest"


def report(path: Path) -> None:
    print(f"{path} ({path.stat().st_size} bytes) SHA-256 {sha256(path.read_bytes()).hexdigest()}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--property-handoff", action="store_true")
    parser.add_argument("--stock210", action="store_true")
    args = parser.parse_args()
    if args.stock210 and not args.property_handoff:
        parser.error("--stock210 requires --property-handoff")
    OUT.mkdir(exist_ok=True)
    wrapper_build = ["python3", str(HERE / "build_wrapper_candidate.py")]
    if args.property_handoff:
        wrapper_build.append("--property-handoff")
    if args.stock210:
        wrapper_build.append("--stock210")
    subprocess.run(wrapper_build, check=True)
    artifact = OUT / ("dio_owner_exec.handoff.offline" if args.property_handoff
                      else "dio_owner_exec.offline")
    owner_command = [
        "docker", "run", "--rm", "--platform=linux/amd64",
        "-v", f"{AUDI}:/src:ro", "-v", f"{OUT}:/out",
        "-w", "/src/Patches/mhi2-carplay", IMAGE,
        "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-Os",
        "-Wall", "-Wextra", "-Werror", "-o", f"/out/{artifact.name}",
        "wireless/integration/dio_owner_exec.c", "-lsocket",
    ]
    if args.property_handoff:
        owner_command.insert(-2, "-DMHI2Q_REQUIRE_QUIESCE_ALWAYS")
    subprocess.run(owner_command, check=True)
    data = artifact.read_bytes()
    if data[:6] != b"\x7fELF\x01\x01" or struct.unpack_from("<H", data, 18)[0] != 40:
        raise RuntimeError("owner helper is not ARM little-endian ELF32")
    supervisor_bin = OUT / ("wireless_dio_supervisor_bin.handoff.offline"
                            if args.property_handoff
                            else "wireless_dio_supervisor_bin.offline")
    supervisor_command = [
        "docker", "run", "--rm", "--platform=linux/amd64",
        "-v", f"{AUDI}:/src:ro", "-v", f"{OUT}:/out",
        "-w", "/src/Patches/mhi2-carplay", IMAGE,
        "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-Os",
        "-Wall", "-Wextra", "-Werror", "-o", f"/out/{supervisor_bin.name}",
        "wireless/integration/wireless_dio_supervisor.c", "-lsocket",
    ]
    if args.property_handoff:
        supervisor_command.insert(-2, "-DMHI2Q_REQUIRE_QUIESCE_ALWAYS")
    subprocess.run(supervisor_command, check=True)
    data = supervisor_bin.read_bytes()
    if data[:6] != b"\x7fELF\x01\x01" or struct.unpack_from("<H", data, 18)[0] != 40:
        raise RuntimeError("wireless supervisor is not ARM little-endian ELF32")
    for source, name in (("wireless_runtime.c", "wireless_runtime"),
                         ("mdns_probe.c", "mhi2q_mdns_probe")):
        subprocess.run([
            'docker', 'run', '--rm', '--platform=linux/amd64',
            '-v', f'{AUDI}:/src:ro', '-v', f'{OUT}:/out',
            '-w', '/src/Patches/mhi2-carplay', IMAGE,
            'arm-unknown-nto-qnx6.5.0eabi-gcc', '-std=c11', '-Os',
            '-Wall', '-Wextra', '-Werror', '-o', f'/out/{name}',
            f'wireless/integration/{source}', '-lsocket',
        ], check=True)
        report(OUT / name)
    supervisor = OUT / "wireless_dio_supervisor.offline.sh"
    shutil.copyfile(HERE / "wireless_dio_supervisor.sh", supervisor)
    subprocess.run(["sh", "-n", str(supervisor)], check=True)
    for path in (artifact, supervisor_bin, supervisor,
                 OUT / ("carplay_startup.stock210.offline.sh" if args.stock210 else
                        "carplay_startup.handoff.offline.sh" if args.property_handoff
                        else "carplay_startup.wireless.offline.sh")):
        report(path)
    print("review-only handoff candidate; no head-unit access")


if __name__ == "__main__":
    main()
