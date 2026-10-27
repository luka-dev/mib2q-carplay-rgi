#!/usr/bin/env python3
"""Exercise native wireless owner/daemon supervisor in disposable QNX QEMU."""

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


def docker(work: Path, *args: str) -> None:
    subprocess.run(["docker", "run", "--rm", "--platform=linux/amd64",
                    "-v", f"{AUDI}:/src:ro", "-v", f"{work}:/work",
                    "-w", "/src/Patches/mhi2-carplay", IMAGE, *args], check=True,
                   capture_output=True, text=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--require-quiesce", action="store_true",
                        help="require bridge confirmation for wired takeover and supervisor shutdown")
    options = parser.parse_args()
    build = ["python3", str(HERE / "build_owner_offline.py")]
    if options.require_quiesce:
        build.append("--property-handoff")
    subprocess.run(build, check=True,
                   capture_output=True, text=True)
    OUT.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mhi2q-supervisor-qemu-") as temp:
        work = Path(temp)
        files = {
            "startup-virt": SDP / "tests/qemu/startup-virt",
            "libstartup.a": SDP / "tests/qemu/libstartup.a",
            "devc-serdebug": SDP / "tests/qemu/devc-serdebug",
            "procnto-smp": BSP / "procnto-smp",
            "libc.so.3": BSP / "libc.so.3",
            "dio_owner_exec": OUT / ("dio_owner_exec.handoff.offline" if options.require_quiesce
                                     else "dio_owner_exec.offline"),
            "wireless_dio_supervisor_bin": OUT / (
                "wireless_dio_supervisor_bin.handoff.offline" if options.require_quiesce
                else "wireless_dio_supervisor_bin.offline"),
        }
        for name, source in files.items():
            shutil.copyfile(source, work / name)
        for name in ("supervisor_fake_child", "supervisor_qemu_probe"):
            define = (["-DMHI2Q_QEMU_REQUIRE_QUIESCE"]
                      if options.require_quiesce and name == "supervisor_qemu_probe"
                      else [])
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-Os",
                   "-Wall", "-Wextra", "-Werror", "-o", f"/work/{name}",
                   f"wireless/integration/{name}.c", *define, "-lsocket")
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
    CACHE_NOP=1 /proc/boot/io-pkt-v4-hc
    waitfor /dev/socket 8
    /proc/boot/ifconfig lo0 up
    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=8192 blk cache=256k
    waitfor /dev/hd0t77 8
    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/owner
    display_msg "=== supervisor QEMU start ==="
    /proc/boot/supervisor_qemu_probe
    display_msg "=== supervisor QEMU returned ==="
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[type=link] /tmp=/dev/shmem
[type=dir] /mnt/owner
[perms=+r,+x]
/proc/boot/libc.so.3=/work/libc.so.3
/proc/boot/procnto-smp=/work/procnto-smp
/proc/boot/devc-serdebug=/work/devc-serdebug
/proc/boot/waitfor=/opt/qnx650/target/qnx6/armle-v7/bin/waitfor
/proc/boot/io-pkt-v4-hc=/opt/qnx650/target/qnx6/armle-v7/sbin/io-pkt-v4-hc
/proc/boot/ifconfig=/opt/qnx650/target/qnx6/armle-v7/sbin/ifconfig
/proc/boot/libsocket.so.3=/opt/qnx650/target/qnx6/armle-v7/lib/libsocket.so.3
/proc/boot/devb-ram=/opt/qnx650/target/qnx6/armle-v7/sbin/devb-ram
/proc/boot/mount=/opt/qnx650/target/qnx6/armle-v7/bin/mount
/proc/boot/io-blk.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/io-blk.so
/proc/boot/cam-disk.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/cam-disk.so
/proc/boot/fs-qnx4.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/fs-qnx4.so
/proc/boot/libcam.so.2=/opt/qnx650/target/qnx6/armle-v7/lib/libcam.so.2
/proc/boot/dio_owner_exec=/work/dio_owner_exec
/proc/boot/wireless_dio_supervisor_bin=/work/wireless_dio_supervisor_bin
/proc/boot/supervisor_fake_child=/work/supervisor_fake_child
/proc/boot/supervisor_qemu_probe=/work/supervisor_qemu_probe
'''
        (work / "supervisor.build").write_text(manifest)
        docker(work, "mkifs", "/work/supervisor.build", "/work/supervisor.bin")
        log = OUT / ("qemu_wireless_supervisor_quiesce.log" if options.require_quiesce
                     else "qemu_wireless_supervisor.log")
        with log.open("w") as stream:
            process = subprocess.Popen([
                str(QEMU), "-M", "virt", "-m", "512", "-cpu", "cortex-a15",
                "-device", f"loader,file={work / 'supervisor.bin'},"
                "addr=0x40200000,force-raw=on,cpu-num=0",
                "-display", "none", "-serial", "stdio", "-monitor", "none",
            ], stdout=stream, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 50
            try:
                while process.poll() is None and time.monotonic() < deadline:
                    if "=== supervisor QEMU returned ===" in log.read_text(errors="replace"):
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
        required = (
            "[supervisor-qemu] first wireless and daemon started",
            "[supervisor-qemu] wired handoff stopped daemon and cleared IE",
            "[supervisor-qemu] lifecycle passed",
            "=== supervisor QEMU returned ===",
        )
        missing = [marker for marker in required if marker not in output]
        if missing:
            raise RuntimeError(f"QNX wireless supervisor failed: {missing}; log: {log}")
        print(f"QNX wireless supervisor lifecycle passed; log: {log}")


if __name__ == "__main__":
    main()
