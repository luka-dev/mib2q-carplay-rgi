#!/usr/bin/env python3
"""Syntax-check the four offline PF profiles with the MU1316 pfctl in QEMU."""

from __future__ import annotations

from hashlib import sha256
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
AUDI = HERE.parents[3]
SDP = AUDI / "Tools/qnx-65-sdp-docker"
BSP = AUDI / "Tools/bsp-qemu-arm/extract"
QEMU = AUDI / "Tools/qemu-src/build/qemu-system-arm"
FW = AUDI / ("Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
             "advanced/MU1316-appimg/armle")
SYSTEM = AUDI / ("Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                 "advanced/MU1316-system/etc")
IMAGE = "qnx65-armv7-toolchain:latest"
LOG = HERE / "out/qemu_pfctl.log"
NAMES = ("pf.conf", "pf.ecm0.conf", "pf.mlan0.conf", "pf.ppp0.conf")
BASELINE = tuple(f"wired.{name}" for name in NAMES)
PFCTL_SHA256 = "25fd9dd8e6c992a214ec9d17af8e144fe87aef21936e071a436649a1c5259446"


def docker(work: Path, *command: str) -> None:
    subprocess.run(["docker", "run", "--rm", "--platform=linux/amd64",
                    "-v", f"{AUDI}:/src:ro", "-v", f"{work}:/work",
                    "-w", "/work", IMAGE, *command], check=True,
                   capture_output=True, text=True)


def main() -> None:
    subprocess.run(["python3", str(HERE / "build_offline.py")], check=True,
                   capture_output=True, text=True)
    if sha256((FW / "sbin/pfctl").read_bytes()).hexdigest() != PFCTL_SHA256:
        raise SystemExit("factory MU1316 pfctl differs from reviewed parser")
    LOG.parent.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mhi2q-pfctl-qemu-") as temp:
        work = Path(temp)
        sources = {
            "startup-virt": SDP / "tests/qemu/startup-virt",
            "libstartup.a": SDP / "tests/qemu/libstartup.a",
            "procnto-smp": BSP / "procnto-smp",
            "devc-serdebug": SDP / "tests/qemu/devc-serdebug",
            "libc.so.3": FW / "lib/libc.so.3",
            "libm.so.2": SDP / "sdp/target/qnx6/armle-v7/lib/libm.so.2",
            "libsocket.so.3": SDP / "sdp/target/qnx6/armle-v7/lib/libsocket.so.3",
            "io-pkt-v4-hc": SDP / "sdp/target/qnx6/armle-v7/sbin/io-pkt-v4-hc",
            "ifconfig": SDP / "sdp/target/qnx6/armle-v7/sbin/ifconfig",
            "pfctl": FW / "sbin/pfctl",
            "pf.os": SYSTEM / "pf.os",
            "pf.data.default": SYSTEM / "pf.data.default",
            "services": SYSTEM / "services",
            "protocols": SYSTEM / "protocols",
            "networks": SYSTEM / "networks",
            **{name: HERE / "out" / name for name in NAMES},
            **{f"wired.{name}": REPO / "deploy/pf/patched" / name
               for name in NAMES},
        }
        for name, source in sources.items():
            shutil.copyfile(source, work / name)
        docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-Os",
               "-Wall", "-Wextra", "-Werror", "-o", "/work/pfctl_qemu_probe",
               f"/src/{(HERE / 'pfctl_qemu_probe.c').relative_to(AUDI)}")
        manifest = r'''[linker="(40;1;)qcc -bootstrap -nostdlib -Wl,--no-keep-memory -Vgcc_ntoarmv7%(m!=3,%(m!=6,%(e==0, -EL%)%(e==1, -EB%)%)%)%(h!=0, -Wl,-Ttext -Wl,0x%t%)%(d!=0, -Wl,-Tdata -Wl,0x%d%) -o%o %i %[M -L%^i -Wl,-uinit_%n -lmod_%n%] /work/libstartup.a -L/opt/qnx650/target/qnx6/armle-v7/lib -L/opt/qnx650/target/qnx6/armle-v7/usr/lib -llzo -lucl -ldrvr"]
[linker="(40;2;)cp %i %o"]
[image=0x40200000]
[virtual=armle-v7,raw] .bootstrap = {
    /work/startup-virt -S
    PATH=/proc/boot LD_LIBRARY_PATH=/proc/boot /work/procnto-smp
}
[+script] .script = {
    procmgr_symlink ../../proc/boot/libc.so.3 /usr/lib/ldqnx.so.2
    procmgr_symlink ../../proc/boot/pf.os /etc/pf.os
    procmgr_symlink ../../proc/boot/pf.data.default /etc/pf.data.default
    procmgr_symlink ../../proc/boot/services /etc/services
    procmgr_symlink ../../proc/boot/protocols /etc/protocols
    procmgr_symlink ../../proc/boot/networks /etc/networks
    /proc/boot/devc-serdebug -F -S
    waitfor /dev/ser1 4
    reopen /dev/ser1
    CACHE_NOP=1 /proc/boot/io-pkt-v4-hc
    /proc/boot/ifconfig lo0 alias 127.0.0.1
    /proc/boot/pfctl_qemu_probe
    display_msg "=== pfctl QEMU returned ==="
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[perms=+r,+x]
/proc/boot/libc.so.3=/work/libc.so.3
/proc/boot/libm.so.2=/work/libm.so.2
/proc/boot/libsocket.so.3=/work/libsocket.so.3
/proc/boot/procnto-smp=/work/procnto-smp
/proc/boot/devc-serdebug=/work/devc-serdebug
/proc/boot/waitfor=/opt/qnx650/target/qnx6/armle-v7/bin/waitfor
/proc/boot/pfctl=/work/pfctl
/proc/boot/io-pkt-v4-hc=/work/io-pkt-v4-hc
/proc/boot/ifconfig=/work/ifconfig
/proc/boot/pfctl_qemu_probe=/work/pfctl_qemu_probe
/proc/boot/pf.os=/work/pf.os
/proc/boot/pf.data.default=/work/pf.data.default
/proc/boot/services=/work/services
/proc/boot/protocols=/work/protocols
/proc/boot/networks=/work/networks
'''
        manifest += "".join(f"/proc/boot/{name}=/work/{name}\n"
                            for name in NAMES + BASELINE)
        (work / "pfctl.build").write_text(manifest)
        docker(work, "mkifs", "pfctl.build", "pfctl.bin")
        with LOG.open("w") as stream:
            process = subprocess.Popen([
                str(QEMU), "-M", "virt", "-m", "256", "-cpu", "cortex-a15",
                "-device", f"loader,file={work / 'pfctl.bin'},"
                           "addr=0x40200000,force-raw=on,cpu-num=0",
                "-display", "none", "-serial", "stdio", "-monitor", "none",
            ], stdout=stream, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 45
            try:
                while process.poll() is None and time.monotonic() < deadline:
                    if "=== pfctl QEMU returned ===" in LOG.read_text(errors="replace"):
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
        output = LOG.read_text(errors="replace")
        print("\n".join(line for line in output.splitlines()
                        if "[pfctl-qemu]" in line or "pfctl:" in line))
        checks = []
        for name in NAMES:
            candidate = f"[pfctl-qemu] {name} status="
            baseline = f"[pfctl-qemu] wired.{name} status="
            checks.append((candidate, baseline))
        if any(candidate not in output or baseline not in output
               for candidate, baseline in checks):
            raise SystemExit(f"pfctl QEMU run incomplete; log: {LOG}")
        if ("[pfctl-qemu] pf.conf status=0" not in output or
                "[pfctl-qemu] wired.pf.conf status=0" not in output):
            raise SystemExit(f"factory pfctl rejected the exact default profile; log: {LOG}")
        # The remaining factory profiles define multiple ALTQ interfaces.
        # QEMU has only lo0, so aliases collide. Compare the diagnostics to
        # their pinned wired baselines to detect an added-rule regression.
        groups: dict[str, list[str]] = {}
        current = None
        for line in output.splitlines():
            if line.startswith("[pfctl-qemu] begin "):
                current = line.removeprefix("[pfctl-qemu] begin ")
                groups[current] = []
            elif current is not None and line.startswith("[pfctl-qemu] "):
                current = None
            elif current is not None and (line.startswith("pfctl: ") or
                                          "errors in queue definition" in line):
                # Remove the profile path and line number; compare the actual
                # parser complaint, not the insertion-shifted source line.
                if "errors in queue definition" in line:
                    groups[current].append("errors in queue definition")
                else:
                    groups[current].append(line)
        for name in NAMES[1:]:
            candidate = groups.get(name)
            baseline = groups.get(f"wired.{name}")
            if (not candidate or candidate != baseline or
                    f"[pfctl-qemu] {name} status=1" not in output or
                    f"[pfctl-qemu] wired.{name} status=1" not in output):
                raise SystemExit(f"pfctl baseline/candidate divergence in {name}; log: {LOG}")
        print(f"MU1316 pfctl parsed exact default profile; other three match pinned "
              f"wired parser diagnostics under QEMU interface aliases; log: {LOG}")


if __name__ == "__main__":
    main()
