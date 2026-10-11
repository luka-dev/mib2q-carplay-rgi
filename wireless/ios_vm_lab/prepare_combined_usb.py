#!/usr/bin/env python3
"""Provision only a stopped, cloned iOS VM image with the native USB helper."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import struct
import subprocess
import tempfile
import zlib

from lab import DEFAULT_LAB, running_pid, check_guest_disks


def decode_trustcache(container: bytes) -> bytes:
    """Read the bounded IM4P payload without modifying Apple's source file."""
    if len(container) > 1024 * 1024:
        raise ValueError("trustcache container too large")
    def tlv(data, offset):
        if offset + 2 > len(data):
            raise ValueError("truncated DER")
        tag, length = data[offset:offset + 2]
        start = offset + 2
        if length & 128:
            count = length & 127
            if not 1 <= count <= 4 or start + count > len(data):
                raise ValueError("invalid DER length")
            length = int.from_bytes(data[start:start + count], "big")
            start += count
        end = start + length
        if end > len(data):
            raise ValueError("truncated DER value")
        return tag, data[start:end], end
    tag, body, end = tlv(container, 0)
    if tag != 0x30 or end != len(container):
        raise ValueError("expected IM4P DER sequence")
    values, offset = [], 0
    while offset < len(body):
        tag, value, offset = tlv(body, offset)
        values.append((tag, value))
    if (len(values) < 4 or values[0] != (0x16, b"IM4P") or
            values[1] not in ((0x16, b"rtsc"), (0x16, b"trst")) or values[3][0] != 4):
        raise ValueError("unexpected trustcache IM4P")
    return values[3][1]


def initialize(lab: Path, base: Path) -> None:
    lab, base = lab.resolve(), base.resolve()
    if not base.is_relative_to(lab):
        raise RuntimeError("clone directory must remain inside the lab")
    if (base / "disks").exists():
        raise RuntimeError("initialization requires a new clone directory")
    command = json.loads((lab / "ios-normal-command.json").read_text())
    check_guest_disks(lab, command)
    source_cache = None
    for index, item in enumerate(command):
        if item == "-M":
            options = command[index + 1].split(",")
            source_cache = next((Path(x.split("=", 1)[1]).resolve() for x in options
                                 if x.startswith("trustcache=")), None)
            command[index + 1] = ",".join("trustcache=" + str(base / "trustcache.raw")
                if x.startswith("trustcache=") else x for x in options)
    if source_cache is None or not source_cache.is_relative_to(lab):
        raise RuntimeError("normal profile must use a local trustcache")
    cache = decode_trustcache(source_cache.read_bytes())
    # Validate v1 entries before cloning anything; the added helper hash comes later.
    if len(cache) < 24 or struct.unpack_from("<I", cache)[0] != 1 or len(cache) != 24 + struct.unpack_from("<I", cache, 20)[0] * 22:
        raise ValueError("invalid source trustcache")
    with tempfile.TemporaryDirectory(prefix="combined-clone-", dir=lab) as temp:
        stage = Path(temp) / "disks"
        stage.mkdir()
        names = set()
        for index, item in enumerate(command):
            if item != "-drive":
                continue
            options = command[index + 1].split(",")
            source = Path(next(x.split("=", 1)[1] for x in options if x.startswith("file="))).resolve()
            if source.name in names:
                raise ValueError("duplicate disk names in normal profile")
            names.add(source.name)
            subprocess.run(["cp", "-c", str(source), str(stage / source.name)], check=True)
            command[index + 1] = ",".join("file=" + str(base / "disks" / source.name)
                if x.startswith("file=") else x for x in options)
        stage.rename(base / "disks")
    (base / "trustcache.raw").write_bytes(cache)
    (lab / "ios-normal-combined-usb-command.json").write_text(json.dumps(command, indent=2) + "\n")
    (base / "clone-record.json").write_text(json.dumps({"disks": sorted(names),
        "source_profile": str(lab / "ios-normal-command.json"), "consistent_stopped_snapshot": True}, indent=2) + "\n")


def gpt_view(head: bytes, tail: bytes, size: int) -> tuple[list[tuple[int, bytes]], int, int]:
    h = head[4096:4096 + 92]
    if len(h) != 92 or h[:8] != b"EFI PART" or size % 4096:
        raise ValueError("expected native 4096-byte GPT")
    checked = bytearray(h)
    crc = struct.unpack_from("<I", checked, 16)[0]
    struct.pack_into("<I", checked, 16, 0)
    if (struct.unpack_from("<I", h, 12)[0] != 92 or zlib.crc32(checked) != crc or
            struct.unpack_from("<QQ", h, 24) != (1, size // 4096 - 1)):
        raise ValueError("invalid native GPT header")
    lba, count, entry_size, checksum = struct.unpack_from("<QIII", h, 72)
    if not 1 <= count <= 1024 or entry_size != 128 or lba < 2 or lba * 4096 + count * entry_size > len(head):
        raise ValueError("unsupported GPT entry array")
    table = bytearray(head[lba * 4096:lba * 4096 + count * entry_size])
    if zlib.crc32(table) != checksum:
        raise ValueError("GPT entry checksum mismatch")
    bounds = []
    for i in range(count):
        off = i * entry_size
        if not any(table[off:off + 16]):
            continue
        first, last = struct.unpack_from("<QQ", table, off + 32)
        if not first <= last or (last + 1) * 4096 > size:
            raise ValueError("invalid GPT partition bounds")
        bounds.append((first * 4096, (last + 1) * 4096))
        struct.pack_into("<QQ", table, off + 32, first * 8, last * 8 + 7)
    if not bounds:
        raise ValueError("GPT has no partitions")
    start = min(x[0] for x in bounds)
    end = max(x[1] for x in bounds)
    sectors = size // 512
    slots = (len(table) + 511) // 512
    if (1024 + len(table) > start or start > len(head) or
            end > (sectors - 1 - slots) * 512 or size - end > len(tail)):
        raise ValueError("GPT view would overlap a filesystem")
    def header(current, backup, entry):
        data = bytearray(h)
        struct.pack_into("<I", data, 16, 0)
        struct.pack_into("<QQ", data, 24, current, backup)
        struct.pack_into("<QQ", data, 40, struct.unpack_from("<Q", h, 40)[0] * 8,
                         struct.unpack_from("<Q", h, 48)[0] * 8 + 7)
        struct.pack_into("<Q", data, 72, entry)
        struct.pack_into("<I", data, 88, zlib.crc32(table))
        struct.pack_into("<I", data, 16, zlib.crc32(data))
        return bytes(data) + bytes(512 - len(data))
    mbr = bytearray(512)
    mbr[450] = 0xEE
    struct.pack_into("<II", mbr, 454, 1, min(sectors - 1, 0xFFFFFFFF))
    mbr[510:512] = b"\x55\xaa"
    return [(0, bytes(mbr)), (512, header(1, sectors - 1, 2)), (1024, bytes(table)),
            ((sectors - 1 - slots) * 512, bytes(table)),
            ((sectors - 1) * 512, header(sectors - 1, 1, sectors - 1 - slots))], start, end


def build_helper(base: Path, screen_render_probe: bool = False,
                 screen_framebuffer_probe: bool = False) -> str:
    capture_enabled = screen_render_probe or screen_framebuffer_probe
    libraries = {
        "libSystem": ("/usr/lib/libSystem.B.dylib", ["_printf", "_puts", "_snprintf", "_sleep", "_alarm", "_strcmp", "_getuid", "_setuid", "_setgid", "_fflush",
            "_notify_register_check", "_notify_set_state", "_notify_post", "_notify_cancel",
            "_memset", "_malloc", "_free", "_fopen", "_fwrite", "_fclose", "_fork", "_execv", "_waitpid", "___error", "__exit",
            "___stack_chk_fail", "___stack_chk_guard"]),
        "CoreFoundation": ("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", [
            "_kCFTypeDictionaryKeyCallBacks", "_kCFTypeDictionaryValueCallBacks", "_kCFTypeArrayCallBacks",
            "_kCFBooleanTrue", "_kCFBooleanFalse", "_CFStringCreateWithCString", "_CFNumberCreate", "_CFDictionaryCreateMutable",
            "_CFDictionaryCreateMutableCopy", "_CFDictionaryGetValue", "_CFDictionarySetValue",
            "_CFArrayCreate", "_CFEqual", "_CFRelease", "_CFDictionaryGetCount", "_CFStringGetCString", "_CFPreferencesSetAppValue", "_CFPreferencesAppSynchronize",
            "_kCFPreferencesAnyUser", "_kCFPreferencesAnyHost", "_CFPreferencesCopyValue", "_CFPreferencesSetValue",
            "_CFPreferencesSynchronize", "_CFPreferencesGetAppIntegerValue"]),
        "VideoToolbox": ("/System/Library/Frameworks/VideoToolbox.framework/VideoToolbox", [
            "_VTCopySupportedPropertyDictionaryForEncoder", "_VTCompressionSessionCreate", "_VTSessionSetProperty",
            "_VTCompressionSessionPrepareToEncodeFrames", "_VTCompressionSessionEncodeFrame",
            "_VTCompressionSessionCompleteFrames", "_VTCompressionSessionInvalidate",
            "_kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder",
            "_kVTCompressionPropertyKey_RealTime", "_kVTCompressionPropertyKey_AllowFrameReordering"]),
        "CoreMedia": ("/System/Library/Frameworks/CoreMedia.framework/CoreMedia", [
            "_CMSampleBufferGetDataBuffer", "_CMSampleBufferGetFormatDescription", "_CMBlockBufferGetDataLength",
            "_CMBlockBufferCopyDataBytes", "_CMVideoFormatDescriptionGetH264ParameterSetAtIndex"]),
        "CoreVideo": ("/System/Library/Frameworks/CoreVideo.framework/CoreVideo", [
            "_CVPixelBufferCreate", "_CVPixelBufferLockBaseAddress", "_CVPixelBufferUnlockBaseAddress",
            "_CVPixelBufferGetBaseAddressOfPlane", "_CVPixelBufferGetBytesPerRowOfPlane", "_CVPixelBufferGetPlaneCount"]),
        "Metal": ("/System/Library/Frameworks/Metal.framework/Metal", ["_MTLCreateSystemDefaultDevice"]),
        "objc": ("/usr/lib/libobjc.A.dylib", ["_objc_release"]),
        "CoreUtils": ("/System/Library/PrivateFrameworks/CoreUtils.framework/CoreUtils", ["_SocketGetInterfaceInfo"]),
        "IOKit": ("/System/Library/Frameworks/IOKit.framework/Versions/A/IOKit", [
            "_IOServiceMatching", "_IOServiceGetMatchingService", "_IORegistryEntryCreateCFProperty",
            "_IORegistryEntrySetCFProperties", "_IOObjectRelease"])}
    extra_objects = []
    defines = []
    if capture_enabled:
        libraries["libSystem"][1].extend(["_unlink", "_bzero", "_mach_task_self_", "_mach_absolute_time",
            "_memcmp", "_dlopen", "__dyld_image_count", "__dyld_get_image_name", "__dyld_get_image_header",
            "__dyld_get_image_vmaddr_slide"])
        libraries["IOKit"][1].extend(["_IOServiceOpen", "_IOServiceClose"])
        libraries["CoreFoundation"][1].append("_CFRetain")
        libraries["CoreFoundation"][1].extend(["_CFRunLoopGetCurrent", "_CFRunLoopAddSource",
            "_CFRunLoopRemoveSource", "_CFRunLoopRunInMode", "_kCFRunLoopDefaultMode"])
        libraries["CoreVideo"][1].extend(["_CVPixelBufferGetIOSurface", "_kCVPixelBufferIOSurfacePropertiesKey"])
        libraries.update({
            "QuartzCore": ("/System/Library/Frameworks/QuartzCore.framework/QuartzCore", ["_CARenderServerRenderDisplay"]),
            "IOSurface": ("/System/Library/Frameworks/IOSurface.framework/IOSurface", [
                "_IOSurfaceCreate", "_IOSurfaceAlignProperty", "_IOSurfaceGetWidth", "_IOSurfaceGetHeight",
                "_IOSurfaceLookup", "_IOSurfaceGetPixelFormat",
                "_IOSurfaceGetPlaneCount", "_IOSurfaceGetWidthOfPlane", "_IOSurfaceGetHeightOfPlane",
                "_IOSurfaceGetBytesPerRowOfPlane", "_IOSurfaceGetBaseAddressOfPlane",
                "_IOSurfaceGetBytesPerRow", "_IOSurfaceGetAllocSize", "_IOSurfaceGetBaseAddress", "_IOSurfaceLock", "_IOSurfaceUnlock",
                "_kIOSurfaceWidth", "_kIOSurfaceHeight", "_kIOSurfaceBytesPerElement", "_kIOSurfaceBytesPerRow",
                "_kIOSurfaceAllocSize", "_kIOSurfacePixelFormat", "_kIOSurfaceIsGlobal"])})
        extra_objects = [str(base / "screen_render_probe.o")]
        defines = ["-DMHI2_IOS_VM_SCREEN_RENDER_PROBE"]
    for name, (install, symbols) in libraries.items():
        (base / f"{name}.tbd").write_text("--- !tapi-tbd-v3\narchs: [ arm64 ]\nplatform: ios\ninstall-name: " +
            install + "\nexports:\n  - archs: [ arm64 ]\n    symbols: [ " + ", ".join(symbols) + " ]\n...\n")
    (base / "empty-sysroot").mkdir(exist_ok=True)
    subprocess.run(["clang", "-target", "arm64-apple-ios14.0", "-isysroot", str(base / "empty-sysroot"),
                    "-O2", "-Wall", "-Wextra", "-Werror", "-c", str(Path(__file__).with_name("video_probe.c")),
                    "-o", str(base / "video_probe.o")], check=True,
                   env={k: v for k, v in os.environ.items() if k != "SDKROOT"})
    if capture_enabled:
        subprocess.run(["clang", "-target", "arm64-apple-ios14.0", "-isysroot", str(base / "empty-sysroot"),
                        "-O2", "-Wall", "-Wextra", "-Werror", "-c", str(Path(__file__).with_name("screen_render_probe.c")),
                        "-o", str(base / "screen_render_probe.o")], check=True,
                       env={k: v for k, v in os.environ.items() if k != "SDKROOT"})
    subprocess.run(["clang", "-target", "arm64-apple-ios14.0", "-isysroot", str(base / "empty-sysroot"),
                    "-O2", "-Wall", "-Wextra", "-Werror", *defines, "-c", str(Path(__file__).with_name("usb_fixture.c")),
                    "-o", str(base / "usb_fixture.o")], check=True,
                   env={k: v for k, v in os.environ.items() if k != "SDKROOT"})
    subprocess.run(["ld", "-ld_classic", "-arch", "arm64", "-platform_version", "ios", "14.0", "14.0",
                    "-o", str(base / "usb_fixture"), str(base / "usb_fixture.o"), str(base / "video_probe.o"), *extra_objects,
                    *[str(base / f"{name}.tbd") for name in libraries]], check=True)
    entitlement = base / "entitlements.plist"
    rights = {"com.apple.private.usbdevice.setdescription": True}
    if capture_enabled:
        rights.update({
            "com.apple.security.iokit-user-client-class": ["IOSurfaceRootUserClient"],
            "com.apple.QuartzCore.global-capture": True,
            "com.apple.QuartzCore.get-client-port": True})
    if screen_framebuffer_probe:
        # vfbserv_verifyConnection in the pinned iOS checks this exact right.
        # Keep it out of capture-only and default USB helper builds.
        rights["com.apple.backboardd"] = True
    entitlement.write_bytes(plistlib.dumps(rights))
    subprocess.run(["codesign", "-f", "-s", "-", "--entitlements", str(entitlement), str(base / "usb_fixture")], check=True)
    signed = subprocess.run(["codesign", "-d", "--verbose=4", str(base / "usb_fixture")],
                            capture_output=True, text=True, check=True)
    return re.search(r"^CDHash=([a-f0-9]+)$", signed.stderr, re.M)[1]


def trustcache(payload: bytes, digest: str) -> bytes:
    if not re.fullmatch(r"[a-f0-9]{40}", digest):
        raise ValueError("invalid code directory hash")
    if len(payload) < 24 or struct.unpack_from("<I", payload)[0] != 1:
        raise ValueError("expected trustcache v1")
    count = struct.unpack_from("<I", payload, 20)[0]
    if len(payload) != 24 + count * 22:
        raise ValueError("invalid trustcache length")
    entries = {payload[24 + i * 22:24 + (i + 1) * 22] for i in range(count)}
    entries.add(bytes.fromhex(digest) + b"\x02\0")
    return payload[:20] + struct.pack("<I", len(entries)) + b"".join(sorted(entries, key=lambda x: x[:20]))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--init", action="store_true", help="clone all normal-profile disks while the VM is stopped")
    parser.add_argument("--complete-setup", action="store_true",
        help="install an explicit mobile-user setup-completed state fixture in the clone")
    parser.add_argument("--network-info", action="store_true", help="install a bounded read-only interface classifier probe")
    parser.add_argument("--codec-info", action="store_true", help="install a bounded native VideoToolbox property probe")
    screen = parser.add_mutually_exclusive_group()
    screen.add_argument("--screen-render-probe", action="store_true",
        help="capture the actual CarPlay Wireless display tree through the native software renderer")
    screen.add_argument("--screen-framebuffer-probe", action="store_true",
        help="capture real CarPlay UI and submit at most 15 frames through native framebuffer 1")
    video = parser.add_mutually_exclusive_group()
    video.add_argument("--no-video-probe", action="store_true",
        help="remove the bounded video/encoder-mode launch job for a readback-only boot")
    video.add_argument("--video-info", action="store_true", help="install a bounded native Metal/CPU-fed H.264 probe")
    video.add_argument("--video-info-software", action="store_true",
        help="temporarily set UnifiedEncoderMode=0 during the native H.264 probe")
    video.add_argument("--software-encoder-session", action="store_true",
        help="hold UnifiedEncoderMode=0 for a bounded 300-second native CarPlay session")
    codec = parser.add_mutually_exclusive_group()
    codec.add_argument("--codec-properties-experiment", action="store_true",
        help="opt into the pinned software-fallback property recursion experiment")
    codec.add_argument("--revert-codec-properties", action="store_true",
        help="restore the exact pre-experiment codec instruction and cache hash")
    args = parser.parse_args()
    lab = DEFAULT_LAB.resolve()
    if running_pid(lab):
        raise RuntimeError("stop the owned iOS guest before provisioning its clone")
    base = lab / "combined-usb"
    base.mkdir(exist_ok=True)
    if args.init:
        initialize(lab, base)
    if not (base / "trustcache.raw").is_file() or not (base / "disks/root").is_file():
        raise RuntimeError("initial cloned disks/trustcache must exist")
    digest = build_helper(base, args.screen_render_probe, args.screen_framebuffer_probe)
    image = base / "disks/root"
    if image.is_symlink() or not image.resolve().is_relative_to(lab):
        raise RuntimeError("clone must be a regular image inside the lab")
    info = plistlib.loads(subprocess.check_output(["hdiutil", "info", "-plist"]))
    if any(Path(i.get("image-path", "/")).resolve() == image for i in info.get("images", [])):
        raise RuntimeError("clone is already attached")
    with image.open("rb") as f:
        head = f.read(1024 * 1024)
        f.seek(-1024 * 1024, 2)
        tail = f.read(1024 * 1024)
    writes, start, end = gpt_view(head, tail, image.stat().st_size)
    device = None
    try:
        with image.open("r+b") as f:
            for offset, data in writes:
                f.seek(offset)
                f.write(data)
        result = subprocess.check_output(["hdiutil", "attach", "-imagekey", "diskimage-class=CRawDiskImage",
            "-noverify", "-noautofsck", "-nobrowse", "-owners", "off", "-mountroot", str(base / "mounts"),
            "-plist", str(image)])
        attached = plistlib.loads(result)
        (base / "attach.plist").write_bytes(result)
        devices = [e["dev-entry"] for e in attached["system-entities"] if re.fullmatch(r"/dev/disk\d+", e.get("dev-entry", ""))]
        device = min(devices, key=lambda x: int(x.removeprefix("/dev/disk")))
        system = base / "mounts/System"
        if args.codec_properties_experiment or args.revert_codec_properties:
            from codec_patch import patch_cache
            cache = system / "System/Library/Caches/com.apple.dyld/dyld_shared_cache_arm64e"
            if not cache.resolve().is_relative_to(system.resolve()):
                raise RuntimeError("cache must remain inside the owned cloned System volume")
            record = patch_cache(cache, revert=args.revert_codec_properties)
            (base / "codec-properties-experiment.json").write_text(json.dumps(record, indent=2) + "\n")
        target = system / "usr/libexec/mhi2-usb-fixture"
        shutil.copy2(base / "usb_fixture", target)
        target.chmod(0o755)
        launch_path = system / "System/Library/xpc/launchd.plist"
        launch = plistlib.loads(launch_path.read_bytes())
        usb_job = "/System/Library/LaunchDaemons/com.luka.ios-vm.usbfixture.plist"
        launch["LaunchDaemons"][usb_job] = {
            "Label": "com.luka.ios-vm.usbfixture", "ProgramArguments": ["/usr/libexec/mhi2-usb-fixture"],
            "RunAtLoad": True, "LaunchOnlyOnce": True, "UserName": "root",
            "StandardOutPath": "/dev/console", "StandardErrorPath": "/dev/console"}
        if args.complete_setup:
            job = "/System/Library/LaunchDaemons/com.luka.ios-vm.setupfixture.plist"
            launch["LaunchDaemons"][job] = {
                "Label": "com.luka.ios-vm.setupfixture",
                "ProgramArguments": ["/usr/libexec/mhi2-usb-fixture", "--complete-setup"],
                "RunAtLoad": True, "LaunchOnlyOnce": True, "UserName": "root",
                "StandardOutPath": "/dev/console", "StandardErrorPath": "/dev/console"}
        if args.network_info:
            launch["LaunchDaemons"]["/System/Library/LaunchDaemons/com.luka.ios-vm.networkinfo.plist"] = {
                "Label": "com.luka.ios-vm.networkinfo",
                "ProgramArguments": ["/usr/libexec/mhi2-usb-fixture", "--network-info"],
                "RunAtLoad": True, "LaunchOnlyOnce": True, "UserName": "root",
                "StandardOutPath": "/dev/console", "StandardErrorPath": "/dev/console"}
        if args.codec_info:
            launch["LaunchDaemons"]["/System/Library/LaunchDaemons/com.luka.ios-vm.codecinfo.plist"] = {
                "Label": "com.luka.ios-vm.codecinfo",
                "ProgramArguments": ["/usr/libexec/mhi2-usb-fixture", "--codec-info"],
                "RunAtLoad": True, "LaunchOnlyOnce": True, "UserName": "root",
                "StandardOutPath": "/dev/console", "StandardErrorPath": "/dev/console"}
        if args.video_info or args.video_info_software or args.software_encoder_session:
            launch["LaunchDaemons"]["/System/Library/LaunchDaemons/com.luka.ios-vm.videoinfo.plist"] = {
                "Label": "com.luka.ios-vm.videoinfo",
                "ProgramArguments": ["/usr/libexec/mhi2-usb-fixture",
                    "--software-encoder-session" if args.software_encoder_session else
                    "--video-info-software" if args.video_info_software else "--video-info"],
                "RunAtLoad": True, "LaunchOnlyOnce": True, "UserName": "root",
                "StandardOutPath": "/dev/console", "StandardErrorPath": "/dev/console"}
        elif args.no_video_probe:
            launch["LaunchDaemons"].pop("/System/Library/LaunchDaemons/com.luka.ios-vm.videoinfo.plist", None)
        if args.screen_render_probe or args.screen_framebuffer_probe:
            launch["LaunchDaemons"]["/System/Library/LaunchDaemons/com.luka.ios-vm.screenrender.plist"] = {
                "Label": "com.luka.ios-vm.screenrender",
                "ProgramArguments": ["/usr/libexec/mhi2-usb-fixture",
                    "--screen-framebuffer-probe" if args.screen_framebuffer_probe else "--screen-render-probe"],
                "RunAtLoad": True, "LaunchOnlyOnce": True, "UserName": "root",
                "StandardOutPath": "/dev/console", "StandardErrorPath": "/dev/console"}
        else:
            launch["LaunchDaemons"].pop("/System/Library/LaunchDaemons/com.luka.ios-vm.screenrender.plist", None)
        launch_path.write_bytes(plistlib.dumps(launch, fmt=plistlib.FMT_BINARY))
        setup_fixture = "/System/Library/LaunchDaemons/com.luka.ios-vm.setupfixture.plist" in launch["LaunchDaemons"]
    finally:
        if device:
            subprocess.run(["hdiutil", "detach", device], check=True)
        with image.open("r+b") as f:
            f.seek(0)
            f.write(head[:start])
            f.seek(end)
            f.write(tail[-(image.stat().st_size - end):])
    cache = base / "trustcache.raw"
    cache.write_bytes(trustcache(cache.read_bytes(), digest))
    (base / "helper-provision.json").write_text(json.dumps({"cdhash": digest,
        "binary_sha256": hashlib.sha256((base / "usb_fixture").read_bytes()).hexdigest(),
        "entitlement": "com.apple.private.usbdevice.setdescription", "setup_state_fixture": setup_fixture,
        "screen_render_probe": args.screen_render_probe,
        "screen_framebuffer_probe": args.screen_framebuffer_probe,
        "native_GPT_restored": True}, indent=2) + "\n")
    print("Native helper rebuilt and installed in owned clone", digest)


if __name__ == "__main__":
    main()
