#!/usr/bin/env python3
"""Assemble all MU1316 wireless CarPlay artifacts for offline review only.

This script reads pinned firmware inputs and local builders. It never connects
to the head unit or modifies a firmware image. Its guarded installer candidate
is packaged for offline review and sandbox testing only.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile

from elftools.elf.elffile import ELFFile


REPO = Path(__file__).resolve().parents[1]
AUDI = REPO.parents[1]
WIRELESS = REPO / "wireless"
OUT = WIRELESS / "out"
ARCHIVE = OUT / "mhi2q-wireless-offline-review.tar"
HANDOFF_ARCHIVE = OUT / "mhi2q-wireless-handoff-review.tar"
IMAGE = "qnx65-armv7-toolchain:latest"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(*args: str) -> None:
    subprocess.run(args, cwd=REPO, check=True)


def check_arm_elf(path: Path, kind: str) -> None:
    with path.open("rb") as stream:
        elf = ELFFile(stream)
        if elf.elfclass != 32 or not elf.little_endian or elf["e_machine"] != "EM_ARM":
            raise RuntimeError(f"{path}: expected little-endian ARM ELF32")
        if elf["e_type"] != kind:
            raise RuntimeError(f"{path}: expected {kind}, got {elf['e_type']}")


def add_file(archive: tarfile.TarFile, name: str, source: Path,
             report: dict[str, object], executable: bool = False) -> None:
    data = source.read_bytes()
    info = tarfile.TarInfo(name)
    info.size = len(data)
    info.mode = 0o755 if executable else 0o644
    info.mtime = 0
    archive.addfile(info, io.BytesIO(data))
    report[name] = {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)}


def add_text(archive: tarfile.TarFile, name: str, content: str) -> None:
    data = content.encode()
    info = tarfile.TarInfo(name)
    info.size = len(data)
    info.mode = 0o644
    info.mtime = 0
    archive.addfile(info, io.BytesIO(data))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--property-handoff-review", action="store_true",
                        help="build a separate review archive with guarded copy-property handoff")
    args = parser.parse_args()
    archive_path = HANDOFF_ARCHIVE if args.property_handoff_review else ARCHIVE
    run("python3", str(WIRELESS / "verify_graft_points.py"))
    run("python3", str(WIRELESS / "graft/build_offline.py"))
    run("sh", str(WIRELESS / "receiver_bridge/build_airplay280_qemu.sh"), "on_unit")
    bridge_build = ["python3", str(WIRELESS / "receiver_bridge/build_offline.py")]
    if args.property_handoff_review:
        bridge_build.append("--property-handoff")
    run(*bridge_build)
    owner_build = ["python3", str(WIRELESS / "integration/build_owner_offline.py")]
    launch_build = ["python3", str(WIRELESS / "integration/build_launch_candidate.py")]
    if args.property_handoff_review:
        owner_build.append("--property-handoff")
        launch_build.append("--property-handoff")
    run(*owner_build)
    run(*launch_build)
    run("sh", "-n", str(WIRELESS / "integration/install_wireless_candidate.sh"))
    run("python3", str(WIRELESS / "firewall/build_offline.py"))
    run("python3", str(WIRELESS / "firewall/qemu_pfctl.py"))
    run("python3", str(WIRELESS / "daemon/probes/qemu_wifi_ie.py"),
        "--delayed-ap-qemu")
    i2c_bsp_include = AUDI / (
        "Tools/mhi2q-qemu/guest/startup/bsp-qnx65-qemu-virt-a15/"
        "prebuilt/usr/include"
    )
    if not (i2c_bsp_include / "hw/i2c.h").is_file():
        raise RuntimeError("QNX BSP hw/i2c.h is required for I2C ABI verification")
    run(
        "docker", "run", "--rm", "--platform=linux/amd64",
        "-v", f"{AUDI}:/src", "-w", "/src/Patches/mhi2-carplay", IMAGE,
        "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-D_DEFAULT_SOURCE",
        "-I", "/src/Tools/mhi2q-qemu/guest/startup/"
              "bsp-qnx65-qemu-virt-a15/prebuilt/usr/include",
        "-fsyntax-only", "wireless/daemon/probes/qnx_i2c_abi_check.c",
    )
    rust_build = subprocess.run((
        "docker", "run", "--rm", "--platform=linux/amd64",
        "-v", f"{AUDI}:/src", "-w", "/src/Patches/mhi2-carplay", IMAGE,
        "/opt/rust/qnx-cargo", "build", "-q", "--locked",
        "--manifest-path", "/src/Patches/mhi2-carplay/wireless/daemon/Cargo.toml",
        "--bin", "mhi2q-wireless-daemon",
    ), cwd=REPO, capture_output=True, text=True)
    if rust_build.returncode:
        raise RuntimeError(f"QNX Rust build failed:\n{rust_build.stderr}")

    graft = WIRELESS / "graft/out/btstack.graft.offline"
    daemon = WIRELESS / (
        "daemon/target/armv7-unknown-nto-qnx650/release/mhi2q-wireless-daemon"
    )
    bridge = WIRELESS / "receiver_bridge/out" / (
        "libdio_receiver_bridge.handoff.offline.so" if args.property_handoff_review
        else "libdio_receiver_bridge.offline.so"
    )
    owner = WIRELESS / "integration/out" / (
        "dio_owner_exec.handoff.offline" if args.property_handoff_review
        else "dio_owner_exec.offline")
    supervisor_bin = WIRELESS / "integration/out" / (
        "wireless_dio_supervisor_bin.handoff.offline" if args.property_handoff_review
        else "wireless_dio_supervisor_bin.offline")
    startup = WIRELESS / "integration/out" / (
        "carplay_startup.handoff.offline.sh" if args.property_handoff_review
        else "carplay_startup.wireless.offline.sh")
    supervisor = WIRELESS / "integration/out/wireless_dio_supervisor.offline.sh"
    rc_local = WIRELESS / "integration/out/rc.local.wireless.offline.sh"
    env_example = WIRELESS / "integration/out" / (
        "wireless_carplay.handoff.env.example" if args.property_handoff_review
        else "wireless_carplay.env.example")
    airplay = WIRELESS / "receiver_bridge/out/airplay280_on_unit/libairplay.on_unit.so"
    stock_btstack = AUDI / (
        "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
        "advanced/MU1316-appimg/eso/bin/apps/btstack"
    )
    stock_airplay = AUDI / (
        "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
        "advanced/MU1316-appimg/eso/lib/libairplay.so"
    )
    wired_startup = REPO / "deploy/smartphone_integrator/carplay_startup.sh"
    rc_before = REPO.parent / "wifi_fix/rc.local"
    for path, kind in ((graft, "ET_EXEC"), (daemon, "ET_EXEC"),
                       (bridge, "ET_DYN"), (airplay, "ET_DYN"),
                       (owner, "ET_EXEC"), (supervisor_bin, "ET_EXEC")):
        check_arm_elf(path, kind)

    graft_report = json.loads((WIRELESS / "graft/out/build_report.json").read_text())
    firewall_report = json.loads((WIRELESS / "firewall/out/build_report.json").read_text())
    if graft_report["status"] != "OFFLINE_STATIC_REVIEW_ONLY" or \
            graft_report["output_sha256"] != digest(graft) or \
            digest(airplay) != "1ad8a4f641646754f185e808e48837c2f845af5452f047d62affca4fe85c9050":
        raise RuntimeError("builder report does not match an offline artifact")
    if digest(stock_btstack) != graft_report["source_sha256"] or \
            digest(stock_airplay) != "01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2" or \
            digest(wired_startup) != "90d9e65bc97c8a835c4aa85f785271702bfe7f09d292ab45001ea453b78f87a2" or \
            digest(rc_before) != "588e18e429605f122daece60c3bb2b2be5c9dba55cf6969e474ba1841ee6e0be":
        raise RuntimeError("installation reference differs from reviewed source")
    for name, details in firewall_report.items():
        if digest(REPO / "deploy/pf/patched" / name) != details["input_sha256"] or \
                digest(WIRELESS / "firewall/out" / name) != details["output_sha256"]:
            raise RuntimeError(f"{name}: PF reference or output changed")

    entries: dict[str, object] = {}
    OUT.mkdir(exist_ok=True)
    with tarfile.open(archive_path, "w") as archive:
        for name, path, executable in (
            ("bin/btstack", graft, True),
            ("bin/mhi2q-wireless-daemon", daemon, True),
            ("lib/libdio_receiver_bridge.so", bridge, False),
            ("lib/libairplay.so", airplay, False),
            ("bin/dio_owner_exec", owner, True),
            ("bin/wireless_dio_supervisor_bin", supervisor_bin, True),
            ("integration/carplay_startup.sh", startup, True),
            ("integration/wireless_dio_supervisor.sh", supervisor, True),
            ("integration/rc.local.wireless.candidate.sh", rc_local, True),
            ("integration/wireless_carplay.env.example", env_example, False),
            ("integration/install_wireless_candidate.sh",
             WIRELESS / "integration/install_wireless_candidate.sh", True),
            ("integration/install_review.md",
             REPO / "docs/wireless-carplay/history/EXPERIMENTAL280_INSTALLER.md", False),
            ("reference/identity_and_pairing.md",
             REPO / "docs/wireless-carplay/history/RECEIVER_IDENTITY.md", False),
            ("reference/btstack.stock", stock_btstack, False),
            ("reference/libairplay.stock.so", stock_airplay, False),
            ("reference/carplay_startup.wired.sh", wired_startup, False),
            ("reference/rc.local.before.sh", rc_before, False),
        ):
            add_file(archive, name, path, entries, executable)
        if args.property_handoff_review:
            add_file(archive, "reference/info_cfdiag.md",
                     REPO / "docs/wireless-carplay/history/INFO_SERIALIZATION.md", entries)
        for name in ("pf.conf", "pf.ecm0.conf", "pf.mlan0.conf", "pf.ppp0.conf"):
            add_file(archive, f"pf/{name}", WIRELESS / "firewall/out" / name, entries)
            add_file(archive, f"reference/{name}.wired",
                     REPO / "deploy/pf/patched" / name, entries)
        add_text(archive, "checksums.txt", "".join(
            f"{details['sha256']} {name}\n"
            for name, details in sorted(entries.items())
        ))
        manifest = {
            "status": "OFFLINE_REVIEW_ONLY",
            "target": "MU1316 MHI2Q, pinned firmware inputs",
            "source_btstack_sha256": graft_report["source_sha256"],
            "source_libairplay_sha256": digest(stock_airplay),
            "candidate_libairplay_sha256": digest(airplay),
            "i2c_abi_check": {
                "result": "compile_passed",
                "bsp_header_sha256": digest(i2c_bsp_include / "hw/i2c.h"),
                "checker_sha256": digest(
                    WIRELESS / "daemon/probes/qnx_i2c_abi_check.c"
                ),
            },
            "pf_parser_check": {
                "result": "default_profile_parsed; other_profiles_match_wired_baseline_diagnostics",
                "factory_pfctl_sha256": "25fd9dd8e6c992a214ec9d17af8e144fe87aef21936e071a436649a1c5259446",
            },
            "ap_ie_recovery_check": {
                "result": "three_unavailable_queries_then_install_ap_reset_restore_and_removal_passed_in_qnx_qemu",
            },
            "files": entries,
            "remaining": [
                "AirPlay 280 completed independent QNX-client pairing, encrypted /info, screen-110 video and MainAudio PCM in QEMU; real iPhone, display/audio hardware and on-unit media remain unverified; intermittent /info HTTP 500 remains unexplained",
                "Owner helper passed host and QNX6.5/QEMU QNX4-FS handoff, but QNX6 persist-FS locking is unverified",
                "Exact wrapper, stock dio and ordinary daemon passed a combined supervisor QEMU run with synthetic phone/MFi/Wi-Fi; QNX6 persist-FS and real hardware remain unverified",
                "Pinned rc.local launch candidate exists, but SI/on-unit install and startup validation remain",
                "No on-unit MFi, radio, multicast, PF, or phone validation",
                "Wireless-only install/rollback candidate passed host and QNX6.5/QEMU QNX4-FS lifecycle checks; not run on the unit",
            ],
        }
        if args.property_handoff_review:
            manifest["status"] = "OFFLINE_HANDOFF_REVIEW_ONLY"
            manifest["receiver_bridge_variant"] = "guarded-copy-property-handoff"
            manifest["remaining"][0] = (
                "AirPlay 280 independent QNX-client pairing, encrypted /info, screen video "
                "and PCM passed in QEMU with copy-property handoff; real iPhone and hardware "
                "remain unverified"
            )
            manifest["remaining"].append(
                "QUIESCE drained an active property callback, closed AirPlay, rejected a later START, "
                "and preceded manager teardown in QEMU; guarded owner/supervisor executables require the "
                "acknowledgement before SIGTERM, with BUSY/OK retry checked in host and QNX/QEMU. "
                "Real unit lifecycle is unverified; this remains a separate offline review candidate"
            )
            manifest["remaining"].append(
                "Active encrypted screen/PCM in QEMU passed QUIESCE BUSY, wired DRAIN via receiver "
                "sessionDied, then QUIESCE OK and stock manager release; host owner helper verified "
                "DRAIN request order and no SIGTERM before OK. A combined QNX fixture exercised "
                "the exact guarded owner helper, generated wrapper and stock manager with active "
                "encrypted media; unowned DRAIN was denied and the wired child launched. The native "
                "supervisor then restarted stock dio after a QEMU-only USB release, obtained a fresh "
                "receiver START, and cleared the IE for both generations. The AirPlay 280 "
                "select loop now retries EINTR; the same integrated QEMU scenario passed "
                "without its earlier assertion. Physical USB/iPhone transition "
                "and writable QNX6 lock behavior remain unverified"
            )
        add_text(archive, "manifest.json", json.dumps(manifest, indent=2) + "\n")
        readme = ("Offline review artifacts for the pinned MU1316 MHI2Q firmware.\n"
                  "Contains a review-only installer candidate and exact baseline files.\n")
        if args.property_handoff_review:
            readme += ("Copy-property handoff variant: QUIESCE passed QEMU callback/teardown "
                       "and host owner-ordering tests; on-unit lifecycle remains unverified.\n")
        readme += ("Do not run on a head unit until filesystem locking, configuration,\n"
                   "hardware behavior and rollback are validated there.\n")
        add_text(archive, "README.txt", readme)
    with tarfile.open(archive_path) as archive:
        names = archive.getnames()
        if len(names) != len(set(names)) or set(names) != set(entries) | {
            "manifest.json", "README.txt", "checksums.txt"
        }:
            raise RuntimeError("bundle has unexpected or duplicate entries")
        bundled_manifest = json.load(archive.extractfile("manifest.json"))
        if bundled_manifest != manifest:
            raise RuntimeError("bundle manifest differs from generated data")
        expected_sums = "".join(
            f"{details['sha256']} {name}\n" for name, details in sorted(entries.items())
        )
        if archive.extractfile("checksums.txt").read().decode() != expected_sums:
            raise RuntimeError("bundle checksum list differs from generated data")
        for name, details in entries.items():
            bundled = archive.extractfile(name)
            if bundled is None or hashlib.sha256(bundled.read()).hexdigest() != details["sha256"]:
                raise RuntimeError(f"bundle entry {name} does not match its manifest")
    print(f"{archive_path} ({archive_path.stat().st_size} bytes, SHA-256 {digest(archive_path)})")
    print(f"{len(entries)} pinned offline artifacts; no head-unit action")


if __name__ == "__main__":
    main()
