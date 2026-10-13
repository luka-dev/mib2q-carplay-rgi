#!/usr/bin/env python3
"""Build/control the isolated stock-210 QNX receiver with virtual USB Ethernet."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import signal
import shutil
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_LAB = ROOT / "build/carplay-lab"

OUT = DEFAULT_LAB / "qnx-network"
IMAGE_HASH = "1a05a14c8d8f2350aa15636c16f78591f1815e976f999da04f6974717d9ee2a6"
sys.path.insert(0, str(ROOT / "wireless/receiver_bridge"))
import qemu_capture as capture
from wireless.lab_common import qnx_clock


def set_output(directory: Path) -> None:
    """Keep receiver images, logs and PID records within the owned build tree."""
    global OUT
    directory = directory.resolve()
    if not directory.is_relative_to((ROOT / "build").resolve()):
        raise ValueError("QNX laboratory directory must remain inside build/")
    OUT = directory


def network_manifest(source: str, pairing: bool = False, media: bool = False, software_mfi: bool = False,
                     legacy_altscreen: bool = False, paired_altscreen: bool = False,
                     pacing_only: bool = False, capture_only_altscreen: bool = False) -> str:
    if capture_only_altscreen and not paired_altscreen:
        raise ValueError("capture-only AltScreen requires paired QNX lab")
    if pacing_only and not software_mfi:
        raise ValueError("pacing-only requires software MFi")
    if paired_altscreen and (legacy_altscreen or not software_mfi):
        raise ValueError("paired AltScreen requires software MFi and excludes the legacy profile")
    if legacy_altscreen and not software_mfi:
        raise ValueError("legacy AltScreen requires the isolated software MFi build")
    if software_mfi and not media:
        raise ValueError("software MFi requires native media")
    if media and not pairing:
        raise ValueError("native media requires pairing")
    anchor = "    /proc/boot/ifconfig lo0 alias 10.0.0.16\n"
    if source.count(anchor) != 1:
        raise ValueError("expected one network insertion point")
    source = source.replace(anchor, anchor + """    /proc/boot/pipe &
    waitfor /dev/pipe 6
    /proc/boot/io-usb -vv -d ohci ioport=0x090d0000,irq=44 &
    waitfor /dev/io-usb 10
    waitfor /mhi2_usb_enumeration 3
    /proc/boot/mount -T io-pkt -o verbose /proc/boot/devnp-ecmplus.so
    waitfor /mhi2_network_settle 3
    /proc/boot/ifconfig ecm0 10.0.2.15 netmask 255.255.255.0 up
    /proc/boot/ifconfig -a
""")
    if software_mfi:
        source = source.replace("    /proc/boot/ifconfig -a\n",
            "    /proc/boot/ifconfig ecm1 10.0.3.15 netmask 255.255.255.0 up\n    /proc/boot/ifconfig -a\n")
    sdk = "/opt/qnx650/target/qnx6/armle-v7"
    assets = {
        "io-usb": sdk + "/sbin/io-usb", "pipe": sdk + "/sbin/pipe",
        "devu-ohci.so": sdk + "/lib/dll/devu-ohci.so",
        "devnp-ecmplus.so": "/src/Tools/qnx-gl-passthrough/build/vm_pull/root/mntapp/armle/lib/dll/devnp-ecmplus.so"}
    if pairing:
        preload = "    LD_PRELOAD=/proc/boot/libdio_capture_probe.so:/proc/boot/libdioprobe.so\n"
        if source.count(preload) != 1:
            raise ValueError("expected one stock receiver preload")
        source = source.replace(preload,
            "    MHI2Q_DIO_OWNER_MODE=wireless\n"
            "    MHI2Q_CP_WIRELESS_PAIRING=1\n"
            + ("    MHI2Q_CP_WIRELESS_MEDIA=1\n" if media else "") +
            "    MHI2Q_CP_PAIRING_DIR=/mnt/pairing/store\n"
            "    LD_PRELOAD=/proc/boot/libcarplay_hook.so:/proc/boot/libdio_capture_probe.so:/proc/boot/libdioprobe.so\n")
        if legacy_altscreen:
            # This is a separate legacy MFi-SAP session. Never enable the AES
            # stream-111 receiver on paired ChaCha media or remove its guard.
            source = source.replace("    MHI2Q_DIO_OWNER_MODE=wireless\n", "    MHI2Q_DIO_OWNER_MODE=wired\n")
            source = source.replace("    MHI2Q_CP_WIRELESS_PAIRING=1\n", "    MHI2Q_CP_WIRELESS_PAIRING=0\n")
            source = source.replace("    MHI2Q_CP_WIRELESS_MEDIA=1\n", "    MHI2Q_CP_WIRELESS_MEDIA=0\n"
                "    ALTSCREEN_HANDLE_ALT=1\n    ALTSCREEN_ADVERT_DISPLAYS=1\n")
        if paired_altscreen:
            source = source.replace("    MHI2Q_CP_WIRELESS_MEDIA=1\n", "    MHI2Q_CP_WIRELESS_MEDIA=1\n"
                "    MHI2Q_CP_WIRELESS_ALT_QEMU=1\n    ALTSCREEN_HANDLE_ALT=1\n    ALTSCREEN_ADVERT_DISPLAYS=1\n")
        source = source.replace("    reopen /dev/ser1\n", "    reopen /dev/ser1\n"
            "    CACHE_NOP=1 /proc/boot/devb-ram ram capacity=8192 blk cache=256k\n"
            "    waitfor /dev/hd0t77 8\n"
            "    /proc/boot/mount -t qnx4 /dev/hd0t77 /mnt/pairing\n")
        assets["libcarplay_hook.so"] = "/work/libcarplay_hook.so"
        assets["libz.so.2"] = sdk + "/usr/lib/libz.so.2"
        # Use the exact factory library; the actual hook supplies pairing.
        stock = capture.AUDI / "Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib/libairplay.so"
        if hashlib.sha256(stock.read_bytes()).hexdigest() != "01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2":
            raise ValueError("factory stock210 hash changed")
        lines = source.splitlines()
        source = "\n".join(f"/proc/boot/libairplay.so=/src/{stock.relative_to(capture.AUDI)}"
            if line.startswith("/proc/boot/libairplay.so=") else line for line in lines) + "\n"
    if software_mfi:
        source = source.replace("LD_PRELOAD=/proc/boot/libcarplay_hook.so:/proc/boot/libdio_capture_probe.so:",
            "LD_PRELOAD=/proc/boot/libdio_capture_probe.so:/proc/boot/libcarplay_hook.so:")
    if capture_only_altscreen:
        source = source.replace("    MHI2Q_CP_WIRELESS_ALT_QEMU=1\n", "    MHI2Q_CP_WIRELESS_ALT_QEMU=1\n    MHI2Q_CP_QEMU_CAPTURE_ONLY=1\n")
    if pacing_only:
        source = source.replace("    MHI2Q_DIO_OWNER_MODE=", "    PROBE_LOG=/dev/null\n    PROBE_IO_CAP=1\n    MHI2Q_DIO_OWNER_MODE=", 1)
    if legacy_altscreen or paired_altscreen:
        source = source.replace("    reopen /dev/ser1\n", "    reopen /dev/ser1\n"
            "    procmgr_symlink /dev/ser1 /dev/shmem/carplay_hook.log\n"
            "    procmgr_symlink /dev/null /dev/shmem/carplay_verbose\n")
    # Late ELFs cannot map in this bootstrap after the large factory block.
    index = source.index("/eso/lib/factories/")
    extra = ("\n[type=dir] /mnt/pairing\n" if pairing else "\n") + "".join(f"[type=file perms=0755 data=copy] /proc/boot/{name}={path}\n"
                            for name, path in assets.items())
    source = source[:index] + extra + source[index:]
    # mkifs defaults to data=uip; those executables are removed after one use.
    return source.replace("/proc/boot/ifconfig=/work/", "[data=copy] /proc/boot/ifconfig=/work/").replace(
        "/proc/boot/mount=/work/", "[data=copy] /proc/boot/mount=/work/")


def command(transport: str = "user") -> list[str]:
    record_path = OUT / "build-record.json"
    record_value = json.loads(record_path.read_text()) if record_path.exists() else {}
    cpu = f"cortex-a15,cntfrq={qnx_clock.FREQUENCY}" if record_value.get("clock_realtime") else "cortex-a15"
    result = [str(capture.QEMU), "-M", "virt", "-m", "2048", "-cpu", cpu,
            "-device", f"loader,file={OUT / 'qemu_start_debug.bin'},addr=0x40200000,force-raw=on,cpu-num=0",
            "-display", "none", "-serial", "stdio", "-monitor", "none",
            "-netdev", ("socket,id=n0,listen=127.0.0.1:32501" if transport == "socket" else
                        "user,id=n0,hostfwd=tcp:127.0.0.1:32500-:5000"), "-device", "usb-net,netdev=n0"]
    record = OUT / "build-record.json"
    if record.exists() and json.loads(record.read_text()).get("software_mfi"):
        mfi_network = "user,id=mfi,net=10.0.3.0/24"
        if json.loads(record.read_text()).get("input_control"):
            mfi_network += ",hostfwd=tcp:127.0.0.1:32503-10.0.3.15:32503"
        result += ["-netdev", mfi_network, "-device",
                   "usb-net,netdev=mfi,mac=52:54:00:12:34:57"]
    return result


def running() -> int | None:
    record = OUT / "qnx.pid"
    if not record.exists():
        return None
    pid = int(record.read_text())
    transport = (OUT / "transport.txt").read_text().strip() if (OUT / "transport.txt").exists() else "user"
    check = subprocess.run(["ps", "-p", str(pid), "-o", "command="], capture_output=True, text=True)
    return pid if check.returncode == 0 and check.stdout.strip() == " ".join(command(transport)) else None


def build(image: str, pairing: bool = False, media: bool = False, software_mfi: bool = False,
          request_ui: bool = False, request_ui_music: bool = False, capture_video: bool = False,
          capture_video_long: bool = False, legacy_altscreen: bool = False,
          paired_altscreen: bool = False, request_ui_maps: bool = False,
          pacing_only: bool = False, input_control: bool = False,
          capture_memory: bool = False, clock_realtime: bool = False,
          capture_only_altscreen: bool = False) -> None:
    if clock_realtime and not software_mfi:
        raise ValueError("realtime clock profile requires isolated software MFi")
    if capture_only_altscreen and (not paired_altscreen or not capture_memory):
        raise ValueError("capture-only AltScreen requires paired RAM capture")
    if capture_memory and (not input_control or not software_mfi or capture_video):
        raise ValueError("memory capture requires input control/software MFi and excludes UART video capture")
    if input_control and not software_mfi:
        raise ValueError("input control requires isolated software MFi network")
    if request_ui_maps and (not request_ui or request_ui_music):
        raise ValueError("Maps UI requires --request-ui and excludes Music UI")
    if pacing_only and (not software_mfi or capture_video):
        raise ValueError("pacing-only requires software MFi and excludes video payload capture")
    if paired_altscreen and (legacy_altscreen or not software_mfi):
        raise ValueError("paired AltScreen requires software MFi and excludes the legacy profile")
    if capture_video_long and not capture_video:
        raise ValueError("long video capture requires --capture-video")
    if legacy_altscreen and not software_mfi:
        raise ValueError("legacy AltScreen requires the isolated software MFi build")
    if request_ui and not software_mfi:
        raise ValueError("native UI requires software MFi")
    if request_ui_music and not request_ui:
        raise ValueError("Music UI requires the native UI request")
    if capture_video and not software_mfi:
        raise ValueError("native video capture requires software MFi")
    if running():
        raise RuntimeError("stop the owned QNX guest before rebuilding")
    base = capture.CARPLAY / "dio_mesh_provider_twin.bin"
    if hashlib.sha256(base.read_bytes()).hexdigest() != IMAGE_HASH:
        raise RuntimeError("base QNX image hash changed")
    hook = ROOT / ("build/wireless-alt-qemu/libcarplay_hook.so" if paired_altscreen else "build/wireless/libcarplay_hook.so")
    if paired_altscreen and (not hook.is_file() or
            b"paired stream 111 QEMU: fresh authenticated key claimed" not in hook.read_bytes()):
        raise ValueError("build the dedicated WIRELESS_ALT_QEMU hook before enabling paired AltScreen")
    capture.IMAGE, capture.OUT = image, OUT
    original_manifest, original_docker = capture.make_manifest, capture.docker
    def manifest(*args, **kwargs):
        original_manifest(*args, **kwargs)
        path = args[0] / "capture.build"
        if pairing:
            shutil.copy2(hook, args[0] / "libcarplay_hook.so")
        path.write_text(network_manifest(path.read_text(), pairing, media, software_mfi, legacy_altscreen, paired_altscreen, pacing_only, capture_only_altscreen))
        if clock_realtime:
            startup = args[0] / "startup-virt"
            startup.write_bytes(qnx_clock.corrected(startup.read_bytes()))
        shutil.copy2(path, OUT / "network.build")
    def docker(work, *args, **kwargs):
        args = tuple('-DMHI2Q_CAPTURE_INTERFACE_NAME="ecm0"' if
                     arg == '-DMHI2Q_CAPTURE_INTERFACE_NAME="lo0"' else arg for arg in args)
        if pairing and "/work/libdio_capture_probe.so" in args:
            args += ("-DMHI2Q_CAPTURE_DUMP_INFO_QEMU", "-DMHI2Q_CAPTURE_PAIRING_IDENTITY_QEMU",
                     "-DMHI2Q_CAPTURE_HTTP_HEADER_QEMU")
        if software_mfi and "/work/libdio_capture_probe.so" in args:
            args += ("-DMHI2Q_CAPTURE_HTTP_DELEGATE_QEMU", "-DMHI2Q_CAPTURE_SOFTWARE_MFI_QEMU")
        if request_ui and "/work/libdio_capture_probe.so" in args:
            args += ("-DMHI2Q_CAPTURE_NATIVE_UI_QEMU",)
        if request_ui_music and "/work/libdio_capture_probe.so" in args:
            args += ("-DMHI2Q_CAPTURE_REQUEST_MUSIC_UI_QEMU",)
        if request_ui_maps and "/work/libdio_capture_probe.so" in args:
            args += ("-DMHI2Q_CAPTURE_REQUEST_MAPS_UI_QEMU",)
        if input_control and "/work/libdio_capture_probe.so" in args:
            args += ("-DMHI2Q_CAPTURE_INPUT_QEMU",)
        if capture_memory and "/work/libdio_capture_probe.so" in args:
            args += ("-DMHI2Q_CAPTURE_VIDEO_QEMU", "-DMHI2Q_CAPTURE_VIDEO_MEMORY_QEMU")
        if capture_video and "/work/libdio_capture_probe.so" in args:
            args += ("-DMHI2Q_CAPTURE_VIDEO_QEMU",)
        if capture_video_long and "/work/libdio_capture_probe.so" in args:
            args += ("-DMHI2Q_CAPTURE_VIDEO_LONG_QEMU",)
        return original_docker(work, *args, **kwargs)
    capture.make_manifest, capture.docker = manifest, docker
    sys.argv = ["qemu_capture.py", "--mesh", "--start-experiment", "--mdnsd",
                "--seed-display", "--skip-hid", "--mock-mac", "--interface-lo0-qemu", "--build-only"]
    try:
        capture.main()
    finally:
        capture.make_manifest, capture.docker = original_manifest, original_docker
    (OUT / "build-record.json").write_text(json.dumps({"base_ifs_sha256": IMAGE_HASH,
        "output_sha256": hashlib.sha256((OUT / "qemu_start_debug.bin").read_bytes()).hexdigest(),
        "toolchain": image, "interface": "ecm0", "pairing_hook": pairing,
        "paired_pairing": pairing and not legacy_altscreen,
        "native_media": media and not legacy_altscreen, "software_mfi": software_mfi, "request_ui": request_ui,
        "request_ui_music": request_ui_music,
        "request_ui_maps": request_ui_maps, "pacing_only": pacing_only,
        "input_control": input_control,
        "capture_memory": capture_memory, "clock_realtime": clock_realtime,
        "timer_frequency_hz": qnx_clock.FREQUENCY if clock_realtime else None,
        "capture_only_altscreen": capture_only_altscreen,
        "probe_trace_sink": "/dev/null" if pacing_only else "UART",
        "capture_video": capture_video,
        "capture_video_long": capture_video_long, "legacy_altscreen": legacy_altscreen,
        "paired_altscreen": paired_altscreen,
        "pairing_hook_sha256": hashlib.sha256(hook.read_bytes()).hexdigest() if pairing else None,
        "synthetic_phone": False}, indent=2) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("build", "start", "stop", "status", "info"))
    parser.add_argument("--image", default="qnx65-armv7-toolchain:8.5")
    parser.add_argument("--transport", choices=("user", "socket"), default="user")
    parser.add_argument("--lab-dir", type=Path, help="receiver artifacts inside the owned build tree")
    parser.add_argument("--pairing", action="store_true", help="build with the existing stock210 pairing hook and writable test store")
    parser.add_argument("--media", action="store_true", help="enable the existing paired media adapters; requires --pairing")
    parser.add_argument("--software-mfi", action="store_true", help="enable the QEMU-only host MFi-SAP responder; requires --pairing --media")
    parser.add_argument("--request-ui", action="store_true", help="request the native CarPlay UI after RECORD; requires --software-mfi")
    parser.add_argument("--request-ui-music", action="store_true", help="request the Music CarPlay UI URL instead of allowing the phone to choose Maps")
    parser.add_argument("--request-ui-maps", action="store_true", help="request the native Maps CarPlay UI URL")
    parser.add_argument("--pacing-only", action="store_true", help="disable AVC payload capture and sink socket probe traces to /dev/null; preserves authentication")
    parser.add_argument("--clock-realtime", action="store_true", help="correct pinned QNX qtime to the explicit 62.5 MHz QEMU timer")
    parser.add_argument("--capture-only-altscreen", action="store_true", help="observe authenticated stream111 RAM without a renderer forwarder; no presentation success reported")
    parser.add_argument("--input-control", action="store_true", help="register virtual primary touch HID and expose its bounded test endpoint on loopback")
    parser.add_argument("--capture-memory", action="store_true", help="retain a bounded AVC GOP per screen in RAM and retrieve through the owned loopback endpoint")
    parser.add_argument("--capture-video", action="store_true", help="capture bounded plaintext AVC at the mock decoder; requires --software-mfi")
    parser.add_argument("--capture-video-long", action="store_true", help="increase decoder capture bound to 4096 records/16 MiB; requires --capture-video")
    parser.add_argument("--legacy-altscreen", action="store_true", help="isolated legacy AES AltScreen negotiation; disables paired wireless media")
    parser.add_argument("--paired-altscreen", action="store_true", help="QEMU-only authenticated paired stream 111; requires the dedicated hook and --software-mfi")
    args = parser.parse_args()
    if args.lab_dir is not None:
        set_output(args.lab_dir)
    if args.paired_altscreen and (args.legacy_altscreen or not args.software_mfi):
        parser.error("--paired-altscreen requires --software-mfi and excludes --legacy-altscreen")
    if args.capture_video_long and not args.capture_video:
        parser.error("--capture-video-long requires --capture-video")
    if args.legacy_altscreen and not args.software_mfi:
        parser.error("--legacy-altscreen requires --software-mfi")
    if args.request_ui_music and not args.request_ui:
        parser.error("--request-ui-music requires --request-ui")
    if args.capture_video and not args.software_mfi:
        parser.error("--capture-video requires --software-mfi")
    if args.request_ui and not args.software_mfi:
        parser.error("--request-ui requires --software-mfi")
    if args.software_mfi and not args.media:
        parser.error("--software-mfi requires --media")
    if args.media and not args.pairing:
        parser.error("--media requires --pairing")
    OUT.mkdir(parents=True, exist_ok=True)
    if args.action == "build":
        build(args.image, args.pairing, args.media, args.software_mfi, args.request_ui,
              args.request_ui_music, args.capture_video, args.capture_video_long, args.legacy_altscreen, args.paired_altscreen,
              args.request_ui_maps, args.pacing_only, args.input_control, args.capture_memory, args.clock_realtime, args.capture_only_altscreen)
    elif args.action == "start":
        if running():
            raise RuntimeError("owned QNX receiver is already running")
        if not (OUT / "qemu_start_debug.bin").is_file():
            raise RuntimeError("build the QNX receiver first")
        # Host USB diagnostics can interrupt individual UART characters when
        # merged into the same file. Keep decoder-input capture intact.
        with (OUT / "serial.log").open("w") as stream, (OUT / "qemu-host.log").open("w") as host_log:
            child = subprocess.Popen(command(args.transport), stdin=subprocess.DEVNULL, stdout=stream,
                                     stderr=host_log, start_new_session=True)
        (OUT / "qnx.pid").write_text(str(child.pid) + "\n")
        (OUT / "transport.txt").write_text(args.transport + "\n")
        print(json.dumps({"qnx_pid": child.pid, "transport": args.transport}))
    elif args.action == "stop":
        pid = running()
        if pid:
            os.kill(pid, signal.SIGTERM)
            deadline = time.monotonic() + 10
            while running() == pid:
                if time.monotonic() >= deadline:
                    raise RuntimeError("owned QNX guest has not stopped")
                time.sleep(.1)
        print(json.dumps({"stopped_pid": pid}))
    elif args.action == "status":
        transport = (OUT / "transport.txt").read_text().strip() if (OUT / "transport.txt").exists() else "user"
        print(json.dumps({"qnx_pid": running(), "transport": transport,
                          "port": 32501 if transport == "socket" else 32500}))
    else:
        import urllib.request
        transport = (OUT / "transport.txt").read_text().strip() if (OUT / "transport.txt").exists() else "user"
        if transport == "socket":
            serial = (OUT / "serial.log").read_text(errors="replace")
            packets = re.findall(r"info5000-hex=([0-9a-f]+)", serial)
            if not packets:
                raise RuntimeError("native QNX /info diagnostic is not ready")
            body = bytes.fromhex(packets[-1]).split(b"\r\n\r\n", 1)[1]
        else:
            with urllib.request.urlopen("http://127.0.0.1:32500/info", timeout=5) as response:
                body = response.read(65537)
        if len(body) > 65536:
            raise ValueError("receiver /info response too large")
        (OUT / "info.bplist").write_bytes(body)
        value = plistlib.loads(body)
        if transport == "socket":
            keys = re.findall(r"receiver-edpk=([0-9a-f]{64})(?![0-9a-f])", serial)
            if keys:
                value["pk"] = bytes.fromhex(keys[-1])
        print(json.dumps({key: item.hex() if isinstance(item, bytes) else item
            for key, item in value.items() if key in ("pk", "deviceID", "features", "sourceVersion", "model",
                "extendedFeatures", "displays", "displayPanels")},
                default=lambda obj: obj.hex() if isinstance(obj, bytes) else str(obj)))


if __name__ == "__main__":
    main()
