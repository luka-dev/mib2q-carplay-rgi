"""Bounded native USB inspection and opt-in iAP2 configuration on our vphone."""
from __future__ import annotations

import argparse
import base64
import fcntl
import json
import os
from pathlib import Path
import plistlib
import re
import subprocess
import time

from . import lab

GUEST_DIR = "/private/var/tmp/mhi2-vphone-lab"
ACTIONS = ("probe", "apply", "apply-destroy", "ncm-direct", "restore", "reconnect", "network", "wireless-network", "wireless-profile", "wireless-adapter-probe", "network-classify",
           "iap-list", "iap-wake", "iap-inspect", "iap-registry", "iap-smoke", "iap-peer", "iap-smoke-bluetooth", "iap-peer-bluetooth", "iap-stream-bluetooth", "manager-apply", "manager-remove", "port-prepare")
PEER_ACTIONS = ("iap-peer", "iap-peer-bluetooth", "iap-stream-bluetooth")
READ_ONLY_ACTIONS = ("probe", "network", "wireless-network", "wireless-profile", "network-classify", "iap-list", "iap-wake", "iap-inspect", "iap-registry")


def build(*, catalogue: bool = False) -> Path:
    out = lab.OUT / "usb"
    out.mkdir(parents=True, exist_ok=True)
    libraries = {
        "libSystem": ("/usr/lib/libSystem.B.dylib", ["putchar", "snprintf", "strstr", "free", "printf", "puts", "strcmp", "getuid", "sleep", "alarm", "fflush", "dlopen", "dlsym", "dlerror", "fopen", "fread", "fclose", "socket", "connect", "time", "__error", "os_unfair_lock_lock", "os_unfair_lock_unlock", "close", "send", "recv", "setsockopt", "inet_pton", "if_nametoindex", "htons", "memcmp", "xpc_connection_create_mach_service", "xpc_connection_set_event_handler", "xpc_connection_activate", "xpc_dictionary_create", "xpc_connection_send_message", "xpc_connection_cancel", "xpc_release", "_NSConcreteGlobalBlock", "_NSConcreteStackBlock", "__chkstk_darwin", "__stack_chk_guard", "__stack_chk_fail"]),
        "CoreFoundation": ("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", [
            "kCFTypeDictionaryKeyCallBacks", "kCFTypeDictionaryValueCallBacks", "kCFTypeArrayCallBacks", "kCFBooleanTrue",
            "CFStringCreateWithCString", "CFDictionaryCreateMutable", "CFDictionaryGetValue", "CFDictionarySetValue",
            "CFArrayCreate", "CFRelease", "CFStringGetCString", "CFPropertyListCreateData", "CFDataGetBytePtr", "CFDataGetLength",
            "CFDataCreate", "CFPropertyListCreateWithData", "CFGetTypeID", "CFDictionaryGetTypeID", "CFDataGetTypeID", "CFStringGetTypeID", "CFNumberGetTypeID", "CFNumberGetValue", "CFNumberCreate"]),
        "IOKit": ("/System/Library/Frameworks/IOKit.framework/IOKit", ["IOServiceGetMatchingService", "IOServiceMatching",
            "IORegistryEntryCreateCFProperty", "IORegistryEntrySearchCFProperty", "IORegistryEntryFromPath", "IORegistryEntrySetCFProperties", "IOObjectRelease", "IOObjectIsEqualTo", "IOObjectConformsTo", "IORegistryEntryGetParentEntry", "IORegistryEntryGetChildIterator", "IOIteratorNext", "IORegistryEntryInPlane"]),
        "objc": ("/usr/lib/libobjc.A.dylib", ["objc_getClass", "sel_registerName", "objc_msgSend", "class_copyMethodList", "method_getName", "sel_getName", "class_getName", "objc_copyClassList", "class_getImageName"]),
    }
    for name, (install, symbols) in libraries.items():
        (out / f"{name}.tbd").write_text(
            "--- !tapi-tbd-v3\narchs: [ arm64 ]\nplatform: ios\ninstall-name: " + install +
            "\nexports:\n  - archs: [ arm64 ]\n    symbols: [ " + ", ".join("_" + s for s in symbols) + " ]\n...\n")
    sysroot = out / "empty-sysroot"
    sysroot.mkdir(exist_ok=True)
    env = {k: v for k, v in os.environ.items() if k != "SDKROOT"}
    subprocess.run(["clang", "-target", "arm64-apple-ios18.0", "-isysroot", str(sysroot),
                    "-O2", "-Wall", "-Wextra", "-Werror", "-fblocks", "-I", str(Path(__file__).parent), "-c", str(Path(__file__).with_name("usb_description.c")),
                    "-o", str(out / "usb.o")], env=env, check=True)
    binary = out / "usb-description"
    subprocess.run(["ld", "-ld_classic", "-arch", "arm64", "-platform_version", "ios", "18.0", "18.0",
                    "-o", str(binary), str(out / "usb.o"), *[str(out / f"{n}.tbd") for n in libraries]], check=True)
    entitlement = out / "entitlements.plist"
    entitlements = {"com.apple.private.usbdevice.setdescription": True}
    if catalogue:
        entitlements["com.apple.private.security.iocatalog-management"] = True
    entitlement.write_bytes(plistlib.dumps(entitlements))
    subprocess.run(["codesign", "-f", "-s", "-", "--entitlements", str(entitlement), str(binary)], check=True)
    return binary


def _validate(mode,inspect_class,stream_type,peer_nonce,peer_seconds,on_launch):
    if mode not in ACTIONS:
        raise ValueError("unknown native USB action")
    if inspect_class is not None and (mode != "iap-inspect" or not re.fullmatch(r"ACCTransport[A-Za-z]{1,80}", inspect_class)):
        raise ValueError("inspection requires a native transport class")
    if mode == "port-prepare":
        if type(stream_type) is not int or stream_type not in (1, 4, 5):
            raise ValueError("port metadata experiments require an explicit reviewed stream type: 1, 4 or 5")
    elif stream_type is not None:
        raise ValueError("stream type applies only to a port metadata experiment")
    if mode in PEER_ACTIONS:
        if not isinstance(peer_nonce, str) or not re.fullmatch(r"[0-9a-f]{16}", peer_nonce) or not callable(on_launch):
            raise ValueError("native API peer requires a scoped nonce and launch observer")
        if type(peer_seconds) is not int or not 60 <= peer_seconds <= 180:
            raise ValueError("native API peer lifetime must be 60..180 seconds")
    elif peer_nonce is not None or on_launch is not None or peer_seconds != 60:
        raise ValueError("peer options apply only to iap-peer")


def _run(mode: str, inspect_class: str | None = None, stream_type: int | None = None,
         *, peer_nonce: str | None = None, peer_seconds: int = 60, on_launch=None) -> dict:
    _validate(mode,inspect_class,stream_type,peer_nonce,peer_seconds,on_launch)
    settings = lab.profile()
    machine = lab.require_machine(settings, "running")
    udid = machine.get("udid", "")
    if not re.fullmatch(r"0000FE01-[0-9A-F]{16}", udid):
        raise ValueError("expected the owned research VM identity and an explicit USB action")
    info = lab.rpc(settings, "device.info").get("result", {})
    if info.get("model") != "iPhone99,11" or info.get("ios_version") != "26.6.2":
        raise RuntimeError("guest model or userland no longer matches the reviewed profile")
    # /var/tmp can be cleared at a guest restart, even when the host backup exists.
    lab.rpc(settings, "files.mkdir", {"path": GUEST_DIR})
    if mode in ("apply", "apply-destroy", "ncm-direct", "restore"):
        backup = lab.OUT / "usb-original.json"
        if mode != "restore" and not backup.exists():
            probe = _run("probe")["log"]["result"]
            if probe.get("truncated"):
                raise RuntimeError("original USB description was truncated")
            content = probe["content"]
            description = plistlib.loads(content[content.index("<?xml"):].encode())
            lab.save("usb-original.json", {"udid": udid, "description": description})
        saved = json.loads(backup.read_text())
        if saved.get("udid") != udid or saved["description"].get("uniqueDeviceID") != udid:
            raise RuntimeError("USB backup belongs to another guest")
        lab.rpc(settings, "files.write", {"path": GUEST_DIR + "/usb-original.plist",
                                         "content": plistlib.dumps(saved["description"]).decode()})
        lab.rpc(settings, "files.chmod", {"path": GUEST_DIR + "/usb-original.plist", "mode": "600"})
    binary = build(catalogue=mode in ("manager-apply", "manager-remove", "port-prepare"))
    remote = GUEST_DIR + "/usb-description"
    lab.rpc(settings, "files.write", {"path": remote, "content": base64.b64encode(binary.read_bytes()).decode(), "encoding": "base64"})
    lab.rpc(settings, "files.chmod", {"path": remote, "mode": "700"})
    # Separate one-shot labels allow inspection after configuration without a
    # boot-time job or changing any Apple daemon. No shell is installed.
    name = "usb-" + mode + "-" + str(time.monotonic_ns())
    label = "com.mhi2.vphone." + name
    log = GUEST_DIR + "/" + name + ".log"
    path = GUEST_DIR + "/" + name + ".plist"
    arguments = [remote, mode, udid] + ([inspect_class] if inspect_class else []) + ([str(stream_type)] if stream_type is not None else []) + ([peer_nonce] if peer_nonce else [])
    if mode in PEER_ACTIONS and peer_seconds != 60:
        arguments.append(str(peer_seconds))
    job = {"Label": label, "ProgramArguments": arguments, "RunAtLoad": True,
           "StandardOutPath": log, "StandardErrorPath": log}
    lab.rpc(settings, "files.write", {"path": path, "content": plistlib.dumps(job).decode()})
    loaded = lab.rpc(settings, "services.load", {"paths": [path]})
    deadline = time.monotonic() + (peer_seconds + 15 if mode in PEER_ACTIONS else 30)
    try:
        if on_launch:
            on_launch(log)
        while True:
            status = lab.rpc(settings, "services.status", {"label": label}).get("result", {})
            if not status.get("running"):
                break
            if time.monotonic() >= deadline:
                raise RuntimeError("bounded USB helper has not exited")
            time.sleep(0.5)
    except RuntimeError as error:
        lab.save("usb-" + mode + "-failure.json", {"mode": mode, "udid": udid, "error": str(error),
                 "loaded": loaded, "console": lab.client("vm", "log", *lab.machine_arguments(settings), "--lines", "100")})
        raise
    result = {"mode": mode, "inspect_class": inspect_class, "stream_type": stream_type, "udid": udid, "loaded": loaded, "status": status,
              "log": lab.rpc(settings, "files.read", {"path": log, "limit": 64000 if mode == "iap-stream-bluetooth" else 6000})}
    lab.save("usb-" + mode + ".json", result)
    # Remove our completed one-shot job only, never an Apple service.
    lab.rpc(settings, "services.unload", {"paths": [path], "force": True})
    lab.rpc(settings, "files.remove", {"path": path})
    lab.rpc(settings, "files.remove", {"path": log})
    if status.get("last_exit_status") != 0:
        raise RuntimeError("native USB helper failed; inspect build/vphone-lab/usb-" + mode + ".json")
    if result["log"]["result"].get("truncated"):
        raise RuntimeError("native USB diagnostic was truncated; inspect the saved report")
    return result


def run(mode: str, inspect_class: str | None = None, stream_type: int | None = None, *, peer_nonce: str | None = None, peer_seconds: int = 60, on_launch=None) -> dict:
    _validate(mode,inspect_class,stream_type,peer_nonce,peer_seconds,on_launch)
    lab.OUT.mkdir(parents=True, exist_ok=True)
    with open(os.open(lab.OUT / ".usb.lock", os.O_RDWR | os.O_CREAT, 0o600), "r+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with lab.rpc_batch(lab.profile()):
            return _run(mode, inspect_class, stream_type, peer_nonce=peer_nonce, peer_seconds=peer_seconds, on_launch=on_launch)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=[mode for mode in ACTIONS if mode not in PEER_ACTIONS])
    parser.add_argument("--experimental", action="store_true",
                        help="run a live USB or accessory-manager experiment; may stop this VM")
    parser.add_argument("--inspect-class", help="inspect metadata of a class from the native iAP transport image")
    parser.add_argument("--stream-type", type=int, choices=(1, 4, 5), help="explicit protocol metadata experiment; not evidence of iAP2 or MFi acceptance")
    args = parser.parse_args()
    if args.mode not in READ_ONLY_ACTIONS and not args.experimental:
        parser.error("live USB/accessory configuration requires --experimental")
    if args.mode == "port-prepare" and args.stream_type is None:
        parser.error("port-prepare requires an explicit --stream-type")
    if args.mode != "port-prepare" and args.stream_type is not None:
        parser.error("--stream-type applies only to port-prepare")
    print(json.dumps(run(args.mode, args.inspect_class, args.stream_type), indent=2))
