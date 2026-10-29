#!/usr/bin/env python3
"""Control the dedicated vphone guest through its official Launchpad client."""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "build/vphone-lab"
CLI = Path("/Applications/vphone-launchpad.app/Contents/MacOS/vphone-launchpad-cli")
PROFILE_PATH = Path(__file__).with_name("profile.json")
LIBRARY = Path.home() / ".vphone/machines"
_rpc_batch = threading.local()


def profile() -> dict:
    value = json.loads(PROFILE_PATH.read_text())
    if value.get("schema") != 1 or value.get("backend") != "vphone":
        raise ValueError("unsupported guest profile")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,31}", value["machine"]):
        raise ValueError("invalid machine name")
    if not re.fullmatch(r"\d+\.\d+\.\d+", value["bundle"]):
        raise ValueError("an explicit release version is required")
    for name in ("iphone_source", "cloudos_source"):
        url = urlparse(value[name])
        if url.scheme != "https" or url.netloc != "updates.cdn-apple.com" or url.query or url.fragment:
            raise ValueError("firmware must come from the pinned Apple CDN sources")
    for name in ("cpu", "memory_mb", "disk_gb", "minimum_free_gb"):
        if type(value[name]) is not int or value[name] <= 0:
            raise ValueError(f"invalid {name}")
    if value["network"] != "nat" or value["slim"] != "off":
        raise ValueError("bring-up preserves native services on the NAT profile")
    return value


def client(*arguments: str, timeout: int | None = 30):
    if not CLI.is_file():
        raise RuntimeError("Launchpad CLI missing; run tests/carplay_lab/vphone/bootstrap.py")
    # The app streams progress and owns long creation operations. Never kill a
    # creation because a client-side deadline or cancelled waiter expired.
    # Inventory scans can exceed 30 s after a long-lived VM has accumulated
    # diagnostics. This read-only call gets a larger, still bounded deadline;
    # mutation timeouts are unchanged and are never retried automatically.
    if arguments == ('vm','list') and timeout == 30:timeout=90
    result = subprocess.run([str(CLI), *arguments], stdout=subprocess.PIPE,
                            text=True, timeout=timeout, check=False)
    if result.returncode:
        raise RuntimeError(f"Launchpad command failed: {' '.join(arguments[:2])}")
    try:
        value = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise ValueError("Launchpad returned invalid JSON") from error
    if isinstance(value, dict) and value.get("ok") is False:
        raise RuntimeError(str(value.get("error", "guest refused the request")))
    return value


def save(name: str, value) -> Path:
    OUT.mkdir(parents=True, exist_ok=True)
    destination = OUT / name
    with tempfile.NamedTemporaryFile(mode="w", dir=OUT, prefix=f".{name}-", delete=False) as file:
        temporary = Path(file.name)
        try:
            json.dump(value, file, indent=2)
            file.write("\n")
        except BaseException:
            temporary.unlink()
            raise
    temporary.replace(destination)
    return destination


def owned_machine(settings: dict, machines: list) -> dict | None:
    if not isinstance(machines, list) or any(not isinstance(m, dict) for m in machines):
        raise ValueError("expected Launchpad's machine list")
    matches = [m for m in machines if m.get("name") == settings["machine"] and
               isinstance(m.get("libraryRoot"), str) and
               Path(m["libraryRoot"]).expanduser().resolve() == LIBRARY.resolve()]
    if len(matches) > 1:
        raise ValueError("ambiguous owned machine")
    return matches[0] if matches else None


def machine_arguments(settings: dict) -> list[str]:
    return [settings["machine"], "--root", str(LIBRARY)]


def creation_arguments(settings: dict) -> list[str]:
    return ["vm", "create", *machine_arguments(settings), "--bundle", settings["bundle"],
            "--device", settings["device"], "--iphone-source", settings["iphone_source"],
            "--cloudos-source", settings["cloudos_source"], "--cpu", str(settings["cpu"]),
            "--memory", str(settings["memory_mb"]), "--disk-size", str(settings["disk_gb"]),
            "--network", settings["network"], "--slim", settings["slim"]]


def doctor(settings: dict) -> dict:
    status = client("status")
    # A fresh installation has no Core Bundle with which to inspect guests.
    # Its status already reports zero machines; listing them needlessly runs
    # bundle-dependent discovery before Host Setup is complete.
    machines = [] if status.get("machines") == 0 else client("vm", "list")
    research = subprocess.run(["csrutil", "allow-research-guests", "status"],
                              capture_output=True, text=True, check=False)
    research_enabled = research.returncode == 0 and research.stdout.strip().endswith(": enabled")
    free = shutil.disk_usage(Path.home()).free
    info = CLI.parent.parent / "Info.plist"
    version = plistlib.loads(info.read_bytes())["CFBundleShortVersionString"]
    blockers = []
    if not research_enabled:
        blockers.append("Research Guests: run csrutil allow-research-guests enable in macOS Recovery")
    if not status.get("developerTools"):
        blockers.append("Grant Developer Tools access in Launchpad Host Setup")
    if not status.get("canInstallBundles"):
        blockers.append("Complete Launchpad Host Setup and install its helper")
    if version.split(".")[:2] != settings["bundle"].split(".")[:2]:
        blockers.append("Launchpad and the pinned Core Bundle must share a release series")
    if not status.get("bundleReady") or status.get("defaultBundle") != settings["bundle"]:
        blockers.append(f"Install and verify Core Bundle {settings['bundle']}")
    if not status.get("hostReady"):
        blockers.append("Launchpad host preflight has not passed")
    report = {"backend": "vphone", "profile": settings, "launchpad_version": version,
              "status": status, "research_guests_enabled": research_enabled,
              "free_gb": round(free / 1024**3, 2), "blockers": blockers,
              "machine": owned_machine(settings, machines), "ready": not blockers}
    save("doctor.json", report)
    return report


def create(settings: dict):
    report = doctor(settings)
    if report["machine"] is not None:
        raise RuntimeError("dedicated machine already exists; inspect it instead of recreating")
    if not report["ready"]:
        raise RuntimeError("; ".join(report["blockers"]))
    if report["free_gb"] < settings["minimum_free_gb"]:
        raise RuntimeError("insufficient free space for the firmware restore pipeline")
    machines = client("vm", "list")
    if any(str(m.get("state", "")).startswith("busy:") for m in machines):
        raise RuntimeError("another machine operation is active; creation must be serial")
    result = client(*creation_arguments(settings), timeout=None)
    save("creation.json", result)
    return finish_creation(settings, result)


def finish_creation(settings: dict, result: dict):
    if (result.get("name") != settings["machine"] or result.get("bundle") != settings["bundle"] or
            result.get("libraryRoot") != str(LIBRARY)):
        raise RuntimeError("creation report does not match the dedicated guest profile")
    steps = result.get("steps")
    if not isinstance(steps, list) or not steps or any(
            not isinstance(step, dict) or step.get("status") != "passed" for step in steps) or not any(
            step.get("step") == "firstBoot" for step in steps):
        raise RuntimeError("creation report has no successful first boot")
    machine = owned_machine(settings, client("vm", "list"))
    # Launchpad 2.9.2 keeps its verified first boot running. The standalone
    # CLI's stopped-at-return convention is not the app's lifecycle contract.
    if machine is None or machine.get("state") not in ("stopped", "running") or \
            machine.get("bundle") != settings["bundle"]:
        raise RuntimeError("creation did not leave the expected available guest")
    ledger = OUT / "machine.json"
    if ledger.exists():
        recorded = json.loads(ledger.read_text())
        if any(recorded.get(field) != machine.get(field) for field in ("name", "libraryRoot", "bundle", "udid")):
            raise RuntimeError("creation recovery cannot replace a different guest ownership record")
    save("machine.json", machine)
    return machine


def require_machine(settings: dict, state: str | None = None) -> dict:
    batch=getattr(_rpc_batch,'value',None)
    if batch is not None:
        if settings!=batch['settings']:raise RuntimeError('guest RPC batch belongs to a different profile')
        if time.monotonic()>batch['deadline']:raise TimeoutError('bounded guest RPC batch expired')
        if state and state!=batch['machine']['state']:raise RuntimeError('guest RPC batch state mismatch')
        return dict(batch['machine'])
    machine = owned_machine(settings, client("vm", "list"))
    if machine is None:
        raise RuntimeError("dedicated vphone guest does not exist")
    if machine.get("bundle") != settings["bundle"]:
        raise RuntimeError("the guest is bound to a different Core Bundle")
    if state and machine.get("state") != state:
        raise RuntimeError(f"expected {state} guest, got {machine.get('state')}")
    ledger = OUT / "machine.json"
    if not ledger.is_file():
        raise RuntimeError("the matching guest was not created by this laboratory profile")
    recorded = json.loads(ledger.read_text())
    for field in ("name", "libraryRoot", "bundle"):
        if recorded.get(field) != machine.get(field):
            raise RuntimeError("guest ownership record no longer matches Launchpad")
    if recorded.get("udid") and recorded["udid"] != machine.get("udid"):
        raise RuntimeError("guest identity changed; inspect before operating it")
    return machine


@contextmanager
def rpc_batch(settings: dict, seconds: int = 600):
    """Pin one verified running guest for a bounded, single-thread RPC job.

    Each RPC still targets the exact machine/library through Launchpad. Avoid
    repeated UI vm-list requests during media: a slow inventory request must
    not interrupt an otherwise healthy native stream. No lifecycle operations
    belong inside this scope; unrelated threads do their own verification.
    """
    if type(seconds) is not int or not 1<=seconds<=600:raise ValueError('RPC batch must be bounded to 1..600 seconds')
    if getattr(_rpc_batch,'value',None) is not None:raise RuntimeError('nested guest RPC batches are unsupported')
    machine=require_machine(settings,'running')
    _rpc_batch.value={'settings':dict(settings),'machine':machine,'deadline':time.monotonic()+seconds}
    try:yield dict(machine)
    finally:del _rpc_batch.value


def rpc(settings: dict, method: str, parameters: dict | None = None):
    require_machine(settings, "running")
    return client("guest", "rpc", settings["machine"], method,
                  json.dumps(parameters or {}), "--root", str(LIBRARY), timeout=60)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("doctor", "plan", "create", "recover-create", "start", "debug-start", "stop",
                                          "status", "snapshot", "diagnose", "unlock", "screenshot"))
    parser.add_argument("--headless", action="store_true")
    args = parser.parse_args()
    settings = profile()
    if args.action == "plan":
        result = {"profile": settings, "create_command": [str(CLI), *creation_arguments(settings)]}
    elif args.action == "doctor":
        result = doctor(settings)
    elif args.action == "create":
        result = create(settings)
    elif args.action == "recover-create":
        result = finish_creation(settings, json.loads((OUT / "creation.json").read_text()))
    elif args.action == "status":
        status = client("status")
        machines = [] if status.get("machines") == 0 else client("vm", "list")
        result = {"backend": "vphone", "host": status, "machine": owned_machine(settings, machines)}
    elif args.action == "debug-start":
        require_machine(settings, "stopped")
        result = client("exec", "--bundle", settings['bundle'], "vm", "launch",
                        "--library-root", str(LIBRARY), settings['machine'],
                        "--kernel-debug-port", "60321", "--headless", timeout=None)
    elif args.action == "start":
        require_machine(settings, "stopped")
        arguments = ["vm", "start", *machine_arguments(settings), "--wait", "--timeout", "300"]
        if args.headless:
            arguments.append("--headless")
        result = client(*arguments, timeout=None)
        save("guest-info.json", rpc(settings, "device.info"))
    elif args.action == "stop":
        require_machine(settings, "running")
        result = client("vm", "stop", *machine_arguments(settings), timeout=60)
    elif args.action == "snapshot":
        result = rpc(settings, "device.info")
        save("guest-info.json", result)
        save("guest-network.json", rpc(settings, "device.network"))
    elif args.action == "diagnose":
        result = {"device": rpc(settings, "device.info"),
                  "network": rpc(settings, "device.network"), "processes": {}}
        for process in ("SpringBoard", "backboardd", "mediaserverd", "carkitd", "iap2d"):
            result["processes"][process] = rpc(settings, "processes.list", {"filter": process})
        result["media_log"] = rpc(settings, "logs.syslog", {
            "seconds": 2, "process": "mediaserverd", "max_lines": 100})
        save("guest-diagnostics.json", result)
    elif args.action == "unlock":
        require_machine(settings, "running")
        result = client("guest", "unlock", *machine_arguments(settings), timeout=60)
    else:
        require_machine(settings, "running")
        OUT.mkdir(parents=True, exist_ok=True)
        shot = OUT / f"phone-{time.time_ns()}.png"
        result = client("guest", "send", settings["machine"],
                        json.dumps({"t": "screenshot", "path": str(shot)}),
                        "--root", str(LIBRARY), timeout=60)
        if not shot.is_file():
            raise RuntimeError("guest screenshot did not produce a file")
    print(json.dumps(result, indent=2))
    return int(args.action == "doctor" and not result["ready"])


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, RuntimeError, OSError, subprocess.TimeoutExpired) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
