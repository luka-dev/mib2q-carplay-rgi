#!/usr/bin/env python3
"""Control only the local, disposable iOS recovery guest; no physical devices."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_LAB = ROOT / "build/ios-vm-lab"


def running_pid(lab: Path) -> int | None:
    path = lab / "ios.pid"
    if not path.exists():
        return None
    pid = int(path.read_text().strip())
    check = subprocess.run(["ps", "-p", str(pid), "-o", "command="],
                           capture_output=True, text=True, check=False)
    expected = str(lab / "inferno/build/qemu-system-aarch64")
    return pid if check.returncode == 0 and check.stdout.strip().startswith(expected + " ") else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["status", "start", "stop", "query", "restore-preflight"])
    parser.add_argument("--lab", type=Path, default=DEFAULT_LAB)
    args = parser.parse_args()
    lab = args.lab.resolve()
    pid = running_pid(lab)
    if args.action == "status":
        print(json.dumps({"ios_pid": pid, "mode": "recovery", "lab": str(lab)}))
    elif args.action == "start":
        if pid:
            print(f"Recovery guest already running: {pid}")
            return 0
        command = json.loads((lab / "ios-command.json").read_text())
        if command[0] != str(lab / "inferno/build/qemu-system-aarch64"):
            raise RuntimeError("unexpected emulator path in local launch record")
        with (lab / "ios-host.log").open("w") as log:
            child = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=log,
                                     stderr=subprocess.STDOUT, start_new_session=True)
        (lab / "ios.pid").write_text(f"{child.pid}\n")
        print(f"Started recovery guest: {child.pid}")
    elif args.action == "stop":
        if pid:
            os.kill(pid, signal.SIGTERM)
            print(f"Stopped recovery guest: {pid}")
        else:
            print("Recovery guest is stopped")
    elif args.action == "query":
        return subprocess.run([str(lab / "query-restore")],
            env=dict(os.environ, USBMUXD_SOCKET_ADDRESS="127.0.0.1:32226"),
            timeout=20, check=False).returncode
    else:
        # idevicerestore extracts a 5.15GB OS image and then writes the guest's
        # >=7.34GB system partition. Keep a reserve for the host and guest data.
        import shutil
        available = shutil.disk_usage(lab).free
        needed = 15 * 1024**3
        print(json.dumps({"free_bytes": available, "required_bytes": needed,
                          "restore_allowed": available >= needed}))
        return 0 if available >= needed else 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
