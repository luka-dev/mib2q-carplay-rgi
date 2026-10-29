"""Repack the saved native AltScreen receiver for the private EHCI QEMU.

Uses the already built receiver's binaries, never the in-progress hook tree.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

from tests.carplay_lab.qnx import qemu_capture
from tests.carplay_lab.common.qnx_clock import corrected

ROOT = Path(__file__).resolve().parents[3]
BASE = ROOT / "build/wired-altscreen-30fps-re"
OUT = BASE / "native/qnx-network"
SOURCE = ROOT / "build/vphone-lab/qnx-network"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    qemu_capture.IMAGE = "qnx65-armv7-toolchain:8.5"
    image = SOURCE / "qemu_start_debug.bin"
    record = json.loads((SOURCE / "build-record.json").read_text())
    if digest(image) != record["output_sha256"]:
        raise ValueError("saved receiver image fingerprint changed")
    for key in ("software_mfi", "input_control", "capture_memory", "paired_altscreen", "clock_realtime"):
        if record.get(key) is not True:
            raise ValueError(f"saved receiver lacks {key}")
    extracted = OUT / "extracted"
    if extracted.exists():
        raise RuntimeError("receiver extraction already exists; retain it and use the built image")
    extracted.mkdir()
    listing = qemu_capture.docker(OUT, "dumpifs", "-v", "/src/" + str(image.relative_to(qemu_capture.AUDI)),
                                  capture_output=True).stdout
    for line in listing.splitlines():
        match = re.match(r"^\s+[0-9a-f]+\s+[0-9a-f]+\s+([^\s]+)", line)
        if match and not match[1].startswith(("Startup-", "Image-", "Root-", "*.")) and not match[1].endswith(".*"):
            (extracted / match[1]).parent.mkdir(parents=True, exist_ok=True)
    qemu_capture.docker(OUT, "sh", "-c", "cd /work/extracted && dumpifs -x /src/" +
                        str(image.relative_to(qemu_capture.AUDI)), capture_output=True)
    (OUT / "startup-virt").write_bytes(corrected((qemu_capture.SDP / "tests/qemu/startup-virt").read_bytes()))
    shutil.copyfile(qemu_capture.SDP / "tests/qemu/libstartup.a", OUT / "libstartup.a")
    source = (SOURCE / "network.build").read_text()
    mapped = 0
    def freeze(match):
        nonlocal mapped
        attributes, target, origin = match.groups()
        if "type=link" in attributes:
            return match[0]
        asset = extracted / target.lstrip("/")
        if not asset.is_file():
            raise ValueError(f"saved image lacks manifest asset: {target}")
        mapped += 1
        return f"{attributes}{target}=/work/extracted/{target.lstrip('/')}"
    source = re.sub(r"^(\s*(?:\[[^\]\n]*\]\s*)?)(/[^\s=]+)=(/[^\s]+)\s*$", freeze, source, flags=re.M)
    old = "/proc/boot/io-usb -vv -d ohci ioport=0x090d0000,irq=44 &"
    if source.count(old) != 1:
        raise ValueError("saved USB startup changed")
    source = source.replace(old, "/proc/boot/io-usb -d ehci ioport=0x090d0000,irq=44,int_thresh=1 &")
    old = "/proc/boot/devu-ohci.so=/work/extracted/proc/boot/devu-ohci.so"
    if source.count(old) != 1:
        raise ValueError("saved USB driver asset changed")
    source = source.replace(old, "/proc/boot/devu-ehci.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/devu-ehci.so")
    (OUT / "network.build").write_text(source)
    qemu_capture.docker(OUT, "mkifs", "network.build", "qemu_start_debug.bin", capture_output=True)
    record.update(output_sha256=digest(OUT / "qemu_start_debug.bin"),
                  snapshot_ifs_sha256=digest(image), snapshot_directory=str(SOURCE),
                  usb_controller="platform-ehci-usb", usb_interrupt_threshold=1,
                  frozen_manifest_assets=mapped,
                  frozen_hook_sha256=digest(extracted / "proc/boot/libcarplay_hook.so"),
                  frozen_probe_sha256=digest(extracted / "proc/boot/libdio_capture_probe.so"))
    (OUT / "build-record.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    argparse.ArgumentParser(description=__doc__).parse_args()
    build()
