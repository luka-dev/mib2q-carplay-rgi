#!/usr/bin/env python3
"""Compare QNX loader behavior for stock and grafted btstack in a disposable IFS.

The guest has no Marvell radio. This only tests ELF loading and early startup;
it never edits a firmware image or contacts the head unit.
"""

from __future__ import annotations

import argparse
import json
from hashlib import sha256
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from elftools.elf.elffile import ELFFile


HERE = Path(__file__).resolve().parent
AUDI = HERE.parents[3]
TOOLS = AUDI / "Tools"
SDP = TOOLS / "qnx-65-sdp-docker"
CARPLAY = TOOLS / "qnx-carplay-emu/carplay"
QEMU = TOOLS / "qemu-src/build/qemu-system-arm"
FIRMWARE = AUDI / (
    "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
    "advanced/MU1316-appimg"
)
FACTORY_CONFIG = AUDI / (
    "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
    "advanced/MU1316-system/etc/eso/production/connectivity.json"
)
SOURCE = FIRMWARE / "eso/bin/apps/btstack"
SOURCE_SHA = "154c17826f0773a57b2004f5b17d818e3437c46fcbb6a857d40c696898c29bc5"
BASE = CARPLAY / "dio_mesh_provider_twin"
IMAGE = "qnx65-armv7-toolchain:latest"
STARTUP_FACTORY_BASENAMES = {
    "lib_03171582-f255-4991-b8ae-83f90521c08b.so",
    "lib_11eaa58a-7811-47de-9981-4f8914ce7c1e.so",
    "lib_8d4f7cec-be0c-49b7-b1b6-1e3e4c26b847.so",
    "lib_b73ead7a-5569-11e0-afa1-5f3656ea1da0.so",
    "lib_c0c7c76f-cbaf-4d93-8683-8f2b1a4b5d01.so",
    "lib_c575bf98-4e5b-4f9c-9d38-a6b23f8055ec.so",
    "lib_c6dbc5bd-24a2-53b2-837f-0fa20e31905a.so",
    "lib_f6db1c7c-7ff9-5fd1-97ba-7298745f1eb1.so",
    "lib_fe1720e3-31d6-4eb8-8a64-b6ed89dc4461.so",
    "lib_e0f8829c-7990-5dd2-b13d-5d15671ccd6b.so",
    "lib_133c937f-bbad-4723-a502-4020cab96d54.so",
    "lib_abec271c-8352-4a6b-a4a5-a36dfd6f8f02.so",
    "lib_7019ab65-bfac-5d07-8e83-e9a702af571a.so",
    "lib_9afc0af0-5cc2-581b-978c-9767a9c31424.so",
    "lib_2d68efa1-ab18-5d3c-9663-2d15182efc71.so",
    "lib_3b8fb5ed-21b3-47c4-cafe-f6b50bdbfec1.so",
    "lib_a58995e5-3a88-5f1a-9a3c-1786b9a38589.so",
}
BLUETOOTH_TRAP_SITES = {
    0x1155C0: bytes.fromhex("f04f2de9"),  # main entry, trap positive control
    0x1159CC: bytes.fromhex("61fbffeb"),  # launcher-client service registration
    0x1159E4: bytes.fromhex("384086e2"),  # registration succeeded; enter event loop
    0x120A4C: bytes.fromhex("f0452de9"),  # CBT_APP::initApplication entry
    0x136DB8: bytes.fromhex("f0452de9"),  # CBT_APP::runApplication entry
    0x136E68: bytes.fromhex("638000eb"),  # read Bluetooth persistency
    0x136E74: bytes.fromhex("31cfffeb"),  # subscribe to persistence values
    0x136E98: bytes.fromhex("8094ffeb"),  # start SWaP/FEC interface
    0x136ECC: bytes.fromhex("06feffeb"),  # register BluetoothInformationService
    0x136EF4: bytes.fromhex("35daffeb"),  # register BluetoothControl service
    0x136F14: bytes.fromhex("52efffeb"),  # start DSIBluetooth
    0x136F4C: bytes.fromhex("00a0a0e1"),  # start DSIMobileEquipment
    0x136F74: bytes.fromhex("eceeffeb"),  # start DSIObexAuthentication
    0x136FE0: bytes.fromhex("069702eb"),  # final diagnosis interfaces
    0x136FF8: bytes.fromhex("d80000da"),  # runApplication success branch
    0x137068: bytes.fromhex("100000da"),  # runApplication failure branch
    0x13B428: bytes.fromhex("f0412de9"),  # callback that may set persistence gate
    0x1399D0: bytes.fromhex("f04f2de9"),  # checkBluetoothAppState entry
    0x139DD0: bytes.fromhex("570e85e2"),  # enter persistence/proxy gate
    0x139DE0: bytes.fromhex("580595e5"),  # persistence flag passed
    0x139DF4: bytes.fromhex("33ff2fe1"),  # proxy pointer non-null
    0x139E00: bytes.fromhex("060094e7"),  # proxy isAlive passed
    0x13AC04: bytes.fromhex("3cff2fe1"),  # dispatch startBtStack via BluetoothServices proxy
}


def patch_bluetooth_trap(path: Path, address: int) -> None:
    """Put an ARM UDF in a disposable client copy to prove gate coverage."""
    expected = BLUETOOTH_TRAP_SITES[address]
    with path.open("r+b") as stream:
        elf = ELFFile(stream)
        offsets = [segment["p_offset"] + address - segment["p_vaddr"]
                   for segment in elf.iter_segments()
                   if segment["p_type"] == "PT_LOAD" and
                   segment["p_vaddr"] <= address < segment["p_vaddr"] + segment["p_filesz"]]
        if len(offsets) != 1:
            raise RuntimeError(f"Bluetooth trap address not uniquely file-backed: {address:#x}")
        stream.seek(offsets[0])
        if stream.read(4) != expected:
            raise RuntimeError(f"Bluetooth instruction changed at {address:#x}")
        stream.seek(offsets[0])
        stream.write(bytes.fromhex("f000f0e7"))  # QNX ARM assembler: udf #0


def bypass_bluetooth_gate(path: Path, address: int, expected: bytes) -> None:
    """NOP a verified readiness BEQ in a disposable QEMU client copy."""
    replacement = bytes.fromhex("0000a0e1")  # MOV R0, R0
    with path.open("r+b") as stream:
        elf = ELFFile(stream)
        offsets = [segment["p_offset"] + address - segment["p_vaddr"]
                   for segment in elf.iter_segments()
                   if segment["p_type"] == "PT_LOAD" and
                   segment["p_vaddr"] <= address < segment["p_vaddr"] + segment["p_filesz"]]
        if len(offsets) != 1:
            raise RuntimeError(f"Bluetooth gate not uniquely file-backed: {address:#x}")
        stream.seek(offsets[0])
        if stream.read(4) != expected:
            raise RuntimeError(f"Bluetooth gate changed at {address:#x}")
        stream.seek(offsets[0])
        stream.write(replacement)


def run(*args: str, **kwargs: object) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, check=True, text=True, **kwargs)


def docker(work: Path, *args: str, **kwargs: object) -> subprocess.CompletedProcess[str]:
    return run(
        "docker", "run", "--rm", "--platform=linux/amd64",
        "-v", f"{work}:/work", "-v", f"{AUDI}:/src:ro",
        "-w", "/work", IMAGE, *args, **kwargs,
    )


def manifest(work: Path, btstack: Path, storage: str, probe_launcher: bool,
             probe_bluetooth: bool, probe_startup_config: bool,
             probe_exec_path: bool, probe_regular_path: bool,
             probe_factory_bluetooth: bool,
             probe_factory_bluetooth_wrapper: bool,
             probe_factory_persistence: bool,
             probe_factory_persistence_config: bool,
             probe_local_persistence: bool,
             probe_persistence_open_trace: bool,
             probe_factory_fec: bool,
             probe_factory_diagnosis: bool,
             probe_bluetooth_reg_trace: bool,
             probe_btstack_reg_trace: bool,
             probe_graft_socket: bool,
             probe_daemon_bridge: bool,
             probe_slog_stream: bool) -> None:
    if probe_startup_config:
        factory_startup = FACTORY_CONFIG.with_name("dsistartup.json")
        startup_text = factory_startup.read_text()
        for old, new in (("\"wait_for_ooc\" : true", "\"wait_for_ooc\" : false"),
                         ("\"dm_supervision_enabled\" : true", "\"dm_supervision_enabled\" : false")):
            if startup_text.count(old) != 1:
                raise RuntimeError(f"factory startup setting changed: {old}")
            startup_text = startup_text.replace(old, new)
        startup_text = startup_text.replace(
            '"wait_for_ooc" : false,',
            '"wait_for_ooc" : false,\n\t"default_startup_mode" : "on_startup_normal",',
        )
        (work / "dsistartup.json").write_text(startup_text)
    if probe_local_persistence:
        factory_persistence = FACTORY_CONFIG.with_name("persistence.json").read_text()
        start = factory_persistence.index('            "rcc": [')
        end = factory_persistence.index('            "speech-service":', start)
        local_persistence = (
            factory_persistence[:start]
            + '            "rcc": ["SQLite", "/dev/shmem/persistence_rcc.sqlite", '
              '{"filesize_max_kb": 3500}],\n'
            + factory_persistence[end:]
        )
        database_path = "/mnt/persist/var/fw/persistence.sqlite"
        if local_persistence.count(database_path) != 1:
            raise RuntimeError("factory persistence database path changed")
        local_persistence = local_persistence.replace(
            database_path, "/dev/shmem/persistence_default.sqlite")
        (work / "persistence_local.json").write_text(local_persistence)
    source = (BASE.with_suffix(".build")).read_text()
    source = source.replace(
        "/root/bsp-qemu-virt-src/src/hardware/startup/lib/arm/a.le.v7/libstartup.a",
        "/work/libstartup.a",
    ).replace(
        "/usr/qnx650/target/qnx6/armle-v7",
        "/opt/qnx650/target/qnx6/armle-v7",
    ).replace(
        "/root/startup-virt", "/work/startup-virt",
    ).replace(
        "/root/bsp_extract/procnto-smp", "/work/extracted/proc/boot/procnto-smp",
    )
    if probe_slog_stream:
        slogger_marker = "    /proc/boot/slogger"
        if source.count(slogger_marker) != 1:
            raise RuntimeError("base IFS slogger marker changed")
        source = source.replace(slogger_marker,
                                slogger_marker + "\n    /proc/boot/sloginfo -w &")
    if probe_startup_config:
        preload_marker = "    LD_PRELOAD=/proc/boot/mesh_probe.so\n    /proc/boot/servicemgrmibhigh &"
        if source.count(preload_marker) != 1:
            raise RuntimeError("base IFS servicemgr preload changed")
        source = source.replace(
            preload_marker,
            "    LD_PRELOAD=/proc/boot/spawn_probe.so:/proc/boot/mesh_probe.so\n"
            "    /proc/boot/servicemgrmibhigh &",
        )
        tail_marker = '    display_msg "=== PROVIDER_MESH_MARKER_ESOPOS_V1'
        tail_start = source.find(tail_marker)
        tail_end = source.find("\n}", tail_start)
        if tail_start < 0 or tail_end < 0:
            raise RuntimeError("base IFS post-mesh script changed")
        source = (source[:tail_start] + "    waitfor /startup_probe_settle 45"
                  + source[tail_end:])
        marker = "    procmgr_symlink /pps /ramdisk/pps"
        if source.count(marker) != 1:
            raise RuntimeError("base IFS PPS marker changed")
        source = source.replace(
            marker,
            marker
            + ("\n    /proc/boot/graft_socket_probe &" if probe_graft_socket else "")
            + (
                "\n    /proc/boot/pipe &"
                "\n    /proc/boot/random -t"
                "\n    procmgr_symlink /dev/random /dev/urandom"
                "\n    /proc/boot/cp -p /proc/boot/fake_uaputl /dev/shmem/fake_uaputl"
                "\n    PATH=/proc/boot:/dev/shmem"
                "\n    LD_LIBRARY_PATH=/proc/boot"
                "\n    LD_PRELOAD="
                "\n    MHI2Q_CP_IE_INDEX=2"
                "\n    MHI2Q_CP_UAPUTL=/dev/shmem/fake_uaputl"
                "\n    MHI2Q_CP_BT_MAC=00:01:02:03:04:05"
                "\n    IPL_CONFIG_DIR_DIO_MANAGER=/config"
                "\n    MHI2Q_CP_MODEL=MHI2Q"
                "\n    MHI2Q_CP_SERIAL=QEMU"
                "\n    MHI2Q_CP_HARDWARE=MHI2Q"
                "\n    MHI2Q_CP_SSID=QemuCarPlay"
                "\n    MHI2Q_CP_PASSPHRASE=testpassword"
                "\n    MHI2Q_CP_CHANNEL=11"
                "\n    MHI2Q_CP_IP_ADDRESS=127.0.0.1"
                "\n    MHI2Q_CP_RECEIVER=experimental280"
                "\n    MHI2Q_CP_AIRPLAY_PORT=7000"
                "\n    MHI2Q_CP_DEVICE_ID=02:00:00:00:00:01"
                "\n    MHI2Q_CP_SOURCE_VERSION=280.33.8"
                "\n    /proc/boot/mhi2q-wireless-daemon &"
                "\n    LD_PRELOAD=/proc/boot/mesh_probe.so"
                if probe_daemon_bridge else ""
            )
            + "\n    procmgr_symlink /dev/null /tmp/mvloaded"
            + ("\n    /eso/bin/apps/btstack btstack 0 &" if probe_exec_path else "")
            + "\n    /proc/boot/graft_state_probe 35 &"
            + "\n    /proc/boot/graft_state_probe 55 &",
        )
    marker = "    waitfor /dev/eso/servicemgr 15"
    if source.count(marker) != 1:
        raise RuntimeError("base IFS diagnostic point changed")
    source = source.replace(
        marker,
        marker + "\n"
        '    display_msg "=== btstack loader diagnostic ==="\n'
        "    LD_PRELOAD=\n"
        "    LD_LIBRARY_PATH=/proc/boot:/eso/lib\n"
        + ("    waitfor /dev/shmem/btstack-loader-pid 25\n"
           if probe_startup_config else
           f"    /proc/boot/{'btstack btstack 0' if storage == 'ifs' else 'btstack_copy'} &\n")
        + ("    /proc/boot/graft_state_probe 17 &\n"
           if probe_bluetooth and storage == "shmem" else "")
        + "    waitfor /btstack_loader_delay 5\n"
        + f"    /proc/boot/pidin -p {'btstack' if storage == 'ifs' else 'btstack-loader-probe'}\n"
        + ("    /proc/boot/graft_state_probe\n" if storage == "shmem" else "")
        + '    display_msg "=== btstack loader diagnostic end ==="\n'
        + (
            '    display_msg "=== connectivity launcher diagnostic ==="\n'
            '    IPL_CONFIG_DIR=/config\n'
            '    /proc/boot/connectivity_launcher -sm-id 315 -sm-name connectivity_launcher -sm-restart 0 -sm-time 1 &\n'
            '    waitfor /launcher_probe_delay 5\n'
            '    /proc/boot/pidin -p connectivity_launcher\n'
            '    display_msg "=== connectivity launcher diagnostic end ==="\n'
            if probe_launcher else ""
        )
        + (
            '    display_msg "=== bluetooth client diagnostic ==="\n'
            '    IPL_CONFIG_DIR=/config\n'
            '    /proc/boot/bluetooth bluetooth 0 &\n'
            '    waitfor /bluetooth_probe_delay 5\n'
            '    /proc/boot/pidin -p bluetooth\n'
            '    display_msg "=== bluetooth client diagnostic end ==="\n'
            if probe_bluetooth else ""
        )
        + "    LD_PRELOAD=/proc/boot/mesh_probe.so",
    )
    result = []
    for line in source.splitlines():
        if line.startswith("/") and "=" in line:
            destination = line.split("=", 1)[0]
            line = f"{destination}=/work/extracted{destination}"
            if destination == "/proc/boot/libc.so.3":
                line = f"{destination}=/src/{(FIRMWARE / 'armle/lib/libc.so.3').relative_to(AUDI)}"
            if destination == "/config/dsistartup.json" and probe_startup_config:
                line = f"{destination}=/work/dsistartup.json"
            if destination == "/eso/lib/factories/lib_fe1720e3-31d6-4eb8-8a64-b6ed89dc4461.so":
                original_factory = FIRMWARE / destination.lstrip("/")
                line = f"{destination}=/src/{original_factory.relative_to(AUDI)}"
        result.append(line)
    if probe_startup_config:
        result = [line for line in result if not line.startswith("/eso/lib/factories/")
                  or line.split("=", 1)[0].rsplit("/", 1)[-1] in STARTUP_FACTORY_BASENAMES]
    if storage == "ifs":
        staged_btstack = work / "btstack"
        shutil.copyfile(btstack, staged_btstack)
        staged_btstack.chmod(0o755)
    additions = []
    additions.append(f"/config/connectivity.json=/src/{FACTORY_CONFIG.relative_to(AUDI)}")
    factory_destination = "/eso/lib/factories/lib_fe1720e3-31d6-4eb8-8a64-b6ed89dc4461.so"
    factory_line = next(line for line in result if line.startswith(factory_destination + "="))
    result.remove(factory_line)
    additions.append(factory_line)
    for name, filename in (
        ("libxml2.so.1", FIRMWARE / "armle/usr/lib/libxml2.so.1"),
        ("libicuuc.so.58", FIRMWARE / "eso/lib/libicuuc.so.58"),
        ("libicudata.so.58", FIRMWARE / "eso/lib/libicudata.so.58"),
        ("libz.so.2", FIRMWARE / "armle/usr/lib/libz.so.2"),
    ):
        if not filename.is_file():
            raise FileNotFoundError(filename)
        additions.append(f"/proc/boot/{name}=/src/{filename.relative_to(AUDI)}")
    if storage == "ifs":
        additions.append("/proc/boot/btstack=/work/btstack")
    else:
        if probe_startup_config:
            additions.append("/proc/boot/btstack_copy=/work/btstack_copy")
            if probe_regular_path:
                additions.append("/eso/bin/apps/btstack=/work/btstack_copy")
            else:
                additions.append("[type=link] /eso/bin/apps/btstack=/proc/boot/btstack_copy")
        else:
            additions.append("/proc/boot/btstack_copy=/work/btstack_copy")
        additions.append("/proc/boot/graft_state_probe=/work/graft_state_probe")
    if probe_launcher:
        launcher = FIRMWARE / "eso/bin/apps/connectivity_launcher"
        additions.append(f"/proc/boot/connectivity_launcher=/src/{launcher.relative_to(AUDI)}")
    if probe_startup_config:
        launcher = FIRMWARE / "eso/bin/apps/connectivity_launcher"
        additions.append(f"/eso/bin/apps/connectivity_launcher=/src/{launcher.relative_to(AUDI)}")
        # servicemgrmibhigh runs with / as cwd in this QEMU mesh, while the
        # factory framework.json uses ./bin/apps/... relative executable paths.
        additions.append(f"/bin/apps/connectivity_launcher=/src/{launcher.relative_to(AUDI)}")
        additions.append("/proc/boot/spawn_probe.so=/work/spawn_probe.so")
        if probe_persistence_open_trace:
            additions.append("/proc/boot/persistence_open_trace.so=/work/persistence_open_trace.so")
        if probe_bluetooth_reg_trace:
            additions.append("/proc/boot/bluetooth_reg_trace.so=/work/bluetooth_reg_trace.so")
        if probe_btstack_reg_trace:
            additions.append("/proc/boot/btstack_reg_trace.so=/work/btstack_reg_trace.so")
        if probe_graft_socket:
            additions.append("/proc/boot/graft_socket_probe=/work/graft_socket_probe")
        if probe_daemon_bridge:
            for name in ("pipe", "random", "cp", "fake_uaputl", "mhi2q-wireless-daemon"):
                additions.append(f"/proc/boot/{name}=/work/{name}")
        if probe_factory_persistence:
            additions.append("/bin/apps/persistenceTNG_mibhigh=/work/persistence_copy")
        if probe_factory_persistence_config:
            persistence_config = FACTORY_CONFIG.with_name("persistence.json")
            additions.append(
                "/config/persistence.json=/work/persistence_local.json"
                if probe_local_persistence else
                f"/config/persistence.json=/src/{persistence_config.relative_to(AUDI)}")
        if probe_factory_fec:
            additions.append("/bin/apps/fec=/work/fec_copy")
        if probe_factory_diagnosis:
            additions.append("/bin/apps/diagnosis=/work/diagnosis_copy")
        if probe_factory_bluetooth:
            staged = "bluetooth_copy" if probe_factory_bluetooth_wrapper else "bluetooth"
            additions.append(f"/eso/bin/apps/bluetooth=/work/{staged}")
    if probe_bluetooth or probe_factory_bluetooth:
        bluetooth = FIRMWARE / "eso/bin/apps/bluetooth"
        staged_bluetooth = work / "bluetooth"
        if not staged_bluetooth.exists():
            shutil.copyfile(bluetooth, staged_bluetooth)
        staged_bluetooth.chmod(0o755)
        if probe_bluetooth:
            additions.append("/proc/boot/bluetooth=/work/bluetooth")
    # The mesh includes a large factory tree. Place new loader inputs beside
    # early boot programs so they are not beyond the guest's mapped IFS range.
    anchor = next(i for i, line in enumerate(result)
                  if line.startswith("/proc/boot/devstub="))
    result[anchor + 1:anchor + 1] = additions
    (work / "loader.build").write_text("\n".join(result) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--variant", choices=("stock", "graft"), required=True)
    parser.add_argument("--storage", choices=("ifs", "shmem"), default="shmem")
    parser.add_argument("--probe-launcher", action="store_true")
    parser.add_argument("--probe-bluetooth", action="store_true")
    parser.add_argument("--probe-startup-config", action="store_true")
    parser.add_argument("--probe-exec-path", action="store_true")
    parser.add_argument("--probe-regular-path", action="store_true",
                        help="stage a regular executable instead of an IFS symlink")
    parser.add_argument("--probe-factory-bluetooth", action="store_true",
                        help="stage the factory Bluetooth client for the real launcher")
    parser.add_argument("--probe-factory-bluetooth-wrapper", action="store_true",
                        help="run the factory Bluetooth client through a QEMU-only preload scrubber")
    parser.add_argument("--probe-factory-persistence", action="store_true",
                        help="stage the factory persistence service with a QEMU-only preload scrubber")
    parser.add_argument("--probe-factory-persistence-config", action="store_true",
                        help="also stage exact factory persistence.json in the QEMU image")
    parser.add_argument("--probe-local-persistence", action="store_true",
                        help="QEMU-only: route RCC persistence backend to a local SQLite file")
    parser.add_argument("--probe-persistence-open-trace", action="store_true",
                        help="trace factory persistence file opens in the disposable QEMU image")
    parser.add_argument("--probe-factory-fec", action="store_true",
                        help="stage the factory FEC/SWaP service with a QEMU-only preload scrubber")
    parser.add_argument("--probe-factory-diagnosis", action="store_true",
                        help="stage the factory diagnosis service with a QEMU-only preload scrubber")
    parser.add_argument("--probe-bluetooth-mesh", action="store_true",
                        help="trace the factory Bluetooth client with only the passive OSAL probe")
    parser.add_argument("--probe-bluetooth-reg-trace", action="store_true",
                        help="QEMU-only LD_PRELOAD probe for Bluetooth service registration results")
    parser.add_argument("--probe-btstack-reg-trace", action="store_true",
                        help="QEMU-only LD_PRELOAD probe for btstack service registration results")
    parser.add_argument("--probe-graft-socket", action="store_true",
                        help="QEMU-only loopback peer that closes four graft worker connections")
    parser.add_argument("--probe-daemon-bridge", action="store_true",
                        help="QEMU-only real daemon peer for synthetic RFCOMM iAP2 marker")
    parser.add_argument("--probe-synthetic-rfcomm", action="store_true",
                        help="QEMU-only callback-to-socket data delivery check")
    parser.add_argument("--probe-slog-stream", action="store_true",
                        help="stream the QEMU guest slog buffer to serial during factory startup")
    parser.add_argument("--probe-bluetooth-trap", type=lambda value: int(value, 0),
                        help="UDF-trap one verified Bluetooth readiness-gate instruction in the QEMU copy")
    parser.add_argument("--probe-bypass-persistence-gate", action="store_true",
                        help="QEMU-only: skip the Bluetooth client's persistence-ready BEQ")
    parser.add_argument("--probe-bypass-proxy-alive-gate", action="store_true",
                        help="QEMU-only: skip the Bluetooth client's proxy-isAlive BEQ")
    parser.add_argument("--qemu-timeout", type=int,
                        help="override QEMU diagnostic duration in seconds")
    parser.add_argument("--probe-static-wrapper", action="store_true",
                        help="link the QEMU-only btstack copy wrapper without shared libraries")
    parser.add_argument("--probe-child-libpath", action="store_true",
                        help="give launcher-spawned btstack the same QEMU library path as direct launch")
    parser.add_argument("--probe-keep-child-preload", action="store_true",
                        help="negative control: keep inherited QEMU tracing preloads in btstack")
    parser.add_argument("--build-only", action="store_true")
    options = parser.parse_args()
    if sha256(SOURCE.read_bytes()).hexdigest() != SOURCE_SHA:
        raise SystemExit("target btstack SHA-256 mismatch")
    if options.variant == "graft":
        run("python3", str(HERE / "build_offline.py"),
            *(["--qemu-synthetic"] if options.probe_synthetic_rfcomm else []),
            *(["--qemu-iap2-marker"] if options.probe_daemon_bridge else []),
            capture_output=True)
    btstack = SOURCE if options.variant == "stock" else HERE / "out" / (
        "btstack.graft.iap2-qemu.offline" if options.probe_daemon_bridge else
        "btstack.graft.synthetic.offline" if options.probe_synthetic_rfcomm
        else "btstack.graft.offline")
    out = HERE / "out"
    out.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mhi2q-btstack-loader-") as temp:
        work = Path(temp)
        extracted = work / "extracted"
        extracted.mkdir()
        listing = docker(work, "dumpifs", "-v", f"/src/{BASE.relative_to(AUDI)}.bin",
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
               f"/src/{BASE.relative_to(AUDI)}.bin")
        for filename in ("startup-virt", "libstartup.a"):
            shutil.copyfile(SDP / "tests/qemu" / filename, work / filename)
        if options.storage == "shmem":
            (work / "btstack_blob.S").write_text(
                ".section .rodata\n.balign 4\n"
                ".global btstack_blob_start\n.global btstack_blob_end\n"
                "btstack_blob_start:\n.incbin \"/work/btstack\"\n"
                "btstack_blob_end:\n"
            )
            shutil.copyfile(btstack, work / "btstack")
            # Factory SpawnFactory inherits the service-manager tracer when it
            # passes envp=NULL. Clear it in the diagnostic wrapper so the
            # actual btstack runs with an uninstrumented QNX loader state.
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc",
                   *(["-static"] if options.probe_static_wrapper else []),
                   *(["-DPROBE_CHILD_LIBPATH"] if options.probe_child_libpath else []),
                   *(["-DPROBE_CHILD_NO_PRELOAD"]
                     if options.probe_startup_config and not options.probe_keep_child_preload else []),
                   *(["-DPROBE_BTSTACK_REG_TRACE"] if options.probe_btstack_reg_trace else []),
                   "-O2", "-Wall", "-Wextra", "-Werror", "-o", "/work/btstack_copy",
                   f"/src/{(HERE / 'loader_copy.c').relative_to(AUDI)}",
                   "/work/btstack_blob.S")
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-O2", "-Wall",
                   "-Wextra", "-Werror", "-o", "/work/graft_state_probe",
                   f"/src/{(HERE / 'state_probe.c').relative_to(AUDI)}")
        if options.probe_startup_config:
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-shared", "-fPIC",
                   "-O2", "-Wall", "-Wextra", "-Werror", "-o", "/work/spawn_probe.so",
                   f"/src/{(HERE / 'spawn_probe.c').relative_to(AUDI)}")
        if options.probe_factory_bluetooth_wrapper:
            bluetooth = FIRMWARE / "eso/bin/apps/bluetooth"
            shutil.copyfile(bluetooth, work / "bluetooth")
            if options.probe_bypass_persistence_gate:
                bypass_bluetooth_gate(work / "bluetooth", 0x139DDC,
                                      bytes.fromhex("0a01000a"))  # BEQ loc_13A20C
            if options.probe_bypass_proxy_alive_gate:
                bypass_bluetooth_gate(work / "bluetooth", 0x139DFC,
                                      bytes.fromhex("0201000a"))  # BEQ loc_13A20C
            if options.probe_bluetooth_trap is not None:
                if options.probe_bluetooth_trap not in BLUETOOTH_TRAP_SITES:
                    raise SystemExit("unsupported Bluetooth trap address")
                patch_bluetooth_trap(work / "bluetooth", options.probe_bluetooth_trap)
            (work / "bluetooth_blob.S").write_text(
                ".section .rodata\n.balign 4\n"
                ".global bluetooth_blob_start\n.global bluetooth_blob_end\n"
                "bluetooth_blob_start:\n.incbin \"/work/bluetooth\"\n"
                "bluetooth_blob_end:\n"
            )
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc",
                   "-DPROBE_BLUETOOTH_WRAPPER", "-DPROBE_CHILD_NO_PRELOAD",
                   *(["-DPROBE_CHILD_MESH_ONLY"] if options.probe_bluetooth_mesh else []),
                   *(["-DPROBE_CHILD_REG_TRACE"] if options.probe_bluetooth_reg_trace else []),
                   "-O2", "-Wall", "-Wextra", "-Werror", "-o", "/work/bluetooth_copy",
                   f"/src/{(HERE / 'loader_copy.c').relative_to(AUDI)}",
                   "/work/bluetooth_blob.S")
        if options.probe_bluetooth_reg_trace:
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc",
                   "-shared", "-fPIC", "-O2", "-Wall", "-Wextra", "-Werror",
                   "-o", "/work/bluetooth_reg_trace.so",
                   f"/src/{(HERE / 'bluetooth_reg_trace.c').relative_to(AUDI)}")
        if options.probe_btstack_reg_trace:
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc",
                   "-shared", "-fPIC", "-O2", "-Wall", "-Wextra", "-Werror",
                   '-DTRACE_PATH="/dev/shmem/btstack-reg-trace.log"',
                   '-DTRACE_LABEL="BTREGTRACE"',
                   "-o", "/work/btstack_reg_trace.so",
                   f"/src/{(HERE / 'bluetooth_reg_trace.c').relative_to(AUDI)}")
        if options.probe_graft_socket:
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc",
                   "-O2", "-Wall", "-Wextra", "-Werror",
                   *(["-DMHI2Q_SOCKET_SYNTHETIC"] if options.probe_synthetic_rfcomm else []),
                   "-o", "/work/graft_socket_probe",
                   f"/src/{(HERE / 'socket_probe.c').relative_to(AUDI)}", "-lsocket")
        if options.probe_daemon_bridge:
            daemon = HERE.parent / "daemon"
            run("docker", "run", "--rm", "--platform=linux/amd64",
                "-v", f"{AUDI}:/src", "-w", "/src/Patches/mhi2-carplay", IMAGE,
                "/opt/rust/qnx-cargo", "build", "-q", "--manifest-path",
                f"/src/{(daemon / 'Cargo.toml').relative_to(AUDI)}", "--bin",
                "mhi2q-wireless-daemon", capture_output=True)
            sdk_sys = SDP / "sdp/target/qnx6/armle-v7"
            for name, source in {
                "pipe": sdk_sys / "sbin/pipe",
                "random": sdk_sys / "usr/sbin/random",
                "cp": TOOLS / "bsp-qemu-arm/extract/cp",
                "mhi2q-wireless-daemon": daemon / "target/armv7-unknown-nto-qnx650/release/mhi2q-wireless-daemon",
            }.items():
                shutil.copy2(source, work / name)
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-O2", "-Wall",
                   "-Wextra", "-Werror", "-o", "/work/fake_uaputl",
                   f"/src/{(daemon / 'probes/fake_uaputl.c').relative_to(AUDI)}")
        if options.probe_factory_persistence:
            persistence = FIRMWARE / "eso/bin/apps/persistenceTNG_mibhigh"
            shutil.copyfile(persistence, work / "persistence")
            (work / "persistence_blob.S").write_text(
                ".section .rodata\n.balign 4\n"
                ".global persistence_blob_start\n.global persistence_blob_end\n"
                "persistence_blob_start:\n.incbin \"/work/persistence\"\n"
                "persistence_blob_end:\n"
            )
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc",
                   "-DPROBE_PERSISTENCE_WRAPPER", "-DPROBE_CHILD_NO_PRELOAD",
                   *(["-DPROBE_CHILD_PERSISTENCE_OPEN_TRACE"]
                     if options.probe_persistence_open_trace else []),
                   "-O2", "-Wall", "-Wextra", "-Werror", "-o", "/work/persistence_copy",
                   f"/src/{(HERE / 'loader_copy.c').relative_to(AUDI)}",
                   "/work/persistence_blob.S")
        if options.probe_persistence_open_trace:
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc",
                   "-shared", "-fPIC", "-O2", "-Wall", "-Wextra", "-Werror",
                   "-o", "/work/persistence_open_trace.so",
                   f"/src/{(HERE / 'persistence_open_trace.c').relative_to(AUDI)}")
        if options.probe_factory_fec:
            fec = FIRMWARE / "eso/bin/apps/fec"
            shutil.copyfile(fec, work / "fec")
            (work / "fec_blob.S").write_text(
                ".section .rodata\n.balign 4\n"
                ".global fec_blob_start\n.global fec_blob_end\n"
                "fec_blob_start:\n.incbin \"/work/fec\"\n"
                "fec_blob_end:\n"
            )
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc",
                   "-DPROBE_FEC_WRAPPER", "-DPROBE_CHILD_NO_PRELOAD",
                   "-O2", "-Wall", "-Wextra", "-Werror", "-o", "/work/fec_copy",
                   f"/src/{(HERE / 'loader_copy.c').relative_to(AUDI)}",
                   "/work/fec_blob.S")
        if options.probe_factory_diagnosis:
            diagnosis = FIRMWARE / "eso/bin/apps/diagnosis"
            shutil.copyfile(diagnosis, work / "diagnosis")
            (work / "diagnosis_blob.S").write_text(
                ".section .rodata\n.balign 4\n"
                ".global diagnosis_blob_start\n.global diagnosis_blob_end\n"
                "diagnosis_blob_start:\n.incbin \"/work/diagnosis\"\n"
                "diagnosis_blob_end:\n"
            )
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc",
                   "-DPROBE_DIAGNOSIS_WRAPPER", "-DPROBE_CHILD_NO_PRELOAD",
                   "-O2", "-Wall", "-Wextra", "-Werror", "-o", "/work/diagnosis_copy",
                   f"/src/{(HERE / 'loader_copy.c').relative_to(AUDI)}",
                   "/work/diagnosis_blob.S")
        if options.probe_bluetooth and options.storage != "shmem":
            raise SystemExit("bluetooth probe requires --storage shmem")
        if options.probe_startup_config and options.storage != "shmem":
            raise SystemExit("factory startup probe requires --storage shmem")
        if options.probe_exec_path and not options.probe_startup_config:
            raise SystemExit("exec-path probe requires --probe-startup-config")
        if options.probe_regular_path and not options.probe_startup_config:
            raise SystemExit("regular-path probe requires --probe-startup-config")
        if options.probe_factory_bluetooth and not options.probe_startup_config:
            raise SystemExit("factory Bluetooth probe requires --probe-startup-config")
        if options.probe_factory_bluetooth_wrapper and not options.probe_factory_bluetooth:
            raise SystemExit("factory Bluetooth wrapper requires --probe-factory-bluetooth")
        if options.probe_bluetooth_mesh and not options.probe_factory_bluetooth_wrapper:
            raise SystemExit("Bluetooth mesh trace requires --probe-factory-bluetooth-wrapper")
        if options.probe_bluetooth_reg_trace and not options.probe_factory_bluetooth_wrapper:
            raise SystemExit("Bluetooth registration trace requires --probe-factory-bluetooth-wrapper")
        if options.probe_btstack_reg_trace and not options.probe_startup_config:
            raise SystemExit("btstack registration trace requires --probe-startup-config")
        if options.probe_graft_socket and not options.probe_startup_config:
            raise SystemExit("graft socket probe requires --probe-startup-config")
        if options.probe_daemon_bridge and not (
                options.variant == "graft" and options.storage == "shmem" and
                options.probe_startup_config and options.probe_synthetic_rfcomm and
                options.probe_bypass_persistence_gate and
                options.probe_factory_bluetooth_wrapper and
                not options.probe_graft_socket):
            raise SystemExit("daemon bridge probe requires synthetic graft/shmem, factory "
                             "Bluetooth wrapper, persistence-gate bypass, and no socket probe")
        if options.probe_synthetic_rfcomm and not (
                options.variant == "graft" and options.storage == "shmem" and
                (options.probe_graft_socket or options.probe_daemon_bridge) and
                options.probe_bypass_persistence_gate and
                options.probe_factory_bluetooth_wrapper):
            raise SystemExit("synthetic RFCOMM probe requires graft/shmem, socket probe, "
                             "factory Bluetooth wrapper, and persistence-gate bypass")
        if options.probe_bluetooth_trap is not None and not options.probe_factory_bluetooth_wrapper:
            raise SystemExit("Bluetooth trap requires --probe-factory-bluetooth-wrapper")
        if options.probe_bypass_persistence_gate and not options.probe_factory_bluetooth_wrapper:
            raise SystemExit("persistence-gate bypass requires --probe-factory-bluetooth-wrapper")
        if options.probe_bypass_proxy_alive_gate and not options.probe_bypass_persistence_gate:
            raise SystemExit("proxy-alive bypass requires --probe-bypass-persistence-gate")
        if options.probe_bypass_persistence_gate and options.probe_bluetooth_trap == 0x139DD0:
            raise SystemExit("persistence-gate bypass cannot be paired with its entry trap")
        if options.probe_factory_persistence and not options.probe_startup_config:
            raise SystemExit("factory persistence probe requires --probe-startup-config")
        if options.probe_factory_persistence_config and not options.probe_factory_persistence:
            raise SystemExit("factory persistence config requires --probe-factory-persistence")
        if options.probe_local_persistence and not options.probe_factory_persistence_config:
            raise SystemExit("local persistence requires --probe-factory-persistence-config")
        if options.probe_persistence_open_trace and not options.probe_factory_persistence:
            raise SystemExit("persistence open trace requires --probe-factory-persistence")
        if options.probe_factory_fec and not options.probe_startup_config:
            raise SystemExit("factory FEC probe requires --probe-startup-config")
        if options.probe_factory_diagnosis and not options.probe_startup_config:
            raise SystemExit("factory diagnosis probe requires --probe-startup-config")
        if options.qemu_timeout is not None and options.qemu_timeout <= 0:
            raise SystemExit("QEMU diagnostic duration must be positive")
        if options.probe_slog_stream and not options.probe_startup_config:
            raise SystemExit("slog stream probe requires --probe-startup-config")
        if options.probe_static_wrapper and not options.probe_startup_config:
            raise SystemExit("static-wrapper probe requires --probe-startup-config")
        if options.probe_child_libpath and not options.probe_startup_config:
            raise SystemExit("child-libpath probe requires --probe-startup-config")
        if options.probe_keep_child_preload and not options.probe_startup_config:
            raise SystemExit("child-preload negative control requires --probe-startup-config")
        manifest(work, btstack, options.storage, options.probe_launcher,
                 options.probe_bluetooth, options.probe_startup_config,
                 options.probe_exec_path, options.probe_regular_path,
                 options.probe_factory_bluetooth,
                 options.probe_factory_bluetooth_wrapper,
                 options.probe_factory_persistence,
                 options.probe_factory_persistence_config,
                 options.probe_local_persistence,
                 options.probe_persistence_open_trace,
                 options.probe_factory_fec,
                 options.probe_factory_diagnosis,
                 options.probe_bluetooth_reg_trace,
                 options.probe_btstack_reg_trace,
                 options.probe_graft_socket,
                 options.probe_daemon_bridge,
                 options.probe_slog_stream)
        docker(work, "mkifs", "loader.build", "loader.bin")
        print(f"QEMU IFS size: {(work / 'loader.bin').stat().st_size} bytes")
        if options.build_only:
            listing = docker(work, "dumpifs", "-v", "/work/loader.bin",
                             capture_output=True).stdout
            for line in listing.splitlines():
                if "btstack_copy" in line or "/eso/bin/apps/btstack" in line:
                    print(line)
            return
        if options.storage == "ifs":
            listing = docker(work, "dumpifs", "-v", "/work/loader.bin",
                             capture_output=True).stdout
            inspection = work / "inspection"
            inspection.mkdir()
            for line in listing.splitlines():
                match = re.match(r"^\s+[0-9a-f]+\s+[0-9a-f]+\s+([^\s]+)", line)
                if match and not match.group(1).startswith(("Startup-", "Image-", "Root-", "*.")):
                    (inspection / match.group(1)).parent.mkdir(parents=True, exist_ok=True)
            docker(work, "sh", "-c", "cd /work/inspection && dumpifs -x /work/loader.bin",
                   capture_output=True)
            shutil.copyfile(inspection / "proc/boot/btstack",
                            out / f"qemu_loader_{options.variant}_ifs_extracted.elf")
        suffix = ("_launcher" if options.probe_launcher else "") + (
            "_bluetooth" if options.probe_bluetooth else "") + (
            "_startup" if options.probe_startup_config else "") + (
            "_path" if options.probe_exec_path else "")
        suffix += "_regular" if options.probe_regular_path else ""
        suffix += "_factory_bluetooth" if options.probe_factory_bluetooth else ""
        suffix += "_wrapped" if options.probe_factory_bluetooth_wrapper else ""
        suffix += "_persistence" if options.probe_factory_persistence else ""
        suffix += "_pconfig" if options.probe_factory_persistence_config else ""
        suffix += "_localps" if options.probe_local_persistence else ""
        suffix += "_ps_open" if options.probe_persistence_open_trace else ""
        suffix += "_fec" if options.probe_factory_fec else ""
        suffix += "_diagnosis" if options.probe_factory_diagnosis else ""
        suffix += "_bluetooth_mesh" if options.probe_bluetooth_mesh else ""
        suffix += "_reg_trace" if options.probe_bluetooth_reg_trace else ""
        suffix += "_btreg_trace" if options.probe_btstack_reg_trace else ""
        suffix += "_socket_probe" if options.probe_graft_socket else ""
        suffix += "_daemon_bridge" if options.probe_daemon_bridge else ""
        suffix += "_synthetic_rfcomm" if options.probe_synthetic_rfcomm else ""
        suffix += "_slog" if options.probe_slog_stream else ""
        suffix += (f"_trap_{options.probe_bluetooth_trap:x}"
                   if options.probe_bluetooth_trap is not None else "")
        suffix += "_bypass_persistence" if options.probe_bypass_persistence_gate else ""
        suffix += "_bypass_proxy" if options.probe_bypass_proxy_alive_gate else ""
        suffix += "_static" if options.probe_static_wrapper else ""
        suffix += "_libpath" if options.probe_child_libpath else ""
        suffix += "_with_preload" if options.probe_keep_child_preload else ""
        log = out / f"qemu_loader_{options.variant}_{options.storage}{suffix}.log"
        with log.open("w") as stream:
            command = [
                str(QEMU), "-M", "virt", "-m", "2048", "-cpu", "cortex-a15",
                "-device", f"loader,file={work / 'loader.bin'},"
                "addr=0x40200000,force-raw=on,cpu-num=0",
                "-display", "none", "-serial", "stdio", "-monitor", "none",
            ]
            try:
                subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT,
                               timeout=options.qemu_timeout or (
                                   50 if (options.probe_bluetooth_trap is not None or
                                          options.probe_bluetooth_reg_trace or
                                          options.probe_btstack_reg_trace) else 70),
                               check=False)
            except subprocess.TimeoutExpired:
                pass
    output = log.read_text(errors="replace")
    if "=== btstack loader diagnostic ===" not in output:
        raise SystemExit(f"guest did not reach loader diagnostic; log: {log}")
    print(f"{options.variant} QEMU loader log: {log}")
    if options.probe_persistence_open_trace:
        opened = re.findall(r"PSTRACE open64 path=([^ ]+) flags=(0x[0-9a-f]+) fd=(-?\d+)", output)
        if not opened:
            raise SystemExit(f"persistence open interposer produced no relevant calls; log: {log}")
        print(f"persistence file opens: {opened[:20]}")
    staged_program = "btstack" if options.storage == "ifs" else "btstack_copy"
    diagnostic = output.split("=== btstack loader diagnostic ===", 1)[1].split(
        "=== btstack loader diagnostic end ===", 1
    )[0]
    if options.probe_startup_config:
        if options.probe_exec_path:
            marker = "=== late graft state probe ==="
            if marker not in output or "graft-state: pid=" not in output.split(marker, 1)[1]:
                raise SystemExit(f"direct-path btstack process not proven; log: {log}")
            print(f"direct path spawned {options.variant} btstack; process-state probe succeeded")
        else:
            launched = re.search(r"\[SP:\d+\] spawn result=(\d+) path=/eso/bin/apps/btstack", output)
            sampled = re.search(
                r"\[SP:\d+\] prior btstack child=(\d+) as_fd=(\d+) errno=0 "
                r"graft_read=(-?\d+) once=(\d+) status=0x([0-9a-f]+)", output)
            if not launched or not sampled or launched.group(1) != sampled.group(1):
                raise SystemExit(f"factory-spawned btstack mapping not proven; log: {log}")
            print(f"factory launcher spawned {options.variant} btstack PID {launched.group(1)}; "
                  "process address space was readable")
    else:
        spawn_error = re.search(rf'Unable to start "/proc/boot/{staged_program}" \((\d+)\)', diagnostic)
        if spawn_error:
            number = spawn_error.group(1)
            raise SystemExit(f"guest rejected {staged_program} from {options.storage} "
                             f"(errno {number}); log: {log}")
        if "btstack-copy: exec errno=" in diagnostic:
            raise SystemExit(f"guest loader rejected copied {options.variant} btstack; log: {log}")
        if "unknown symbol:" in diagnostic or "ldd:FATAL" in diagnostic:
            raise SystemExit(f"guest dynamic linker rejected {options.variant} btstack; log: {log}")
        process_name = "btstack" if options.storage == "ifs" else "btstack-loader-probe"
        terminated = re.search(rf"Process \d+ \({process_name}\) terminated", diagnostic)
        displayed_name = "stack-loader-probe" if options.storage == "shmem" else process_name
        alive = re.search(rf"^\s*\d+\s+\d+\s+{displayed_name}\s+", diagnostic, re.M)
        if not (terminated or alive):
            raise SystemExit(f"no evidence that {options.variant} btstack reached startup; log: {log}")
        state = "alive at checkpoint" if alive else "terminated after startup"
        print(f"{options.variant} btstack reached startup in QEMU ({state}); radio state is not validated")
    if options.probe_factory_bluetooth:
        bluetooth_spawn = re.search(
            r"\[SP:\d+\] spawn result=(\d+) path=/eso/bin/apps/bluetooth", output)
        if not bluetooth_spawn:
            raise SystemExit(f"factory Bluetooth client spawn not proven; log: {log}")
        print(f"factory launcher spawned Bluetooth client PID {bluetooth_spawn.group(1)}")
        if options.probe_factory_bluetooth_wrapper:
            bluetooth_sample = re.search(
                r"\[SP:\d+\] prior bluetooth child=(\d+) as_fd=\d+ "
                r"errno=0 stage=X read=1", output)
            if (options.probe_bluetooth_trap is None and
                    (not bluetooth_sample or bluetooth_sample.group(1) != bluetooth_spawn.group(1))):
                raise SystemExit(f"factory Bluetooth client mapping not proven; log: {log}")
            crashed = re.search(
                rf"\[SP:\d+\] waitid bluetooth={bluetooth_spawn.group(1)} "
                r"result=0 code=2 status=11", output)
            if crashed:
                raise SystemExit(f"factory Bluetooth client SIGSEGV after mapping; log: {log}")
            if options.probe_bluetooth_trap is None:
                print("factory Bluetooth client reached its ELF and remained readable at checkpoint")
            if options.probe_bluetooth_trap is not None:
                trapped = re.search(
                    rf"\[SP:\d+\] waitid bluetooth={bluetooth_spawn.group(1)} "
                    r"result=0 code=2 status=4", output)
                if not trapped:
                    raise SystemExit(
                        f"Bluetooth gate {options.probe_bluetooth_trap:#x} was not reached; log: {log}")
                print(f"Bluetooth gate {options.probe_bluetooth_trap:#x} reached QEMU-only UDF")
    if options.probe_factory_fec:
        fec_spawn = re.search(r"\[SP:\d+\] spawn result=(\d+) path=\./bin/apps/fec", output)
        fec_exec = "spawn path=/dev/shmem/fec-loader-probe" in output
        if not fec_spawn or not fec_exec:
            raise SystemExit(f"factory FEC exact-ELF launch not proven; log: {log}")
        print(f"factory service manager spawned exact FEC ELF PID {fec_spawn.group(1)}")
    if options.probe_factory_diagnosis:
        diagnosis_spawn = re.search(
            r"\[SP:\d+\] spawn result=(\d+) path=\./bin/apps/diagnosis", output)
        diagnosis_exec = "spawn path=/dev/shmem/diagnosis-loader-probe" in output
        if not diagnosis_spawn or not diagnosis_exec:
            raise SystemExit(f"factory diagnosis exact-ELF launch not proven; log: {log}")
        print(f"factory service manager spawned exact diagnosis ELF PID {diagnosis_spawn.group(1)}")
    if options.probe_bluetooth_reg_trace:
        registrations = re.findall(
            r"REGTRACE call=(\d+) self=([0-9a-fx]+) result=(0x[0-9a-f]+)",
            output)
        if not registrations:
            raise SystemExit(f"Bluetooth registration interposer produced no calls; log: {log}")
        print(f"Bluetooth registration results: {registrations}")
    if options.probe_btstack_reg_trace:
        registrations = re.findall(
            r"BTREGTRACE call=(\d+) self=([0-9a-fx]+) result=(0x[0-9a-f]+)",
            output)
        if not registrations:
            raise SystemExit(f"btstack registration interposer produced no calls; log: {log}")
        print(f"btstack registration results: {registrations}")
    if options.probe_graft_socket:
        if options.probe_synthetic_rfcomm:
            if "[graft-socket-probe] synthetic_data=1" not in output:
                raise SystemExit(f"synthetic RFCOMM data did not reach socket; log: {log}")
            print("synthetic RFCOMM data reached loopback peer")
        else:
            accepts = [int(value) for value in re.findall(
                r"\[graft-socket-probe\] accepted=(\d+)", output)]
            if ("[graft-socket-probe] listening" not in output or
                    not any(first >= 2 and second == first + 1
                            for first, second in zip(accepts, accepts[1:]))):
                raise SystemExit(f"graft worker did not reconnect after socket close; log: {log}")
            print(f"graft worker loopback connections: {accepts}")
    if options.probe_daemon_bridge:
        for marker in ("[bridge] listening on 127.0.0.1:57342",
                       "[bridge] iAP2 marker received from RFCOMM",
                       "graft reply flags=0x00000007"):
            if marker not in output:
                raise SystemExit(f"graft-to-daemon marker absent: {marker}; log: {log}")
        print("synthetic RFCOMM callback and ordinary QNX daemon exchanged fragmented iAP2 marker/SYN")
    if options.variant == "graft" and options.storage == "shmem":
        report = json.loads((HERE / "out" / (
            "build_report.iap2-qemu.json" if options.probe_daemon_bridge else
            "build_report.synthetic.json" if options.probe_synthetic_rfcomm
            else "build_report.json")).read_text())
        code_offset = int(report["code_file_offset"], 16)
        expected_code = btstack.read_bytes()[code_offset:code_offset + 16].hex()
        probe_text = diagnostic
        if options.probe_bluetooth or options.probe_startup_config:
            if options.probe_startup_config and not options.probe_exec_path:
                probe_text = output
            else:
                marker = "=== late graft state probe ==="
                if marker not in output:
                    raise SystemExit(f"QEMU probe has no late state sample; log: {log}")
                probe_text = output.split(marker, 1)[1]
        if f"graft-code:{expected_code}" not in probe_text:
            raise SystemExit(f"graft code segment does not match built bytes; log: {log}")
        if options.probe_startup_config and not options.probe_exec_path:
            graft_state = re.search(
                r"\[SP:\d+\] prior btstack child=\d+ as_fd=\d+ errno=0 "
                r"graft_read=8 once=(\d+) status=0x([0-9a-f]+)", probe_text)
        else:
            graft_state = re.search(r"graft-state: pid=\d+ once=(\d+) status=0x([0-9a-f]+)", probe_text)
        if not graft_state:
            raise SystemExit(f"graft state segment was not readable; log: {log}")
        print(f"graft code mapped; state once={graft_state.group(1)}, status=0x{graft_state.group(2)}")
        late_states = re.findall(
            r"btstack state sample=\d+ once=\d+->(\d+) "
            r"status=0x[0-9a-f]+->0x([0-9a-f]+) channel=(\d+)",
            output)
        if late_states:
            once, status, channel = late_states[-1]
            print(f"graft runtime state once={once}, status=0x{status}, RFCOMM channel={channel}")
        if (options.probe_bypass_persistence_gate and
                not options.probe_bypass_proxy_alive_gate and
                options.probe_bluetooth_trap is None):
            if not late_states or late_states[-1][0] != "1" or \
                    int(late_states[-1][1], 16) != 0 or \
                    not 1 <= int(late_states[-1][2]) <= 6:
                raise SystemExit(f"QEMU did not prove graft initialization; log: {log}")
    if options.probe_bluetooth:
        bluetooth_diagnostic = output.split("=== bluetooth client diagnostic ===", 1)[1].split(
            "=== bluetooth client diagnostic end ===", 1
        )[0]
        error = re.search(r'Unable to start "/proc/boot/bluetooth" \((\d+)\)', bluetooth_diagnostic)
        if error:
            raise SystemExit(f"bluetooth client failed to spawn (errno {error.group(1)}); log: {log}")
        if "PersistenceServicesLifeCycleListener::PersistenceServicesLifeCycleListener: entered" not in bluetooth_diagnostic:
            raise SystemExit(f"bluetooth client did not reach initialization; log: {log}")
        print("factory bluetooth client entered initialization; ASI startBtStack call is not validated")
    for line in output.splitlines():
        if "btstack" in line.lower() or "library" in line.lower() or "graft-state" in line or "graft-code" in line:
            print(line[:240])


if __name__ == "__main__":
    main()
