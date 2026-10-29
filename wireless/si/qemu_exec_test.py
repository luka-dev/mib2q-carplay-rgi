#!/usr/bin/env python3
"""Loader-acceptance exec test: boots the minimal QNX IFS with ONE data file
staged as /etc/probe.bin, copies it to /tmp and exec's it. Prints
EXEC:OK (guest reached main and printed the marker) or EXEC:ENOEXEC.
Usage: qemu_exec_test.py <elf-file> [guest-marker]"""
from __future__ import annotations

import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
AUDI = REPO.parents[1]
TOOLS = AUDI / "Tools"
QEMU = TOOLS / "qemu-src/build/qemu-system-arm"
IMAGE = "qnx65-armv7-toolchain:latest"
BSP = TOOLS / "bsp-qemu-arm/extract"
SDP_QEMU = TOOLS / "qnx-65-sdp-docker/tests/qemu"
SDP_LIB = "/opt/qnx650/target/qnx6/armle-v7/lib"
WORK = REPO / "build/si-wireless-stage1/qemu-exec"


def docker(script: str) -> None:
    subprocess.run(["docker", "run", "--rm", "--platform=linux/amd64",
                    "-v", f"{REPO}:/src", "-v", f"{AUDI}:/audi", IMAGE,
                    "bash", "-c", script], check=True)


def main() -> int:
    elf = Path(sys.argv[1]).resolve()
    marker = sys.argv[2] if len(sys.argv) > 2 else "EXEC-PROBE-MAIN"
    WORK.mkdir(parents=True, exist_ok=True)
    stage = "/src/build/si-wireless-stage1/qemu-exec"
    buildfile = f"""
[linker="(40;1;)qcc -bootstrap -nostdlib -Wl,--no-keep-memory -Vgcc_nto%(m==40,arm%)%(m!=3,%(m!=6,%(e==0, -EL%)%(e==1, -EB%)%)%)%(h!=0, -Wl,-Ttext -Wl,0x%t%)%(d!=0, -Wl,-Tdata -Wl,0x%d%) -o%o %i %[M -L%^i -Wl,-uinit_%n -lmod_%n%] /audi/Tools/qnx-65-sdp-docker/tests/qemu/libstartup.a -L{SDP_LIB} -L{SDP_LIB}/usr/lib -llzo -lucl -ldrvr"]
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
    /proc/boot/devc-serdebug -F -S
    waitfor /dev/ser1 4
    reopen /dev/ser1
    display_msg "=== EXEC TEST BOOT ==="
    cp /etc/probe.bin /tmp/probe
    chmod 755 /tmp/probe
    /tmp/probe
    display_msg "=== EXEC TEST DONE ==="
}}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[type=link] /tmp=/dev/shmem
/proc/boot/procnto-smp=/audi/Tools/bsp-qemu-arm/extract/procnto-smp
/proc/boot/startup-virt=/audi/Tools/qnx-65-sdp-docker/tests/qemu/startup-virt
/proc/boot/devc-serdebug=/audi/Tools/bsp-qemu-arm/extract/devc-serdebug
/proc/boot/waitfor=/audi/Tools/bsp-qemu-arm/extract/waitfor
/proc/boot/cp=/audi/Tools/bsp-qemu-arm/extract/cp
/proc/boot/chmod=/audi/Tools/bsp-qemu-arm/extract/chmod
/proc/boot/sleep=/audi/Tools/bsp-qemu-arm/extract/sleep
/proc/boot/ls=/audi/Tools/bsp-qemu-arm/extract/ls
/proc/boot/libc.so.3={SDP_LIB}/libc.so.3
/etc/probe.bin={stage}/probe.bin
"""
    (WORK / "exec.build").write_text(buildfile)
    subprocess.run(["cp", str(elf), str(WORK / "probe.bin")], check=True)
    docker("cd /src/build/si-wireless-stage1/qemu-exec && "
           "export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH && "
           "mkifs exec.build exec.bin")

    log_path = WORK / "exec.log"
    command = [str(QEMU), "-M", "virt", "-m", "1024", "-cpu", "cortex-a15",
               "-device", f"loader,file={WORK / 'exec.bin'},"
                          "addr=0x40200000,force-raw=on,cpu-num=0",
               "-display", "none", "-serial", "stdio", "-monitor", "none"]
    with log_path.open("w") as stream:
        process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 90
        try:
            while time.monotonic() < deadline:
                if "=== EXEC TEST DONE ===" in log_path.read_text(errors="replace"):
                    time.sleep(0.5)
                    break
                time.sleep(0.5)
        finally:
            process.terminate()
            try:
                process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                process.kill()
    text = log_path.read_text(errors="replace")
    print(f"marker in log: {marker in text}")
    for bad in ("Unable to start", "ENOEXEC", "FATAL"):
        if bad in text:
            line = next(l.strip() for l in text.splitlines() if bad in l)
            print(f"reject line: {line[:160]}")
            return 1
    return 0 if marker in text else 1


if __name__ == "__main__":
    sys.exit(main())
