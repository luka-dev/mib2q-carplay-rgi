#!/usr/bin/env python3
"""Run the QNX dio owner/exec handoff in a disposable QEMU IFS."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


HERE = Path(__file__).resolve().parent
AUDI = HERE.parents[3]
SDP = AUDI / "Tools/qnx-65-sdp-docker"
BSP = AUDI / "Tools/bsp-qemu-arm/extract"
QEMU = AUDI / "Tools/qemu-src/build/qemu-system-arm"
OUT = HERE / "out"
IMAGE = "qnx65-armv7-toolchain:latest"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qnx6", action="store_true",
                        help="format disposable RAM disk as QNX6 before testing locks")
    parser.add_argument("--qnx6-loopback", action="store_true",
                        help="format a loopback-backed QNX6 disk inside a QNX4 RAM disk")
    parser.add_argument("--qnx6-factory-module", action="store_true",
                        help="load factory fs-qnx6.so from writable QNX4 backing disk")
    options = parser.parse_args()
    if options.qnx6 and options.qnx6_loopback:
        parser.error("choose one QNX6 backing mode")
    if options.qnx6_factory_module and not options.qnx6_loopback:
        parser.error("--qnx6-factory-module requires --qnx6-loopback")
    subprocess.run(["python3", str(HERE / "build_owner_offline.py")], check=True)
    OUT.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mhi2q-owner-qemu-") as temp:
        work = Path(temp)
        for name, source in {
            "startup-virt": SDP / "tests/qemu/startup-virt",
            "libstartup.a": SDP / "tests/qemu/libstartup.a",
            "devc-serdebug": SDP / "tests/qemu/devc-serdebug",
            "procnto-smp": BSP / "procnto-smp",
            "libc.so.3": BSP / "libc.so.3",
            "dio_owner_exec": OUT / "dio_owner_exec.offline",
        }.items():
            shutil.copyfile(source, work / name)
        manifest = r'''[linker="(40;1;)qcc -bootstrap -nostdlib -Wl,--no-keep-memory -Vgcc_ntoarmv7%(m!=3,%(m!=6,%(e==0, -EL%)%(e==1, -EB%)%)%)%(h!=0, -Wl,-Ttext -Wl,0x%t%)%(d!=0, -Wl,-Tdata -Wl,0x%d%) -o%o %i %[M -L%^i -Wl,-uinit_%n -lmod_%n%] /work/libstartup.a -L/opt/qnx650/target/qnx6/armle-v7/lib -L/opt/qnx650/target/qnx6/armle-v7/usr/lib -llzo -lucl -ldrvr"]
[linker="(40;2;)cp %i %o"]
[image=0x40200000]
[virtual=armle-v7,raw] .bootstrap = {
    /work/startup-virt -S
    PATH=/proc/boot LD_LIBRARY_PATH=/proc/boot /work/procnto-smp
}
[+script] .script = {
    procmgr_symlink ../../proc/boot/libc.so.3 /usr/lib/ldqnx.so.2
    /proc/boot/devc-serdebug -F -S
    waitfor /dev/ser1 4
    reopen /dev/ser1
    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=8192 blk cache=256k
    waitfor /dev/hd0t77 8
    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/owner
    MHI2Q_DIO_OWNER_LOCK=/mnt/owner/owner.lock
    MHI2Q_DIO_WIRED_PRIORITY_LOCK=/mnt/owner/priority.lock
    display_msg "=== owner QEMU test start ==="
    /proc/boot/owner_qemu_probe
    display_msg "=== owner QEMU test returned ==="
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[type=link] /tmp=/dev/shmem
[type=dir] /mnt/owner
[perms=+r,+x]
/proc/boot/libc.so.3=/work/libc.so.3
/proc/boot/procnto-smp=/work/procnto-smp
/proc/boot/devc-serdebug=/work/devc-serdebug
/proc/boot/waitfor=/opt/qnx650/target/qnx6/armle-v7/bin/waitfor
/proc/boot/devb-ram=/opt/qnx650/target/qnx6/armle-v7/sbin/devb-ram
/proc/boot/mount=/opt/qnx650/target/qnx6/armle-v7/bin/mount
/proc/boot/io-blk.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/io-blk.so
/proc/boot/cam-disk.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/cam-disk.so
/proc/boot/fs-qnx4.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/fs-qnx4.so
/proc/boot/libcam.so.2=/opt/qnx650/target/qnx6/armle-v7/lib/libcam.so.2
/proc/boot/owner_qemu_probe=/work/owner_qemu_probe
/proc/boot/owner_helper_1=/work/owner_helper_1
/proc/boot/owner_helper_2=/work/owner_helper_2
/proc/boot/owner_helper_3=/work/owner_helper_3
/proc/boot/owner_child_1=/work/owner_child_1
/proc/boot/owner_child_2=/work/owner_child_2
/proc/boot/owner_child_3=/work/owner_child_3
'''
        if options.qnx6:
            manifest = manifest.replace(
                "    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=8192 blk cache=256k\n"
                "    waitfor /dev/hd0t77 8\n"
                "    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/owner",
                "    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=8192,nodinit blk cache=256k\n"
                "    waitfor /dev/hd0 8\n"
                "    /proc/boot/mkqnx6fs -q /dev/hd0\n"
                "    /proc/boot/mount -t qnx6 /dev/hd0 /mnt/owner",
            ).replace(
                "/proc/boot/fs-qnx4.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/fs-qnx4.so",
                "/proc/boot/fs-qnx6.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/fs-qnx6.so\n"
                "/proc/boot/mkqnx6fs=/opt/qnx650/target/qnx6/armle-v7/sbin/mkqnx6fs",
            )
        if options.qnx6_loopback:
            manifest = manifest.replace(
                "    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=8192 blk cache=256k",
                "    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=65536 blk cache=256k",
            ).replace(
                "    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/owner",
                "    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/backing\n"
                "    /proc/boot/qnx6_backing_file\n"
                "    /proc/boot/devb-loopback loopback fd=/mnt/backing/qnx6.img blk cache=256k\n"
                "    waitfor /dev/lo0 8\n"
                "    /proc/boot/mkqnx6fs -q /dev/lo0\n"
                "    /proc/boot/mount2 -t qnx6 /dev/lo0 /mnt/owner",
            ).replace(
                "[type=dir] /mnt/owner",
                "[type=dir] /mnt/owner\n[type=dir] /mnt/backing",
            ).replace(
                "/proc/boot/fs-qnx4.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/fs-qnx4.so",
                "/proc/boot/fs-qnx4.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/fs-qnx4.so\n"
                "/proc/boot/fs-qnx6.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/fs-qnx6.so\n"
                "/proc/boot/mkqnx6fs=/opt/qnx650/target/qnx6/armle-v7/sbin/mkqnx6fs\n"
                "/proc/boot/devb-loopback=/opt/qnx650/target/qnx6/armle-v7/sbin/devb-loopback\n"
                "/proc/boot/qnx6_backing_file=/work/qnx6_backing_file\n"
                "/proc/boot/mount2=/opt/qnx650/target/qnx6/armle-v7/bin/mount",
            )
            if options.qnx6_factory_module:
                manifest = manifest.replace(
                    "    /proc/boot/devb-loopback loopback fd=/mnt/backing/qnx6.img blk cache=256k",
                    "    LD_LIBRARY_PATH=/mnt/backing:/proc/boot "
                    "/mnt/backing/devb-loopback loopback fd=/mnt/backing/qnx6.img blk cache=256k",
                ).replace(
                    "    /proc/boot/mount2 -t qnx6 /dev/lo0 /mnt/owner",
                    "    /proc/boot/mount2 -t qnx6 -o rw /dev/lo0 /mnt/owner",
                ).replace(
                    "/proc/boot/fs-qnx6.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/fs-qnx6.so\n",
                    "/proc/boot/factory_qnx6_xor=/work/factory_qnx6_xor\n"
                    "/proc/boot/factory_ioblk_xor=/work/factory_ioblk_xor\n"
                    "/proc/boot/factory_camdisk_xor=/work/factory_camdisk_xor\n"
                    "/proc/boot/factory_libcam_xor=/work/factory_libcam_xor\n"
                    "/proc/boot/factory_loopback_xor=/work/factory_loopback_xor\n",
                )
                module_root = AUDI / (
                    "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                    "rcc_extracted/ifs2_out"
                )
                app_root = AUDI / (
                    "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                    "advanced/MU1316-appimg/armle"
                )
                for name, module in {
                    "factory_qnx6_xor": module_root / "lib/dll/fs-qnx6.so",
                    "factory_ioblk_xor": module_root / "lib/dll/io-blk.so",
                    "factory_camdisk_xor": module_root / "lib/dll/cam-disk.so",
                    "factory_libcam_xor": module_root / "lib/libcam.so.2",
                    "factory_loopback_xor": app_root / "sbin/devb-loopback",
                }.items():
                    (work / name).write_bytes(
                        bytes(byte ^ 0xa5 for byte in module.read_bytes())
                    )
        (work / "owner.build").write_text(manifest)
        subprocess.run([
            "docker", "run", "--rm", "--platform=linux/amd64",
            "-v", f"{AUDI}:/src:ro", "-v", f"{work}:/work",
            "-w", "/src/Patches/mhi2-carplay", IMAGE,
            "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-Os",
            "-Wall", "-Wextra", "-Werror", "-o", "/work/owner_qemu_probe",
            "wireless/integration/owner_qemu_probe.c",
        ], check=True)
        if options.qnx6_loopback:
            subprocess.run([
                "docker", "run", "--rm", "--platform=linux/amd64",
                "-v", f"{AUDI}:/src:ro", "-v", f"{work}:/work",
                "-w", "/src/Patches/mhi2-carplay", IMAGE,
                "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-Os",
                "-Wall", "-Wextra", "-Werror",
                *(["-DMHI2Q_FACTORY_QNX6_MODULE"] if options.qnx6_factory_module else []),
                "-o", "/work/qnx6_backing_file",
                "wireless/integration/qnx6_backing_file.c",
            ], check=True)
        for sequence in (1, 2, 3):
            shutil.copyfile(work / "dio_owner_exec", work / f"owner_helper_{sequence}")
            shutil.copyfile(work / "owner_qemu_probe", work / f"owner_child_{sequence}")
        subprocess.run([
            "docker", "run", "--rm", "--platform=linux/amd64",
            "-v", f"{work}:/work", "-w", "/work", IMAGE,
            "mkifs", "owner.build", "owner.bin",
        ], check=True, capture_output=True, text=True)
        log = OUT / ("qemu_owner_handoff_qnx6_factory.log" if options.qnx6_factory_module
                     else "qemu_owner_handoff_qnx6_loopback.log" if options.qnx6_loopback
                     else "qemu_owner_handoff_qnx6.log" if options.qnx6
                     else "qemu_owner_handoff.log")
        with log.open("w") as stream:
            process = subprocess.Popen([
                str(QEMU), "-M", "virt", "-m", "256", "-cpu", "cortex-a15",
                "-device", f"loader,file={work / 'owner.bin'},"
                "addr=0x40200000,force-raw=on,cpu-num=0",
                "-display", "none", "-serial", "stdio", "-monitor", "none",
            ], stdout=stream, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 45
            try:
                while process.poll() is None and time.monotonic() < deadline:
                    if "=== owner QEMU test returned ===" in log.read_text(errors="replace"):
                        break
                    time.sleep(0.2)
            finally:
                if process.poll() is None:
                    process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        output = log.read_text(errors="replace")
        required = ("[owner-qemu] wireless started", "[owner-qemu] wireless stopped",
                    "[owner-qemu] wired started after wireless stop=1",
                    "[owner-qemu] handoff and wired priority passed",
                    "=== owner QEMU test returned ===")
        missing = [marker for marker in required if marker not in output]
        if missing or "[owner-qemu] wired takeover failed" in output:
            raise RuntimeError(f"QEMU owner handoff failed: {missing}; log: {log}")
        print(f"QNX 6.5 QEMU dio-owner handoff passed"
              f" ({'QNX6 loopback' if options.qnx6_loopback else 'QNX6' if options.qnx6 else 'QNX4'} disk); log: {log}")


if __name__ == "__main__":
    main()
