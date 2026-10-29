#!/usr/bin/env python3
"""Boot the ordinary QNX daemon and drive its iAP2 bridge in QEMU."""
from __future__ import annotations

import argparse
import json
import os
from hashlib import sha256
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

from qemu_wifi_ie import AUDI, DAEMON, IMAGE, OUT, QEMU, SDP, SYS, TOOLS, docker


DAEMON_BIN = DAEMON / "target/armv7-unknown-nto-qnx650/release/mhi2q-wireless-daemon"
FW_APP = AUDI / ("Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                 "advanced/MU1316-appimg/armle")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phone-probe", action="store_true",
                        help="QEMU-only full iAP2/MFi/availability exchange with fake MFi and receiver")
    parser.add_argument("--software-mfi-qemu", action="store_true",
                        help="QEMU-only: host software MFi vectors, actual RSA verification by QNX phone")
    parser.add_argument("--software-mfi-tamper-qemu", action="store_true",
                        help="QEMU-only: corrupt AA03 signature and require AA04 with closed session gates")
    parser.add_argument("--reject-once", action="store_true",
                        help="fake receiver rejects one START; phone retries without replacing iAP2 link")
    parser.add_argument("--control-probe", action="store_true",
                        help="QEMU-only DNS-SD and HTTP CarPlay Control exchange")
    parser.add_argument("--recovery-qemu", action="store_true",
                        help="same RFCOMM link, modeled SI/DIO end and Wi-Fi-only recovery")
    parser.add_argument("--mfi-retry-qemu", action="store_true",
                        help="QEMU-only transient NACK and short-transfer MFi retry")
    parser.add_argument("--repeat-phone-qemu", action="store_true",
                        help="QEMU-only two complete phone sessions through one daemon")
    parser.add_argument("--mfi-timeout-recovery-qemu", action="store_true",
                        help="QEMU-only persistent NACK timeout, unlock, then successful new phone")
    parser.add_argument("--openssl-check", action="store_true",
                        help="run factory QNX openssl SHA-256 for installer integrity checks")
    parser.add_argument("--ie-lifecycle", action="store_true",
                        help="verify IE absent before graft connect and removed after EOF")
    options = parser.parse_args()
    if options.recovery_qemu and (not options.control_probe or options.repeat_phone_qemu or options.reject_once):
        parser.error("--recovery-qemu requires --phone-probe --control-probe; excludes repeat/reject")
    if options.software_mfi_qemu and not options.phone_probe:
        parser.error("--software-mfi-qemu requires --phone-probe")
    if options.software_mfi_tamper_qemu and not options.software_mfi_qemu:
        parser.error("--software-mfi-tamper-qemu requires --software-mfi-qemu")
    if options.software_mfi_tamper_qemu and (options.repeat_phone_qemu or options.reject_once):
        parser.error("software MFi tamper uses a single rejected session")
    if options.ie_lifecycle and options.phone_probe:
        parser.error("--ie-lifecycle uses the standalone bridge client")
    if options.reject_once and not options.phone_probe:
        parser.error("--reject-once requires --phone-probe")
    if options.control_probe and not options.phone_probe:
        parser.error("--control-probe requires --phone-probe")
    if options.mfi_retry_qemu and not options.phone_probe:
        parser.error("--mfi-retry-qemu requires --phone-probe")
    if options.repeat_phone_qemu and not options.phone_probe:
        parser.error("--repeat-phone-qemu requires --phone-probe")
    if options.mfi_timeout_recovery_qemu and not options.repeat_phone_qemu:
        parser.error("--mfi-timeout-recovery-qemu requires --repeat-phone-qemu")
    if options.mfi_timeout_recovery_qemu and options.mfi_retry_qemu:
        parser.error("choose persistent-NACK recovery or transient retry")
    subprocess.run(
        ["docker", "run", "--rm", "--platform=linux/amd64",
         "-v", f"{AUDI}:/src", "-w", "/src/Patches/mhi2-carplay", IMAGE,
         "/opt/rust/qnx-cargo", "build", "-q", "--manifest-path",
         f"/src/{DAEMON.relative_to(AUDI)}/Cargo.toml",
         "--bin", "mhi2q-wireless-daemon"],
        check=True, capture_output=True, text=True)
    if options.phone_probe:
        subprocess.run(
            ["docker", "run", "--rm", "--platform=linux/amd64",
             "-v", f"{AUDI}:/src", "-w", "/src/Patches/mhi2-carplay", IMAGE,
             "/opt/rust/qnx-cargo", "build", "-q", "--manifest-path",
             f"/src/{DAEMON.relative_to(AUDI)}/Cargo.toml", "--features",
             "qemu-probe", "--bin", "phone-qemu-probe"],
            check=True, capture_output=True, text=True)
    OUT.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mhi2q-daemon-qemu-") as temp:
        work = Path(temp)
        files = {
            "startup-virt": SDP / "tests/qemu/startup-virt",
            "libstartup.a": SDP / "tests/qemu/libstartup.a",
            "devc-serdebug": SDP / "tests/qemu/devc-serdebug",
            "waitfor": TOOLS / "bsp-qemu-arm/extract/waitfor",
            "pipe": SYS / "sbin/pipe",
            "cp": TOOLS / "bsp-qemu-arm/extract/cp",
            "random": SYS / "usr/sbin/random",
            "sleep": SYS / "usr/bin/sleep",
            "io-pkt-v4-hc": SYS / "sbin/io-pkt-v4-hc",
            "ifconfig": SYS / "sbin/ifconfig",
            "procnto-smp": TOOLS / "bsp-qemu-arm/extract/procnto-smp",
            "libc.so.3": SYS / "lib/libc.so.3",
            "libm.so.2": SYS / "lib/libm.so.2",
            "libsocket.so.3": SYS / "lib/libsocket.so.3",
            "libz.so.2": SYS / "usr/lib/libz.so.2",
            "mhi2q-wireless-daemon": DAEMON_BIN,
            "dio_manager.json": DAEMON / "probes/dio_manager.fixture.json",
        }
        if options.phone_probe:
            files["phone-qemu-probe"] = DAEMON / (
                "target/armv7-unknown-nto-qnx650/release/phone-qemu-probe")
        if options.openssl_check:
            files.update({
                "openssl": FW_APP / "usr/bin/openssl",
                "libssl.so.2": FW_APP / "usr/lib/libssl.so.2",
                "libcrypto.so.2": FW_APP / "usr/lib/libcrypto.so.2",
            })
        for name, source in files.items():
            shutil.copy2(source, work / name)
        probes = DAEMON / "probes"
        sources = [("fake_uaputl.c", "fake_uaputl")]
        sources.append(("mock_mfi_qemu.c", "libmock_mfi_qemu.so") if options.phone_probe
                       else ("bridge_client.c", "bridge_client"))
        if options.control_probe:
            sources.append(("mock_dns_sd_qemu.c", "libqemu_dns_sd.so"))
        for source, result in sources:
            docker(work, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-O2", "-Wall",
                   "-Wextra", "-Werror",
                   *(["-fPIC", "-shared"] if result.endswith(".so") else []),
                   "-o", f"/work/{result}",
                   f"/src/{(probes / source).relative_to(AUDI)}",
                   *(["-lsocket"] if result == "bridge_client" else []))
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
    display_msg "=== daemon bridge QEMU probe ==="
    /proc/boot/pipe &
    /proc/boot/random -t
    procmgr_symlink /dev/random /dev/urandom
    CACHE_NOP=1 /proc/boot/io-pkt-v4-hc
    /proc/boot/ifconfig lo0 alias 127.0.0.1
    /proc/boot/cp -p /proc/boot/fake_uaputl /proc/boot/mhi2q-wireless-daemon /dev/shmem
    PATH=/proc/boot:/dev/shmem
    LD_LIBRARY_PATH=/proc/boot
    MHI2Q_CP_IE_INDEX=2
    MHI2Q_CP_UAPUTL=/dev/shmem/fake_uaputl
    MHI2Q_CP_BT_MAC=00:01:02:03:04:05
    IPL_CONFIG_DIR_DIO_MANAGER=/proc/boot
    MHI2Q_PHONE_QEMU_FACTORY_FIXTURE=1
    MHI2Q_CP_MODEL=MHI2Q
    MHI2Q_CP_SERIAL=QEMU
    MHI2Q_CP_HARDWARE=MHI2Q
    MHI2Q_CP_SSID=QemuCarPlay
    MHI2Q_CP_PASSPHRASE=testpassword
    MHI2Q_CP_CHANNEL=11
    MHI2Q_CP_IP_ADDRESS=127.0.0.1
    MHI2Q_CP_AIRPLAY_PORT=5000
    MHI2Q_CP_DEVICE_ID=02:00:00:00:00:01
    MHI2Q_CP_RECEIVER=stock210
    MHI2Q_CP_SOURCE_VERSION=210.81
    MHI2Q_PHONE_QEMU_SOURCE_VERSION=210.81
    /dev/shmem/mhi2q-wireless-daemon --check-config
    display_msg "=== daemon config check returned ==="
    /proc/boot/mhi2q-wireless-daemon &
    /proc/boot/bridge_client
    display_msg "=== daemon bridge probe returned ==="
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[type=link] /dev/console=/dev/ser1
[type=link] /tmp=/dev/shmem
[perms=+r,+x]
/proc/boot/libc.so.3=/work/libc.so.3
/proc/boot/libm.so.2=/work/libm.so.2
/proc/boot/libsocket.so.3=/work/libsocket.so.3
/proc/boot/libz.so.2=/work/libz.so.2
/proc/boot/procnto-smp=/work/procnto-smp
/proc/boot/devc-serdebug=/work/devc-serdebug
/proc/boot/waitfor=/work/waitfor
/proc/boot/pipe=/work/pipe
/proc/boot/cp=/work/cp
/proc/boot/random=/work/random
/proc/boot/sleep=/work/sleep
/proc/boot/io-pkt-v4-hc=/work/io-pkt-v4-hc
/proc/boot/ifconfig=/work/ifconfig
/proc/boot/fake_uaputl=/work/fake_uaputl
/proc/boot/bridge_client=/work/bridge_client
/proc/boot/mhi2q-wireless-daemon=/work/mhi2q-wireless-daemon
/proc/boot/dio_manager.json=/work/dio_manager.json
"""
        if options.phone_probe:
            reject_once = ("    MHI2Q_PHONE_QEMU_REJECT_ONCE=1\n"
                           if options.reject_once else "")
            control_probe = ("    MHI2Q_PHONE_QEMU_CONTROL=1\n"
                             if options.control_probe else "")
            mfi_retry = ("    MHI2Q_MFI_QEMU_RETRY=1\n"
                         if options.mfi_retry_qemu else "")
            mfi_fail_first = ("    MHI2Q_MFI_QEMU_FAIL_FIRST=1\n"
                              if options.mfi_timeout_recovery_qemu else "")
            manifest = manifest.replace(
                "    /proc/boot/mhi2q-wireless-daemon &\n"
                "    /proc/boot/bridge_client\n",
                "    MHI2Q_CP_MFI_PATH=/dev/shmem/qemu_mfi\n"
                f"{control_probe}"
                f"{mfi_retry}"
                f"{mfi_fail_first}"
                + ("    MHI2Q_MFI_QEMU_TAMPER=1\n" if options.software_mfi_tamper_qemu else "") +
                "    LD_PRELOAD=/proc/boot/libmock_mfi_qemu.so\n"
                "    /proc/boot/mhi2q-wireless-daemon &\n"
                "    LD_PRELOAD=\n"
                f"{reject_once}"
                "    /proc/boot/phone-qemu-probe\n",
            ).replace(
                "/proc/boot/bridge_client=/work/bridge_client\n",
                "/proc/boot/libmock_mfi_qemu.so=/work/libmock_mfi_qemu.so\n"
                "/proc/boot/phone-qemu-probe=/work/phone-qemu-probe\n",
            )
            if options.control_probe:
                manifest += "/proc/boot/libqemu_dns_sd.so=/work/libqemu_dns_sd.so\n"
            if options.recovery_qemu:
                # This fixture models SI/DIO and has no hardware entropy.
                # Supply fresh HOST CSPRNG bytes as a disposable peripheral;
                # production still requires /dev/urandom, with no fallback.
                (work / "recovery-entropy").write_bytes(os.urandom(32))
                manifest=manifest.replace("    MHI2Q_PHONE_QEMU_CONTROL=1\n",
                    "    MHI2Q_PHONE_QEMU_CONTROL=1\n    MHI2Q_PHONE_QEMU_RECOVERY=1\n"
                    "    MHI2Q_CP_SI_CONTROL=/dev/shmem/recovery-si.sock\n"
                    "    /proc/boot/sleep 2\n")
                manifest=manifest.replace("/proc/boot/random -t\n","/proc/boot/random -t -p\n")
                manifest=manifest.replace("procmgr_symlink /dev/random /dev/urandom",
                    "procmgr_symlink /proc/boot/recovery-entropy /dev/urandom")
                manifest+="\n/proc/boot/recovery-entropy=/work/recovery-entropy\n"
            if options.repeat_phone_qemu:
                marker = "    /proc/boot/phone-qemu-probe\n"
                if manifest.count(marker) != 1:
                    raise RuntimeError("cannot find one phone probe command")
                if options.mfi_timeout_recovery_qemu:
                    manifest = manifest.replace(marker,
                        "    MHI2Q_PHONE_QEMU_EXPECT_MFI_TIMEOUT=1\n" + marker,
                        1)
                manifest = manifest.replace(marker,
                    marker + "    display_msg \"=== first phone session returned ===\"\n"
                    + ("    MHI2Q_PHONE_QEMU_EXPECT_MFI_TIMEOUT=0\n"
                       if options.mfi_timeout_recovery_qemu else "") +
                    "    /proc/boot/phone-qemu-probe-2\n",
                    1)
                manifest += "/proc/boot/phone-qemu-probe-2=/work/phone-qemu-probe\n"
        if options.openssl_check:
            (work / "sha_probe.txt").write_bytes(b"mhi2q-integrity-probe\n")
            manifest = manifest.replace(
                "    display_msg \"=== daemon config check returned ===\"\n",
                "    display_msg \"=== daemon config check returned ===\"\n"
                "    /proc/boot/openssl dgst -sha256 /proc/boot/sha_probe.txt\n",
            )
            manifest += (
                "/proc/boot/openssl=/work/openssl\n"
                "/proc/boot/libssl.so.2=/work/libssl.so.2\n"
                "/proc/boot/libcrypto.so.2=/work/libcrypto.so.2\n"
                "/proc/boot/sha_probe.txt=/work/sha_probe.txt\n"
            )
        if options.ie_lifecycle:
            manifest = manifest.replace(
                "    /proc/boot/bridge_client\n",
                "    /proc/boot/bridge_client --ie-lifecycle\n",
                1,
            )
            manifest = manifest.replace(
                "    MHI2Q_CP_IE_INDEX=2\n",
                "    MHI2Q_QEMU_NO_AP_RESET=1\n"
                "    MHI2Q_CP_IE_INDEX=2\n",
                1,
            )
        (work / "daemon_bridge.build").write_text(manifest)
        software_fixture = None
        if options.software_mfi_qemu:
            from software_mfi_qemu import prepare
            software_fixture = prepare(work, work / "daemon_bridge.build")
        docker(work, "sh", "-c",
               "MKIFS_PATH=/work:/opt/qnx650/target/qnx6/armle-v7/boot/sys "
               "mkifs daemon_bridge.build daemon_bridge.bin")
        log = OUT / ("qemu_daemon_phone_mfi_timeout_recovery.log"
                     if options.mfi_timeout_recovery_qemu else
                     "qemu_daemon_phone_repeat.log" if options.repeat_phone_qemu else
                     "qemu_daemon_phone_mfi_retry.log" if options.mfi_retry_qemu else
                     "qemu_daemon_phone_control.log" if options.control_probe else
                     "qemu_daemon_phone_retry.log" if options.reject_once else
                     "qemu_daemon_phone.log" if options.phone_probe
                     else "qemu_daemon_bridge.log")
        if options.software_mfi_qemu:
            log = log.with_name(log.stem + ("_software_mfi_tamper" if options.software_mfi_tamper_qemu
                                           else "_software_mfi") + ".log")
        if options.recovery_qemu:
            log=log.with_name("qemu_daemon_same_bt_recovery.log")
        with log.open("w") as stream:
            process = subprocess.Popen(
                [str(QEMU), "-M", "virt", "-m", "512", "-cpu", "cortex-a15",
                 "-device", f"loader,file={work / 'daemon_bridge.bin'},"
                            "addr=0x40200000,force-raw=on,cpu-num=0",
                 "-display", "none", "-serial", "stdio", "-monitor", "none"],
                stdout=stream, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + (90 if options.phone_probe else 60 if options.ie_lifecycle else 45)
            try:
                while time.monotonic() < deadline and process.poll() is None:
                    if "=== daemon bridge probe returned ===" in log.read_text(errors="replace"):
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
        markers = ["=== daemon config check returned ===",
                   "[bridge] listening on 127.0.0.1:57342",
                   "=== daemon bridge probe returned ==="]
        if not re.search(r"Process \d+ \(mhi2q-wireless-daemon\) exited status=0\.", output):
            markers.append("QNX daemon --check-config exited status=0")
        if options.openssl_check:
            expected = sha256((work / "sha_probe.txt").read_bytes()).hexdigest()
            if expected not in output or not re.search(
                r"Process \d+ \(openssl\) exited status=0\.", output
            ):
                markers.append("factory QNX openssl SHA-256 matches host digest")
        if options.phone_probe and options.software_mfi_tamper_qemu:
            markers += ["[phone-qemu] iap2 normal=1", "[phone-qemu] identification=1",
                        "[phone-qemu] certificate=1", "(phone-qemu-probe) exited status=0"]
        elif options.phone_probe:
            markers += ["[phone-qemu] iap2 normal=1",
                        "[phone-qemu] identification=1",
                        "[mfi-qemu] software certificate read=" if options.software_mfi_qemu else
                        "[mfi-qemu] certificate read=4",
                        "[mfi-qemu] combined SENDRECV command=0xc0140507",
                        "[phone-qemu] certificate=1",
                        "[mfi-qemu] software signature read=128" if options.software_mfi_qemu else
                        "[mfi-qemu] signature read=20",
                        "[phone-qemu] signature=1",
                        "[phone-qemu] wifi=1",
                        "[phone-qemu] receiver START=1",
                        "[phone-qemu] CarPlayStartSession=1",
                        "[phone-qemu] full daemon path=1",
                        "(phone-qemu-probe) exited status=0"]
            if options.recovery_qemu:
                markers += ["[recovery-qemu] two sessions on RFCOMM generation=1; duplicate suppressed; cleanup before rearm PASS",
                            "[recovery-qemu] same-phone TCP reachability probe"]
                if output.count("[phone-qemu] CarPlayStartSession=1")!=2 or output.count("[phone-qemu] signature=1")!=1:
                    markers.append("one MFi exchange and two media starts on the same RFCOMM link")
            else:
                markers.append("[phone-qemu] receiver STOP=1")
            if options.reject_once:
                markers += ["[phone-qemu] receiver rejected first START=1",
                            "[receiver-control] START generation 1:"]
            if options.mfi_retry_qemu:
                markers += ["[mfi-qemu] injected transient NACK on combined read",
                            "[mfi-qemu] injected short combined read"]
            if options.repeat_phone_qemu:
                markers.append("=== first phone session returned ===")
                markers.append("(phone-qemu-probe-2) exited status=0")
                if options.mfi_timeout_recovery_qemu:
                    markers += ["[mfi-qemu] persistent NACK began",
                                "[mfi-qemu] persistent NACK released lock elapsed_ms=",
                                "[phone-qemu] MFi timeout closed bridge=1"]
                    match = re.search(r"persistent NACK released lock elapsed_ms=(\d+)", output)
                    if not match or not 1800 <= int(match.group(1)) <= 5000:
                        markers.append("MFi timeout elapsed 1.8-5 seconds")
                    if output.count("[phone-qemu] full daemon path=1") != 1 or not (
                        output.find("[phone-qemu] MFi timeout closed bridge=1") <
                        output.find("[phone-qemu] full daemon path=1")
                    ):
                        markers.append("failed first session then successful second session")
                else:
                    for repeated in ("[phone-qemu] iap2 normal=1",
                                 "[phone-qemu] certificate=1",
                                 "[phone-qemu] signature=1",
                                 "[phone-qemu] wifi=1",
                                 "[phone-qemu] CarPlayStartSession=1",
                                 "[phone-qemu] receiver START=1",
                                 "[phone-qemu] CarPlay Control connect=1"
                                 if options.control_probe else
                                 "[phone-qemu] availability sent=1",
                                 "[phone-qemu] receiver STOP=1",
                                 "[phone-qemu] full daemon path=1"):
                        if output.count(repeated) != 2:
                            markers.append(f"two instances of {repeated}")
            if options.control_probe:
                markers += ["[dns-qemu] daemon DNS-SD mock loaded"]
                if not options.recovery_qemu:
                    markers += ["[phone-qemu] CarPlay Control connect=1",
                                "[carplay-control] connect accepted: HTTP 200"]
        else:
            markers.append("[bridge-client] daemon_bridge=1")
        if options.phone_probe:
            markers.append("[phone-qemu] factory JSON identity verified on wire")
        if options.ie_lifecycle:
            markers += ["[bridge-client] IE before connect sets=0 clears=0 present=0",
                        "[bridge-client] IE after disconnect sets=1 clears=1 present=0",
                        "(bridge_client) exited status=0"]
        missing = [marker for marker in markers if marker not in output]
        if options.recovery_qemu:
            inputs = [DAEMON_BIN, DAEMON/'target/armv7-unknown-nto-qnx650/release/phone-qemu-probe',
                      Path(__file__), DAEMON/'probes/recovery_fixture.rs',
                      DAEMON/'probes/phone_qemu_probe.rs', DAEMON/'probes/mock_dns_sd_qemu.c',
                      *sorted((DAEMON/'src').rglob('*.rs'))]
            (OUT/'qemu_daemon_same_bt_recovery.json').write_text(json.dumps({
                'status': 'FAIL' if missing else 'PASS', 'checks': {m:m in output for m in markers},
                'log_sha256': sha256(log.read_bytes()).hexdigest(),
                'input_sha256': {str(p.relative_to(AUDI)):sha256(p.read_bytes()).hexdigest() for p in inputs},
                'real': ['production Rust daemon', 'iAP2 link and receiver worker', 'DNS-SD parser', 'TCP connector'],
                'modeled': ['phone', 'MFi hardware', 'SI/DIO replies', 'DNS-SD service', 'Wi-Fi availability'],
                'one_rfcomm_generation': 1, 'media_sessions': 2,
                'hu_deployed': False,
            }, indent=2)+'\n')
        if missing:
            raise SystemExit(f"QNX daemon bridge failed, missing={missing}; inspect {log}")
        if software_fixture is not None:
            from software_mfi_qemu import validate
            validate(output, log, software_fixture,
                     sessions=2 if options.repeat_phone_qemu and not options.mfi_timeout_recovery_qemu else 1,
                     tampered=options.software_mfi_tamper_qemu)
        scenario = ("software MFi AA04 rejection" if options.software_mfi_tamper_qemu else
                    "full synthetic phone path" if options.phone_probe else "iAP2 marker/SYN")
        print(f"QNX daemon {scenario} passed; log: {log}")


if __name__ == "__main__":
    main()
