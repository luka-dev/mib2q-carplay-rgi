#!/usr/bin/env python3
"""Run complete installer preflight/install/rollback on a disposable QNX4 disk."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import time


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
AUDI = HERE.parents[3]
TOOLS = AUDI / "Tools"
SDP = TOOLS / "qnx-65-sdp-docker"
SDK = SDP / "sdp/target/qnx6/armle-v7"
FW = AUDI / ("Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
             "advanced/MU1316-appimg/armle")
QEMU = TOOLS / "qemu-src/build/qemu-system-arm"
OUT = HERE / "out/qemu_install_wireless.log"
IMAGE = "qnx65-armv7-toolchain:latest"
CONFIG = """export MHI2Q_CP_IE_INDEX=2
export IPL_CONFIG_DIR_DIO_MANAGER=/proc/boot
export MHI2Q_CP_BT_MAC=00:01:02:03:04:05
export MHI2Q_CP_DEVICE_ID=02:00:00:00:00:01
export MHI2Q_CP_MODEL=MHI2Q
export MHI2Q_CP_SERIAL=QEMU
export MHI2Q_CP_HARDWARE=MHI2Q
export MHI2Q_CP_SSID=QemuCarPlay
export MHI2Q_CP_PASSPHRASE=testpassword
export MHI2Q_CP_CHANNEL=11
export MHI2Q_CP_IP_ADDRESS=127.0.0.1
export MHI2Q_CP_RECEIVER=experimental280
export MHI2Q_CP_AIRPLAY_PORT=7000
export MHI2Q_CP_SOURCE_VERSION=280.33.8
"""


def docker(work: Path, *command: str) -> None:
    subprocess.run([
        "docker", "run", "--rm", "--platform=linux/amd64",
        "-v", f"{AUDI}:/src:ro", "-v", f"{work}:/work",
        "-w", "/work", IMAGE, *command,
    ], check=True, capture_output=True, text=True)


def encoded(work: Path, name: str, data: bytes) -> None:
    (work / name).write_bytes(bytes(byte ^ 0xA5 for byte in data))


def map_rows(entries: list[tuple[str, str, int]]) -> str:
    return "\n".join(
        f"    {{{json.dumps(image)}, {json.dumps(relative)}, 0{mode:o}}},"
        for image, relative, mode in entries
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--stock210", action="store_true", help="test the separate stock210 HU installer")
    options = parser.parse_args()
    archive_path = options.archive or REPO / ('wireless/out/mhi2q-stock210-hu-test.tar'
        if options.stock210 else 'wireless/out/mhi2q-wireless-offline-review.tar')
    log = HERE / 'out/qemu_install_stock210_test.log' if options.stock210 else (HERE / "out/qemu_install_wireless_handoff.log"
           if archive_path.name == "mhi2q-wireless-handoff-review.tar" else OUT)
    if not archive_path.is_file():
        raise SystemExit("build wireless offline bundle first")
    with tempfile.TemporaryDirectory(prefix="mhi2q-install-qemu-") as temp:
        work = Path(temp)
        stage_entries: list[tuple[str, str, int]] = []
        payloads = {}
        with tarfile.open(archive_path) as archive:
            for number, member in enumerate(archive.getmembers()):
                if not member.isfile():
                    continue
                contents = archive.extractfile(member).read()
                payloads[member.name] = (contents, member.mode)
        if options.stock210:
            values = dict(line.removeprefix('export ').split('=', 1) for line in CONFIG.splitlines())
            values.update(MHI2Q_CP_RECEIVER='stock210', MHI2Q_CP_SOURCE_VERSION='210.81', MHI2Q_CP_AIRPLAY_PORT='5000')
            config = payloads['integration/wireless_carplay.env'][0].decode()
            for key, value in values.items():
                pattern = r'^export '+key+r'=.*$'
                if re.search(pattern, config, flags=re.MULTILINE):
                    config = re.sub(pattern, f'export {key}={value}', config, flags=re.MULTILINE)
                else:
                    config += f'\nexport {key}={value}\n'
            payloads['integration/wireless_carplay.env'] = (config.encode(), 0o600)
            payloads['baseline-status.txt'] = (b'verified\n', 0o600)
            app = FW.parent
            for name, data in (('libairplay.so', (app / 'eso/lib/libairplay.so').read_bytes()),
                               ('dio_manager', (app / 'eso/bin/apps/dio_manager').read_bytes()),
                               ('mdnsd', (FW / 'usr/sbin/mdnsd').read_bytes()),
                               ('libdns_sd.so.1', (FW / 'usr/lib/libdns_sd.so.1').read_bytes()),
                               ('libcarplay_hook.so', b'QEMU wired hook sentinel'),
                               ('rc.local', b'QEMU startup sentinel')):
                payloads[f'reference/untouched.{name}'] = (data, 0o600)
            guards = b''
            for name, path in (('libairplay.so', '/mnt/app/eso/lib/libairplay.so'),
                               ('dio_manager', '/mnt/app/eso/bin/apps/dio_manager'),
                               ('mdnsd', '/mnt/app/armle/usr/sbin/mdnsd'),
                               ('libdns_sd.so.1', '/mnt/app/armle/usr/lib/libdns_sd.so.1'),
                               ('libcarplay_hook.so', '/mnt/app/root/hooks/libcarplay_hook.so'),
                               ('rc.local', '/mnt/app/root/rc.local')):
                data = payloads[f'reference/untouched.{name}'][0]
                guards += f'{hashlib.sha256(data).hexdigest()} {path}\n'.encode()
            payloads['untouched.txt'] = (guards, 0o600)
            payloads['checksums.txt'] = (''.join(f'{hashlib.sha256(data).hexdigest()} {name}\n'
                for name, (data, mode) in sorted(payloads.items()) if name != 'checksums.txt').encode(), 0o600)
        else:
            payloads['integration/wireless_carplay.env'] = (CONFIG.encode(), 0o600)
        for number, (name, (data, mode)) in enumerate(payloads.items()):
            image_name = f'stage_{number:02d}'
            encoded(work, image_name, data)
            stage_entries.append((image_name, name, mode))
        cli_sources = {
            "sh": FW / "bin/sh",
            "cp": SDK / "bin/cp",
            "cmp": FW / "usr/bin/cmp",
            "mv": SDK / "bin/mv",
            "rm": SDK / "bin/rm",
            "chmod": SDK / "bin/chmod",
            "printf": FW / "usr/bin/printf",
            "openssl": FW / "usr/bin/openssl",
        }
        if options.stock210:
            cli_sources.update(mkdir=SDK / 'bin/mkdir', rmdir=SDK / 'usr/bin/rmdir')
        cli_entries: list[tuple[str, str, int]] = []
        for number, (name, source) in enumerate(cli_sources.items()):
            image_name = f"cli_{number:02d}"
            encoded(work, image_name, source.read_bytes())
            cli_entries.append((image_name, name, 0o755))
        (work / "stage_map.h").write_text(
            "static const struct file_entry stage_files[] = {\n"
            + map_rows(stage_entries) + "\n};\n"
            + "static const struct file_entry cli_files[] = {\n"
            + map_rows(cli_entries) + "\n};\n"
        )
        direct = {
            "dio_manager.json": REPO / "wireless/daemon/probes/dio_manager.fixture.json",
            "startup-virt": SDP / "tests/qemu/startup-virt",
            "libstartup.a": SDP / "tests/qemu/libstartup.a",
            "procnto-smp": TOOLS / "bsp-qemu-arm/extract/procnto-smp",
            "devc-serdebug": SDP / "tests/qemu/devc-serdebug",
            "libc.so.3": FW / "lib/libc.so.3",
            "libm.so.2": SDK / "lib/libm.so.2",
            "libsocket.so.3": SDK / "lib/libsocket.so.3",
            "libz.so.2": SDK / "usr/lib/libz.so.2",
            "libssl.so.2": FW / "usr/lib/libssl.so.2",
            "libcrypto.so.2": FW / "usr/lib/libcrypto.so.2",
        }
        for name, source in direct.items():
            shutil.copyfile(source, work / name)
        docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-Os",
               "-Wall", "-Wextra", "-Werror", "-I/work", "-o", "/work/install_qemu_probe",
               *(['-DMHI2Q_STOCK210_TEST_INSTALL'] if options.stock210 else []),
               f"/src/{(HERE / 'install_qemu_probe.c').relative_to(AUDI)}")
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
    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=65536 blk cache=256k
    waitfor /dev/hd0t77 8
    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/owner
    /proc/boot/pipe &
    /proc/boot/install_qemu_probe &
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[type=link] /tmp=/dev/shmem
[type=dir] /mnt/owner
[perms=+r,+x]
/proc/boot/libc.so.3=/work/libc.so.3
/proc/boot/dio_manager.json=/work/dio_manager.json
/proc/boot/libm.so.2=/work/libm.so.2
/proc/boot/libsocket.so.3=/work/libsocket.so.3
/proc/boot/libz.so.2=/work/libz.so.2
/proc/boot/libssl.so.2=/work/libssl.so.2
/proc/boot/libcrypto.so.2=/work/libcrypto.so.2
/proc/boot/procnto-smp=/work/procnto-smp
/proc/boot/devc-serdebug=/work/devc-serdebug
/proc/boot/waitfor=/opt/qnx650/target/qnx6/armle-v7/bin/waitfor
/proc/boot/devb-ram=/opt/qnx650/target/qnx6/armle-v7/sbin/devb-ram
/proc/boot/mount=/opt/qnx650/target/qnx6/armle-v7/bin/mount
/proc/boot/pipe=/opt/qnx650/target/qnx6/armle-v7/sbin/pipe
/proc/boot/io-blk.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/io-blk.so
/proc/boot/cam-disk.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/cam-disk.so
/proc/boot/fs-qnx4.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/fs-qnx4.so
/proc/boot/libcam.so.2=/opt/qnx650/target/qnx6/armle-v7/lib/libcam.so.2
/proc/boot/install_qemu_probe=/work/install_qemu_probe
'''
        manifest += "".join(
            f"/proc/boot/{image}=/work/{image}\n"
            for image, _, _ in stage_entries + cli_entries
        )
        (work / "install.build").write_text(manifest)
        docker(work, "mkifs", "install.build", "install.bin")
        log.parent.mkdir(exist_ok=True)
        with log.open("w") as stream:
            process = subprocess.Popen([
                str(QEMU), "-M", "virt", "-m", "1024", "-cpu", "cortex-a15",
                "-device", f"loader,file={work / 'install.bin'},"
                           "addr=0x40200000,force-raw=on,cpu-num=0",
                "-display", "none", "-serial", "stdio", "-monitor", "none",
            ], stdout=stream, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 240
            try:
                while process.poll() is None and time.monotonic() < deadline:
                    output = log.read_text(errors="replace")
                    if "[install-qemu] preflight, failure rollback, install and explicit rollback passed" in output or \
                            "(install_qemu_probe) terminated" in output:
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
        output = log.read_text(errors="replace")
        markers = ("[install-qemu] preflight=0", "[install-qemu] forced failure=1",
                   "[install-qemu] install=0", "[install-qemu] rollback=0",
                   "[install-qemu] preflight, failure rollback, install and explicit rollback passed")
        missing = [marker for marker in markers if marker not in output]
        if missing:
            raise SystemExit(f"QNX installer lifecycle failed, missing={missing}; log: {log}")
        print(f"QNX shell installer lifecycle passed; log: {log}")


if __name__ == "__main__":
    main()
