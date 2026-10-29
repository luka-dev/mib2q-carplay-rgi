"""Bounded QNX receiver check, with local sockets and no external interfaces."""
import hashlib
import json
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import time

from . import qemu_capture

ROOT = Path(__file__).resolve().parents[3]
BASE = ROOT / "build/wired-altscreen-30fps-re"
OUT = BASE / "primary-alt-diff/runtime"


def run():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "result.json").write_text(json.dumps({"passed": False, "state": "not completed"})+"\n")
    snapshot = BASE / "native/qnx-network"
    extracted = snapshot / "extracted"
    if not (extracted / "proc/boot/libairplay.so").is_file():
        raise RuntimeError("the frozen native receiver extraction is required")
    firmware = qemu_capture.AUDI / "Patches/carplay_vc/libairplay.so"
    firmware_bytes = firmware.read_bytes()
    fixture_bytes = (extracted / "proc/boot/libairplay.so").read_bytes()
    if hashlib.sha256(firmware_bytes).hexdigest() != "01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2":
        raise RuntimeError("reviewed stock-210 firmware fingerprint changed")
    # Both reviewed ELF images have VA=file offset here. mkifs changes other
    # parts of the fixture, but the complete scheduling helper must agree.
    helper = firmware_bytes[0x97e14:0x97e14+84]
    if fixture_bytes[0x97e14:0x97e14+84] != helper:
        raise RuntimeError("fixture scheduling helper differs from stock firmware")
    cflite = firmware_bytes[0x4c000:0x63000]
    if fixture_bytes[0x4c000:0x63000] != cflite:
        raise RuntimeError("fixture CFLite/CFUtils code differs from reviewed stock firmware")
    qemu_capture.IMAGE = "qnx65-armv7-toolchain:8.5"
    for name in ("startup-virt", "libstartup.a"):
        shutil.copyfile(snapshot / name, OUT / name)
    shutil.copyfile(ROOT / "hook/altscreen/receiver_runtime.h", OUT / "receiver_runtime.h")
    shutil.copyfile(ROOT / "hook/altscreen/sender_telemetry.h", OUT / "sender_telemetry.h")
    shutil.copyfile(ROOT / "hook/altscreen/receiver_io.h", OUT / "receiver_io.h")
    io_test = (ROOT / "tests/alt_receiver_io_test.c").read_text()
    old_include = '#include "../hook/altscreen/receiver_io.h"'
    if io_test.count(old_include) != 1:
        raise RuntimeError("receiver I/O test include layout changed")
    (OUT / "receiver_io_test.c").write_text(io_test.replace(old_include, '#include "receiver_io.h"'))
    shutil.copyfile(Path(__file__).with_name("alt_receiver_runtime_probe.c"), OUT / "probe.c")
    fields = ("sentFramesAvg", "queuedFramesAvg", "encoderCurrentFPS", "rttAvg",
              "txUsageAvg", "txCapacityAvg", "lossAvg", "submitSurfaceFPS",
              "beforeEncoderFPS", "sinkOverflowDropFPS", "encoderDropFPS",
              "encoderQueueDropFPS", "idleEncoderFPS", "idleDropFPS")
    complete = {name: i + .25 for i, name in enumerate(fields)}
    complete.update(sentFramesAvg=2, submitSurfaceFPS=30, sinkOverflowDropFPS=28)
    partial = dict(sentFramesAvg=0, queuedFramesAvg=True, encoderCurrentFPS="30",
                   rttAvg=float("nan"), txUsageAvg=float("inf"), txCapacityAvg=-1,
                   lossAvg=1e13, submitSurfaceFPS=None, beforeEncoderFPS=30.5,
                   sinkOverflowDropFPS=0, idleEncoderFPS=1, unrelated="ignored")
    cases = {"complete": complete, "partial": partial, "empty": {}, "array": [30]}
    fixtures = "/* Generated binary plists: real stock CFLite parses these in QNX. */\n"
    for name, case in cases.items():
        wire = plistlib.dumps(case, fmt=plistlib.FMT_BINARY)
        fixtures += f"static const unsigned char sender_{name}[] = {{" + ",".join(map(str, wire)) + "};\n"
    (OUT / "sender_cases.h").write_text(fixtures)
    qemu_capture.docker(OUT, "qcc", "-Vgcc_ntoarmv7le", "-O2", "-Wall", "-Wextra", "-Werror",
                       "probe.c", "-o", "probe", "-lsocket")
    template = (snapshot / "network.build").read_text()
    source = template[:template.index("[+script]")]
    source = source.replace("/work/extracted/", "/src/" + str(extracted.relative_to(qemu_capture.AUDI)) + "/")
    script = """[+script] .script = {
    procmgr_symlink /proc/boot/libc.so.3 /usr/lib/ldqnx.so.2
    /proc/boot/devc-serdebug -F -S
    waitfor /dev/ser1 4
    reopen /dev/ser1
    LD_LIBRARY_PATH=/proc/boot:/eso/lib
    CACHE_NOP=1 /proc/boot/io-pkt-v6-hc
    waitfor /dev/socket 8
    /proc/boot/rxprobe
}
"""
    source += script
    source += "[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3\n"
    # Explicit copy mappings keep the bootstrap and runtime libraries independent
    # of the original full image's manifest attributes.
    source += "[type=file perms=0755 data=copy] /proc/boot/rxprobe=/work/probe\n"
    for asset in sorted((extracted / "proc/boot").iterdir()):
        if asset.is_file() and asset.name != "procnto-smp" and not asset.name.startswith("."):
            source += f"[type=file perms=0755 data=copy] /proc/boot/{asset.name}=/src/{asset.relative_to(qemu_capture.AUDI)}\n"
    for asset in sorted((extracted / "eso/lib").rglob("*")):
        if asset.is_file():
            source += f"[type=file perms=0755 data=copy] /eso/lib/{asset.relative_to(extracted / 'eso/lib')}=/src/{asset.relative_to(qemu_capture.AUDI)}\n"
    (OUT / "probe.build").write_text(source)
    qemu_capture.docker(OUT, "mkifs", "probe.build", "probe.bin", capture_output=True)
    record = json.loads((BASE / "qemu-highspeed-poll/build.json").read_text())
    binary = Path(record["binary"])
    if hashlib.sha256(binary.read_bytes()).hexdigest() != record["sha256"]:
        raise RuntimeError("private QEMU fingerprint changed")
    command = [str(binary), "-M", "virt", "-m", "2048", "-cpu", "cortex-a15,cntfrq=62500000",
               "-device", f"loader,file={OUT / 'probe.bin'},addr=0x40200000,force-raw=on,cpu-num=0",
               "-display", "none", "-serial", "stdio", "-monitor", "none", "-nic", "none"]
    with (OUT / "serial.log").open("w") as serial, (OUT / "host.log").open("w") as host:
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=serial, stderr=host)
        try:
            deadline = time.monotonic() + 35
            while time.monotonic() < deadline:
                log = (OUT / "serial.log").read_text(errors="replace")
                if ("ALT_RX_RUNTIME_PASS" in log or "ALT_RX_RUNTIME_FAIL" in log or
                        re.search(r"Process \d+ \(rxprobe\) exited", log) or process.poll() is not None):
                    break
                time.sleep(.2)
            else:
                raise RuntimeError("QNX scheduling probe timeout")
            if any(token not in log for token in ("ALT_RX_RUNTIME_PASS", "ALT_SENDER_TELEMETRY_PASS", "ALT_RECEIVER_IO_PASS")):
                raise RuntimeError("QNX scheduling probe failed; inspect its serial/host logs")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
    result = {"passed": True, "qemu_pid": process.pid, "command": command,
              "firmware_sha256": hashlib.sha256(firmware_bytes).hexdigest(),
              "fixture_libairplay_sha256": hashlib.sha256(fixture_bytes).hexdigest(),
              "stock_helper_matches": True, "stock_helper_sha256": hashlib.sha256(helper).hexdigest(),
              "stock_cflite_matches": True, "stock_cflite_range": [0x4c000, 0x63000],
              "stock_cflite_sha256": hashlib.sha256(cflite).hexdigest(),
              "helper_sha256": hashlib.sha256((OUT / "receiver_runtime.h").read_bytes()).hexdigest(),
              "sender_telemetry_sha256": hashlib.sha256((OUT / "sender_telemetry.h").read_bytes()).hexdigest(),
              "sender_telemetry": "real stock CFLite: fractional/integer, missing/zero, invalid type/nonfinite/bounds, format and lifetime checks passed",
              "receiver_io_sha256": hashlib.sha256((OUT / "receiver_io.h").read_bytes()).hexdigest(),
              "receiver_io": "real QNX local sockets: buffered drain, fragmented records, EOF and cancellation passed",
              "scope": "separate guest; local sockets only; no external network devices/host ports, vphone access, shared QEMU replacement or HU deployment"}
    (OUT / "result.json").write_text(json.dumps(result, indent=2)+"\n")
    print(log)
    return result


if __name__ == "__main__":
    print(json.dumps(run(), indent=2))
