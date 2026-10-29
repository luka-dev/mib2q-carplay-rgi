#!/usr/bin/env python3
"""Rebuild a local stock-dio QEMU image with only the capture preload.

Uses the existing offline Track-F IFS as a filesystem source; removes its
force-spawn preloads. No head-unit connection or firmware file is modified.
"""

from __future__ import annotations

import argparse
import os
from hashlib import sha256
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import time


REPO = Path(__file__).resolve().parents[3]
AUDI = REPO.parents[1]
if __package__ in (None, ""):
    sys.path.insert(0, str(REPO))
    sys.modules.setdefault("tests.carplay_lab.qnx.qemu_capture", sys.modules[__name__])
TOOLS = AUDI / "Tools"
CARPLAY = TOOLS / "qnx-carplay-emu/carplay"
SDP = TOOLS / "qnx-65-sdp-docker"
QEMU = TOOLS / "qemu-src/build/qemu-system-arm"
OUT = REPO / "wireless/receiver_bridge/out"
IMAGE = "qnx65-armv7-toolchain:latest"
RUST_IMAGE = os.environ.get("QNX_RUST_IMAGE", "qnx65-armv7-toolchain:8.5-rust")


def run(*args: str, **kwargs: object) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, check=True, text=True, **kwargs)


def docker(work: Path, *args: str, **kwargs: object) -> subprocess.CompletedProcess[str]:
    return run(
        "docker", "run", "--rm", "--platform=linux/amd64",
        "-v", f"{work}:/work", "-v", f"{AUDI}:/src:ro",
        "-v", f"{SDP / 'tools'}:/opt/tools:ro", "-w", "/work",
        IMAGE, *args, **kwargs,
    )


def parse_txt_record(raw: bytes) -> dict[str, str]:
    entries: dict[str, str] = {}
    index = 0
    while index < len(raw):
        length = raw[index]
        index += 1
        if length == 0 or index + length > len(raw):
            raise ValueError("malformed DNS-SD TXT record")
        item = raw[index:index + length].decode("ascii")
        index += length
        key, separator, value = item.partition("=")
        if not separator or not key or key in entries:
            raise ValueError("malformed DNS-SD TXT entry")
        entries[key] = value
    return entries


def make_manifest(work: Path, base_build: Path, start_experiment: bool,
                  feature37: bool, airplay280_qemu: bool, mdnsd: bool,
                  receiver_control_probe: bool,
                  daemon_phone_qemu: bool, daemon_control_qemu: bool,
                  existing_hook_qemu: bool, owner_exec_qemu: bool,
                  owner_wrapper_qemu: bool, owner_supervisor_qemu: bool,
                  active_owner_wired_qemu: bool,
                  active_owner_supervisor_qemu: bool,
                  independent_srp_qemu: bool, screen_data_qemu: bool,
                  stock_mfi_probe_qemu: bool) -> None:
    source = base_build.read_text()
    source = source.replace(
        "/root/bsp-qemu-virt-src/src/hardware/startup/lib/arm/a.le.v7/libstartup.a",
        "/work/libstartup.a",
    ).replace(
        "/usr/qnx650/target/qnx6/armle-v7",
        "/opt/qnx650/target/qnx6/armle-v7",
    ).replace(
        "/root/startup-virt", "/work/startup-virt",
    ).replace(
        "/root/bsp_extract/procnto-smp",
        "/work/extracted/proc/boot/procnto-smp",
    ).replace(
        "LD_PRELOAD=/proc/boot/libtrackf_stock_spawn.so:/proc/boot/libdioprobe.so",
        "LD_PRELOAD=/proc/boot/libdio_capture_probe.so",
    ).replace(
        "=== Track-F REAL-STOCK dio_manager force-spawn (run@0x15c214) ===",
        "=== stock dio_manager capture-only probe ===",
    ).replace(
        "--- launching REAL stock dio_manager (LD_PRELOAD spawn+probe) ---",
        "--- launching stock dio_manager with capture probe ---",
    )
    if start_experiment:
        source = source.replace(
            "LD_PRELOAD=/proc/boot/libdioprobe.so",
            "LD_PRELOAD=/proc/boot/libdio_capture_probe.so:/proc/boot/libdioprobe.so",
        )
        source = source.replace(
            "/proc/boot/devstub /dev/otg-cinemo /dev/i2c8",
            "/proc/boot/devstub /dev/otg-cinemo /dev/i2c8 /dev/urandom /dev/random",
        )
    else:
        source = source.replace(
            "LD_PRELOAD=/proc/boot/libdioprobe.so",
            "LD_PRELOAD=/proc/boot/libdio_capture_probe.so",
        )
    if existing_hook_qemu:
        probe_preload = "LD_PRELOAD=/proc/boot/libdio_capture_probe.so:/proc/boot/libdioprobe.so"
        if source.count(probe_preload) != 1:
            raise RuntimeError("cannot locate unique stock dio QEMU preload")
        source = source.replace(
            probe_preload,
            "LD_PRELOAD=/proc/boot/libcarplay_hook.so:/proc/boot/libdio_capture_probe.so:"
            "/proc/boot/libdioprobe.so",
        )
    if owner_exec_qemu:
        mount_point = "    reopen /dev/ser1\n"
        if source.count(mount_point) != 1:
            raise RuntimeError("cannot locate QEMU mount insertion point")
        source = source.replace(mount_point, mount_point +
            "    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=8192 blk cache=256k\n"
            "    waitfor /dev/hd0t77 8\n"
            "    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/owner\n")
        launch = re.compile(r"    LD_PRELOAD=(/proc/boot/[^\n]+)\n"
                            r"    /proc/boot/dio_manager &\n")
        match = launch.search(source)
        if not match or len(launch.findall(source)) != 1:
            raise RuntimeError("cannot locate unique stock dio preload and launch")
        source = launch.sub(
            "    MHI2Q_DIO_OWNER_LOCK=/mnt/owner/owner.lock\n"
            "    MHI2Q_DIO_WIRED_PRIORITY_LOCK=/mnt/owner/priority.lock\n"
            f"    MHI2Q_OWNER_DIO_PRELOAD={match.group(1)}\n"
            "    LD_PRELOAD=\n"
            "    /proc/boot/dio_owner_exec wireless /proc/boot/owner_stock_dio_child &\n",
            source)
        gate = "    waitfor /dev/eso/dio_manager 25\n"
        if source.count(gate) != 1:
            raise RuntimeError("cannot locate unique stock dio gate")
        source = source.replace(gate, gate +
            "    /proc/boot/owner_stock_dio_probe\n")
    if owner_wrapper_qemu:
        mount_point = "    reopen /dev/ser1\n"
        if source.count(mount_point) != 1:
            raise RuntimeError("cannot locate QEMU wrapper mount point")
        source = source.replace(mount_point, mount_point +
            "    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=8192 blk cache=256k\n"
            "    waitfor /dev/hd0t77 8\n"
            "    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/owner\n")
        launch = re.compile(r"    LD_PRELOAD=(/proc/boot/[^\n]+)\n"
                            r"    /proc/boot/dio_manager &\n")
        if len(launch.findall(source)) != 1:
            raise RuntimeError("cannot locate unique stock dio launch for wrapper")
        source = launch.sub(
            "    LD_PRELOAD=\n"
            "    PATH=/proc/boot:/bin\n"
            "    /proc/boot/owner_wrapper_qemu_probe &\n", source)
        probe = "    /proc/boot/owner_wrapper_qemu_probe &\n"
        script_end = source.find("\n}\n[type=link]", source.find(probe))
        if script_end < 0:
            raise RuntimeError("cannot locate QEMU startup-script end")
        source = source[:source.find(probe) + len(probe)] + source[script_end:]
    if mdnsd:
        source = source.replace("io-pkt-v4-hc", "io-pkt-v6-hc")
        source += "\n[type=dir] /var/run/mdnsd\n"
    if receiver_control_probe:
        old = "    /proc/boot/ifconfig lo0 alias 10.0.0.16\n"
        if source.count(old) != 1:
            raise RuntimeError("cannot locate QEMU script's network startup")
        source = source.replace(
            old,
            old +
            "    /proc/boot/receiver-control-qemu-probe &\n",
            1,
        )
    if independent_srp_qemu:
        old = "    /proc/boot/ifconfig lo0 alias 10.0.0.16\n"
        if source.count(old) != 1:
            raise RuntimeError("cannot locate independent SRP QEMU startup point")
        source = source.replace(old, old +
            "    /proc/boot/independent-srp-qemu &\n", 1)
    if stock_mfi_probe_qemu:
        old = "    /proc/boot/ifconfig lo0 alias 10.0.0.16\n"
        if source.count(old) != 1:
            raise RuntimeError("cannot locate stock MFi QEMU startup point")
        source = source.replace(old, old +
            "    MHI2Q_STOCK_MFI_QEMU=1\n"
            "    LD_PRELOAD=/proc/boot/libmock_mfi_qemu.so\n"
            "    /proc/boot/stock-mfi-qemu-probe &\n"
            "    LD_PRELOAD=\n", 1)
    if screen_data_qemu and not owner_wrapper_qemu:
        launch = "    /proc/boot/dio_manager &\n"
        if source.count(launch) != 1:
            raise RuntimeError("cannot locate unique dio_manager launch for asynchronous fake OMX")
        source = source.replace(launch,
            "    FAKEOMX_ASYNC_FILL=1 /proc/boot/dio_manager &\n", 1)
    if daemon_phone_qemu:
        old = "    /proc/boot/ifconfig lo0 alias 10.0.0.16\n"
        if source.count(old) != 1:
            raise RuntimeError("cannot locate QEMU script's network startup")
        daemon_start = """    /proc/boot/pipe &
    /proc/boot/cp -p /proc/boot/fake_uaputl /dev/shmem/fake_uaputl
    MHI2Q_CP_IE_INDEX=2
    MHI2Q_CP_UAPUTL=/dev/shmem/fake_uaputl
    MHI2Q_CP_MFI_PATH=/dev/shmem/qemu_mfi
    MHI2Q_CP_BT_MAC=00:01:02:03:04:05
    MHI2Q_CP_MODEL=MHI2Q
    MHI2Q_CP_SERIAL=QEMU
    MHI2Q_CP_HARDWARE=MHI2Q
    MHI2Q_CP_SSID=QemuCarPlay
    MHI2Q_CP_PASSPHRASE=testpassword
    MHI2Q_CP_CHANNEL=11
    MHI2Q_CP_IP_ADDRESS=127.0.0.1
    MHI2Q_CP_AIRPLAY_PORT=5000
    MHI2Q_CP_DEVICE_ID=02:00:00:00:00:01
    MHI2Q_CP_SOURCE_VERSION=210.81
    LD_PRELOAD=/proc/boot/libmock_mfi_qemu.so
    /proc/boot/mhi2q-wireless-daemon &
    LD_PRELOAD=
    MHI2Q_PHONE_QEMU_REAL_RECEIVER=1
    /proc/boot/phone-qemu-probe &
"""
        if daemon_control_qemu:
            daemon_start = daemon_start.replace(
                "    LD_PRELOAD=/proc/boot/libmock_mfi_qemu.so\n",
                "    MHI2Q_PHONE_QEMU_CONTROL=1\n"
                "    LD_PRELOAD=/proc/boot/libmock_mfi_qemu.so\n",
            )
        if airplay280_qemu:
            daemon_start = daemon_start.replace(
                "    MHI2Q_CP_AIRPLAY_PORT=5000\n",
                "    MHI2Q_CP_AIRPLAY_PORT=7000\n"
                "    MHI2Q_PHONE_QEMU_AIRPLAY_PORT=7000\n"
                "    MHI2Q_PHONE_QEMU_SOURCE_VERSION=280.33.8\n",
            ).replace(
                "    MHI2Q_CP_SOURCE_VERSION=210.81\n",
                "    MHI2Q_CP_SOURCE_VERSION=280.33.8\n    MHI2Q_CP_RECEIVER=experimental280\n",
            )
        if owner_supervisor_qemu:
            daemon_start = daemon_start.replace(
                "    MHI2Q_CP_IE_INDEX=2\n",
                "    MHI2Q_QEMU_NO_AP_RESET=1\n"
                "    MHI2Q_CP_IE_INDEX=2\n",
                1,
            )
            daemon_start = daemon_start.replace(
                "    LD_PRELOAD=/proc/boot/libmock_mfi_qemu.so\n"
                "    /proc/boot/mhi2q-wireless-daemon &\n"
                "    LD_PRELOAD=\n",
                "    MHI2Q_CP_DAEMON_PRELOAD=/proc/boot/libmock_mfi_qemu.so\n"
                "    MHI2Q_OWNER_SUPERVISOR_QEMU=1\n",
            )
        source = source.replace(old, old + daemon_start, 1)
    result = []
    for line in source.splitlines():
        if line.startswith("/proc/boot/libdioprobe.so="):
            result.append(
                "/proc/boot/libdio_capture_probe.so=/work/libdio_capture_probe.so"
            )
            if existing_hook_qemu or owner_wrapper_qemu:
                result.append("/proc/boot/libcarplay_hook.so=/work/libcarplay_hook.so")
                result.append(
                    "/proc/boot/libz.so.2=/src/Firmwares/HU/"
                    "MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                    "advanced/MU1316-appimg/armle/usr/lib/libz.so.2"
                )
            if start_experiment:
                result.append("/proc/boot/libdioprobe.so=/work/extracted/proc/boot/libdioprobe.so")
            continue
        if line.startswith("/proc/boot/libtrackf_stock_spawn.so="):
            continue
        if feature37 and line.startswith("/proc/boot/libairplay.so="):
            result.append("/proc/boot/libairplay.so=/work/libairplay.feature37.so")
            continue
        if airplay280_qemu and line.startswith("/proc/boot/libairplay.so="):
            result.append("/proc/boot/libairplay.so=/work/libairplay280.so")
            continue
        if owner_wrapper_qemu and not active_owner_wired_qemu and \
                line.startswith("/proc/boot/libc.so.3="):
            result.append(
                "/proc/boot/libc.so.3=/src/Firmwares/HU/"
                "MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                "advanced/MU1316-appimg/armle/lib/libc.so.3"
            )
            continue
        if mdnsd and line.startswith("/proc/boot/io-pkt-v6-hc="):
            result.append(
                "/proc/boot/io-pkt-v6-hc="
                "/opt/qnx650/target/qnx6/armle-v7/sbin/io-pkt-v6-hc"
            )
            continue
        if line.startswith("/") and "=" in line:
            destination = line.split("=", 1)[0]
            line = f"{destination}=/work/extracted{destination}"
        result.append(line)
        if owner_exec_qemu and line.startswith("/proc/boot/dio_manager="):
            result.extend((
                "[perms=+r,+x]",
                "/proc/boot/dio_owner_exec=/work/dio_owner_exec",
                "/proc/boot/owner_stock_dio_child=/work/owner_stock_dio_child",
                "/proc/boot/owner_stock_dio_probe=/work/owner_stock_dio_probe",
            ))
        if owner_wrapper_qemu and line.startswith("/proc/boot/dio_manager="):
            receiver_source = ("/work/libairplay.feature37.so" if feature37 else
                               "/work/libairplay280.so" if airplay280_qemu else
                               "/work/extracted/proc/boot/libairplay.so")
            result.extend((
                "[perms=+r,+x]",
                f"/mnt/app/eso/lib/libairplay.so={receiver_source}",
                "/proc/boot/dio_owner_exec=/work/dio_owner_exec",
                "/proc/boot/owner_wrapper_qemu_probe=/work/owner_wrapper_qemu_probe",
                "/proc/boot/monitor_stub=/work/owner_wrapper_monitor_stub",
                "/proc/boot/carplay_startup_source=/work/carplay_startup.sh",
                "/proc/boot/libdio_receiver_bridge.so=/work/libdio_receiver_bridge.so",
                "/proc/boot/sh=/src/Firmwares/HU/"
                "MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                "advanced/MU1316-appimg/armle/bin/sh",
                "/proc/boot/mv=/src/Firmwares/HU/"
                "MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                "rcc_extracted/ifs2_out/bin/mv",
                "/proc/boot/rm=/opt/qnx650/target/qnx6/armle-v7/bin/rm",
                ("/bin/sh=/src/Firmwares/HU/"
                 "MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                 "advanced/MU1316-appimg/armle/bin/sh"
                 if active_owner_supervisor_qemu else
                 "[type=link] /bin/sh=/proc/boot/sh"),
            ))
            if owner_supervisor_qemu or active_owner_supervisor_qemu:
                result.append("/proc/boot/wireless_dio_supervisor=/work/wireless_dio_supervisor")
            if active_owner_supervisor_qemu:
                result.append("/proc/boot/mhi2q-wireless-daemon=/work/mhi2q-wireless-daemon")
        if receiver_control_probe and line.startswith("/proc/boot/ifconfig="):
            result.extend((
                "[perms=+r,+x]",
                "/proc/boot/receiver-control-qemu-probe="
                "/work/receiver-control-qemu-probe",
            ))
        if daemon_phone_qemu and line.startswith("/proc/boot/ifconfig="):
            additions = [
                "[perms=+r,+x]",
                "/proc/boot/pipe="
                "/opt/qnx650/target/qnx6/armle-v7/sbin/pipe",
                "/proc/boot/cp=/work/cp",
                "/proc/boot/fake_uaputl=/work/fake_uaputl",
                "/proc/boot/libmock_mfi_qemu.so=/work/libmock_mfi_qemu.so",
                "/proc/boot/mhi2q-wireless-daemon=/work/mhi2q-wireless-daemon",
                "/proc/boot/phone-qemu-probe=/work/phone-qemu-probe",
            ]
            if daemon_control_qemu:
                additions.append("/proc/boot/libqemu_dns_sd.so=/work/libqemu_dns_sd.so")
            result.extend(additions)
        if independent_srp_qemu and line.startswith("/proc/boot/ifconfig="):
            result.extend((
                "[perms=+r,+x]",
                "/proc/boot/independent-srp-qemu=/work/independent-srp-qemu",
                "/proc/boot/libcrypto.so.1=/opt/qnx650/target/qnx6/"
                "armle-v7/usr/lib/libcrypto.so.1",
            ))
            if active_owner_wired_qemu:
                result.append("/proc/boot/wired-confirm-qemu=/work/wired-confirm-qemu")
        if stock_mfi_probe_qemu and line.startswith("/proc/boot/ifconfig="):
            result.extend((
                "[perms=+r,+x]",
                "/proc/boot/stock-mfi-qemu-probe=/work/stock-mfi-qemu-probe",
                "/proc/boot/libmock_mfi_qemu.so=/work/libmock_mfi_qemu.so",
                "/proc/boot/libairplay.stock.so=/src/Firmwares/HU/"
                "MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                "advanced/MU1316-appimg/eso/lib/libairplay.so",
            ))
    if owner_exec_qemu or owner_wrapper_qemu:
        result.append("[type=dir] /mnt/owner")
    (work / "capture.build").write_text("\n".join(result) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mesh", action="store_true",
                        help="use the broker/servicemgr stock-dio mesh IFS")
    parser.add_argument("--start-experiment", action="store_true",
                        help="QEMU only: post the stock StartAirPlay event on captured manager")
    parser.add_argument("--seed-display", action="store_true",
                        help="QEMU A/B only: seed four manager display words to 800x480")
    parser.add_argument("--seed-bt-ids-qemu", action="store_true",
                        help="QEMU only: copy one valid Bluetooth ID through stock DSI list helper")
    parser.add_argument("--bt-property-trace-qemu", action="store_true",
                        help="QEMU only: record manager Bluetooth vector around server property callbacks")
    parser.add_argument("--bt-property-direct-qemu", action="store_true",
                        help="QEMU A/B only: call stock property builder directly, bypass shared result slots")
    parser.add_argument("--bt-property-reset-qemu", action="store_true",
                        help="QEMU A/B only: serialize stock callbacks and clear their shared signal before post/wait")
    parser.add_argument("--bt-property-handoff-qemu", action="store_true",
                        help="QEMU A/B only: transfer the worker's returned CF object out of the manager slot")
    parser.add_argument("--property-handoff-production-qemu", action="store_true",
                        help="QEMU only: link the minimal production property-handoff module")
    parser.add_argument("--property-handoff-force-guard-fail-qemu", action="store_true",
                        help="QEMU only: force that module's image guard to reject installation")
    parser.add_argument("--property-handoff-race-qemu", action="store_true",
                        help="QEMU only: overlap a copy-property callback with bridge STOP and manager teardown")
    parser.add_argument("--property-handoff-quiesce-qemu", action="store_true",
                        help="QEMU only: drain a live callback with QUIESCE before manager teardown")
    parser.add_argument("--active-session-quiesce-qemu", action="store_true",
                        help="QEMU only: QUIESCE while encrypted screen and PCM session is active")
    parser.add_argument("--active-wired-drain-qemu", action="store_true",
                        help="QEMU A/B: SDK sessionDied during active media, then QUIESCE")
    parser.add_argument("--active-owner-wired-qemu", action="store_true",
                        help="QEMU: exact guarded wired owner takes active-media dio lock")
    parser.add_argument("--active-owner-supervisor-qemu", action="store_true",
                        help="QEMU: native supervisor restarts wireless after active-media USB release")
    parser.add_argument("--wireless-lifecycle-qemu", action="store_true",
                        help="stock210 hook + native supervisor + iAP2 + two media generations + USB handoff")
    parser.add_argument("--wireless-active-stop-qemu", action="store_true",
                        help="real stock210 active media: TERM/QUIESCE refusal and explicit recovery")
    parser.add_argument("--repeat-media-qemu", action="store_true",
                        help="QEMU only: two encrypted screen/PCM sessions on one receiver, then QUIESCE")
    parser.add_argument("--repeat-media-keep-server-qemu", action="store_true",
                        help="QEMU A/B: keep receiver server running between media sessions")
    parser.add_argument("--hold-stock-runloop-qemu", action="store_true",
                        help="QEMU A/B: suppress stock CFRunLoopStop from 0x15897c")
    parser.add_argument("--screen-abrupt-close-qemu", action="store_true",
                        help="QEMU only: reset the control socket during screen/audio without TEARDOWN")
    parser.add_argument("--screen-rst-delay-qemu", action="store_true",
                        help="QEMU only: leave timing UDP closed for 3 seconds before control RST")
    parser.add_argument("--property-handoff-force-busy-teardown-qemu", action="store_true",
                        help="QEMU only: reproduce unsafe teardown after a BUSY STOP")
    parser.add_argument("--skip-hid-qemu", action="store_true",
                        help="QEMU A/B only: bypass the virtual HID branch that blocks in the emulator")
    parser.add_argument("--trackf-gate-qemu", action="store_true",
                        help="QEMU A/B only: use Track-F getifaddrs failure after start")
    parser.add_argument("--feature37", action="store_true",
                        help="QEMU only: stage the exact one-byte CarPlay feature patch and inspect /info")
    parser.add_argument("--airplay280-qemu", action="store_true",
                        help="QEMU only: stage existing from-source 280 pairing candidate")
    parser.add_argument("--airplay280-image", type=Path,
                        help="QEMU only: use an explicitly built AirPlay 280 image")
    parser.add_argument("--dump-txt-qemu", action="store_true",
                        help="QEMU only: compare stock AirPlay Bonjour TXT with /info")
    parser.add_argument("--http-delegate-qemu", action="store_true",
                        help="QEMU only: observe stock 210 HTTPServerCreate interposition")
    parser.add_argument("--mdnsd", action="store_true",
                        help="QEMU only: stage and run stock firmware mdnsd")
    parser.add_argument("--mock-mac-qemu", action="store_true",
                        help="QEMU A/B only: provide a synthetic primary MAC to stock libairplay")
    parser.add_argument("--interface-lo0-qemu", action="store_true",
                        help="QEMU A/B only: set dio's interfaceName property to lo0")
    parser.add_argument("--restart-qemu", action="store_true",
                        help="QEMU only: stop the first stock AirPlay thread, then post StartAirPlay again")
    parser.add_argument("--stop-only-qemu", action="store_true",
                        help="QEMU only: stop the first stock AirPlay thread without a second START")
    parser.add_argument("--soft-restart-qemu", action="store_true",
                        help="QEMU only: send direct stopServer/startServer to the captured receiver")
    parser.add_argument("--bridge-qemu", action="store_true",
                        help="QEMU only: exercise two localhost START/STOP generations")
    parser.add_argument("--bridge-first-start-qemu", action="store_true",
                        help="QEMU only: let the bridge post the initial StartAirPlay event")
    parser.add_argument("--bridge-teardown-qemu", action="store_true",
                        help="QEMU only: exercise retained-server release on stock manager shutdown")
    parser.add_argument("--receiver-control-probe", action="store_true",
                        help="QEMU only: run the actual QNX/Rust START/STOP client against stock dio")
    parser.add_argument("--daemon-phone-qemu", action="store_true",
                        help="QEMU only: ordinary daemon and synthetic phone drive stock dio receiver")
    parser.add_argument("--software-mfi-qemu", action="store_true",
                        help="QEMU only: software MFi public vectors and actual QNX RSA verification")
    parser.add_argument("--daemon-control-qemu", action="store_true",
                        help="QEMU only: also exercise daemon DNS-SD and CarPlay Control HTTP")
    parser.add_argument("--stock-mfi-probe-qemu", action="store_true",
                        help="QEMU only: call exact stock MFi exports with a synthetic I2C chip")
    parser.add_argument("--existing-hook-qemu", action="store_true",
                        help="QEMU only: co-load the existing wired CarPlay hook in stock dio")
    parser.add_argument("--wireless-media-qemu", action="store_true", help="test stock210 hook media adapters")
    parser.add_argument("--wireless-hook-qemu", action="store_true",
                        help="QEMU only: stock 210 plus hook/wireless pairing and encrypted HTTP")
    parser.add_argument("--owner-exec-qemu", action="store_true",
                        help="QEMU only: launch stock dio through owner helper and check inherited lock")
    parser.add_argument("--owner-wrapper-qemu", action="store_true",
                        help="QEMU only: execute exact generated shell wrapper into stock dio")
    parser.add_argument("--owner-supervisor-qemu", action="store_true",
                        help="QEMU only: supervise exact wrapper and full synthetic phone session")
    parser.add_argument("--build-only", action="store_true",
                        help="save the rebuilt IFS in out/ for QEMU debugging")
    parser.add_argument("--independent-srp-qemu", action="store_true",
                        help="QEMU only: independent pairing and encrypted /info")
    parser.add_argument("--independent-setup-qemu", action="store_true",
                        help="QEMU only: after independent pairing, probe control-only SETUP lifecycle")
    parser.add_argument("--independent-screen-setup-qemu", action="store_true",
                        help="QEMU only: request screen stream 110 in independent SETUP")
    parser.add_argument("--screen-linger-qemu", action="store_true",
                        help="QEMU only: keep verified client socket open two seconds after screen TEARDOWN")
    parser.add_argument("--screen-data-qemu", action="store_true",
                        help="QEMU only: connect to SETUP dataPort and send a screen KeepAlive header")
    parser.add_argument("--screen-frame-qemu", action="store_true",
                        help="QEMU only: send AVC configuration and an encrypted AVCC frame")
    parser.add_argument("--screen-audio-qemu", action="store_true",
                        help="QEMU only: request main PCM audio alongside the screen stream")
    parser.add_argument("--trace-unicode-dtor-qemu", action="store_true",
                        help="QEMU only: trace stock UnicodeString8 destructor calls")
    parser.add_argument("--trace-sigsegv-qemu", action="store_true",
                        help="QEMU only: log ARM registers when dio_manager faults")
    options = parser.parse_args()
    if options.software_mfi_qemu and not (options.daemon_phone_qemu or options.wireless_lifecycle_qemu):
        parser.error("--software-mfi-qemu requires --daemon-phone-qemu or --wireless-lifecycle-qemu")
    if options.independent_setup_qemu and not options.independent_srp_qemu:
        parser.error("--independent-setup-qemu requires --independent-srp-qemu")
    if options.independent_screen_setup_qemu and not options.independent_setup_qemu:
        parser.error("--independent-screen-setup-qemu requires --independent-setup-qemu")
    if options.screen_linger_qemu and not options.independent_screen_setup_qemu:
        parser.error("--screen-linger-qemu requires --independent-screen-setup-qemu")
    if options.screen_data_qemu and not options.independent_screen_setup_qemu:
        parser.error("--screen-data-qemu requires --independent-screen-setup-qemu")
    if options.screen_frame_qemu and not options.screen_data_qemu:
        parser.error("--screen-frame-qemu requires --screen-data-qemu")
    if options.screen_audio_qemu and not options.screen_frame_qemu:
        parser.error("--screen-audio-qemu requires --screen-frame-qemu")
    if options.screen_abrupt_close_qemu and not (
            options.independent_srp_qemu and options.screen_audio_qemu and
            not options.screen_linger_qemu):
        parser.error("--screen-abrupt-close-qemu requires independent screen/audio and no linger")
    if options.screen_rst_delay_qemu and not options.screen_abrupt_close_qemu:
        parser.error("--screen-rst-delay-qemu requires --screen-abrupt-close-qemu")
    if options.trace_unicode_dtor_qemu and not options.independent_screen_setup_qemu:
        parser.error("--trace-unicode-dtor-qemu requires --independent-screen-setup-qemu")
    if options.trace_sigsegv_qemu and not options.independent_screen_setup_qemu:
        parser.error("--trace-sigsegv-qemu requires --independent-screen-setup-qemu")
    if options.owner_supervisor_qemu:
        if not (options.owner_wrapper_qemu and options.daemon_phone_qemu and
                options.bridge_first_start_qemu):
            parser.error("--owner-supervisor-qemu requires --owner-wrapper-qemu, "
                         "--daemon-phone-qemu, and --bridge-first-start-qemu")
    if options.start_experiment and not options.mesh:
        parser.error("--start-experiment requires --mesh")
    if options.seed_display and not options.start_experiment:
        parser.error("--seed-display requires --start-experiment")
    if options.seed_bt_ids_qemu and not options.start_experiment:
        parser.error("--seed-bt-ids-qemu requires --start-experiment")
    if options.bt_property_trace_qemu and not options.airplay280_qemu:
        parser.error("--bt-property-trace-qemu requires --airplay280-qemu")
    if options.bt_property_direct_qemu and not options.bt_property_trace_qemu:
        parser.error("--bt-property-direct-qemu requires --bt-property-trace-qemu")
    if options.bt_property_reset_qemu and not options.bt_property_trace_qemu:
        parser.error("--bt-property-reset-qemu requires --bt-property-trace-qemu")
    if options.bt_property_reset_qemu and options.bt_property_direct_qemu:
        parser.error("choose one Bluetooth property A/B mode")
    if options.bt_property_handoff_qemu and not options.bt_property_reset_qemu:
        parser.error("--bt-property-handoff-qemu requires --bt-property-reset-qemu")
    if options.property_handoff_production_qemu and (
            not options.airplay280_qemu or options.bt_property_trace_qemu):
        parser.error("--property-handoff-production-qemu requires --airplay280-qemu and no diagnostic property wrapper")
    if options.property_handoff_force_guard_fail_qemu and not options.property_handoff_production_qemu:
        parser.error("--property-handoff-force-guard-fail-qemu requires --property-handoff-production-qemu")
    if options.property_handoff_race_qemu and not (
            options.property_handoff_production_qemu and options.bridge_teardown_qemu):
        parser.error("--property-handoff-race-qemu requires production handoff and bridge teardown")
    if options.property_handoff_force_busy_teardown_qemu and not options.property_handoff_race_qemu:
        parser.error("--property-handoff-force-busy-teardown-qemu requires --property-handoff-race-qemu")
    if options.property_handoff_quiesce_qemu and not options.property_handoff_race_qemu:
        parser.error("--property-handoff-quiesce-qemu requires --property-handoff-race-qemu")
    if options.property_handoff_quiesce_qemu and options.property_handoff_force_busy_teardown_qemu:
        parser.error("choose QUIESCE or forced BUSY teardown")
    if options.active_session_quiesce_qemu and not (
            options.independent_srp_qemu and options.screen_audio_qemu and
            options.bridge_first_start_qemu and options.property_handoff_production_qemu and
            not options.bridge_teardown_qemu):
        parser.error("--active-session-quiesce-qemu requires independent screen/audio, "
                     "bridge first start, production handoff, and no bridge selftest teardown")
    if options.active_wired_drain_qemu and not (
            options.active_session_quiesce_qemu and not options.repeat_media_qemu and
            not options.screen_abrupt_close_qemu):
        parser.error("--active-wired-drain-qemu requires active-session QUIESCE without repeat or abrupt close")
    if options.active_owner_wired_qemu and not (
            options.active_session_quiesce_qemu and options.owner_wrapper_qemu and
            not options.active_wired_drain_qemu and not options.repeat_media_qemu and
            not options.screen_abrupt_close_qemu and not options.owner_supervisor_qemu):
        parser.error("--active-owner-wired-qemu requires active-session QUIESCE and owner wrapper")
    if options.active_owner_supervisor_qemu and not (
            options.active_owner_wired_qemu and not options.daemon_phone_qemu):
        parser.error("--active-owner-supervisor-qemu requires active wired owner without daemon-phone fixture")
    if options.repeat_media_qemu and not (options.active_session_quiesce_qemu and
                                          not options.screen_abrupt_close_qemu):
        parser.error("--repeat-media-qemu requires active-session QUIESCE without abrupt close")
    if options.repeat_media_keep_server_qemu and not options.repeat_media_qemu:
        parser.error("--repeat-media-keep-server-qemu requires --repeat-media-qemu")
    if options.hold_stock_runloop_qemu and not options.repeat_media_qemu:
        parser.error("--hold-stock-runloop-qemu requires --repeat-media-qemu")
    if options.repeat_media_keep_server_qemu and not options.hold_stock_runloop_qemu:
        parser.error("--repeat-media-keep-server-qemu requires --hold-stock-runloop-qemu")
    if options.skip_hid_qemu and not options.start_experiment:
        parser.error("--skip-hid-qemu requires --start-experiment")
    if options.trackf_gate_qemu and not options.start_experiment:
        parser.error("--trackf-gate-qemu requires --start-experiment")
    if options.feature37 and not (options.start_experiment and options.skip_hid_qemu):
        parser.error("--feature37 requires --start-experiment --skip-hid-qemu")
    if options.airplay280_qemu and options.feature37:
        parser.error("choose one AirPlay library variant")
    if options.airplay280_qemu and not (options.start_experiment and options.skip_hid_qemu):
        parser.error("--airplay280-qemu requires --start-experiment --skip-hid-qemu")
    if options.airplay280_image and not options.airplay280_qemu:
        parser.error("--airplay280-image requires --airplay280-qemu")
    if options.independent_srp_qemu and not (options.airplay280_image and
                                              options.mdnsd and
                                              (not options.bridge_qemu or
                                               options.active_session_quiesce_qemu)):
        parser.error("--independent-srp-qemu requires --airplay280-image --mdnsd; "
                     "bridge only with --active-session-quiesce-qemu")
    if options.independent_srp_qemu and options.dump_txt_qemu:
        parser.error("--independent-srp-qemu uses short readiness markers; run --dump-txt-qemu separately")
    if options.dump_txt_qemu and not ((options.feature37 or options.airplay280_qemu) and options.mdnsd and
                                      options.mock_mac_qemu and options.interface_lo0_qemu):
        parser.error("--dump-txt-qemu requires --feature37 or --airplay280-qemu, --mdnsd "
                     "--mock-mac-qemu --interface-lo0-qemu")
    if options.dump_txt_qemu and (options.bridge_qemu or options.bridge_first_start_qemu):
        parser.error("--dump-txt-qemu uses the capture-only Bonjour interposer")
    if options.mdnsd and not options.start_experiment:
        parser.error("--mdnsd requires --start-experiment")
    if options.mock_mac_qemu and not options.mdnsd:
        parser.error("--mock-mac-qemu requires --mdnsd")
    if options.interface_lo0_qemu and not options.mock_mac_qemu:
        parser.error("--interface-lo0-qemu requires --mock-mac-qemu")
    if options.restart_qemu and not (options.start_experiment and options.skip_hid_qemu):
        parser.error("--restart-qemu requires --start-experiment --skip-hid-qemu")
    if options.stop_only_qemu and not (options.start_experiment and options.skip_hid_qemu):
        parser.error("--stop-only-qemu requires --start-experiment --skip-hid-qemu")
    if options.stop_only_qemu and options.restart_qemu:
        parser.error("--stop-only-qemu and --restart-qemu are mutually exclusive")
    if options.soft_restart_qemu and not (options.start_experiment and
                                         options.skip_hid_qemu and options.mdnsd):
        parser.error("--soft-restart-qemu requires --start-experiment --skip-hid-qemu --mdnsd")
    if options.soft_restart_qemu and (options.stop_only_qemu or options.restart_qemu):
        parser.error("--soft-restart-qemu excludes stock thread stop experiments")
    if options.bridge_qemu and not (options.start_experiment and
                                    options.skip_hid_qemu and
                                    (options.feature37 or options.airplay280_qemu) and
                                    options.mdnsd and options.mock_mac_qemu and
                                    options.interface_lo0_qemu):
        parser.error("--bridge-qemu requires the full working receiver QEMU fixture")
    if options.bridge_qemu and (options.soft_restart_qemu or
                                options.stop_only_qemu or options.restart_qemu):
        parser.error("--bridge-qemu excludes other lifecycle experiments")
    if options.bridge_first_start_qemu and not options.bridge_qemu:
        parser.error("--bridge-first-start-qemu requires --bridge-qemu")
    if options.bridge_teardown_qemu and not options.bridge_first_start_qemu:
        parser.error("--bridge-teardown-qemu requires --bridge-first-start-qemu")
    if options.receiver_control_probe and not options.bridge_first_start_qemu:
        parser.error("--receiver-control-probe requires --bridge-first-start-qemu")
    if options.receiver_control_probe and options.bridge_teardown_qemu:
        parser.error("--receiver-control-probe excludes the teardown experiment")
    if options.daemon_phone_qemu and not options.bridge_first_start_qemu:
        parser.error("--daemon-phone-qemu requires --bridge-first-start-qemu")
    if options.daemon_phone_qemu and (options.receiver_control_probe or
                                     options.bridge_teardown_qemu):
        parser.error("--daemon-phone-qemu excludes other bridge clients")
    if options.daemon_control_qemu and not options.daemon_phone_qemu:
        parser.error("--daemon-control-qemu requires --daemon-phone-qemu")
    if options.stock_mfi_probe_qemu and not (
            options.airplay280_qemu and options.start_experiment and
            options.mdnsd and options.mock_mac_qemu and
            options.interface_lo0_qemu and not options.daemon_phone_qemu):
        parser.error("--stock-mfi-probe-qemu requires ordinary AirPlay 280 QEMU fixture without daemon phone")
    if options.existing_hook_qemu and not options.bridge_first_start_qemu:
        parser.error("--existing-hook-qemu requires --bridge-first-start-qemu")
    if options.wireless_lifecycle_qemu and not options.wireless_media_qemu:
        parser.error("--wireless-lifecycle-qemu requires --wireless-media-qemu")
    if options.wireless_active_stop_qemu and not options.wireless_lifecycle_qemu:
        parser.error("--wireless-active-stop-qemu requires --wireless-lifecycle-qemu")
    if options.wireless_media_qemu and not options.wireless_hook_qemu:
        parser.error("--wireless-media-qemu requires --wireless-hook-qemu")
    if options.wireless_hook_qemu:
        if not (options.mesh and options.start_experiment and options.mdnsd and
                options.seed_display and options.skip_hid_qemu and
                options.mock_mac_qemu and options.interface_lo0_qemu):
            parser.error("--wireless-hook-qemu requires mesh/start/mdnsd/seed-display/skip-hid/mock-mac/interface-lo0")
        if any((options.airplay280_qemu, options.feature37, options.existing_hook_qemu,
                options.bridge_qemu, options.owner_wrapper_qemu, options.owner_exec_qemu,
                options.independent_srp_qemu, options.restart_qemu, options.stop_only_qemu,
                options.soft_restart_qemu)):
            parser.error("--wireless-hook-qemu excludes replacement receivers and lifecycle experiments")
    if options.owner_exec_qemu and not options.mesh:
        parser.error("--owner-exec-qemu requires --mesh")
    if options.owner_wrapper_qemu and not options.mesh:
        parser.error("--owner-wrapper-qemu requires --mesh")
    if options.owner_wrapper_qemu and options.owner_exec_qemu:
        parser.error("choose one owner QEMU experiment")
    name = "dio_mesh_provider_twin" if options.mesh else "dio_trackF_stock_real"
    base_ifs = CARPLAY / f"{name}.bin"
    base_build = CARPLAY / f"{name}.build"
    run("python3", str(REPO / "wireless/receiver_bridge/verify_capture_point.py"))
    for source in (base_ifs, base_build, QEMU):
        if not source.exists():
            raise SystemExit(f"missing local QEMU prerequisite: {source}")
    OUT.mkdir(exist_ok=True)

    if options.receiver_control_probe:
        subprocess.run(
            ["docker", "run", "--rm", "--platform=linux/amd64",
             "-v", f"{AUDI}:/src", "-w", "/src/Patches/mhi2-carplay",
             RUST_IMAGE, "/opt/rust/qnx-cargo", "build", "-q",
             "--manifest-path",
             "/src/Patches/mhi2-carplay/wireless/daemon/Cargo.toml",
             "--features", "qemu-probe", "--bin", "receiver-control-qemu-probe"],
            check=True, capture_output=True, text=True,
        )
    if options.daemon_phone_qemu:
        subprocess.run(
            ["docker", "run", "--rm", "--platform=linux/amd64",
             "-v", f"{AUDI}:/src", "-w", "/src/Patches/mhi2-carplay",
             RUST_IMAGE, "/opt/rust/qnx-cargo", "build", "-q",
             "--manifest-path",
             "/src/Patches/mhi2-carplay/wireless/daemon/Cargo.toml",
             "--bin", "mhi2q-wireless-daemon", "--bin", "phone-qemu-probe",
             "--features", "qemu-probe"],
            check=True, capture_output=True, text=True,
        )

    with tempfile.TemporaryDirectory(prefix="mhi2q-capture-") as temp:
        work = Path(temp)
        extracted = work / "extracted"
        extracted.mkdir()
        listing = docker(work, "dumpifs", "-v", f"/src/{base_ifs.relative_to(AUDI)}",
                         capture_output=True).stdout
        for line in listing.splitlines():
            match = re.match(r"^\s+[0-9a-f]+\s+[0-9a-f]+\s+([^\s]+)", line)
            if not match:
                continue
            name = match.group(1)
            if name.startswith(("Startup-", "Image-", "Root-", "*.")) or name.endswith(".*"):
                continue
            (extracted / name).parent.mkdir(parents=True, exist_ok=True)
        docker(work, "sh", "-c", "cd /work/extracted && dumpifs -x "
               f"/src/{base_ifs.relative_to(AUDI)}")
        for filename in ("startup-virt", "libstartup.a"):
            shutil.copyfile(SDP / "tests/qemu" / filename, work / filename)
        if options.existing_hook_qemu or options.owner_wrapper_qemu:
            existing_hook = REPO / "build/unprotected/libcarplay_hook.so"
            if sha256(existing_hook.read_bytes()).hexdigest() != (
                "64d4a84888a1706e9462e30ed1fefca6b948ff1926cf01cd6d92b537e2f10c2b"
            ):
                raise SystemExit("existing wired CarPlay QEMU hook hash changed")
            shutil.copyfile(existing_hook, work / "libcarplay_hook.so")
        if options.receiver_control_probe:
            shutil.copyfile(
                REPO / "wireless/daemon/target/armv7-unknown-nto-qnx650/"
                       "release/receiver-control-qemu-probe",
                work / "receiver-control-qemu-probe",
            )
        if options.daemon_phone_qemu:
            daemon = REPO / "wireless/daemon"
            release = daemon / "target/armv7-unknown-nto-qnx650/release"
            for name in ("mhi2q-wireless-daemon", "phone-qemu-probe"):
                shutil.copyfile(release / name, work / name)
            shutil.copyfile(TOOLS / "bsp-qemu-arm/extract/cp", work / "cp")
            sources = [("fake_uaputl.c", "fake_uaputl"),
                       ("mock_mfi_qemu.c", "libmock_mfi_qemu.so")]
            if options.daemon_control_qemu:
                sources.append(("mock_dns_sd_qemu.c", "libqemu_dns_sd.so"))
            for source, name in sources:
                docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-O2",
                       "-Wall", "-Wextra", "-Werror",
                       *(["-fPIC", "-shared"] if name.endswith(".so") else []),
                       "-o", f"/work/{name}",
                       f"/src/{(daemon / 'probes' / source).relative_to(AUDI)}")
        if options.stock_mfi_probe_qemu:
            stock_airplay = AUDI / (
                "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                "advanced/MU1316-appimg/eso/lib/libairplay.so"
            )
            if sha256(stock_airplay.read_bytes()).hexdigest() != (
                    "01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2"):
                raise SystemExit("stock MFi library hash mismatch")
            for source, output in (("stock_mfi_qemu_probe.c", "stock-mfi-qemu-probe"),
                                   ("mock_mfi_qemu.c", "libmock_mfi_qemu.so")):
                docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=gnu99",
                       "-O2", "-Wall", "-Wextra", "-Werror",
                       *(["-fPIC", "-shared"] if output.endswith(".so") else []),
                       "-o", f"/work/{output}",
                       f"/src/{(REPO / 'wireless/daemon/probes' / source).relative_to(AUDI)}")
        if options.independent_srp_qemu:
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=gnu99",
                   "-O2", "-Wall", "-Wextra", "-Werror",
                   *(["-DMHI2Q_INDEPENDENT_CONTROL_SETUP"] if options.independent_setup_qemu else []),
                   *(["-DMHI2Q_INDEPENDENT_SCREEN_SETUP"] if options.independent_screen_setup_qemu else []),
                   *(["-DMHI2Q_SCREEN_LINGER_QEMU"] if options.screen_linger_qemu else []),
                   *(["-DMHI2Q_SCREEN_DATA_QEMU"] if options.screen_data_qemu else []),
                   *(["-DMHI2Q_SCREEN_FRAME_QEMU"] if options.screen_frame_qemu else []),
                   *(["-DMHI2Q_SCREEN_AUDIO_QEMU"] if options.screen_audio_qemu else []),
                   *(["-DMHI2Q_ACTIVE_SESSION_QUIESCE_QEMU"]
                     if options.active_session_quiesce_qemu else []),
                   *(["-DMHI2Q_ACTIVE_WIRED_DRAIN_QEMU"]
                     if options.active_wired_drain_qemu else []),
                   *(["-DMHI2Q_ACTIVE_OWNER_WIRED_QEMU"]
                     if options.active_owner_wired_qemu else []),
                   *(["-DMHI2Q_REPEAT_MEDIA_QEMU"] if options.repeat_media_qemu else []),
                   *(["-DMHI2Q_REPEAT_MEDIA_KEEP_SERVER_QEMU"]
                     if options.repeat_media_keep_server_qemu else []),
                   *(["-DMHI2Q_SCREEN_ABRUPT_CLOSE_QEMU"]
                     if options.screen_abrupt_close_qemu else []),
                   *(["-DMHI2Q_SCREEN_RST_DELAY_QEMU"]
                     if options.screen_rst_delay_qemu else []),
                   "-o", "/work/independent-srp-qemu",
                   f"/src/{(REPO / 'tests/carplay_lab/qnx/independent_srp_qemu.c').relative_to(AUDI)}",
                   "-lcrypto", "-lsocket")
        if options.owner_exec_qemu:
            run("python3", str(REPO / "wireless/integration/build_owner_offline.py"),
                capture_output=True)
            shutil.copyfile(REPO / "wireless/integration/out/dio_owner_exec.offline",
                            work / "dio_owner_exec")
            for name in ("owner_stock_dio_child", "owner_stock_dio_probe"):
                docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-Os",
                       "-Wall", "-Wextra", "-Werror", "-o", f"/work/{name}",
                       f"/src/Patches/mhi2-carplay/wireless/integration/{name}.c")
        if options.owner_wrapper_qemu:
            owner_build = ["python3", str(REPO / "wireless/integration/build_owner_offline.py")]
            if options.active_owner_wired_qemu:
                owner_build.append("--property-handoff")
            run(*owner_build, capture_output=True)
            if not options.active_owner_wired_qemu:
                run("python3", str(REPO / "wireless/receiver_bridge/build_offline.py"),
                    capture_output=True)
            shutil.copyfile(REPO / "wireless/integration/out" / (
                "dio_owner_exec.handoff.offline" if options.active_owner_wired_qemu
                else "dio_owner_exec.offline"),
                            work / "dio_owner_exec")
            shutil.copyfile(REPO / "wireless/integration/out" / (
                "carplay_startup.handoff.offline.sh" if options.active_owner_wired_qemu
                else "carplay_startup.wireless.offline.sh"),
                            work / "carplay_startup.sh")
            if options.active_owner_supervisor_qemu:
                wrapper = work / "carplay_startup.sh"
                data = wrapper.read_bytes()
                if not data.startswith(b"#!/bin/sh\n"):
                    raise RuntimeError("QEMU wrapper has unexpected shell directive")
                wrapper.write_bytes(b"#!/mnt/owner/sh\n" + data[len(b"#!/bin/sh\n"):])
            if not options.active_owner_wired_qemu:
                shutil.copyfile(REPO / "wireless/receiver_bridge/out/libdio_receiver_bridge.offline.so",
                                work / "libdio_receiver_bridge.so")
            if options.owner_supervisor_qemu or options.active_owner_supervisor_qemu:
                docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-Os",
                       "-Wall", "-Wextra", "-Werror",
                       *(["-DMHI2Q_REQUIRE_QUIESCE_ALWAYS"]
                         if options.active_owner_supervisor_qemu else []),
                       *(["-DMHI2Q_QEMU_SUPERVISOR_TRACE"]
                         if options.active_owner_supervisor_qemu else []),
                       "-o", "/work/wireless_dio_supervisor",
                       "/src/Patches/mhi2-carplay/wireless/integration/wireless_dio_supervisor.c",
                       "-lsocket")
            if options.active_owner_supervisor_qemu:
                docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-Os",
                       "-Wall", "-Wextra", "-Werror", "-o", "/work/mhi2q-wireless-daemon",
                       "/src/Patches/mhi2-carplay/wireless/integration/supervisor_idle_daemon_qemu.c")
            for name in ("owner_wrapper_qemu_probe", "owner_wrapper_monitor_stub"):
                command = ["arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-Os",
                           "-Wall", "-Wextra", "-Werror"]
                if options.owner_supervisor_qemu and name == "owner_wrapper_qemu_probe":
                    command.append("-DMHI2Q_OWNER_SUPERVISOR_QEMU")
                if options.active_owner_wired_qemu and name == "owner_wrapper_qemu_probe":
                    command.append("-DMHI2Q_OWNER_ACTIVE_WIRED_QEMU")
                if options.active_owner_supervisor_qemu and name == "owner_wrapper_qemu_probe":
                    command.append("-DMHI2Q_OWNER_ACTIVE_SUPERVISOR_QEMU")
                docker(work, *command, "-o", f"/work/{name}",
                       f"/src/Patches/mhi2-carplay/wireless/integration/{name}.c",
                       "-lsocket")
            if options.active_owner_wired_qemu:
                docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11",
                       "-Os", "-Wall", "-Wextra", "-Werror",
                       *(["-DMHI2Q_QEMU_WIRED_HOLD"]
                         if options.active_owner_supervisor_qemu else []),
                       "-o", "/work/wired-confirm-qemu",
                       "/src/Patches/mhi2-carplay/wireless/integration/wired_confirm_qemu.c")

        # mkifs rewrites/expands ELF headers, so pin the loaded code and data
        # bytes used by the probe rather than comparing extracted file hashes.
        image_bytes = (extracted / "proc/boot/dio_manager").read_bytes()
        firmware_bytes = (
            AUDI / "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
            "advanced/MU1316-appimg/eso/bin/apps/dio_manager"
        ).read_bytes()
        for address, size in ((0x171C10, 8), (0x171C38, 20),
                              (0x171C50, 12), (0x171C60, 20), (0x1A4848, 4)):
            offset = address - 0x100000
            if image_bytes[offset:offset + size] != firmware_bytes[offset:offset + size]:
                raise SystemExit(f"base IFS has different code/data at {address:#x}")

        compiler = [
            "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-fPIC",
            "-shared", "-Os", "-Wall", "-Wextra", "-Werror",
            "-DMHI2Q_CAPTURE_DIAGNOSTIC",
        ]
        if options.start_experiment:
            compiler.append("-DMHI2Q_CAPTURE_START_EXPERIMENT")
        if options.wireless_hook_qemu:
            compiler.append("-DMHI2Q_CAPTURE_WIRELESS_HOOK_QEMU")
        if options.wireless_lifecycle_qemu:
            compiler.extend(["-DMHI2Q_CAPTURE_BRIDGE", "-DMHI2Q_BRIDGE_STOCK210",
                "-DMHI2Q_BRIDGE_PROPERTY_HANDOFF", "-DMHI2Q_PROPERTY_HANDOFF_ALLOW_DIOPROBE_QEMU",
                "-DMHI2Q_CAPTURE_BRIDGE_DRIVES_START", "-DMHI2Q_BRIDGE_PRESTART",
                "-DMHI2Q_STOCK210_LIFECYCLE_QEMU", "-DMHI2Q_CAPTURE_QEMU_REPEAT_MEDIA",
                '-DMHI2Q_BRIDGE_INTERFACE_NAME="lo0"'])
            compiler.extend([str('/src/Patches/mhi2-carplay/wireless/receiver_bridge/'+f)
                             for f in ('bridge.c','property_handoff.c')])
        if options.wireless_media_qemu:
            compiler.extend(["-DMHI2Q_CAPTURE_WIRELESS_MEDIA_QEMU", "-DMHI2Q_CAPTURE_SCREEN_DATA_QEMU"])
        if options.seed_display and not options.wireless_lifecycle_qemu:
            compiler.append("-DMHI2Q_CAPTURE_SEED_DISPLAY")
        if options.seed_bt_ids_qemu:
            compiler.append("-DMHI2Q_CAPTURE_SEED_BT_IDS_QEMU")
        if options.bt_property_trace_qemu:
            compiler.append("-DMHI2Q_CAPTURE_BT_PROPERTY_TRACE_QEMU")
        if options.bt_property_direct_qemu:
            compiler.append("-DMHI2Q_CAPTURE_BT_PROPERTY_DIRECT_QEMU")
        if options.bt_property_reset_qemu:
            compiler.append("-DMHI2Q_CAPTURE_BT_PROPERTY_RESET_QEMU")
        if options.bt_property_handoff_qemu:
            compiler.append("-DMHI2Q_CAPTURE_BT_PROPERTY_HANDOFF_QEMU")
        if options.skip_hid_qemu:
            compiler.append("-DMHI2Q_CAPTURE_SKIP_HID_QEMU")
        if options.trackf_gate_qemu:
            compiler.append("-DMHI2Q_CAPTURE_TRACKF_GATE_QEMU")
        if (options.feature37 or options.airplay280_qemu) and not options.independent_srp_qemu:
            compiler.append("-DMHI2Q_CAPTURE_DUMP_INFO_QEMU")
        if options.airplay280_qemu:
            compiler.append("-DMHI2Q_CAPTURE_AIRPLAY280_TRACE")
            if options.trace_unicode_dtor_qemu:
                compiler.append("-DMHI2Q_CAPTURE_UNICODE_DTOR_QEMU")
            if options.trace_sigsegv_qemu:
                compiler.append("-DMHI2Q_CAPTURE_SIGSEGV_QEMU")
            if options.screen_data_qemu:
                compiler.append("-DMHI2Q_CAPTURE_SCREEN_DATA_QEMU")
            if options.screen_audio_qemu:
                compiler.append("-DMHI2Q_CAPTURE_SCREEN_AUDIO_QEMU")
            if options.airplay280_image:
                compiler.append("-DMHI2Q_CAPTURE_AIRPLAY280_ABI_FIXED")
            if options.independent_srp_qemu:
                compiler.append("-DMHI2Q_CAPTURE_INDEPENDENT_SRP_QEMU")
        if options.dump_txt_qemu:
            compiler.append("-DMHI2Q_CAPTURE_DUMP_TXT_QEMU")
        if options.http_delegate_qemu:
            compiler.append("-DMHI2Q_CAPTURE_HTTP_DELEGATE_QEMU")
        if options.mdnsd:
            compiler.append("-DMHI2Q_CAPTURE_STAGE_MDNSD_QEMU")
        if options.mock_mac_qemu:
            compiler.append("-DMHI2Q_CAPTURE_MOCK_MAC_QEMU")
        if options.interface_lo0_qemu:
            compiler.append("-DMHI2Q_CAPTURE_IFACE_LO0_QEMU")
            compiler.append('-DMHI2Q_CAPTURE_INTERFACE_NAME="lo0"')
        if options.restart_qemu:
            compiler.append("-DMHI2Q_CAPTURE_RESTART_QEMU")
        if options.stop_only_qemu:
            compiler.append("-DMHI2Q_CAPTURE_STOP_QEMU")
        if options.soft_restart_qemu:
            compiler.append("-DMHI2Q_CAPTURE_SOFT_RESTART_QEMU")
        if options.bridge_qemu:
            compiler.append("-DMHI2Q_CAPTURE_BRIDGE")
            if options.repeat_media_qemu:
                compiler.extend(("-DMHI2Q_CAPTURE_QEMU_REPEAT_MEDIA",
                                 "-DMHI2Q_BRIDGE_QEMU_REPEAT_MEDIA"))
            if options.hold_stock_runloop_qemu:
                compiler.append("-DMHI2Q_CAPTURE_QEMU_HOLD_STOCK_RUNLOOP")
            if options.property_handoff_production_qemu:
                compiler.append("-DMHI2Q_BRIDGE_PROPERTY_HANDOFF")
            if options.property_handoff_race_qemu:
                compiler.append("-DMHI2Q_BRIDGE_QEMU_PROPERTY_RACE")
            if options.property_handoff_quiesce_qemu:
                compiler.append("-DMHI2Q_BRIDGE_QEMU_QUIESCE")
            if options.active_session_quiesce_qemu:
                compiler.append("-DMHI2Q_BRIDGE_QEMU_ACTIVE_SESSION")
            if options.active_wired_drain_qemu:
                compiler.append("-DMHI2Q_BRIDGE_QEMU_UNGUARDED_DRAIN")
            if options.property_handoff_force_busy_teardown_qemu:
                compiler.append("-DMHI2Q_BRIDGE_QEMU_FORCE_BUSY_TEARDOWN")
            if options.feature37:
                compiler.append("-DMHI2Q_BRIDGE_STOCK_QEMU")
            if not (options.receiver_control_probe or options.daemon_phone_qemu or
                    options.active_session_quiesce_qemu):
                compiler.append("-DMHI2Q_BRIDGE_QEMU_SELFTEST")
            compiler.append('-DMHI2Q_BRIDGE_INTERFACE_NAME="lo0"')
        if options.bridge_first_start_qemu:
            compiler.append("-DMHI2Q_CAPTURE_BRIDGE_DRIVES_START")
            compiler.append("-DMHI2Q_BRIDGE_PRESTART")
        if options.bridge_teardown_qemu:
            compiler.append("-DMHI2Q_BRIDGE_QEMU_TEARDOWN")
        if options.existing_hook_qemu:
            compiler.append("-DMHI2Q_CAPTURE_EXISTING_HOOK_QEMU")
        compiler += [
            "-o", "/work/libdio_capture_probe.so",
            f"/src/{(REPO / 'wireless/receiver_bridge/capture_probe.c').relative_to(AUDI)}",
        ]
        if options.airplay280_qemu:
            compiler.append(f"/src/{(REPO / 'tests/carplay_lab/qnx/airplay_pairing_probe.c').relative_to(AUDI)}")
        if options.bridge_qemu:
            compiler.append(
                f"/src/{(REPO / 'wireless/receiver_bridge/bridge.c').relative_to(AUDI)}"
            )
        if options.property_handoff_production_qemu:
            compiler.append(
                f"/src/{(REPO / 'wireless/receiver_bridge/property_handoff.c').relative_to(AUDI)}"
            )
            compiler.append("-DMHI2Q_PROPERTY_HANDOFF_ALLOW_DIOPROBE_QEMU")
            if options.property_handoff_race_qemu:
                compiler.append("-DMHI2Q_PROPERTY_HANDOFF_RACE_QEMU")
            if options.property_handoff_force_guard_fail_qemu:
                compiler.append("-DMHI2Q_PROPERTY_HANDOFF_FORCE_GUARD_FAIL_QEMU")
        if options.mdnsd:
            compiler.append("/work/mdnsd_blob.S")
        if options.wireless_lifecycle_qemu:
            # The real wrapper has two preload entries. Keep capture/bridge
            # interposers in its second entry, with mesh stubs as dependencies.
            compiler += ['-Wl,--no-as-needed', '-L/work/extracted/proc/boot',
                         '-Wl,-rpath,/mnt/pairing/bin', '-l:libdioprobe.so']
        if options.start_experiment:
            compiler.append("-lsocket")
        if options.mdnsd:
            mdnsd_source = AUDI / (
                "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                "advanced/MU1316-appimg/armle/usr/sbin/mdnsd"
            )
            mdnsd = mdnsd_source.read_bytes()
            if sha256(mdnsd).hexdigest() != (
                "843581c874bf075854cb584e827117f609e8dc697ff22509d296444a413273f2"
            ):
                raise SystemExit("factory mdnsd SHA-256 mismatch")
            (work / "mdnsd_blob.S").write_text(
                ".section .rodata\n.balign 4\n"
                ".global mhi2q_mdnsd_blob\n.global mhi2q_mdnsd_blob_end\n"
                "mhi2q_mdnsd_blob:\n"
                f'.incbin "/src/{mdnsd_source.relative_to(AUDI)}"\n'
                "mhi2q_mdnsd_blob_end:\n"
            )
        docker(work, *compiler)
        if options.owner_supervisor_qemu or options.active_owner_wired_qemu:
            shutil.copyfile(work / "libdio_capture_probe.so",
                            work / "libdio_receiver_bridge.so")
        if options.feature37:
            patch = REPO / "wireless/airplay_feature_patch/out/libairplay.feature37.offline.so"
            run("python3", str(REPO / "wireless/airplay_feature_patch/build_offline.py"),
                capture_output=True)
            shutil.copyfile(patch, work / "libairplay.feature37.so")
        if options.airplay280_qemu:
            candidate = options.airplay280_image or (
                REPO.parent / "dio_asm_to_c/_legacy_attempt/libairplay-mhi2q/libairplay.dev.so"
            )
            if not options.airplay280_image and sha256(candidate.read_bytes()).hexdigest() != (
                "7c548f50765d8d54ca06e1665b52902a8086e3d888423dc83320cdffc02d6b7e"
            ):
                raise SystemExit("from-source AirPlay 280 candidate hash changed")
            shutil.copyfile(candidate, work / "libairplay280.so")
        make_manifest(work, base_build, options.start_experiment,
                      options.feature37, options.airplay280_qemu, options.mdnsd,
                      options.receiver_control_probe, options.daemon_phone_qemu,
                      options.daemon_control_qemu, options.existing_hook_qemu,
                      options.owner_exec_qemu, options.owner_wrapper_qemu,
                      options.owner_supervisor_qemu, options.active_owner_wired_qemu,
                      options.active_owner_supervisor_qemu,
                      options.independent_srp_qemu,
                      options.screen_data_qemu, options.stock_mfi_probe_qemu)
        if options.wireless_hook_qemu:
            from tests.carplay_lab.qnx.wireless_hook_fixture import prepare
            prepare(work, media=options.wireless_media_qemu, lifecycle=options.wireless_lifecycle_qemu,
                    active_stop=options.wireless_active_stop_qemu)
        software_fixture = None
        if options.software_mfi_qemu:
            import sys
            sys.path.insert(0, str(REPO / "wireless/daemon/probes"))
            from software_mfi_qemu import prepare as prepare_software_mfi
            software_fixture = prepare_software_mfi(work, work / "capture.build")
        docker(work, "mkifs", "capture.build", "capture.bin")
        if options.build_only:
            saved = OUT / "qemu_start_debug.bin"
            shutil.copyfile(work / "capture.bin", saved)
            print(f"QEMU image ready: {saved}")
            return
        log_name = ("qemu_stock_mfi_probe.log" if options.stock_mfi_probe_qemu else
                    "qemu_property_handoff_production_airplay280.log" if options.property_handoff_production_qemu else
                    "qemu_bt_property_handoff_airplay280.log" if options.bt_property_handoff_qemu else
                    "qemu_bt_property_reset_airplay280.log" if options.bt_property_reset_qemu else
                    "qemu_bt_property_direct_airplay280.log" if options.bt_property_direct_qemu else
                    "qemu_bt_property_trace_airplay280.log" if options.bt_property_trace_qemu else
                    "qemu_seed_bt_ids_airplay280.log" if options.seed_bt_ids_qemu else
                    "qemu_independent_srp_airplay280.log" if options.independent_srp_qemu else
                    "qemu_bridge_airplay280.log" if options.airplay280_qemu and options.bridge_qemu else
                    "qemu_start_airplay280_emulator.log" if options.airplay280_image else
                    "qemu_start_airplay280.log" if options.airplay280_qemu else
                    "qemu_bridge_teardown.log" if options.bridge_teardown_qemu else
                    "qemu_daemon_stock_receiver_control.log" if options.daemon_control_qemu else
                    "qemu_daemon_stock_receiver.log" if options.daemon_phone_qemu else
                    "qemu_receiver_control.log" if options.receiver_control_probe else
                    "qemu_bridge_first_start.log" if options.bridge_first_start_qemu else
                    "qemu_bridge.log" if options.bridge_qemu else
                    "qemu_soft_restart.log" if options.soft_restart_qemu else
                    "qemu_restart.log" if options.restart_qemu else
                    "qemu_stop_only.log" if options.stop_only_qemu else
                    "qemu_start_iface_lo0.log" if options.interface_lo0_qemu else
                    "qemu_start_mdns_mac.log" if options.mock_mac_qemu else
                    "qemu_start_mdnsd.log" if options.mdnsd else
                    "qemu_start_feature37.log" if options.feature37 else
                    "qemu_start_trackf_gate.log" if options.trackf_gate_qemu else
                    "qemu_start_nohid.log" if options.skip_hid_qemu else
                    "qemu_start_seeded.log" if options.seed_display else
                    "qemu_start_experiment.log" if options.start_experiment else
                    "qemu_capture_mesh.log" if options.mesh else "qemu_capture.log")
        if options.existing_hook_qemu:
            log_name = log_name.removesuffix(".log") + "_existing_hook.log"
        if options.owner_exec_qemu:
            log_name = log_name.removesuffix(".log") + "_owner_exec.log"
        if options.owner_wrapper_qemu:
            log_name = ("qemu_owner_supervisor.log" if options.owner_supervisor_qemu
                        else "qemu_owner_wrapper.log")
        if options.property_handoff_force_guard_fail_qemu:
            log_name = "qemu_property_handoff_guard_fail.log"
        if options.property_handoff_race_qemu:
            log_name = "qemu_property_handoff_race.log"
        if options.property_handoff_quiesce_qemu:
            log_name = "qemu_property_handoff_quiesce.log"
        if options.active_session_quiesce_qemu:
            log_name = ("qemu_active_owner_supervisor.log" if options.active_owner_supervisor_qemu
                        else "qemu_active_owner_wired.log" if options.active_owner_wired_qemu
                        else "qemu_active_wired_drain.log" if options.active_wired_drain_qemu
                        else "qemu_repeat_media_keep_server.log" if options.repeat_media_keep_server_qemu
                        else "qemu_repeat_media_held_runloop_start.log" if options.hold_stock_runloop_qemu
                        else "qemu_repeat_media_quiesce.log" if options.repeat_media_qemu
                        else "qemu_active_session_quiesce.log")
        if options.property_handoff_force_busy_teardown_qemu:
            log_name = "qemu_property_handoff_forced_busy_teardown.log"
        if options.wireless_hook_qemu:
            log_name = "qemu_stock210_hook_media.log" if options.wireless_media_qemu else "qemu_stock210_hook_pairing.log"
        if options.wireless_lifecycle_qemu:
            log_name = "qemu_stock210_lifecycle.log"
        if options.wireless_active_stop_qemu:
            log_name = "qemu_stock210_active_stop.log"
        if options.software_mfi_qemu:
            log_name = log_name.removesuffix(".log") + "_software_mfi.log"
        log = OUT / log_name
        with log.open("w") as stream:
            command = [
                str(QEMU), "-M", "virt", "-m", "2048", "-cpu", "cortex-a15",
                "-device", f"loader,file={work / 'capture.bin'},"
                "addr=0x40200000,force-raw=on,cpu-num=0",
                "-display", "none", "-serial", "stdio", "-monitor", "none",
            ]
            if (options.receiver_control_probe or options.daemon_phone_qemu or
                    options.owner_wrapper_qemu or options.airplay280_qemu or options.wireless_hook_qemu):
                process = subprocess.Popen(command, stdout=stream,
                                           stderr=subprocess.STDOUT)
                deadline = time.monotonic() + (420 if options.wireless_lifecycle_qemu else 240 if options.wireless_hook_qemu else 160 if options.owner_supervisor_qemu or
                                               options.active_owner_supervisor_qemu else
                                               130 if options.daemon_phone_qemu else 90)
                completion = ("[dio-bridge] QEMU property race successes=" if options.property_handoff_race_qemu
                              else "[owner-supervisor-qemu] IE sets=" if options.owner_supervisor_qemu
                              else "[owner-wrapper-qemu] exact wrapper stock dio pid=" if options.owner_wrapper_qemu
                              else "(phone-qemu-probe) terminated signo=0 code=0" if options.daemon_phone_qemu
                              else "[dio-bridge] STOP generation 2=0" if options.bridge_qemu
                              else "(receiver-control-qemu-probe) terminated signo=0 code=0")
                try:
                    while process.poll() is None and time.monotonic() < deadline:
                        current = log.read_text(errors="replace")
                        if options.wireless_hook_qemu:
                            from tests.carplay_lab.qnx.wireless_hook_fixture import unexpected_dio_signal
                            if any(marker in current for marker in (
                                    "[independent-srp-qemu] pairing failed",
                                    "ldd:FATAL: Could not preload library /proc/boot/libcarplay_hook.so")) or unexpected_dio_signal(current):
                                break
                            if options.wireless_lifecycle_qemu:
                                if options.wireless_active_stop_qemu:
                                    if '[active-stop-qemu] active media refusal and explicit recovery passed' in current:
                                        break
                                    time.sleep(0.2)
                                    continue
                                if ("[stock210-lifecycle] two media generations, USB priority, wireless resume passed" in current
                                        and "[runtime-fault-qemu] all native runtime fault cases passed" in current):
                                    break
                                if '[runtime-fault-qemu] FAIL' in current:
                                    break
                                time.sleep(0.2)
                                continue
                            if "[independent-srp-qemu] encrypted /info HTTP 200" in current and (
                                    not options.wireless_media_qemu or "[dio-capture] manager released" in current):
                                break
                        if options.property_handoff_force_busy_teardown_qemu and \
                                "(dio_manager) terminated SIG" in current:
                            break
                        if options.property_handoff_force_busy_teardown_qemu and \
                                "[dio-capture] manager released" in current:
                            time.sleep(2)
                            break
                        if options.property_handoff_production_qemu and \
                                "[dio-property] image/delegate guard failed" in current:
                            break
                        if options.stock_mfi_probe_qemu and \
                                "(stock-mfi-qemu-probe) terminated" in current:
                            break
                        if options.repeat_media_qemu and \
                                "[dio-capture] manager released" in current and \
                                "[independent-srp-qemu] second media session complete" not in current:
                            break
                        if options.repeat_media_keep_server_qemu and \
                                "[independent-srp-qemu] second media session complete" in current:
                            break
                        if options.repeat_media_qemu and \
                                "[independent-srp-qemu] pairing failed" in current:
                            break
                        if options.active_wired_drain_qemu and \
                                "[independent-srp-qemu] pairing failed" in current:
                            break
                        if options.active_owner_wired_qemu and \
                                ("[independent-srp-qemu] pairing failed" in current or
                                 ("[independent-srp-qemu] wired helper exit=0" in current and
                                  "[owner-wrapper-qemu] wireless owner exited after wired handoff" in current)):
                            break
                        if options.active_owner_supervisor_qemu and \
                                ("[independent-srp-qemu] pairing failed" in current or
                                 "[owner-supervisor-qemu] wireless generation did not restart" in current or
                                 ("[independent-srp-qemu] wired helper exit=0" in current and
                                  "[owner-supervisor-qemu] wireless generation restarted" in current and
                                  current.count("[wireless-dio] generation stopped") >= 2)):
                            break
                        if options.repeat_media_qemu and \
                                "[independent-srp-qemu] second bridge START=-1" in current:
                            break
                        if options.independent_screen_setup_qemu and \
                                ("[dio-crash-qemu]" in current or
                                 "(dio_manager) terminated SIG" in current or
                                 (("[independent-srp-qemu] second media session complete"
                                   if options.repeat_media_qemu else
                                   "[independent-srp-qemu] post-RST QUIESCE=0"
                                   if options.active_session_quiesce_qemu and options.screen_abrupt_close_qemu else
                                   "[independent-srp-qemu] post-drain QUIESCE=0"
                                   if options.active_wired_drain_qemu else
                                   "[independent-srp-qemu] post-TEARDOWN QUIESCE=0"
                                   if options.active_session_quiesce_qemu else
                                   "[independent-srp-qemu] screen abrupt close without TEARDOWN"
                                   if options.screen_abrupt_close_qemu else
                                   "[independent-srp-qemu] screen TEARDOWN status=200") in current and
                                  "[dio-capture] manager released" in current and
                                  "<DIO_MAIN> Shutdown signaled: 1" in current and
                                  (not options.screen_linger_qemu or
                                   "[independent-srp-qemu] screen linger complete" in current))):
                            break
                        if options.independent_srp_qemu and not options.independent_screen_setup_qemu and \
                                "[independent-srp-qemu] encrypted /info HTTP 200" in current and \
                                "[pair-qemu] encrypted /info result=0" in current:
                            break
                        if options.airplay280_qemu and not options.bridge_qemu and not options.independent_screen_setup_qemu and (
                            "(dio_manager) terminated SIG" in current or
                            ("[dio-capture] GET /info " +
                             ("7000" if options.airplay280_qemu else "5000") +
                             " HTTP 200" in current and
                             "[dio-capture] bonjour-txt-hex=" in current and
                             "[dio-capture] pair-setup-m1-hex=" in current and
                             ("[pair-qemu] encrypted /info result=" in current or
                              ("[pair-qemu] /pair-setup result=" in current and
                               "[pair-qemu] /pair-setup result=0 done=1" not in current)) and
                             (not options.independent_srp_qemu or
                              "[independent-srp-qemu] encrypted /info HTTP 200" in current) and
                             (not options.stock_mfi_probe_qemu or
                              "[stock-mfi-qemu] signature=20 synthetic" in current))
                        ):
                            break
                        if (not options.independent_srp_qemu and completion in current) or (options.owner_wrapper_qemu and
                            ("[owner-wrapper-qemu] stock dio or bridge not ready" in current or
                             "[owner-wrapper-qemu] installed receiver path not selected" in current)):
                            break
                        time.sleep(0.25)
                finally:
                    if process.poll() is None:
                        process.terminate()
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
            else:
                try:
                    subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT,
                                   timeout=60 if options.start_experiment else
                                   50 if options.mesh else 25, check=False)
                except subprocess.TimeoutExpired:
                    pass  # The IFS returns to an idle QNX shell after dio exits.
        output = log.read_text(errors="replace")
        if software_fixture is not None:
            from software_mfi_qemu import validate as validate_software_mfi
            validate_software_mfi(output, log, software_fixture,
                                  sessions=3 if options.wireless_lifecycle_qemu and not options.wireless_active_stop_qemu else 1)
        if options.wireless_hook_qemu:
            from tests.carplay_lab.qnx.wireless_hook_fixture import validate
            validate(output, log, media=options.wireless_media_qemu, lifecycle=options.wireless_lifecycle_qemu,
                     active_stop=options.wireless_active_stop_qemu)
            return
        if options.stock_mfi_probe_qemu:
            required = ("[stock-mfi-qemu] I2C mock preloaded",
                        "[stock-mfi-qemu] intercepted open64",
                        "[mfi-qemu] combined SENDRECV command=0xc0140507",
                        "[stock-mfi-qemu] certificate=4 CERT",
                        "[stock-mfi-qemu] signature=20 synthetic",
                        "(stock-mfi-qemu-probe) terminated signo=0 code=0")
            missing = [marker for marker in required if marker not in output]
            if missing or "(stock-mfi-qemu-probe) terminated SIG" in output:
                raise SystemExit(f"stock MFi QEMU ABI probe failed, missing={missing}; log: {log}")
            print(f"Exact MU1316 stock MFi exports passed with synthetic I2C; log: {log}")
            return
        if options.airplay280_qemu:
            crashed = re.search(r"Process \d+ \(dio_manager\) terminated SIG\w+[^\r\n]*", output)
            if options.property_handoff_force_busy_teardown_qemu:
                if ("[dio-bridge] QEMU property race callback active=1" not in output or
                        "[dio-bridge] QEMU property race first STOP=3" not in output or
                        "[dio-bridge] QEMU forcing teardown after BUSY=1" not in output):
                    raise SystemExit(f"unsafe BUSY-teardown repro did not reach overlap; log: {log}")
                outcome = "crashed" if crashed else "did not crash in this run"
                print(f"unsafe BUSY-teardown overlap {outcome}; log: {log}")
                return
            if crashed:
                raise SystemExit(f"AirPlay 280 QEMU candidate crashed: {crashed.group(0)}; log: {log}")
        if options.property_handoff_force_guard_fail_qemu:
            if ("[dio-property] image/delegate guard failed" not in output or
                    "[dio-property] handoff installed" in output):
                raise SystemExit(f"property handoff image guard did not reject installation; log: {log}")
            print(f"production property handoff rejected by forced image guard; log: {log}")
            return
        if options.property_handoff_production_qemu and (
                "[dio-property] handoff installed" not in output or
                "[dio-property] image/delegate guard failed" in output or
                "[dio-property] handoff slot mismatch" in output or
                "CFLite.c:" in output or
                "[cfrelease-qemu]" in output):
            raise SystemExit(f"production property handoff QEMU check failed; log: {log}")
        if options.property_handoff_quiesce_qemu and (
                "[dio-bridge] QEMU property race callback active=1" not in output or
                "[dio-bridge] QEMU property race QUIESCE=0" not in output or
                "[dio-bridge] QEMU START after QUIESCE=1" not in output or
                "[dio-capture] manager released" not in output):
            raise SystemExit(f"copy-property QUIESCE teardown sequence failed; log: {log}")
        if options.property_handoff_race_qemu and not options.property_handoff_quiesce_qemu and (
                "[dio-bridge] QEMU property race callback active=1" not in output or
                "[dio-bridge] QEMU property race first STOP=3" not in output or
                "[dio-bridge] STOP generation 2=0" not in output or
                "[dio-capture] manager released" not in output):
            raise SystemExit(f"copy-property BUSY/retry teardown sequence failed; log: {log}")
        expected_bt_marker = ("[dio-btprop] direct stock value builder active"
                              if options.bt_property_direct_qemu else
                              "[dio-btprop] stock signal-reset ownership handoff active"
                              if options.bt_property_handoff_qemu else
                              "[dio-btprop] stock signal-reset gate active"
                              if options.bt_property_reset_qemu else
                              "[dio-btprop] stock server callback wrapped")
        if options.bt_property_trace_qemu and (
                expected_bt_marker not in output or
                "[dio-btprop] key=" not in output):
            raise SystemExit(f"Bluetooth property trace was not active; log: {log}")
        if options.bt_property_handoff_qemu and (
                "[dio-btprop] handoff slot mismatch" in output or
                'CFLite.c:465 "CFLRelease"' in output):
            raise SystemExit(f"Bluetooth property ownership handoff failed; log: {log}")
        if options.bt_property_reset_qemu and not options.bt_property_handoff_qemu and \
                'CFLite.c:465 "CFLRelease"' in output:
            raise SystemExit(f"Bluetooth property signal reset left a stale CF slot; log: {log}")
        if options.owner_wrapper_qemu:
            match = re.search(r"\[owner-wrapper-qemu\] exact wrapper stock dio pid=(\d+) lock=1 bridge=1", output)
            stock = re.search(r"DIOManagerApp with pid=(\d+)", output)
            if not match or not stock or match.group(1) != stock.group(1) or \
                    "[dio-bridge] listening=57343" not in output or \
                    not re.search(r"\[owner-wrapper-qemu\] receiver mapped path=/?mnt/app/eso/lib/libairplay\.so", output):
                raise SystemExit(f"exact wrapper stock dio QEMU check failed; log: {log}")
            if options.active_owner_supervisor_qemu:
                sequence = (
                    "[independent-srp-qemu] encrypted PCM RTP packets=80",
                    "[independent-srp-qemu] unowned wired drain=1",
                    "[dio-bridge] wired drain begin=1",
                    "[dio-bridge] wired drain result=0",
                    "[dio-bridge] quiesce idle result=0",
                    "[wired-confirm-qemu] wired child claimed owner",
                    "[wired-confirm-qemu] USB released",
                    "[owner-supervisor-qemu] new receiver START=0",
                    "[owner-supervisor-qemu] wireless generation restarted",
                )
                positions = [output.find(marker) for marker in sequence]
                restart = re.search(r"\[owner-supervisor-qemu\] wireless generation restarted first=(\d+) second=(\d+)", output)
                # Concurrent QNX processes can interleave their stdout at
                # any byte, including the logger prefix and the owner field.
                # The wrapper records both complete PIDs independently.
                owner_logs = (restart is not None and all(
                    f"owner={pid}" in output for pid in restart.groups()))
                if (any(position < 0 for position in positions) or positions != sorted(positions) or
                        not restart or restart.group(1) == restart.group(2) or
                        not owner_logs or
                        output.count("[supervisor-idle-qemu] IE cleanup") < 2 or
                        output.count("[wireless-dio] generation stopped") < 2 or
                        "[independent-srp-qemu] wired helper exit=0" not in output or
                        "select() error:" in output or
                        'CFLite.c:465 "CFLRelease"' in output or
                        "[independent-srp-qemu] pairing failed" in output):
                    raise SystemExit(f"active-media wireless supervisor restart failed; log: {log}")
                print(f"native wireless supervisor restarted dio after active-media USB release; log: {log}")
                return
            if options.active_owner_wired_qemu:
                ordered = (
                    "[independent-srp-qemu] encrypted PCM RTP packets=80",
                    "[independent-srp-qemu] active session QUIESCE=3",
                    "[independent-srp-qemu] unowned wired drain=1",
                    "[dio-bridge] wired drain begin=1",
                    "[dio-bridge] wired drain result=0",
                    "[dio-bridge] quiesce idle result=0",
                    "[wired-confirm-qemu] wired child claimed owner",
                )
                positions = [output.find(marker) for marker in ordered]
                if any(position < 0 for position in positions) or positions != sorted(positions) or \
                        output.find("[independent-srp-qemu] wired helper exit=0") < positions[-1] or \
                        output.find("[owner-wrapper-qemu] wireless owner exited after wired handoff") < positions[5] or \
                        not re.search(r"Process \d+ \(dio_manager\) exited status=0\.", output) or \
                        "[independent-srp-qemu] pairing failed" in output:
                    raise SystemExit(f"active-media wired owner takeover failed; log: {log}")
                print(f"guarded wired owner took active-media dio after drain; log: {log}")
                return
            if options.owner_supervisor_qemu:
                markers = (
                    "[phone-qemu] stock receiver /info ready=1",
                    "[phone-qemu] iap2 normal=1",
                    "[phone-qemu] identification=1",
                    "[phone-qemu] certificate=1",
                    "[phone-qemu] signature=1",
                    "[phone-qemu] wifi=1",
                    "[phone-qemu] CarPlayStartSession=1",
                    "[phone-qemu] stock receiver STOP=1",
                    "[phone-qemu] full daemon path=1",
                    "[owner-supervisor-qemu] phone done=1",
                    "[wireless-dio] generation stopped",
                )
                missing = [marker for marker in markers if marker not in output]
                ie = re.search(r"\[owner-supervisor-qemu\] IE sets=(\d+) clears=(\d+) present=(\d+)", output)
                if missing or not ie or int(ie.group(1)) < 1 or int(ie.group(2)) < 1 or ie.group(3) != "0":
                    raise SystemExit(f"supervised phone QEMU check failed, missing={missing}; log: {log}")
                print(f"supervised stock dio + full phone + IE cleanup QEMU path passed; log: {log}")
                return
            print(f"exact owner -> shell wrapper -> stock dio QEMU path passed; log: {log}")
            return
        if options.owner_exec_qemu:
            stock_pid = re.search(r"DIOManagerApp with pid=(\d+)", output)
            lock_pid = re.search(r"\[owner-stock-dio\] stock dio lock held by pid=(\d+)",
                                 output)
            if not stock_pid or not lock_pid or stock_pid.group(1) != lock_pid.group(1):
                raise SystemExit(f"stock dio PID and owner-lock PID differ; log: {log}")
        markers = [
            "[dio-capture] image guard passed",
            "[dio-capture] manager allocation observed",
            "[dio-capture] manager captured",
        ]
        if options.owner_exec_qemu:
            markers.append("[owner-stock-dio] stock dio lock held by pid=")
        if options.existing_hook_qemu:
            markers.append("[dio-capture] existing hook preload owner=1")
        if not options.mesh:
            markers.append("[dio-capture] manager released")
        if options.start_experiment and not options.bridge_first_start_qemu:
            markers.append("[dio-capture] posting stock StartAirPlay event")
        if options.bridge_first_start_qemu:
            markers.append("[dio-capture] QEMU bridge owns first StartAirPlay")
            if not (options.receiver_control_probe or options.daemon_phone_qemu):
                markers.append("[dio-bridge] prestarted stock receiver=1")
            markers.append("[dio-bridge] stock display width=800")
            markers.append("[dio-bridge] stock display height=480")
            markers.append("[dio-bridge] posting first StartAirPlay=1")
        if options.restart_qemu or options.stop_only_qemu:
            markers.append("[dio-capture] QEMU calling stock AirPlay stop")
            markers.append("[dio-capture] QEMU stopped thread pointer retained status=1")
            markers.append("[dio-capture] QEMU stopped util thread pointer status=0")
            markers.append("[dio-capture] manager released")
            markers.append("<DIO_MAIN> Shutdown signaled: 1")
        if options.restart_qemu:
            markers.append("[dio-capture] QEMU posting second StartAirPlay event")
        if options.soft_restart_qemu:
            markers.append("[dio-capture] QEMU direct stopServer")
            markers.append("[dio-capture] QEMU direct stopServer status=0")
            markers.append("[dio-capture] QEMU stopped TCP5000 connected status=0")
            markers.append("[dio-capture] QEMU direct startServer")
            markers.append("[dio-capture] QEMU direct startServer status=0")
            markers.append("[dio-capture] QEMU restarted TCP5000 connected status=1")
            markers.append("[dio-capture] QEMU AirPlay Bonjour successes after restart status=2")
            markers.append("[dio-capture] QEMU manager retained status=1")
        if options.bridge_qemu:
            bridge_port = 7000 if options.airplay280_qemu else 5000
            markers.append("[dio-bridge] listening=57343")
            if options.active_session_quiesce_qemu:
                markers.append("[independent-srp-qemu] active session QUIESCE=3")
                if options.active_wired_drain_qemu:
                    markers.append("[independent-srp-qemu] active wired drain=0")
                    markers.append("[independent-srp-qemu] post-drain QUIESCE=0")
                elif options.screen_abrupt_close_qemu:
                    markers.append("[independent-srp-qemu] screen abrupt close without TEARDOWN")
                    markers.append("[independent-srp-qemu] post-RST QUIESCE=0")
                else:
                    markers.append("[independent-srp-qemu] screen TEARDOWN status=200")
                    markers.append("[independent-srp-qemu] post-TEARDOWN QUIESCE=0")
            elif not (options.receiver_control_probe or options.daemon_phone_qemu):
                markers.extend([
                "[dio-bridge] START generation 1=0",
                "[dio-bridge] START generation 1 attempts=1",
                "[dio-bridge] wrong AirPlay port rejected=2",
                "[dio-bridge] duplicate START generation 1=0",
                "[dio-bridge] conflicting START generation 2=3",
                "[dio-bridge] wrong STOP generation 2=2",
                "[dio-bridge] STOP generation 1=0",
                "[dio-bridge] START generation 2=0",
                ("[dio-bridge] QEMU property race QUIESCE=0"
                 if options.property_handoff_quiesce_qemu
                 else "[dio-bridge] STOP generation 2=0"),
                f"[dio-bridge] AirPlay /info ready on port={bridge_port}",
                ])
            else:
                if options.receiver_control_probe:
                    markers.extend([
                    "[receiver-control-qemu] START 1=0",
                    "[receiver-control-qemu] STOP 1=0",
                    "[receiver-control-qemu] START 2=0",
                    "[receiver-control-qemu] STOP 2=0",
                    "(receiver-control-qemu-probe) terminated signo=0 code=0",
                    f"[dio-bridge] AirPlay /info ready on port={bridge_port}",
                    ])
                else:
                    markers.extend([
                        "[bridge] listening on 127.0.0.1:57342",
                        "[phone-qemu] stock receiver /info ready=1",
                        "[phone-qemu] iap2 normal=1",
                        "[phone-qemu] identification=1",
                        "[mfi-qemu] software certificate read=" if options.software_mfi_qemu else
                        "[mfi-qemu] certificate read=4",
                        "[mfi-qemu] combined SENDRECV command=0xc0140507",
                        "[phone-qemu] certificate=1",
                        "[mfi-qemu] software signature read=128" if options.software_mfi_qemu else
                        "[mfi-qemu] signature read=20",
                        "[phone-qemu] signature=1",
                        "[phone-qemu] wifi=1",
                        f"[dio-bridge] AirPlay /info ready on port={bridge_port}",
                        "[phone-qemu] CarPlayStartSession=1",
                        ("Deregistered Bonjour _airplay._tcp." if options.airplay280_qemu
                         else "Deregistered Bonjour _airplay._tcp. for stop"),
                        ("AirPlay servers stopped" if options.airplay280_qemu
                         else "Servers stopped"),
                        "[phone-qemu] stock receiver STOP=1",
                        "[phone-qemu] full daemon path=1",
                        "(phone-qemu-probe) terminated signo=0 code=0",
                    ])
                    if options.daemon_control_qemu:
                        markers.extend([
                            "[dns-qemu] daemon DNS-SD mock loaded",
                            "[phone-qemu] CarPlay Control connect=1",
                            "[carplay-control] connect accepted: HTTP 200",
                        ])
        if options.bridge_teardown_qemu:
            markers.extend([
                "[dio-bridge] QEMU invoking stock thread teardown=1",
                "[dio-capture] manager released",
                "[dio-bridge] retained server released=1",
                "<DIO_MAIN> Shutdown signaled: 1",
            ])
        if options.seed_display:
            markers.append("[dio-capture] seeded QEMU-only 800x480 display")
        if options.stock_mfi_probe_qemu:
            markers.extend(("[stock-mfi-qemu] certificate=4 CERT",
                            "[stock-mfi-qemu] signature=20 synthetic"))
        if options.skip_hid_qemu:
            markers.append("[dio-capture] skipped QEMU virtual HID branch")
            if not options.bridge_first_start_qemu:
                markers.append("[dio-capture] GET /info " +
                               ("7000" if options.airplay280_qemu else "5000") +
                               " HTTP 200")
        if options.http_delegate_qemu:
            markers.extend(("[http-delegate-qemu] route-wrapped",
                            "[http-delegate-qemu] route-called"))
        if options.trackf_gate_qemu:
            markers.append("[dio-capture] armed QEMU Track-F network gate")
        if options.mock_mac_qemu:
            markers.append("[dio-capture] QEMU synthetic primary MAC")
            if options.interface_lo0_qemu:
                markers.append("[dio-capture] interfaceName carplay0 -> lo0")
                markers.append("[dio-capture] if_nametoindex name=lo0 index=1")
            else:
                markers.append("[dio-capture] if_nametoindex name=carplay0 index=0")
                markers.append("[dio-capture] QEMU loopback interface index status=1")
            markers.append("[dio-bridge] DNSServiceRegister status=0" if options.bridge_qemu else
                           "[dio-capture] DNSServiceRegister status status=0")
            if not options.bridge_first_start_qemu:
                markers.append("[dio-capture] observed AirPlay Bonjour port status=" +
                               ("7000" if options.airplay280_qemu else "5000"))
                markers.append("[dio-capture] observed AirPlay Bonjour successes status=1")
        if (options.feature37 or options.airplay280_qemu) and \
                not options.bridge_first_start_qemu and not options.independent_srp_qemu:
            info_port = 7000 if options.airplay280_qemu else 5000
            responses = re.findall(rf"\[dio-capture\] info{info_port}-hex=([0-9a-f]+)", output)
            if len(responses) < (2 if options.soft_restart_qemu else 1):
                markers.append(f"[dio-capture] info{info_port}-hex=")
                if options.dump_txt_qemu:
                    raise SystemExit(f"no /info body for Bonjour comparison; log: {log}")
            else:
                for hex_response in responses[:2 if options.soft_restart_qemu else 1]:
                    response = bytes.fromhex(hex_response)
                    separator = response.find(b"\r\n\r\n")
                    if separator < 0:
                        raise SystemExit(f"no HTTP body in candidate /info; log: {log}")
                    info = plistlib.loads(response[separator + 4:])
                    features = info.get("features")
                    if not isinstance(features, int):
                        raise SystemExit(f"no numeric features in candidate /info: {features!r}; log: {log}")
                    if options.feature37 and not features & (1 << 37):
                        raise SystemExit(f"feature 37 absent in candidate /info: {features!r}; log: {log}")
                    if options.airplay280_image and (features & (3 << 37)) != (3 << 37):
                        raise SystemExit(f"AirPlay 280 CarPlay/pairing features absent: {features!r}; log: {log}")
                    if options.seed_bt_ids_qemu and info.get("bluetoothIDs") != ["02:00:00:00:00:01"]:
                        raise SystemExit(f"stock Bluetooth ID missing from /info: {info.get('bluetoothIDs')!r}; log: {log}")
                print(f"QEMU candidate /info features={features:#018x}, bit37={int(bool(features & (1 << 37)))}")
        if options.dump_txt_qemu:
            txt_hex = re.findall(r"\[dio-capture\] bonjour-txt-hex=([0-9a-f]+)", output)
            if not txt_hex:
                raise SystemExit(f"no AirPlay Bonjour TXT capture; log: {log}")
            txt = parse_txt_record(bytes.fromhex(txt_hex[-1]))
            if txt.get("srcvers") != info.get("sourceVersion") or \
                    txt.get("deviceid") != info.get("deviceID"):
                raise SystemExit(f"AirPlay Bonjour /info identity mismatch: {txt!r}; log: {log}")
            if options.airplay280_image and (txt.get("srcvers") != "280.33.8" or
                                             not txt.get("pi")):
                raise SystemExit(f"AirPlay 280 Bonjour pairing identity missing: {txt!r}; log: {log}")
            print("QEMU AirPlay identity: "
                  f"deviceid={txt['deviceid']} srcvers={txt['srcvers']} "
                  f"pi={'present' if 'pi' in txt else 'absent'} "
                  f"pk={'present' if 'pk' in txt else 'absent'}")
        if options.airplay280_qemu and (not options.bridge_qemu or
                                        options.active_session_quiesce_qemu):
            if options.repeat_media_qemu and \
                    "[dio-capture] manager released" in output and \
                    "[independent-srp-qemu] second media session complete" not in output:
                raise SystemExit(f"QEMU dio_manager exited before second media session; log: {log}")
            if options.hold_stock_runloop_qemu and \
                    "[independent-srp-qemu] second bridge START=-1" in output:
                raise SystemExit(f"QEMU held-runloop receiver START timed out; log: {log}")
            if options.independent_srp_qemu and \
                    ("[independent-srp-qemu] M1-M4 proof verified" not in output or
                     "[independent-srp-qemu] M1-M6 identity verified" not in output or
                     "[independent-srp-qemu] pair-verify M1-M4 verified" not in output or
                     "[independent-srp-qemu] encrypted /info HTTP 200" not in output):
                raise SystemExit(f"independent pairing did not complete; log: {log}")
            if options.independent_setup_qemu and \
                    ("[independent-srp-qemu] control SETUP status=200" not in output or
                     '"fn":"cb.sessionCreated"' not in output):
                raise SystemExit(f"independent control SETUP did not complete; log: {log}")
            if options.independent_screen_setup_qemu:
                expected_end = ("[independent-srp-qemu] post-RST QUIESCE=0"
                                if options.active_session_quiesce_qemu and options.screen_abrupt_close_qemu else
                                "[independent-srp-qemu] post-drain QUIESCE=0"
                                if options.active_wired_drain_qemu else
                                "[independent-srp-qemu] post-TEARDOWN QUIESCE=0"
                                if options.active_session_quiesce_qemu else
                                "[independent-srp-qemu] screen abrupt close without TEARDOWN"
                                if options.screen_abrupt_close_qemu else
                                "[independent-srp-qemu] screen TEARDOWN status=200")
                if expected_end not in output:
                    raise SystemExit(f"independent screen shutdown failed; log: {log}")
                if options.repeat_media_qemu:
                    repeated_markers = (
                        "[independent-srp-qemu] pair-verify M1-M4 verified",
                        "[independent-srp-qemu] control SETUP status=200",
                        "[independent-srp-qemu] screen AVCC config+encrypted frame sent",
                        "[independent-srp-qemu] screen TEARDOWN status=200",
                    )
                    if any(output.count(marker) != 2 for marker in repeated_markers) or (
                        "[independent-srp-qemu] first bridge START=0" not in output or
                        (not options.repeat_media_keep_server_qemu and
                         "[independent-srp-qemu] first bridge STOP=0" not in output) or
                        (not options.repeat_media_keep_server_qemu and
                         "[independent-srp-qemu] second bridge START=0" not in output) or
                        output.count("[independent-srp-qemu] encrypted PCM RTP packets=80") != 2 or
                        output.count("[independent-srp-qemu] active session QUIESCE=3") != 1 or
                        output.count("[independent-srp-qemu] post-TEARDOWN QUIESCE=0") != 1 or
                        not (output.find("[independent-srp-qemu] first media session complete") <
                             output.find("[independent-srp-qemu] second media session complete")) or
                        (not options.repeat_media_keep_server_qemu and
                         output.find("[independent-srp-qemu] second media session complete") >
                         output.find("[dio-capture] manager released"))
                    ):
                        raise SystemExit(f"repeated encrypted media/QUIESCE sequence failed; log: {log}")
                if options.screen_abrupt_close_qemu and not (
                    output.find("[independent-srp-qemu] screen abrupt close without TEARDOWN") <
                    output.find("kConnectionErr") <
                    output.find("[dio-capture] manager released")
                ):
                    raise SystemExit(f"abrupt control close did not end the media session cleanly; log: {log}")
                if options.screen_rst_delay_qemu and \
                        "[independent-srp-qemu] timing socket closed before delayed RST" not in output:
                    raise SystemExit(f"delayed RST fixture did not run; log: {log}")
                if options.screen_linger_qemu and \
                        "[independent-srp-qemu] screen linger complete" not in output:
                    raise SystemExit(f"independent screen linger did not complete; log: {log}")
                if options.screen_linger_qemu and not (
                    output.find("[independent-srp-qemu] screen linger before close") <
                    output.find("[dio-capture] manager released") <
                    output.find("[independent-srp-qemu] screen linger complete")
                ):
                    raise SystemExit(f"dio_manager did not release while client socket remained open; log: {log}")
                if options.screen_data_qemu and (
                    not re.search(
                        r"\[independent-srp-qemu\] screen data keepalive port=[1-9][0-9]*",
                        output,
                    ) or not re.search(
                        r"\[independent-srp-qemu\] screen event port=[1-9][0-9]* RECORD status=200",
                        output,
                    ) or "[screen-data-qemu] ScreenStreamStart result=0" not in output or
                    "[screen-data-qemu] ProcessFrames entered" not in output or
                    "FAKEOMX_ASYNC_FILL=1" not in output
                ):
                    raise SystemExit(f"independent screen data socket/keepalive failed; log: {log}")
                if options.screen_frame_qemu and (
                    "[independent-srp-qemu] screen AVCC config+encrypted frame sent" not in output or
                    "[screen-data-qemu] ProcessFrames result=0" not in output or
                    not re.search(r"\[fakeomx\] EmptyThisBuffer #\d+ nFilledLen=30 flags=0x90", output) or
                    not re.search(r"\[fakeomx\] EmptyThisBuffer #\d+ nFilledLen=8 flags=0x10", output) or
                    "### Process frame error" in output
                ):
                    raise SystemExit(f"independent encrypted screen frame did not reach fake OMX; log: {log}")
                if not options.repeat_media_keep_server_qemu and (
                        "[dio-capture] manager released" not in output or
                        "<DIO_MAIN> Shutdown signaled: 1" not in output):
                    raise SystemExit(f"dio_manager did not finish screen-session shutdown; log: {log}")
                bodies = re.findall(r"\[independent-srp-qemu\] screen-setup-body-hex=([0-9a-f]+)", output)
                if not bodies:
                    raise SystemExit(f"no independent screen SETUP body; log: {log}")
                setup_info = plistlib.loads(bytes.fromhex(bodies[-1]))
                streams = setup_info.get("streams", [])
                if not isinstance(streams, list) or not any(
                    isinstance(item, dict) and item.get("type") == 110 and
                    isinstance(item.get("dataPort"), int) and item["dataPort"] > 0
                    for item in streams
                ):
                    raise SystemExit(f"screen stream 110/dataPort absent: {setup_info!r}; log: {log}")
                if options.screen_audio_qemu and not any(
                    isinstance(item, dict) and item.get("type") == 100 and
                    isinstance(item.get("dataPort"), int) and item["dataPort"] > 0
                    for item in streams
                ):
                    raise SystemExit(f"main PCM audio 100/dataPort absent: {setup_info!r}; log: {log}")
                if options.screen_audio_qemu and (
                    not re.search(
                        r"\[independent-srp-qemu\] encrypted PCM RTP packets=80 port=[1-9][0-9]*",
                        output,
                    ) or output.count("[audio-qemu] RTPJitterBufferPutBusyNode result=0") != 80 or
                    "[audio-qemu] main PCM callback contained nonzero samples" not in output or
                    "### Process main audio error" in output
                ):
                    raise SystemExit(f"encrypted main PCM RTP did not reach playback callback; log: {log}")
            pair_responses = re.findall(r"\[dio-capture\] pair-setup-m1-hex=([0-9a-f]+)", output)
            if options.airplay280_image and not pair_responses and not options.independent_srp_qemu:
                raise SystemExit(f"no AirPlay pair-setup M1 response; log: {log}")
            if pair_responses and not options.independent_srp_qemu:
                pair_response = bytes.fromhex(pair_responses[-1])
                headers, separator, body = pair_response.partition(b"\r\n\r\n")
                if not separator or not headers.startswith(b"HTTP/1.1 200") or \
                        b"Content-Type: application/pairing+tlv8" not in headers:
                    raise SystemExit(f"invalid AirPlay pair-setup M1 HTTP response; log: {log}")
                values: dict[int, bytes] = {}
                index = 0
                while index < len(body):
                    if index + 2 > len(body) or index + 2 + body[index + 1] > len(body):
                        raise SystemExit(f"truncated pair-setup M2 TLV; log: {log}")
                    tlv_type, tlv_length = body[index], body[index + 1]
                    index += 2
                    values[tlv_type] = values.get(tlv_type, b"") + body[index:index + tlv_length]
                    index += tlv_length
                if values.get(6) != b"\x02" or len(values.get(2, b"")) != 16 or \
                        len(values.get(3, b"")) != 384:
                    raise SystemExit(f"invalid AirPlay pair-setup M2 payload; log: {log}")
                print("QEMU AirPlay pair-setup M1->M2: HTTP 200, TLV state=2, "
                      "salt=16B, public-key=384B")
            if options.airplay280_image and not options.active_session_quiesce_qemu:
                setup = re.search(r"\[pair-qemu\] /pair-setup result=(-?\d+) done=(\d+)", output)
                verify = re.search(r"\[pair-qemu\] /pair-verify result=(-?\d+) done=(\d+)", output)
                if not setup or setup.groups() != ("0", "1"):
                    raise SystemExit(f"AirPlay pair-setup did not complete; log: {log}")
                if not re.search(r"\[pair-qemu\] keychain mode=0600 bytes=[1-9][0-9]*", output):
                    raise SystemExit(f"AirPlay pairing keychain was not private and nonempty; log: {log}")
                if not verify or verify.groups() != ("0", "1"):
                    raise SystemExit(f"AirPlay pair-verify did not complete; log: {log}")
                encrypted = re.search(r"\[pair-qemu\] encrypted /info result=(-?\d+)", output)
                if not encrypted or encrypted.group(1) != "0":
                    raise SystemExit(f"encrypted AirPlay /info failed; log: {log}")
                print("QEMU AirPlay pair-setup, pair-verify and encrypted /info completed")
        missing = [marker for marker in markers if marker not in output]
        if options.mesh:
            if not (options.restart_qemu or options.stop_only_qemu or
                    options.bridge_teardown_qemu or options.independent_setup_qemu) and \
                    "[dio-capture] manager released" in output:
                missing.append("manager remained alive during the mesh observation")
        elif not re.search(r"Process \d+ \(dio_manager\) exited status=1\.", output):
            missing.append("stock dio_manager reached its known QEMU exit")
        if missing or "terminated SIG" in output or "[dio-crash-qemu]" in output:
            raise SystemExit(f"QEMU capture failed, missing={missing}; log: {log}")
        if options.start_experiment:
            print(f"QEMU start experiment recorded; log: {log}")
        else:
            print(f"QEMU stock-code manager capture OK; log: {log}")
        print(f"extracted IFS dio SHA-256: {sha256(image_bytes).hexdigest()}")


if __name__ == "__main__":
    main()
