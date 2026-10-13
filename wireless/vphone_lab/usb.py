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
ACTIONS = ("probe", "apply", "apply-destroy", "restore", "reconnect", "network",
           "iap-list", "iap-wake", "iap-inspect", "iap-registry", "manager-apply", "manager-remove")
READ_ONLY_ACTIONS = ("probe", "network", "iap-list", "iap-wake", "iap-inspect", "iap-registry")


def build(*, catalogue: bool = False) -> Path:
    out = lab.OUT / "usb"
    out.mkdir(parents=True, exist_ok=True)
    libraries = {
        "libSystem": ("/usr/lib/libSystem.B.dylib", ["putchar", "strstr", "free", "printf", "puts", "strcmp", "getuid", "sleep", "alarm", "fflush", "dlopen", "dlsym", "dlerror", "fopen", "fread", "fclose", "socket", "connect", "close", "send", "recv", "setsockopt", "inet_pton", "if_nametoindex", "htons", "memcmp", "xpc_connection_create_mach_service", "xpc_connection_set_event_handler", "xpc_connection_activate", "xpc_dictionary_create", "xpc_connection_send_message", "xpc_connection_cancel", "xpc_release", "_NSConcreteGlobalBlock", "__chkstk_darwin", "__stack_chk_guard", "__stack_chk_fail"]),
        "CoreFoundation": ("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", [
            "kCFTypeDictionaryKeyCallBacks", "kCFTypeDictionaryValueCallBacks", "kCFTypeArrayCallBacks", "kCFBooleanTrue",
            "CFStringCreateWithCString", "CFDictionaryCreateMutable", "CFDictionaryGetValue", "CFDictionarySetValue",
            "CFArrayCreate", "CFRelease", "CFStringGetCString", "CFPropertyListCreateData", "CFDataGetBytePtr", "CFDataGetLength",
            "CFDataCreate", "CFPropertyListCreateWithData", "CFGetTypeID", "CFDictionaryGetTypeID", "CFDataGetTypeID", "CFStringGetTypeID", "CFNumberGetTypeID", "CFNumberGetValue"]),
        "IOKit": ("/System/Library/Frameworks/IOKit.framework/IOKit", ["IOServiceGetMatchingService", "IOServiceMatching",
            "IORegistryEntryCreateCFProperty", "IORegistryEntryFromPath", "IORegistryEntrySetCFProperties", "IOObjectRelease", "IOObjectIsEqualTo", "IOObjectConformsTo", "IORegistryEntryGetParentEntry", "IORegistryEntryGetChildIterator", "IOIteratorNext", "IORegistryEntryInPlane"]),
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
                    "-O2", "-Wall", "-Wextra", "-Werror", "-fblocks", "-c", str(Path(__file__).with_name("usb_description.c")),
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


def _run(mode: str, inspect_class: str | None = None) -> dict:
    if mode not in ACTIONS:
        raise ValueError("unknown native USB action")
    if inspect_class is not None and (mode != "iap-inspect" or not re.fullmatch(r"ACCTransport[A-Za-z]{1,80}", inspect_class)):
        raise ValueError("inspection requires a native transport class")
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
    if mode in ("apply", "apply-destroy", "restore"):
        backup = lab.OUT / "usb-original.json"
        if mode.startswith("apply") and not backup.exists():
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
    binary = build(catalogue=mode in ("manager-apply", "manager-remove"))
    remote = GUEST_DIR + "/usb-description"
    lab.rpc(settings, "files.write", {"path": remote, "content": base64.b64encode(binary.read_bytes()).decode(), "encoding": "base64"})
    lab.rpc(settings, "files.chmod", {"path": remote, "mode": "700"})
    # Separate one-shot labels allow inspection after configuration without a
    # boot-time job or changing any Apple daemon. No shell is installed.
    name = "usb-" + mode + "-" + str(time.monotonic_ns())
    label = "com.mhi2.vphone." + name
    log = GUEST_DIR + "/" + name + ".log"
    path = GUEST_DIR + "/" + name + ".plist"
    arguments = [remote, mode, udid] + ([inspect_class] if inspect_class else [])
    job = {"Label": label, "ProgramArguments": arguments, "RunAtLoad": True,
           "StandardOutPath": log, "StandardErrorPath": log}
    lab.rpc(settings, "files.write", {"path": path, "content": plistlib.dumps(job).decode()})
    loaded = lab.rpc(settings, "services.load", {"paths": [path]})
    deadline = time.monotonic() + 30
    try:
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
    result = {"mode": mode, "inspect_class": inspect_class, "udid": udid, "loaded": loaded, "status": status,
              "log": lab.rpc(settings, "files.read", {"path": log, "limit": 6000})}
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


def run(mode: str, inspect_class: str | None = None) -> dict:
    lab.OUT.mkdir(parents=True, exist_ok=True)
    with open(os.open(lab.OUT / ".usb.lock", os.O_RDWR | os.O_CREAT, 0o600), "r+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return _run(mode, inspect_class)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=ACTIONS)
    parser.add_argument("--experimental", action="store_true",
                        help="run a live USB or accessory-manager experiment; may stop this VM")
    parser.add_argument("--inspect-class", help="inspect metadata of a class from the native iAP transport image")
    args = parser.parse_args()
    if args.mode not in READ_ONLY_ACTIONS and not args.experimental:
        parser.error("live USB/accessory configuration requires --experimental")
    print(json.dumps(run(args.mode, args.inspect_class), indent=2))
