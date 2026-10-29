#!/usr/bin/env python3
"""Native SI stages 1/2 in an isolated, bounded QNX QEMU process.

Builds a minimal QNX 6.5 IFS (own work dir, own QEMU process — the running
vphone and full-MHI2Q stands are not touched) and checks, inside the guest:

  1. the graft candidate loads: the ELF with the inplace PHDR table + R+X hook
     segment is accepted by the real QNX 6.5 loader;
  2. the spawn-injection contour: a parent preloaded with si_adapter.so calls
     QNX spawn(); ABI 1 injects the explicit wireless fixture override; ABI 2
     selects wired before native WLAN admission. The other child stays clean;
  3. the candidate SI boots far enough for the adapter to publish the slot
     table (STATUS published=1) and an ENQ NOOP job travels the SI controller
     queue and is deleted by native SI;
  4. --stage2 executes SIW2 discovery/replay/rename/remove/reconnect and a stale
     epoch against native C++ vectors. UTF-8 bytes are read back from SI memory.
     Missing coding providers must deny admission, leaving no media child.

The default native Service Manager owns SI init/run; --diagnose is a separate
external-debugger profile, not an acceptance success. Firmware and user VMs
are never modified. Delivery uses compressed non-ELF data plus QNX4 readback.

Run: python3 wireless/si/qemu_stage1.py --stage2 --workdir build/si-qemu
Prerequisites: docker image qnx65-armv7-toolchain:latest, Tools/qemu-src,
Tools/bsp-qemu-arm/extract, Tools/qnx-65-sdp-docker, MU1316 firmware tree.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import zlib
import re
import shlex
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
AUDI = REPO.parents[1]
TOOLS = AUDI / "Tools"
QEMU = TOOLS / "qemu-src/build/qemu-system-arm"
BSP = TOOLS / "bsp-qemu-arm/extract"
SDP_QEMU = TOOLS / "qnx-65-sdp-docker/tests/qemu"
IMAGE = "qnx65-armv7-toolchain:latest"
FW = (AUDI / "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced")
ESO_LIB = FW / "MU1316-appimg/eso/lib"
PRODUCTION = FW / "MU1316-system/etc/eso/production"
EMS_TABLES = FW / "MU1316-appimg/eso/ems_tables"
STAGE1 = REPO / "build/si-wireless-stage1"
WORK = STAGE1 / "qemu-run"
SDP_LIB = "/opt/qnx650/target/qnx6/armle-v7/lib"

ESO_LIBS = ["libosal.so", "libutil.so", "libcomm.so", "libiplcommon.so",
            "libdsicommon.so", "libservmngt.so", "libirc_mmx_adapter.so"]
SDP_LIBS = ["libm.so.2", "libsocket.so.3", "libecpp-ne.so.4"]
TARGET_LIBS = {"libc.so.3": "/audi/" + str(
    (FW / "MU1316-appimg/armle/lib/libc.so.3").relative_to(AUDI))}

TIMEOUT_S = 150
SI_ARGS = "-sm-name smartphone_integrator -sm-id 353 -sm-time 1 -sm-restart 0"
# SpawnFactory/servmngt support also used by the existing disposable btstack
# mesh fixture (wireless/graft/qemu_loader.py). Do not embed all 114 MB of
# unrelated factories into this bootstrap; large late IFS entries fail I/O.
MANAGER_FACTORIES = {
    "lib_03171582-f255-4991-b8ae-83f90521c08b.so",
    "lib_11eaa58a-7811-47de-9981-4f8914ce7c1e.so",
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
    "libservmngtproxy.so", "libservmngtextensionsproxy.so",
}


def docker(script: str, capture: bool = False) -> str:
    result = subprocess.run(
        ["docker", "run", "--rm", "--platform=linux/amd64",
         "-v", f"{REPO}:/src", "-v", f"{AUDI}:/audi", IMAGE,
         "bash", "-c", script],
        check=True, capture_output=capture, text=True)
    return result.stdout if capture else ""


def verify_payload_delivery(work_source: str, payloads: dict) -> None:
    """Check the files actually embedded by mkifs, before starting QEMU."""
    listing = docker(f"dumpifs -v {shlex.quote(work_source + '/si_stage1.bin')}", capture=True)
    extracted = WORK / "extracted"
    extracted.mkdir(exist_ok=True)
    for line in listing.splitlines():
        match = re.match(r"^\s+[0-9a-f]+\s+[0-9a-f]+\s+([^\s]+)", line)
        if match and not match[1].startswith(("Startup-", "Image-", "Root-", "*.")):
            relative = Path(match[1])
            if relative.is_absolute() or ".." in relative.parts:
                raise ValueError(f"unsafe dumpifs entry: {relative}")
            (extracted / relative).parent.mkdir(parents=True, exist_ok=True)
    docker(f"cd {shlex.quote(work_source + '/extracted')} && "
           f"dumpifs -x {shlex.quote(work_source + '/si_stage1.bin')}", capture=True)
    for name, record in payloads.items():
        packed = (extracted / "etc" / (name + ".zlib")).read_bytes()
        if hashlib.sha256(packed).hexdigest() != record["packed_sha256"]:
            raise ValueError(f"mkifs changed compressed {name} payload")
        plain = zlib.decompress(packed)
        if len(plain) != record["size"] or hashlib.sha256(plain).hexdigest() != record["sha256"]:
            raise ValueError(f"extracted {name} payload differs from source")
    print(f"IFS payload extraction: SHA-256 matches all {len(payloads)} original inputs")


def build_guest_and_ifs(candidate_override: str | None = None,
                        no_adapter: bool = False, diagnose: bool = False,
                        debug_broker: bool = False, with_services: bool = False,
                        service_manager: bool = False, stage2: bool = False,
                        vehicle_providers: bool = False,
                        carplay_coded: bool = False,
                        rcc_fec_fixture: bool = False,
                        receiver_hook: Path | None = None, peer_health: bool = False,
                        integrated: bool = False) -> None:
    WORK.mkdir(parents=True, exist_ok=True)
    artifact_dir = REPO / "build" / ("si-wireless-stage2" if stage2 else "si-wireless-stage1")
    cand = Path(candidate_override) if candidate_override else (
        artifact_dir / "smartphone_integrator.si_wireless_stage1")
    # Use the same production adapter as HU, never an older diagnostic shim.
    adapter = artifact_dir / "libsi_adapter.so"
    if not cand.exists() or not adapter.exists():
        raise SystemExit("build the candidate and adapter first "
                         "(build_candidate.py --apply; build_si_adapter.sh)")

    fw = "/audi/" + str(FW.relative_to(AUDI))
    stage_dir = "/src/build/si-wireless-stage1"
    eso_files = "\n".join(f"/eso/lib/{l}={fw}/MU1316-appimg/eso/lib/{l}" for l in ESO_LIBS)
    sdp_files = "\n".join(f"/proc/boot/{l}={SDP_LIB}/{l}" for l in SDP_LIBS)
    sdp_files += "\n" + "\n".join(f"/proc/boot/{l}={src}" for l, src in TARGET_LIBS.items())
    fw_eso = "/audi/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib"
    trace_libs = ["libtracing.so", "libprobes.so", "libprobesTracingBackend.so", "libipte.so"]
    sdp_files += "\n" + "\n".join(f"/eso/lib/{l}={fw_eso}/{l}" for l in trace_libs)
    ems = "\n".join(
        f"/etc/eso/ems_tables/{f}={fw}/MU1316-appimg/eso/ems_tables/{f}"
        for f in sorted(p.name for p in EMS_TABLES.glob("*.json")))
    prod = "\n".join(
        f"/etc/eso/production/{f}={fw}/MU1316-system/etc/eso/production/{f}"
        for f in sorted(p.name for p in PRODUCTION.glob("*.json")))

    # Non-ELF compressed bytes bypass mkifs ELF normalisation entirely.
    payloads = {}
    for name, source in (("si", cand), ("adapter", adapter)):
        raw = source.read_bytes()
        packed = zlib.compress(raw, 9)
        (WORK / (name + ".zlib")).write_bytes(packed)
        payloads[name] = {"size": len(raw), "crc32": f"{zlib.crc32(raw):08x}",
                          "sha256": hashlib.sha256(raw).hexdigest(),
                          "packed_sha256": hashlib.sha256(packed).hexdigest()}
    (WORK / "payloads.json").write_text(json.dumps(payloads, indent=2) + "\n")
    (WORK / "probe_ready.sh").write_text(
        '#!/proc/boot/sh\nset -eu\n'
        'read si_pid < /ramdisk/var/run/smartphone_integrator.pid\n'
        'exec /proc/boot/si_live_probe "$si_pid"\n')

    if service_manager:
        framework = (PRODUCTION / "framework.json").read_text()
        old_exec = '"exec":"./bin/apps/smartphone_integrator"'
        if framework.count(old_exec) != 1:
            raise ValueError("SI framework entry changed")
        (WORK / "framework.json").write_text(framework.replace(
            old_exec, '"exec":"/proc/boot/si_wrapper.sh"'))
        config = json.loads((PRODUCTION / "servicemgr.json").read_text())
        config["startup"] = [
            {"cmd": "init", "app": "smartphone_integrator", "apprunmode": 0,
             "ps_access": "PS_ACC_READ", "timeout": 20000, "blocking": True},
            {"cmd": "run", "app": "smartphone_integrator", "timeout": 20000, "blocking": True},
        ]
        (WORK / "servicemgr.json").write_text(json.dumps(config, indent=2) + "\n")
        (WORK / "sm_disable_wd").write_text("")
        (WORK / "si_wrapper.sh").write_text(
            '#!/proc/boot/sh\n'
            'export LD_LIBRARY_PATH=/eso/lib:/proc/boot\n'
            f'export LD_PRELOAD={"" if no_adapter else "/ramdisk/libsi_adapter.so"}\n'
            'export MHI2Q_SI_HOOK=1 MHI2Q_SI_HOOK_VERBOSE=1 MHI2Q_SI_HOOK_TRACK_JOBS=1\n'
            f'export MHI2Q_SI_HOOK_WIRELESS={1 if stage2 else 0}\n'
            'exec /ramdisk/smartphone_integrator "$@"\n')
        (WORK / "si_wrapper.sh").chmod(0o755)

    buildfile = f"""
[linker="(40;1;)sh /src/wireless/si/linker_wrapper.sh qcc -bootstrap -nostdlib -Wl,--no-keep-memory -Wl,--trace -Wl,-Map,/tmp/startup.map -Vgcc_nto%(m==40,arm%)%(m!=3,%(m!=6,%(e==0, -EL%)%(e==1, -EB%)%)%)%(h!=0, -Wl,-Ttext -Wl,0x%t%)%(d!=0, -Wl,-Tdata -Wl,0x%d%) -o%o %i %[M -L%^i -Wl,-uinit_%n -lmod_%n%] /audi/Tools/qnx-65-sdp-docker/tests/qemu/libstartup.a -L/opt/qnx650/target/qnx6/armle-v7/lib -L/opt/qnx650/target/qnx6/armle-v7/usr/lib -llzo -lucl -ldrvr"]
[linker="(40;2;)cp %i %o"]
[image=0x40200000]
[virtual=armle-v7,raw] .bootstrap = {{
    /audi/Tools/qnx-65-sdp-docker/tests/qemu/startup-virt -S
    /audi/Tools/bsp-qemu-arm/extract/procnto-smp
}}
[+script] .script = {{
    PATH=/proc/boot:/bin
    LD_LIBRARY_PATH=/proc/boot
    procmgr_symlink ../../proc/boot/libc.so.3 /usr/lib/ldqnx.so.2
    /proc/boot/slogger -s 65536 &
    /proc/boot/devc-serdebug -F -S
    waitfor /dev/ser1 4
    reopen /dev/ser1
    display_msg "=== SI-STAGE1 BOOT ==="
    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=32768 blk cache=256k
    waitfor /dev/hd0t77 8
    /proc/boot/mount -t qnx4 /dev/hd0t77 /ramdisk
    /proc/boot/mkdir -p /ramdisk/var/run
    /proc/boot/si_payload /etc/si.zlib /ramdisk/smartphone_integrator {payloads["si"]["size"]} {payloads["si"]["crc32"]} /etc/adapter.zlib /ramdisk/libsi_adapter.so {payloads["adapter"]["size"]} {payloads["adapter"]["crc32"]}

    display_msg "--- mprotect-on-text probe ---"
    /proc/boot/si_mprotect_probe

    display_msg "--- spawn-injection contour ---"
    MHI2Q_SI_HOOK=1
    MHI2Q_SI_HOOK_VERBOSE=1
    MHI2Q_SI_HOOK_CHILD=carplay_startup.sh_probe
    MHI2Q_SI_HOOK_CHILD_ENVS=MHI2Q_DIO_OWNER_MODE=wireless
    IPL_CONFIG_DIR_SMARTPHONE_INTEGRATOR=/etc/eso/production
    LD_PRELOAD=/ramdisk/libsi_adapter.so
    /proc/boot/si_spawn_env_test
    LD_PRELOAD=
    MHI2Q_SI_HOOK_CHILD=carplay_startup.sh
    MHI2Q_SI_HOOK_CHILD_ENVS=

    display_msg "--- service env: io-pkt, pps, comm broker ---"
    CACHE_NOP=1 /proc/boot/io-pkt-v4-hc
    waitfor /dev/socket 8
    /proc/boot/ifconfig lo0 alias 10.0.0.15
    /proc/boot/pps
    waitfor /pps 4
    procmgr_symlink /pps /ramdisk/pps
    LD_LIBRARY_PATH=/eso/lib:/proc/boot
    IPL_CONFIG_DIR=/config
    LD_PRELOAD=
    /eso/lib/broker --node mmx --name broker &
    sleep 1

    display_msg "--- candidate SI from verified writable filesystem ---"
    IPL_CONFIG_DIR_SMARTPHONE_INTEGRATOR=/etc/eso/production
    IPL_CONFIG_DIR_SMARTPHONE-VERSIONS=/etc/eso/production
    CU_SMARTPHONE_ROOT=/eso/bin/apps/customerupdate_smartphone
    CU_SMARTPHONE_AVAILABLE=/var/customerUpdateSmartphoneAvailable
    USB_HOST_LIB_PATH=/lib/libusbdi.so
    USB_DEVICE_LIB_PATH=/mnt/app/armle/lib/libusbdci.so
    MHI2Q_SI_HOOK=1
    MHI2Q_SI_HOOK_VERBOSE=1
    MHI2Q_SI_HOOK_TRACK_JOBS=1
    MHI2Q_SI_HOOK_ROOT=0x1C5E00
    LD_LIBRARY_PATH=/eso/lib:/proc/boot
    LD_PRELOAD=@@ADAPTER_LINE@@
    /ramdisk/smartphone_integrator {SI_ARGS} &
    /proc/boot/si_stage1_client --require-delete
    /proc/boot/sh /etc/probe_ready.sh
    /proc/boot/pidin ar
    /proc/boot/ls /dev/eso
    sleep 1
    display_msg "=== SLOG DUMP ==="
    /proc/boot/sloginfo
    display_msg "=== SLOG END ==="
    display_msg "=== SI-STAGE1 DONE ==="
}}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[type=link] /tmp=/dev/shmem

/proc/boot/procnto-smp=/audi/Tools/bsp-qemu-arm/extract/procnto-smp
/proc/boot/startup-virt=/audi/Tools/qnx-65-sdp-docker/tests/qemu/startup-virt
/proc/boot/devc-serdebug=/audi/Tools/bsp-qemu-arm/extract/devc-serdebug
/proc/boot/waitfor=/audi/Tools/bsp-qemu-arm/extract/waitfor
/proc/boot/sleep=/audi/Tools/bsp-qemu-arm/extract/sleep
/proc/boot/ls=/audi/Tools/bsp-qemu-arm/extract/ls
/proc/boot/cat=/audi/Tools/bsp-qemu-arm/extract/cat
/proc/boot/pidin=/audi/Tools/bsp-qemu-arm/extract/pidin
/proc/boot/sh=/audi/Tools/bsp-qemu-arm/extract/sh
/proc/boot/cp=/audi/Tools/bsp-qemu-arm/extract/cp
/proc/boot/libusbdi.so=/audi/Tools/mibsdk/pkg/lib/libusbdi.so.2
/proc/boot/chmod=/audi/Tools/bsp-qemu-arm/extract/chmod
/proc/boot/slogger=/audi/Tools/bsp-qemu-arm/extract/slogger
/proc/boot/io-pkt-v4-hc=/opt/qnx650/target/qnx6/armle-v7/sbin/io-pkt-v4-hc
/proc/boot/ifconfig=/opt/qnx650/target/qnx6/armle-v7/sbin/ifconfig
/proc/boot/pps=/opt/qnx650/target/qnx6/armle-v7/usr/sbin/pps
/eso/lib/broker=/audi/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/bin/broker
[type=dir] /ramdisk
[type=dir] /etc/eso
/proc/boot/libusbdci.so={fw}/MU1316-appimg/armle/lib/libusbdci.so
[type=link] /mnt/app/armle/lib/libusbdci.so=/proc/boot/libusbdci.so
[type=dir] /mnt/app/armle/lib
[type=dir] /var
[type=dir] /eso/lib/factories
/eso/lib/factories/lib_0b144178-dd14-11e3-a49b-000c29a978ce.so=/audi/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib/factories/lib_0b144178-dd14-11e3-a49b-000c29a978ce.so
/eso/lib/factories/lib_24bd7282-5df9-11e4-a5cc-000c29a978ce.so=/audi/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib/factories/lib_24bd7282-5df9-11e4-a5cc-000c29a978ce.so
/eso/lib/factories/lib_2bd0e8ec-5df9-11e4-aa80-000c29a978ce.so=/audi/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib/factories/lib_2bd0e8ec-5df9-11e4-aa80-000c29a978ce.so
/eso/lib/factories/lib_b66bed86-0446-5574-bf7c-0a212b4857f5.so=/audi/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib/factories/lib_b66bed86-0446-5574-bf7c-0a212b4857f5.so
/eso/lib/factories/lib_c805d609-22ea-5bd6-96c1-cf22fc191981.so=/audi/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib/factories/lib_c805d609-22ea-5bd6-96c1-cf22fc191981.so
/eso/lib/factories/lib_eeae7562-dd13-11e3-b374-000c29a978ce.so=/audi/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib/factories/lib_eeae7562-dd13-11e3-b374-000c29a978ce.so
/eso/lib/factories/lib_8d4f7cec-be0c-49b7-b1b6-1e3e4c26b847.so=/audi/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib/factories/lib_8d4f7cec-be0c-49b7-b1b6-1e3e4c26b847.so
/eso/lib/factories/libbrokerproxy.so=/audi/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib/factories/libbrokerproxy.so
/eso/lib/factories/lib_14400653-60ea-4ff9-8eae-3230ce37f934.so={fw}/MU1316-appimg/eso/lib/factories/lib_14400653-60ea-4ff9-8eae-3230ce37f934.so
/config/framework.json={fw}/MU1316-system/etc/eso/production/framework.json
/config/probes.json={fw}/MU1316-system/etc/eso/production/probes.json
/proc/boot/sloginfo=/audi/Tools/bsp-qemu-arm/extract/sloginfo
{eso_files}
{sdp_files}
{prod}
{ems}

/etc/si.zlib={stage_dir}/qemu-run/si.zlib
/etc/adapter.zlib={stage_dir}/qemu-run/adapter.zlib
/etc/probe_ready.sh={stage_dir}/qemu-run/probe_ready.sh
/proc/boot/si_payload={stage_dir}/qemu-run/si_payload
/proc/boot/si_live_probe={stage_dir}/qemu-run/si_live_probe
/proc/boot/devb-ram=/opt/qnx650/target/qnx6/armle-v7/sbin/devb-ram
/proc/boot/mount=/opt/qnx650/target/qnx6/armle-v7/bin/mount
/proc/boot/mkdir=/opt/qnx650/target/qnx6/armle-v7/bin/mkdir
/proc/boot/io-blk.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/io-blk.so
/proc/boot/cam-disk.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/cam-disk.so
/proc/boot/fs-qnx4.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/fs-qnx4.so
/proc/boot/libcam.so.2=/opt/qnx650/target/qnx6/armle-v7/lib/libcam.so.2
/proc/boot/libz.so.2=/opt/qnx650/target/qnx6/armle-v7/usr/lib/libz.so.2
/proc/boot/si_mprotect_probe={stage_dir}/qemu-run/si_mprotect_probe
/proc/boot/si_stage1_client={stage_dir}/qemu-run/si_stage1_client
/proc/boot/si_spawn_env_test={stage_dir}/qemu-run/si_spawn_env_test
/proc/boot/carplay_startup.sh_probe={stage_dir}/qemu-run/si_spawn_env_child
/proc/boot/si_spawn_env_child={stage_dir}/qemu-run/si_spawn_env_child
/proc/boot/si_debug_launcher={stage_dir}/qemu-run/si_debug_launcher
"""
    if debug_broker:
        a = buildfile.index('    /eso/lib/broker --node')
        b = buildfile.index('    sleep 1', a)
        buildfile = buildfile[:a] + (
            '    SI_DEBUG_PRELOAD=/ramdisk/libsi_adapter.so\n'
            '    LD_PRELOAD=\n'
            '    /proc/boot/si_debug_launcher /eso/lib/broker --node mmx --name broker\n'
        ) + buildfile[b:]
    elif diagnose:
        if not with_services:
            a = buildfile.index('    display_msg "--- service env:')
            b = buildfile.index('    display_msg "--- candidate SI', a)
            buildfile = buildfile[:a] + buildfile[b:]
        buildfile = buildfile.replace(f'    /ramdisk/smartphone_integrator {SI_ARGS} &\n    /proc/boot/si_stage1_client --require-delete',
            '    SI_DEBUG_PRELOAD=@@ADAPTER_LINE@@\n    LD_PRELOAD=\n'
            f'    /proc/boot/si_debug_launcher /ramdisk/smartphone_integrator {SI_ARGS}')
    work_source = "/src/" + str(WORK.relative_to(REPO))
    buildfile = buildfile.replace(stage_dir + "/qemu-run/", work_source + "/")
    buildfile = buildfile.replace("@@ADAPTER_LINE@@",
        "/ramdisk/libsi_adapter.so" if not no_adapter else "")
    if service_manager:
        # The native manager must own the SI process and deliver init/run.
        a = buildfile.index('    display_msg "--- candidate SI')
        b = buildfile.index('    /proc/boot/si_stage1_client', a)
        buildfile = buildfile[:a] + (
            '    display_msg "--- native Service Manager owns SI startup ---"\n'
            '    /proc/boot/servicemgrmibhigh &\n'
            '    waitfor /dev/eso/servicemgr 10\n'
        ) + buildfile[b:]
        a = buildfile.index('/eso/lib/factories/lib_0b144178-')
        b = buildfile.index('/config/framework.json=', a)
        wanted = MANAGER_FACTORIES | {
            Path(line.split('=', 1)[0]).name for line in buildfile[a:b].splitlines()}
        buildfile = buildfile[:a] + "\n".join(
            f"/eso/lib/factories/{p.name}={fw}/MU1316-appimg/eso/lib/factories/{p.name}"
            for p in sorted((ESO_LIB / 'factories').glob('*.so')) if p.name in wanted
        ) + "\n" + buildfile[b:]
        buildfile = buildfile.replace(
            f'/config/framework.json={fw}/MU1316-system/etc/eso/production/framework.json',
            f'/config/framework.json={work_source}/framework.json')
        buildfile += (
            f'\n/proc/boot/servicemgrmibhigh={fw}/MU1316-appimg/eso/bin/servicemgrmibhigh\n'
            f'/proc/boot/si_wrapper.sh={work_source}/si_wrapper.sh\n'
            f'/config/servicemgr.json={work_source}/servicemgr.json\n'
            f'/var/sm_disable_wd={work_source}/sm_disable_wd\n')
        buildfile += "\n".join(
            f'/config/{p.name}={fw}/MU1316-system/etc/eso/production/{p.name}'
            for p in sorted(PRODUCTION.glob('*.json'))
            if p.name not in ('framework.json', 'servicemgr.json', 'probes.json')) + "\n"
    if no_adapter and service_manager:
        buildfile = buildfile.replace('    /proc/boot/si_stage1_client --require-delete',
                                     '    sleep 8')
    if stage2:
        if not no_adapter:
            buildfile = buildfile.replace('    /proc/boot/sh /etc/probe_ready.sh',
                '    /proc/boot/si_qemu_control_probe\n    /proc/boot/sh /etc/probe_ready.sh')
        buildfile += f'/proc/boot/si_qemu_control_probe={work_source}/si_qemu_control_probe\n'
        buildfile = buildfile.replace('    /proc/boot/mkdir -p /ramdisk/var/run',
            '    /proc/boot/mkdir -p /ramdisk/var/run /ramdisk/persist\n'
            '    procmgr_symlink /ramdisk/persist /mnt/persist\n'
            '    /proc/boot/pipe &\n'
            '    waitfor /dev/pipe 4\n'
            '    /proc/boot/random -t\n'
            '    procmgr_symlink /dev/random /dev/urandom')
        buildfile += ('/proc/boot/pipe=/opt/qnx650/target/qnx6/armle-v7/sbin/pipe\n'
                      '/proc/boot/random=/opt/qnx650/target/qnx6/armle-v7/usr/sbin/random\n')
    if vehicle_providers:
        from qemu_vehicle_providers import extend
        buildfile = extend(buildfile, WORK, FW, REPO, payloads,
                           carplay_coded=carplay_coded,
                           rcc_fec_fixture=rcc_fec_fixture)
        (WORK / "payloads.json").write_text(json.dumps(payloads, indent=2) + "\n")
    if rcc_fec_fixture:
        from qemu_si_dio import extend as extend_dio
        buildfile = extend_dio(buildfile, WORK, FW, REPO, payloads, receiver_hook, peer_health, integrated)
        (WORK / "payloads.json").write_text(json.dumps(payloads, indent=2) + "\n")
    # This bootstrap cannot reliably exec late ELF entries in a large IFS.
    # Keep every boot executable and its core libraries before the factories.
    lines = buildfile.splitlines()
    def core_entry(line: str) -> bool:
        return line.startswith(("/proc/boot/", "/eso/lib/lib", "/eso/lib/broker="))
    core = [line for line in lines if core_entry(line)]
    lines = [line for line in lines if not core_entry(line)]
    insertion = lines.index('[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3')
    # RAM IFS defaults to data=uip (one execution only). Shells/clients and
    # restart tests need reusable data segments; see QNX 6.5 mkifs docs.
    lines[insertion:insertion] = ["[data=copy]"] + core
    buildfile = "\n".join(lines) + "\n"
    (WORK / "si_stage1.build").write_text(buildfile)

    gcc = "arm-unknown-nto-qnx6.5.0eabi-gcc"
    for src, out, extra in (
        ("si_payload.c", "si_payload", "-lz"),
        ("si_qemu_control_probe.c", "si_qemu_control_probe",
         "-DSI_STAGE2 /src/wireless/si/si_wire.c -lsocket"),
        ("si_live_probe.c", "si_live_probe",
         f"-I/src/wireless/si -I/src/{artifact_dir.relative_to(REPO)} /src/wireless/si/si_runtime_guard.c"),
        ("si_mprotect_probe.c", "si_mprotect_probe", ""),
        ("si_stage1_client.c", "si_stage1_client", "-lsocket"),
        ("si_spawn_env_test.c", "si_spawn_env_test", ""),
        ("si_spawn_env_child.c", "si_spawn_env_child", ""),
        ("si_debug_launcher.c", "si_debug_launcher", ""),
    ):
        docker(f"cd /src && {gcc} -std=c11 -Os -Wall -Wextra -Werror "
               f"-o {shlex.quote(str(WORK.relative_to(REPO) / out))} "
               f"wireless/si/{src} {extra}")
    docker(f"cd {shlex.quote(work_source)} && "
           f"export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH && "
           f"mkifs si_stage1.build si_stage1.bin")
    verify_payload_delivery(work_source, payloads)
    firmware_inputs = {
        str(path.relative_to(AUDI)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in [FW / "MU1316-appimg/armle/lib/libc.so.3",
                     ESO_LIB / "libutil.so", ESO_LIB / "libcomm.so", ESO_LIB / "libservmngt.so",
                     FW / "MU1316-appimg/eso/bin/broker", FW / "MU1316-appimg/eso/bin/servicemgrmibhigh"]
    }
    build_receipt = {
        "abi": 2 if stage2 else 1, "no_adapter": no_adapter,
        "vehicle_providers": vehicle_providers,
        "carplay_coded": carplay_coded,
        "rcc_fec_fixture": rcc_fec_fixture,
        "receiver_hook_override": str(receiver_hook) if receiver_hook else None,
        "peer_health": peer_health,
        "diagnostic": diagnose or debug_broker,
        "native_service_manager": service_manager, "payloads": payloads,
        "elf_payload_delivery_sha256_verified": True,
        "firmware_inputs": firmware_inputs,
        "sources": {str(p.relative_to(REPO)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in sorted((REPO / "wireless/si").iterdir())
                    if p.suffix in (".c", ".h", ".py", ".sh", ".map")},
        "buildfile_sha256": hashlib.sha256((WORK / "si_stage1.build").read_bytes()).hexdigest(),
        "ifs_sha256": hashlib.sha256((WORK / "si_stage1.bin").read_bytes()).hexdigest(),
    }
    (WORK / "build-receipt.json").write_text(json.dumps(build_receipt, indent=2) + "\n")
    print(f"IFS: {WORK / 'si_stage1.bin'}")


KNOWN_EXPECTED = {
    # mprotect on text is refused by procnto 6.5 (EACCES) — documented finding;
    # the graft therefore never writes text at runtime.
    "mprotect_fail_expected": "MPROT:FAIL rw mprotect errno=13",
}
MARKERS = {
    "spawn_contour_ok": "CHILDENV:MHI2Q_DIO_OWNER_MODE=wireless",
    "spawn_negative_ok": "CHILDENV:MHI2Q_DIO_OWNER_MODE=(null)",
    "payload_verified": "SI-PAYLOAD: verified",
    "spawntest_done": "=== SPAWNTEST DONE ===",
    "client_status": "CLIENT: STATUS published=1",
    "client_enq": "CLIENT: ENQ accepted",
    "client_contour": "CLIENT: ENQ CONTOUR OK",
    "client_deleted": "CLIENT: NATIVE DELETE OK",
    "client_done": "=== CLIENT DONE OK ===",
    "adapter_published": "slot table published",
    "stage1_done": "=== SI-STAGE1 DONE ===",
}
FAILURE_MARKERS = ("ldd:FATAL", "Unable to open elf", "Unable to start", "SIGSEGV", "SIGILL",
                   "SIGABRT", "SI-PAYLOAD: FAIL", "SI-NATIVE: FAIL")


def run_qemu(timeout_s: int, dio_network: bool = False) -> str:
    log_path = WORK / "qemu_stage1.log"
    command = [
        str(QEMU), "-M", "virt", "-m", "1024", "-cpu", "cortex-a15,cntfrq=62500000",
        "-device", f"loader,file={WORK / 'si_stage1.bin'},"
                   "addr=0x40200000,force-raw=on,cpu-num=0",
        "-display", "none", "-serial", "stdio", "-monitor", "none",
    ]
    if dio_network:
        command += ['-netdev', 'user,id=si,net=10.0.3.0/24',
                    '-device', 'usb-net,netdev=si,mac=52:54:00:12:34:58']
    seen_all = False
    with log_path.open("w") as stream:
        process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + timeout_s
        try:
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    break
                try:
                    text = log_path.read_text(errors="replace")
                except FileNotFoundError:
                    time.sleep(0.2)
                    continue
                if MARKERS["stage1_done"] in text:
                    seen_all = True
                    time.sleep(1.0)
                    break
                time.sleep(0.5)
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
    text = log_path.read_text(errors="replace")
    if not seen_all:
        print(f"[warn] no {MARKERS['stage1_done']!r} within {timeout_s}s")
    return text


def acceptance_markers(stage2: bool, no_adapter: bool,
                       vehicle_providers: bool = False,
                       carplay_coded: bool = False,
                       rcc_fec_fixture: bool = False) -> dict[str, str]:
    markers = dict(MARKERS)
    if stage2:
        # Before publication no native WLAN context exists: ABI 2 must select
        # wired for the matched fixture child. It cannot blindly inject WLAN.
        markers["spawn_contour_ok"] = "CHILDENV:MHI2Q_DIO_OWNER_MODE=wired"
        markers["native_discovery"] = "SI-NATIVE: discovery/replay/rename/remove/reconnect/stale PASS"
        markers["native_admission_refusal"] = "SI-NATIVE: native admission refusal and idle cancel PASS"
    markers["live_image"] = "LIVE: immutable_candidate=1"
    if no_adapter:
        markers = {name: value for name, value in markers.items()
                   if name in ("payload_verified", "spawntest_done", "stage1_done")}
        markers["native_startup"] = "AS_RUNNING reported for smartphone_integrator"
    if vehicle_providers:
        for app in ("persistence", "diagnosis", "fec"):
            markers["provider_running_" + app] = "AS_RUNNING reported for " + app
        markers["vehicle_write"] = "SI-VEHICLE: writer returned"
        markers["native_coding_delivery"] = "SI-NATIVE: configuration usb_mode=3"
        # SI onAdaptationChanged (0x19180C) traces the delivered size; 120 is
        # the assembled Car_Function_Adaptations pair from the backup.
        markers["native_adaptation_delivery"] = "onAdaptationChanged: size=120"
        if carplay_coded:
            markers["native_carplay_coded"] = "SI-NATIVE: configuration usb_mode=3 carplay_coding=1"
            # Coding gate passes, the still-missing native FEC exchange denies.
            markers["native_coding_gate_passes"] = "not granted by FEC"
        else:
            # Truthful decode of this car's data: usb mode 3, CarPlay bit 0.
            markers["native_vehicle_values"] = "SI-NATIVE: configuration usb_mode=3 carplay_coding=0"
            markers["native_coding_gate_denies"] = "not granted by Coding"
    if rcc_fec_fixture:
        markers.pop("native_admission_refusal", None)
        markers.pop("native_coding_gate_passes", None)
        markers.update({
            "native_fec_grants": "carplay_coding=1 fec_carplay=1 fec_usb=1",
            "rcc_peer_native_comm": "SI-FEC-PEER: updateFECs sent count=",
            "dio_transport": "[dio-si] SI-bound wireless transport installed",
            "dio_native_asi_setup": "[dio-si] external transport setup complete; awaiting native HMI configuration",
            "dio_native_hmi_configuration": "SI-HMI-QEMU: native startService configured 1024x480 hid=9 touch=1024/1024",
            "dio_native_screen_configuration": "1024 x 480, 30 FPS",
            "dio_native_knob": 'Registered HID "Eso_Knob"',
            "dio_native_trackpad": 'Registered HID "Eso_TrackPad"',
            "dio_receiver_ready": "SI-NATIVE: SI-owned DIO receiver ready PASS",
            "dio_media_observation": "SI-NATIVE: bound media status and health idle safety PASS",
            "mdns_preserved": "SI-NATIVE: external mDNS preserved PASS",
            "dio_cancel": "SI-NATIVE: SI-owned DIO cancel PASS",
            "dio_reconnect": "SI-NATIVE: SI-owned DIO reconnect identity and cleanup PASS",
        })
    return markers


def evaluate(log: str, stage2: bool = False, no_adapter: bool = False,
             vehicle_providers: bool = False,
             carplay_coded: bool = False,
             rcc_fec_fixture: bool = False) -> int:
    failures = []
    markers = acceptance_markers(stage2, no_adapter, vehicle_providers,
                                 carplay_coded, rcc_fec_fixture)
    for name, marker in KNOWN_EXPECTED.items():
        ok = marker in log
        print(f"[{'CONFIRMED (expected)' if ok else 'FAIL'}] {name}")
        if not ok:
            failures.append(name)
    for name, marker in markers.items():
        ok = marker in log
        print(f"[{'PASS' if ok else 'FAIL'}] {name}")
        if not ok:
            failures.append(name)
    for bad in FAILURE_MARKERS:
        if bad in log:
            line = next(l for l in log.splitlines() if bad in l)
            print(f"[FAIL] no loader fatal: {bad!r} — {line.strip()[:200]}")
            failures.append(f"loader:{bad}")
    if rcc_fec_fixture:
        for forbidden in ("TIMEOUT_STARTUP", "CinemoErrorFileOpen", "SI transport guard rejected", "[CIAP2ServiceCinemo] onJob_connect", "[CDIOManager] restartMdnsd :"):
            if forbidden in log: failures.append("dio:" + forbidden)
        status = re.findall(r"CLIENT: FINAL STATUS \[([^\r\n]*)", log)
        if len(status)<2 or any(re.search(r"\b" + key + r"=[1-9]", row)
                             for row in status for key in ("usb_scripts", "mediacon", "pps_usb")):
            failures.append("wireless_no_usb_side_effects")
    if not re.search(r"LIVE: pid=\d+ ready=1", log):
        failures.append("native_live_readiness")
    if failures:
        print(f"FAILURES: {failures}")
        return 1
    print(f"SI ABI {2 if stage2 else 1} QEMU {'fallback startup' if no_adapter else 'native acceptance'}: PASS")
    return 0


def main() -> int:
    global WORK
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--skip-build", action="store_true", help="reuse the built IFS")
    ap.add_argument("--candidate", type=str, help="ELF input delivered unchanged via compressed payload")
    ap.add_argument("--no-adapter", action="store_true",
                    help="A/B: run the candidate WITHOUT LD_PRELOAD (stock-fallback proof)")
    ap.add_argument("--timeout", type=int, default=TIMEOUT_S)
    ap.add_argument("--diagnose", action="store_true", help="external spawn-held debugger; no network/broker")
    ap.add_argument("--debug-broker", action="store_true", help="external broker snapshot with network/PPS")
    ap.add_argument("--with-services", action="store_true", help="keep network/PPS/broker in SI diagnostic")
    ap.add_argument("--service-manager", action="store_true", help="native SM spawns SI and sends init/run")
    ap.add_argument("--stage2", action="store_true", help="ABI 2 production adapter, persistence and random driver")
    ap.add_argument("--vehicle-providers", action="store_true",
                    help="diagnostic native persistence/diagnosis/FEC with copied vehicle data; requires --stage2")
    ap.add_argument("--carplay-coded", action="store_true",
                    help="with --vehicle-providers: write the adaptation CarPlay bit "
                         "through the native persistence writer (dealer-coding "
                         "simulation, not this car's data; the FEC gate still denies)")
    ap.add_argument("--workdir", type=Path, help="isolated output directory inside build/")
    ap.add_argument("--rcc-fec-fixture", action="store_true",
                    help="explicit RCC test peer via native COMM; not license validation")
    ap.add_argument("--receiver-hook", type=Path,
                    help="explicit hook ELF for --rcc-fec-fixture; recorded by path and SHA-256")
    ap.add_argument("--peer-health", action="store_true",
                    help="independent paired native timing peer; requires --rcc-fec-fixture")
    args = ap.parse_args()
    if args.peer_health and not args.rcc_fec_fixture:
        ap.error("--peer-health requires --rcc-fec-fixture")
    if args.rcc_fec_fixture and not (args.vehicle_providers and args.carplay_coded):
        ap.error("--rcc-fec-fixture requires --vehicle-providers --carplay-coded")
    if args.receiver_hook:
        if not args.rcc_fec_fixture:
            ap.error("--receiver-hook requires --rcc-fec-fixture")
        args.receiver_hook=args.receiver_hook.resolve()
        if not args.receiver_hook.is_file():
            ap.error("--receiver-hook must name an existing ELF")
    if args.service_manager and (args.diagnose or args.debug_broker):
        ap.error("--service-manager acceptance and external-debugger profiles are separate")
    service_manager = args.service_manager or not (args.diagnose or args.debug_broker)
    if args.vehicle_providers and (not args.stage2 or not service_manager or args.no_adapter):
        ap.error("--vehicle-providers requires --stage2 and native Service Manager with adapter")
    if args.carplay_coded and not args.vehicle_providers:
        ap.error("--carplay-coded requires --vehicle-providers")
    if (args.vehicle_providers or args.carplay_coded) and not args.workdir:
        ap.error("--vehicle-providers requires a separate --workdir inside build/")
    if args.diagnose or args.debug_broker:
        WORK = STAGE1 / "qemu-diagnose"
    if args.workdir:
        WORK = args.workdir.resolve()
        WORK.relative_to(REPO / "build")
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "run-receipt.json").unlink(missing_ok=True)
    if not args.skip_build:
        build_guest_and_ifs(candidate_override=args.candidate,
                            no_adapter=args.no_adapter, diagnose=args.diagnose,
                            debug_broker=args.debug_broker, with_services=args.with_services,
                            service_manager=service_manager, stage2=args.stage2,
                            vehicle_providers=args.vehicle_providers,
                            carplay_coded=args.carplay_coded,
                            rcc_fec_fixture=args.rcc_fec_fixture,
                            receiver_hook=args.receiver_hook, peer_health=args.peer_health)
    built = json.loads((WORK / "build-receipt.json").read_text())
    if (built["abi"] != (2 if args.stage2 else 1) or built["no_adapter"] != args.no_adapter or
        built["native_service_manager"] != service_manager or
        built.get("vehicle_providers", False) != args.vehicle_providers or
        built.get("carplay_coded", False) != args.carplay_coded or
        built.get("rcc_fec_fixture", False) != args.rcc_fec_fixture or
        built.get("peer_health", False) != args.peer_health or
        built.get("receiver_hook_override") != (str(args.receiver_hook) if args.receiver_hook else None) or
        built["diagnostic"] != bool(args.diagnose or args.debug_broker) or
        built["ifs_sha256"] != hashlib.sha256((WORK / "si_stage1.bin").read_bytes()).hexdigest()):
        raise SystemExit("IFS/profile changed; rebuild before starting the fixture")
    if args.receiver_hook:
        source_hash=hashlib.sha256(args.receiver_hook.read_bytes()).hexdigest()
        if built['payloads']['dio_libcarplay_hook.so']['sha256'] != source_hash:
            raise SystemExit("receiver hook changed; rebuild before starting the fixture")
    log = run_qemu(args.timeout, dio_network=args.rcc_fec_fixture)
    if args.diagnose or args.debug_broker:
        print("\n".join(line for line in log.splitlines() if "SI-DEBUG:" in line or "SI-DEBUG DONE" in line))
        return 0 if "=== SI-DEBUG DONE ===" in log else 1
    result = evaluate(log, stage2=args.stage2, no_adapter=args.no_adapter,
                      vehicle_providers=args.vehicle_providers,
                      carplay_coded=args.carplay_coded,
                      rcc_fec_fixture=args.rcc_fec_fixture)
    health_checks = {}
    if args.peer_health:
        health_checks = {name: marker in log for name, marker in {
            "static_timing_liveness": "[peer-health-qemu] static screen, only native timing replies for 14s PASS",
            "native_silent_peer_finalizer": "[peer-health-qemu] silent paired peer closed through native finalizer PASS",
        }.items()}
        if not all(health_checks.values()):
            result = 1
        print("Native paired peer health:", health_checks)
    receipt = built
    receipt.update({"status": "PASS" if result == 0 else "FAIL",
                    "log_sha256": hashlib.sha256((WORK / "qemu_stage1.log").read_bytes()).hexdigest(),
                    "checks": {name: marker in log for name, marker in
                               acceptance_markers(args.stage2, args.no_adapter,
                                                  args.vehicle_providers,
                                                  args.carplay_coded,
                                                  args.rcc_fec_fixture).items()},
                    "native_ready": bool(re.search(r"LIVE: pid=\d+ ready=1", log)),
                    "not_proven": ["RCC license/signature validation", "successful phone media session",
                                   "native HMI/AltScreen/media", "USB device lifecycle",
                                   "late ASI event source correlation", "automatic orphan recovery"]})
    receipt["checks"].update(health_checks)
    if args.rcc_fec_fixture:
        receipt["dio_inputs"] = json.loads((WORK / "dio-inputs.json").read_text())
        receipt["native_receiver_lifecycle"] = "real SI-owned DIO, ASI, /info, CPRX v3, cancel/reconnect"
        receipt["bootstrap_peer"] = "SIW2 C test peer; physical Bluetooth and full Rust bootstrap not exercised"
    if args.vehicle_providers:
        inputs = WORK / "vehicle-inputs.json"
        if inputs.exists():
            receipt["vehicle_inputs"] = json.loads(inputs.read_text())
    (WORK / "run-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    return result


if __name__ == "__main__":
    sys.exit(main())
