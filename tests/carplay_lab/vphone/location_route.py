"""Stage and run a bounded native locationd trajectory through Launchpad."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import plistlib
import subprocess
import time

from . import wired_native as native, lab, usb


def build():
    out = native.OUT / "location-route"
    out.mkdir(exist_ok=True)
    for name in ("libSystem", "objc"):
        source = (native.OUT / "usb" / (name + ".tbd")).read_text()
        if name == "libSystem":
            source = source.replace("symbols: [ ", "symbols: [ _clock_gettime, ", 1)
        (out / (name + ".tbd")).write_text(source)
    environment = {k:v for k,v in os.environ.items() if k != "SDKROOT"}
    subprocess.run(["clang", "-target", "arm64-apple-ios18.0", "-isysroot", str(native.OUT / "usb/empty-sysroot"),
        "-O2", "-Wall", "-Wextra", "-Werror", "-c", str(Path(__file__).with_suffix(".c")),
        "-o", str(out / "route.o")], check=True, env=environment)
    binary = out / "location-route"
    subprocess.run(["ld", "-ld_classic", "-arch", "arm64", "-platform_version", "ios", "18.0", "18.0",
        "-o", str(binary), str(out / "route.o"), str(out / "libSystem.tbd"), str(out / "objc.tbd")], check=True)
    entitlement = out / "entitlements.plist"
    entitlement.write_bytes(plistlib.dumps({"com.apple.locationd.simulation": True}))
    subprocess.run(["codesign", "-f", "-s", "-", "--entitlements", str(entitlement), str(binary)], check=True)
    return binary


def run():
    settings = lab.profile()
    machine = lab.require_machine(settings, "running")
    if machine.get("syncsHostLocation") is not False:
        raise RuntimeError("host location synchronization must remain disabled")
    binary = build()
    remote = usb.GUEST_DIR + "/location-route"
    label = "com.mhi2.vphone.location-route-" + str(time.monotonic_ns())
    job = usb.GUEST_DIR + "/" + label + ".plist"
    log = usb.GUEST_DIR + "/" + label + ".log"
    lab.rpc(settings, "files.write", {"path": remote, "content": base64.b64encode(binary.read_bytes()).decode(), "encoding": "base64"})
    lab.rpc(settings, "files.chmod", {"path": remote, "mode": "700"})
    specification = {"Label": label, "ProgramArguments": [remote], "RunAtLoad": True,
                     "StandardOutPath": log, "StandardErrorPath": log}
    lab.rpc(settings, "files.write", {"path": job, "content": plistlib.dumps(specification).decode()})
    result = {"label": label, "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
              "host_start_ns": time.time_ns(), "simulation": "one locationd trajectory; no per-point RPC restart"}
    lab.rpc(settings, "services.load", {"paths": [job]})
    print(json.dumps({"route_launched": True, "host_start_ns": result["host_start_ns"]}), flush=True)
    deadline = time.monotonic()+60
    # Observe actual locationd delivery without changing its simulation.
    time.sleep(2)
    result["observations"] = []
    while time.monotonic() < deadline:
        status = lab.rpc(settings, "services.status", {"label": label})["result"]
        if not status.get("running"):
            result["status"] = status
            break
        stamp = time.time_ns()
        try:
            location = lab.rpc(settings, "location.current", {"timeout": 1})
            result["observations"].append({"host_ns": stamp, "reply": location})
            print(json.dumps(result["observations"][-1]), flush=True)
        except RuntimeError as error:
            result["observations"].append({"host_ns": stamp, "error": str(error)})
        time.sleep(3)
    result["host_end_ns"] = time.time_ns()
    result["log"] = lab.rpc(settings, "files.read", {"path": log, "limit": 6000})
    lab.save("native-moving-route.json", result)
    if "status" not in result:
        raise RuntimeError("bounded native route did not finish")
    lab.rpc(settings, "services.unload", {"paths": [job], "force": True})
    for path in (job, log):
        lab.rpc(settings, "files.remove", {"path": path})
    if result["status"].get("last_exit_status") != 0:
        raise RuntimeError("native location helper failed")
    return result


if __name__ == "__main__":
    argparse.ArgumentParser(description=__doc__).parse_args()
    native.configure()
    print(json.dumps(run(), indent=2))
