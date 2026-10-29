"""Passively inspect camera-rate fields in the owned VM's existing Maps process."""
import base64
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import subprocess
import time
import uuid

from . import lab, usb

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "build/wired-altscreen-30fps-re/vphone-source-policy"


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    symbols = {
        "System": ("/usr/lib/libSystem.B.dylib", "printf snprintf freopen __stdoutp putchar puts fflush strcmp strstr getpagesize atoi alarm sleep getuid getpid dlopen dlsym calloc free mach_task_self_ task_for_pid mach_port_deallocate mach_vm_region mach_vm_page_query mach_vm_read_overwrite __stack_chk_guard __stack_chk_fail __chkstk_darwin".split()),
        "objc": ("/usr/lib/libobjc.A.dylib", "objc_getClass class_createInstance class_getSuperclass class_getName objc_copyClassList class_getInstanceVariable ivar_getOffset ivar_getTypeEncoding class_getImageName class_getInstanceSize class_getInstanceMethod sel_registerName method_getImplementation".split()),
    }
    for name, (install, exports) in symbols.items():
        (OUT / (name + ".tbd")).write_text("--- !tapi-tbd-v3\narchs: [ arm64 ]\nplatform: ios\ninstall-name: " + install + "\nexports:\n  - archs: [ arm64 ]\n    symbols: [ " + ", ".join("_" + s for s in exports) + " ]\n...\n")
    empty = OUT / "empty-sysroot"
    empty.mkdir(exist_ok=True)
    environment = {k: v for k, v in os.environ.items() if k != "SDKROOT"}
    source = Path(__file__).with_suffix(".c")
    binary = OUT / "maps-rate-probe"
    subprocess.run(["clang", "-target", "arm64-apple-ios18.0", "-isysroot", str(empty),
                    "-O2", "-Wall", "-Wextra", "-Werror", "-c", str(source),
                    "-o", str(OUT / "probe.o")], check=True, env=environment)
    subprocess.run(["ld", "-ld_classic", "-arch", "arm64", "-platform_version", "ios", "18.0", "18.0",
                    "-o", str(binary), str(OUT / "probe.o"), str(OUT / "System.tbd"), str(OUT / "objc.tbd")], check=True)
    entitlement = OUT / "entitlements.plist"
    entitlement.write_bytes(plistlib.dumps({"com.apple.system-task-ports": True, "task_for_pid-allow": True}))
    subprocess.run(["codesign", "-f", "-s", "-", "--entitlements", str(entitlement), str(binary)], check=True)
    subprocess.run(["codesign", "--verify", "--strict", str(binary)], check=True)
    return binary


def save(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n")


def run(self_test=False):
    binary = build()
    settings = lab.profile()
    machine = lab.require_machine(settings, "running")
    rows = lab.rpc(settings, "processes.list", {"filter": "Maps"})["result"]["processes"]
    maps = [p for p in rows if p.get("bundle_id") == "com.apple.Maps" and p.get("name") == "Maps"]
    if not self_test and len(maps) != 1:
        raise RuntimeError("exactly one existing native Maps process is required; no app is launched by this probe")
    target = {"pid": "self", "start_time": None} if self_test else maps[0]
    nonce = uuid.uuid4().hex[:12]
    directory = usb.GUEST_DIR + "/maps-rate-" + nonce
    remote, job, log = [directory + suffix for suffix in ("/probe", "/job.plist", "/output.log")]
    label = "com.mhi2.vphone.maps-rate-" + nonce
    result = {"machine_udid": machine["udid"], "target_before": target,
              "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
              "source_sha256": hashlib.sha256(Path(__file__).with_suffix(".c").read_bytes()).hexdigest(),
              "label": label, "guest_directory": directory, "host_start_ns": time.time_ns(),
              "scope": "passive reads only; no target pause, remote execution, injection, preference, display or CarPlay-session changes",
              "passed": False}
    save("probe-result.json", result)
    loaded = False
    created = False
    try:
        lab.rpc(settings, "files.mkdir", {"path": directory})
        created = True
        lab.rpc(settings, "files.write", {"path": remote, "encoding": "base64", "content": base64.b64encode(binary.read_bytes()).decode()})
        lab.rpc(settings, "files.chmod", {"path": remote, "mode": "700"})
        spec = {"Label": label, "ProgramArguments": [remote, str(target["pid"]), directory], "RunAtLoad": True,
                "StandardOutPath": log, "StandardErrorPath": log}
        lab.rpc(settings, "files.write", {"path": job, "content": plistlib.dumps(spec).decode()})
        lab.rpc(settings, "services.load", {"paths": [job]})
        loaded = True
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            status = lab.rpc(settings, "services.status", {"label": label})["result"]
            if not status.get("running") and status.get("last_exit_status") is not None:
                result["status"] = status
                break
            time.sleep(.2)
        result["logs"] = {"layout": lab.rpc(settings, "files.read", {"path": log, "limit": 14000})}
        if result.get("status", {}).get("last_exit_status") == 0:
            for sample in (0, 1):
                result["logs"]["sample" + str(sample)] = lab.rpc(settings, "files.read", {
                    "path": directory + "/sample" + str(sample) + ".log", "limit": 14000})
        result["log"] = {"ok": True, "result": {
            "content": "".join(r["result"].get("content", "") for r in result["logs"].values()),
            "truncated": any(r["result"].get("truncated") for r in result["logs"].values())}}
        save("probe-log.json", result["log"])
        log_data = result["log"]["result"]
        text = log_data.get("content", "")
        result["completed"] = (result.get("status", {}).get("last_exit_status") == 0
                               and not log_data.get("truncated") and "MAPS_RATE_READ_ONLY_DONE" in text)
        result["passed"] = result["completed"]
        after = lab.rpc(settings, "processes.list", {"filter": "Maps"})
        save("maps-process-after.json", after)
        result["target_unchanged"] = self_test or any(p.get("pid") == target["pid"] and p.get("start_time") == target["start_time"]
                                                      for p in after["result"]["processes"])
        result["passed"] = result["passed"] and result["target_unchanged"]
        if self_test:
            result["passed"] = result["passed"] and "MAPS_RATE_SELF_TEST_PASS" in text
    finally:
        result["host_end_ns"] = time.time_ns()
        result["cleanup"] = []
        cleanup = []
        if loaded:
            cleanup.append(("services.unload", {"paths": [job], "force": True}))
        if created:
            cleanup.append(("files.remove", {"path": directory, "recursive": True}))
        for method, params in cleanup:
            try:
                reply = lab.rpc(settings, method, params)
                result["cleanup"].append({"method": method, "ok": reply.get("ok")})
            except Exception as error:
                result["cleanup"].append({"method": method, "error": str(error)})
        save("probe-result.json", result)
    if not result["passed"]:
        raise RuntimeError("passive native probe did not pass; inspect probe-result.json and probe-log.json")
    print(json.dumps(result, indent=2))
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        OUT = OUT / "self-test"
    run(args.self_test)
