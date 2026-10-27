#!/usr/bin/env python3
"""Run the QNX Wi-Fi IE installer/monitor with a disposable fake uaputl."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time


HERE = Path(__file__).resolve().parent
DAEMON = HERE.parent
AUDI = HERE.parents[4]
TOOLS = AUDI / "Tools"
SDP = TOOLS / "qnx-65-sdp-docker"
QEMU = TOOLS / "qemu-src/build/qemu-system-arm"
SYS = SDP / "sdp/target/qnx6/armle-v7"
IMAGE = "qnx65-armv7-toolchain:latest"
PROBE = DAEMON / "target/armv7-unknown-nto-qnx650/release/wifi-ie-qemu-probe"
OUT = DAEMON / "out"


def run(*args: str, **kwargs: object) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, text=True, check=True, **kwargs)


def docker(work: Path, *args: str, **kwargs: object) -> subprocess.CompletedProcess[str]:
    return run("docker", "run", "--rm", "--platform=linux/amd64",
               "-v", f"{work}:/work", "-v", f"{AUDI}:/src",
               "-w", "/work", IMAGE, *args, **kwargs)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--delayed-ap-qemu", action="store_true",
                        help="fake uaputl rejects the first three AP queries")
    options = parser.parse_args()
    docker(Path("/tmp"), "/opt/rust/qnx-cargo", "build", "-q",
           "--manifest-path", f"/src/{DAEMON.relative_to(AUDI)}/Cargo.toml",
           "--features", "qemu-probe", "--bin", "wifi-ie-qemu-probe",
           capture_output=True)
    OUT.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mhi2q-wifi-ie-qemu-") as temp:
        work = Path(temp)
        files = {
            "startup-virt": SDP / "tests/qemu/startup-virt",
            "libstartup.a": SDP / "tests/qemu/libstartup.a",
            "devc-serdebug": SDP / "tests/qemu/devc-serdebug",
            "waitfor": TOOLS / "bsp-qemu-arm/extract/waitfor",
            "pipe": SYS / "sbin/pipe",
            "cp": TOOLS / "bsp-qemu-arm/extract/cp",
            "procnto-smp": TOOLS / "bsp-qemu-arm/extract/procnto-smp",
            "libc.so.3": SYS / "lib/libc.so.3",
            "libm.so.2": SYS / "lib/libm.so.2",
            "libsocket.so.3": SYS / "lib/libsocket.so.3",
            "wifi-ie-qemu-probe": PROBE,
        }
        for name, source in files.items():
            shutil.copy2(source, work / name)
        docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc",
               "-O2", "-Wall", "-Wextra", "-Werror", "-o", "/work/fake_uaputl",
               f"/src/{(HERE / 'fake_uaputl.c').relative_to(AUDI)}")
        manifest = """[linker="(40;1;)qcc -bootstrap -nostdlib -Wl,--no-keep-memory -Vgcc_ntoarmv7%(m!=3,%(m!=6,%(e==0, -EL%)%(e==1, -EB%)%)%)%(h!=0, -Wl,-Ttext -Wl,0x%t%)%(d!=0, -Wl,-Tdata -Wl,0x%d%) -o%o %i %[M -L%^i -Wl,-uinit_%n -lmod_%n%] /work/libstartup.a -L/opt/qnx650/target/qnx6/armle-v7/lib -L/opt/qnx650/target/qnx6/armle-v7/usr/lib -llzo -lucl -ldrvr"]
[linker="(40;2;)cp %i %o"]
[image=0x40200000]
[virtual=armle-v7,raw] .bootstrap = {
    startup-virt -v -S
    PATH=:/proc/boot LD_LIBRARY_PATH=:/proc/boot procnto-smp -vvv
}
[+script] .script = {
    procmgr_symlink ../../proc/boot/libc.so.3 /usr/lib/ldqnx.so.2
    /proc/boot/devc-serdebug -e -F -S
    /proc/boot/waitfor /dev/ser1 4
    reopen /dev/ser1
    /proc/boot/pipe &
    display_msg "=== wifi IE QEMU probe ==="
    /proc/boot/cp -p /proc/boot/fake_uaputl /dev/shmem/fake_uaputl
    PATH=/proc/boot:/dev/shmem
    LD_LIBRARY_PATH=/proc/boot
    MHI2Q_CP_IE_INDEX=2
    MHI2Q_CP_UAPUTL=/dev/shmem/fake_uaputl
    /proc/boot/wifi-ie-qemu-probe
    display_msg "=== wifi IE probe returned ==="
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[type=link] /dev/console=/dev/ser1
[perms=+r,+x]
/proc/boot/libc.so.3=/work/libc.so.3
/proc/boot/libm.so.2=/work/libm.so.2
/proc/boot/libsocket.so.3=/work/libsocket.so.3
/proc/boot/procnto-smp=/work/procnto-smp
/proc/boot/devc-serdebug=/work/devc-serdebug
/proc/boot/waitfor=/work/waitfor
/proc/boot/pipe=/work/pipe
/proc/boot/cp=/work/cp
/proc/boot/fake_uaputl=/work/fake_uaputl
/proc/boot/wifi-ie-qemu-probe=/work/wifi-ie-qemu-probe
"""
        if options.delayed_ap_qemu:
            manifest = manifest.replace(
                "    MHI2Q_CP_UAPUTL=/dev/shmem/fake_uaputl\n",
                "    MHI2Q_CP_UAPUTL=/dev/shmem/fake_uaputl\n"
                "    MHI2Q_QEMU_AP_DELAY_QUERIES=3\n",
            )
        (work / "wifi_ie.build").write_text(manifest)
        docker(work, "sh", "-c",
               "MKIFS_PATH=/work:/opt/qnx650/target/qnx6/armle-v7/boot/sys "
               "mkifs wifi_ie.build wifi_ie.bin")
        listing = docker(work, "dumpifs", "-v", "wifi_ie.bin",
                         capture_output=True).stdout
        for name in ("proc/boot/fake_uaputl", "proc/boot/wifi-ie-qemu-probe"):
            if name not in listing:
                raise RuntimeError(f"QEMU IFS lacks {name}")
        log = OUT / ("qemu_wifi_ie_delayed_ap.log" if options.delayed_ap_qemu
                     else "qemu_wifi_ie.log")
        with log.open("w") as stream:
            process = subprocess.Popen(
                [str(QEMU), "-M", "virt", "-m", "256", "-cpu", "cortex-a15",
                 "-device", f"loader,file={work / 'wifi_ie.bin'},"
                            "addr=0x40200000,force-raw=on,cpu-num=0",
                 "-display", "none", "-serial", "stdio", "-monitor", "none"],
                stdout=stream, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 35
            try:
                while time.monotonic() < deadline and process.poll() is None:
                    if "=== wifi IE probe returned ===" in log.read_text(errors="replace"):
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
        for marker in ("[wifi-ie-qemu] initial=ok", "[wifi-ie-qemu] restore=ok",
                       "[wifi-ie-qemu] removal=ok",
                       "=== wifi IE probe returned ==="):
            if marker not in output:
                raise SystemExit(f"missing {marker}; inspect {log}")
        match = re.search(r"\[wifi-ie-qemu\] after removal sets=(\d+) clears=(\d+) queries=(\d+) present=(\d+) unavailable=(\d+)", output)
        expected_queries = 10 if options.delayed_ap_qemu else 7
        expected_unavailable = 3 if options.delayed_ap_qemu else 0
        if not match or tuple(map(int, match.groups())) != (2, 1, expected_queries, 0,
                                                            expected_unavailable):
            raise SystemExit(f"unexpected fake driver state; inspect {log}")
        print(f"QNX custom IE install, AP-reset restore, and removal passed; log: {log}")


if __name__ == "__main__":
    main()
