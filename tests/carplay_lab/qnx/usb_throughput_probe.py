"""Bounded, separate QNX guest to measure the laboratory USB network ceiling."""
from concurrent.futures import ThreadPoolExecutor
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import time

from tests.carplay_lab.common.qnx_clock import corrected
from tests.carplay_lab.qnx import qemu_capture

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "build/wired-altscreen-30fps-re/usb-throughput"
PORT = 33550


def measure(highspeed=False, exynos=False, interrupt_threshold=None, low_latency_host=False, async_usb=False, native_poll=False, transfer_bytes=524288, native_kqueue=False):
    # Uses neither the vphone guest nor any current laboratory receiver ports.
    with socket.socket() as check:
        check.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        check.bind(("127.0.0.1", PORT))
    OUT.mkdir(parents=True, exist_ok=True)
    qemu_capture.IMAGE = "qnx65-armv7-toolchain:8.5"
    base = ROOT / "build/vphone-lab/qnx-network/qemu_start_debug.bin"
    if not base.is_file():
        raise RuntimeError("build the existing QNX laboratory image first")
    extracted = OUT / "extracted"
    # dumpifs refuses to overwrite its previous extracted files/symlinks.
    if extracted.exists():
        shutil.rmtree(extracted)
    extracted.mkdir()
    listing = qemu_capture.docker(OUT, "dumpifs", "-v",
                "/src/"+str(base.relative_to(qemu_capture.AUDI)), capture_output=True).stdout
    for line in listing.splitlines():
        m = re.match(r"^\s+[0-9a-f]+\s+[0-9a-f]+\s+([^\s]+)", line)
        if m and not m[1].startswith(("Startup-", "Image-", "Root-", "*.")) and not m[1].endswith(".*"):
            (extracted / m[1]).parent.mkdir(parents=True, exist_ok=True)
    qemu_capture.docker(OUT, "sh", "-c", "cd /work/extracted && dumpifs -x /src/"+
                       str(base.relative_to(qemu_capture.AUDI)), capture_output=True)
    startup = qemu_capture.SDP / "tests/qemu/startup-virt"
    (OUT / "startup-virt").write_bytes(corrected(startup.read_bytes()))
    shutil.copyfile(qemu_capture.SDP / "tests/qemu/libstartup.a", OUT / "libstartup.a")
    source = Path(__file__).with_suffix(".c")
    shutil.copyfile(source, OUT / source.name)
    qemu_capture.docker(OUT, "qcc", "-Vgcc_ntoarmv7le", "-O2", "-Wall", "-Wextra", "-Werror",
                       "usb_throughput_probe.c", "-o", "usb-throughput", "-lsocket")
    template = (ROOT / "build/vphone-lab/qnx-network/network.build").read_text()
    linkers = "\n".join(s for s in template.splitlines() if s.startswith("[linker="))
    manifest = linkers + """
[image=0x40200000]
[virtual=armle-v7,raw] .bootstrap = {
    /work/startup-virt -S
    PATH=/proc/boot
    LD_LIBRARY_PATH=/proc/boot
    /work/extracted/proc/boot/procnto-smp
}
[+script] .script = {
    procmgr_symlink ../../proc/boot/libc.so.3 /usr/lib/ldqnx.so.2
    /proc/boot/devc-serdebug -F -S
    waitfor /dev/ser1 4
    reopen /dev/ser1
    display_msg "fps-usb bootstrap script running"
    /proc/boot/slogger
    display_msg "fps-usb starting io-pkt"
    CACHE_NOP=1 /proc/boot/io-pkt-v6-hc
    waitfor /dev/socket 8
    display_msg "fps-usb network stack ready"
    /proc/boot/pipe &
    waitfor /dev/pipe 6
    /proc/boot/io-usb -d ohci ioport=0x090d0000,irq=44 &
    waitfor /dev/io-usb 10
    waitfor /mhi2_usb_enumeration 3
    display_msg "fps-usb loading ecmplus"
    /proc/boot/mount -T io-pkt /proc/boot/devnp-ecmplus.so
    waitfor /mhi2_network_settle 3
    /proc/boot/ifconfig ecm0 10.0.2.15 netmask 255.255.255.0 up
    display_msg "fps-usb starting throughput listener"
    /proc/boot/usb-throughput
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[type=link] /dev/console=/dev/ser1
[type=link] /tmp=/dev/shmem
"""
    for name in ("libc.so.3", "libsocket.so.3", "libm.so.2", "libusbdi.so.2", "libbacktrace.so.1",
                 "devc-serdebug", "waitfor", "slogger", "ifconfig", "mount"):
        manifest += f"[data=copy] /proc/boot/{name}=/work/extracted/proc/boot/{name}\n"
    for name, sub in (("io-pkt-v6-hc", "sbin"), ("io-usb", "sbin"), ("pipe", "sbin"),
                      ("devu-ohci.so", "lib/dll")):
        manifest += f"[data=copy] /proc/boot/{name}=/opt/qnx650/target/qnx6/armle-v7/{sub}/{name}\n"
    manifest += "[data=copy] /proc/boot/devnp-ecmplus.so=/src/Tools/qnx-gl-passthrough/build/vm_pull/root/mntapp/armle/lib/dll/devnp-ecmplus.so\n"
    manifest += "[data=copy] /proc/boot/usb-throughput=/work/usb-throughput\n"
    binary = qemu_capture.QEMU
    if highspeed:
        variant = "qemu-highspeed-exynos" if exynos else "qemu-highspeed"
        if async_usb:
            variant += "-async"
        if native_poll:
            variant += "-poll"
        if native_kqueue:
            variant += "-kqueue"
        record = json.loads((ROOT / "build/wired-altscreen-30fps-re" / variant / "build.json").read_text())
        binary = Path(record["binary"])
        if hashlib.sha256(binary.read_bytes()).hexdigest() != record["sha256"]:
            raise RuntimeError("High Speed QEMU fingerprint changed; rebuild the experiment")
        manifest = manifest.replace("-d ohci ioport=", "-d ehci ioport=").replace("devu-ohci.so", "devu-ehci.so")
        if interrupt_threshold is not None:
            manifest = manifest.replace('irq=44 &', f'irq=44,int_thresh={interrupt_threshold} &')
    (OUT / "probe.build").write_text(manifest)
    qemu_capture.docker(OUT, "mkifs", "probe.build", "probe.bin", capture_output=True)
    command = [str(binary), "-M", "virt", "-m", "256", "-cpu", "cortex-a15,cntfrq=62500000",
               "-device", f"loader,file={OUT / 'probe.bin'},addr=0x40200000,force-raw=on,cpu-num=0",
               "-display", "none", "-serial", "stdio", "-monitor",
               "unix:monitor.sock,server=on,wait=off",
               "-netdev", f"user,id=n0,hostfwd=tcp:127.0.0.1:{PORT}-10.0.2.15:{PORT}",
               "-device", "usb-net,netdev=n0"]
    if low_latency_host:
        taskpolicy = shutil.which("taskpolicy")
        if not taskpolicy:
            raise RuntimeError("macOS taskpolicy is unavailable")
        command = [taskpolicy, "-l", "0", "-t", "0"] + command
    with (OUT / "serial.log").open("w") as serial, (OUT / "qemu.log").open("w") as hostlog:
        trace = OUT / "usb-trace.jsonl"
        trace.write_text("")
        environment = dict(os.environ, MHI2Q_QEMU_NET_TRACE=str(trace))
        child = subprocess.Popen(command, cwd=OUT, env=environment, stdin=subprocess.DEVNULL, stdout=serial, stderr=hostlog)
        try:
            deadline = time.monotonic()+35
            while "fps-usb ready" not in (OUT / "serial.log").read_text(errors="replace"):
                if child.poll() is not None:
                    raise RuntimeError(f"isolated QEMU exited with {child.returncode}; inspect serial.log and qemu.log")
                if time.monotonic() > deadline:
                    raise RuntimeError("isolated QNX probe readiness timed out; inspect serial.log")
                time.sleep(.1)

            with socket.socket(socket.AF_UNIX) as monitor:
                monitor.settimeout(2)
                monitor.connect(os.path.relpath(OUT / "monitor.sock"))
                def prompt():
                    response = bytearray()
                    while not response.endswith(b"(qemu) "):
                        part = monitor.recv(4096)
                        if not part or len(response) > 65536:
                            raise RuntimeError("incomplete QEMU monitor response")
                        response.extend(part)
                    return response.decode(errors="replace")
                prompt()
                monitor.sendall(b"info usb\n")
                usb_state = "\n".join(line.strip() for line in prompt().splitlines()
                                      if line.strip().startswith("Device "))
                expected_speed = "480 Mb/s" if highspeed else "12 Mb/s"
                if expected_speed not in usb_state:
                    raise RuntimeError(f"unexpected USB speed: {usb_state!r}")
                (OUT / "usb-state.txt").write_text(usb_state)

            def transfer(size):
                with socket.create_connection(("127.0.0.1", PORT), timeout=20) as connection:
                    connection.settimeout(20)
                    connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    start = time.monotonic_ns()
                    connection.sendall(struct.pack("!II", 0x46505330, size))
                    connection.sendall(bytes(size))
                    reply = bytearray()
                    while len(reply) < 32:
                        chunk = connection.recv(32-len(reply))
                        if not chunk: raise RuntimeError("incomplete throughput reply")
                        reply.extend(chunk)
                    elapsed = time.monotonic_ns()-start
                    count, guest_ns, receive_buffer, priority = struct.unpack("<4Q", reply)
                    if count != size or not guest_ns: raise RuntimeError("invalid throughput reply")
                    return {"bytes": count, "host_ns": elapsed, "guest_ns": guest_ns,
                            "host_mbps": count*8000/elapsed, "guest_mbps": count*8000/guest_ns,
                            "receive_buffer": receive_buffer, "thread_priority": priority}
            single = [transfer(transfer_bytes) for _ in range(2)]
            start = time.monotonic_ns()
            with ThreadPoolExecutor(max_workers=2) as pool:
                paired = list(pool.map(transfer, [transfer_bytes]*2))
            elapsed = time.monotonic_ns()-start
            result = {"single": single, "two_connections": paired,
                      "combined_host_mbps": sum(r["bytes"] for r in paired)*8000/elapsed,
                      "qemu_command": command,
                      "qemu_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                      "base_ifs_sha256": hashlib.sha256(base.read_bytes()).hexdigest(),
                      "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                      "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                      "controller": "EHCI High Speed" if highspeed else "OHCI Full Speed",
                      "usb_state": usb_state,
                      "ehci_interrupt_threshold": interrupt_threshold,
                      "low_latency_host": low_latency_host,
                      "async_usb": async_usb,
                      "native_poll": native_poll,
                      "native_kqueue": native_kqueue,
                      "receive_buffer_request": re.search(r"fps-usb receive-buffer[^\r\n]+",
                          (OUT / "serial.log").read_text()).group(0),
                      "scope": "synthetic TCP -> slirp -> QEMU USB -> QNX ecmplus, no iOS or physical USB"}
            (OUT / "result.json").write_text(json.dumps(result, indent=2)+"\n")
            print(json.dumps(result, indent=2))
        finally:
            child.terminate()
            try: child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=5)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--highspeed", action="store_true", help="use the separately built local EHCI experiment")
    parser.add_argument("--exynos", action="store_true", help="use the alternate EHCI register layout")
    parser.add_argument("--ehci-int-thresh", type=int, choices=(1, 8), help="EHCI interrupt threshold in 125-us units")
    parser.add_argument("--low-latency-host", action="store_true", help="launch only this QEMU with macOS latency/throughput tier zero")
    parser.add_argument("--async-usb", action="store_true", help="use the separately built asynchronous bulk-IN experiment")
    parser.add_argument("--native-poll", action="store_true", help="use the separately built macOS poll experiment")
    parser.add_argument("--native-kqueue", action="store_true", help="use the sub-millisecond Darwin wait experiment")
    parser.add_argument("--transfer-bytes", type=int, default=524288, help="payload per transfer, from 1 byte to 4 MiB")
    args = parser.parse_args()
    if args.native_poll and args.native_kqueue:
        parser.error("choose only one native wait backend")
    if not 1 <= args.transfer_bytes <= 4*1024*1024:
        parser.error("--transfer-bytes must be between 1 and 4194304")
    if args.highspeed:
        OUT = ROOT / "build/wired-altscreen-30fps-re/usb-highspeed-throughput"
    if args.exynos:
        args.highspeed = True
        OUT = ROOT / "build/wired-altscreen-30fps-re/usb-highspeed-exynos-throughput"
    if args.ehci_int_thresh is not None:
        if not args.highspeed:
            parser.error("--ehci-int-thresh requires --highspeed or --exynos")
        OUT = OUT.with_name(OUT.name + f"-int{args.ehci_int_thresh}")
    if args.low_latency_host:
        OUT = OUT.with_name(OUT.name + "-hostlow")
    if args.async_usb:
        if not args.highspeed:
            parser.error("--async-usb requires --highspeed or --exynos")
        OUT = OUT.with_name(OUT.name + "-async")
    if args.native_poll:
        if not args.highspeed:
            parser.error("--native-poll requires --highspeed or --exynos")
        OUT = OUT.with_name(OUT.name + "-poll")
    if args.native_kqueue:
        if not args.highspeed:
            parser.error("--native-kqueue requires --highspeed or --exynos")
        OUT = OUT.with_name(OUT.name + "-kqueue")
    if args.transfer_bytes != 524288:
        OUT = OUT.with_name(OUT.name + f"-{args.transfer_bytes}b")
    OUT.mkdir(parents=True, exist_ok=True)
    status = OUT / "status.json"
    status.write_text(json.dumps({"status": "running"})+"\n")
    try:
        measure(args.highspeed, args.exynos, args.ehci_int_thresh, args.low_latency_host, args.async_usb, args.native_poll, args.transfer_bytes, args.native_kqueue)
    except BaseException as error:
        status.write_text(json.dumps({"status": "failed", "error": str(error)})+"\n")
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stderr or error.stdout or "subprocess failed without captured output")
        raise
    status.write_text(json.dumps({"status": "passed"})+"\n")
