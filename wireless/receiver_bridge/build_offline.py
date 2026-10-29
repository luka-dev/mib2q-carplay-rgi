#!/usr/bin/env python3
"""Cross-build the MU1316 receiver bridge artifact; never install it on the HU."""

from __future__ import annotations

import argparse
from hashlib import sha256
from pathlib import Path
import subprocess


AUDI = Path(__file__).resolve().parents[4]
REPO = Path(__file__).resolve().parents[2]
OUT = REPO / "wireless/receiver_bridge/out"
IMAGE = "qnx65-armv7-toolchain:latest"
ARTIFACT = OUT / "libdio_receiver_bridge.offline.so"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--display-mode", type=int, choices=(0, 1, 2), default=0,
                        help="stock dio_manager display table index (default: 0 = 800x480)")
    parser.add_argument("--property-handoff", action="store_true",
                        help="build a separate offline bridge candidate with guarded copy-property handoff")
    parser.add_argument("--receiver", choices=("stock210", "experimental280"), default="experimental280")
    args = parser.parse_args()
    subprocess.run(
        ["python3", str(REPO / "wireless/receiver_bridge/verify_capture_point.py")],
        check=True,
    )
    OUT.mkdir(exist_ok=True)
    artifact = (OUT / "libdio_receiver_bridge.handoff.offline.so"
                if args.property_handoff else ARTIFACT)
    if args.receiver == "stock210":
        if not args.property_handoff:
            parser.error("stock210 requires --property-handoff")
        artifact = OUT / "libdio_receiver_bridge.stock210.offline.so"
    command = [
        "docker", "run", "--rm", "--platform=linux/amd64",
        "-v", f"{AUDI}:/src:ro", "-v", f"{OUT}:/out",
        "-w", "/src/Patches/mhi2-carplay", IMAGE,
        "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-fPIC",
        "-shared", "-Os", "-Wall", "-Wextra", "-Werror",
        "-DMHI2Q_CAPTURE_BRIDGE",
        "-DMHI2Q_BRIDGE_PRESTART",
        f"-DMHI2Q_BRIDGE_DISPLAY_MODE={args.display_mode}",
        '-DMHI2Q_CAPTURE_INTERFACE_NAME="uap0"',
    ]
    if args.receiver == "stock210":
        command += ["-DMHI2Q_BRIDGE_STOCK210", "-DMHI2Q_BRIDGE_SI_TRANSPORT"]
    if args.property_handoff:
        command.append("-DMHI2Q_BRIDGE_PROPERTY_HANDOFF")
    command += [
        "-o", f"/out/{artifact.name}",
        "wireless/receiver_bridge/capture_probe.c",
        "wireless/receiver_bridge/bridge.c",
    ]
    if args.property_handoff:
        command.append("wireless/receiver_bridge/property_handoff.c")
    if args.receiver == "stock210":
        command.append("wireless/receiver_bridge/si_transport.c")
        command.append("wireless/receiver_bridge/now_playing.c")
        command.append("wireless/receiver_bridge/si_state.c")
        command.append("wireless/receiver_bridge/si_location.c")
    command.append("-lsocket")
    subprocess.run(command, check=True)
    image = artifact.read_bytes()
    if image[:4] != b"\x7fELF" or image[4] != 1 or image[5] != 1:
        raise SystemExit("output is not a little-endian ELF32 object")
    if args.receiver == "stock210":
        from elftools.elf.elffile import ELFFile
        with artifact.open('rb') as stream:
            exports = {s.name for s in ELFFile(stream).get_section_by_name('.dynsym').iter_symbols()
                       if s['st_shndx'] != 'SHN_UNDEF'}
        required={'kill','bind','mhi2q_si_transport_install','mhi2q_si_transport_initialized',
                  'mhi2q_now_playing_capture','mhi2q_now_playing_submit',
                  'mhi2q_si_state_install','mhi2q_si_location_install'}
        if not required <= exports:
            raise SystemExit(f'stock210 bridge omits required interpositions: {required-exports}')
        if any(name.startswith('si_qemu_') or name in
               {'snd_pcm_open_name','snd_pcm_plugin_write','snd_pcm_plugin_read'} for name in exports):
            raise SystemExit('production bridge contains QEMU HMI/audio implementations')
    print(f"{artifact} ({len(image)} bytes)")
    print(f"stock display mode {args.display_mode}")
    print(f"SHA-256 {sha256(image).hexdigest()}")


if __name__ == "__main__":
    main()
