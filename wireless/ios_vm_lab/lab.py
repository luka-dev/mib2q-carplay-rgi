#!/usr/bin/env python3
"""Control only the local, disposable iOS guest; no physical devices."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import plistlib
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_LAB = ROOT / "build/ios-vm-lab"


def check_guest_disks(lab: Path, command: list[str]) -> None:
    """Reject host devices, external files and images still attached to macOS."""
    if "-blockdev" in command:
        raise RuntimeError("unsupported block device launch record")
    disks = set()
    for i, option in enumerate(command):
        if option != "-drive":
            continue
        try:
            values = dict(item.split("=", 1) for item in command[i + 1].split(","))
            path = Path(values["file"]).resolve()
        except (IndexError, KeyError, ValueError) as exc:
            raise RuntimeError("invalid guest drive launch record") from exc
        if not path.is_relative_to(lab) or not path.is_file():
            raise RuntimeError("guest disk must be a regular file inside the laboratory")
        disks.add(path)
    if not disks:
        raise RuntimeError("launch record has no guest disks")
    attached = plistlib.loads(subprocess.check_output(["hdiutil", "info", "-plist"]))
    for entry in attached.get("images", []):
        if entry.get("image-path") and Path(entry["image-path"]).resolve() in disks:
            raise RuntimeError("guest disk is still attached to macOS; detach it before starting the VM")


def running_pid(lab: Path) -> int | None:
    path = lab / "ios.pid"
    if not path.exists():
        return None
    pid = int(path.read_text().strip())
    check = subprocess.run(["ps", "-p", str(pid), "-o", "command="],
                           capture_output=True, text=True, check=False)
    expected = str(lab / "inferno/build/qemu-system-aarch64")
    return pid if check.returncode == 0 and check.stdout.strip().startswith(expected + " ") else None


def stop_guest(lab: Path, *, force: bool = False, timeout: float = 10) -> int | None:
    pid = running_pid(lab)
    if pid is None:
        return None
    os.kill(pid, signal.SIGTERM)
    deadline = time.monotonic() + timeout
    while running_pid(lab) == pid:
        if time.monotonic() >= deadline:
            if not force:
                raise RuntimeError("guest has not exited; use stop --force for a stalled disposable VM")
            os.kill(pid, signal.SIGKILL)
            force = False
            deadline = time.monotonic() + timeout
        time.sleep(0.1)
    return pid


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["status", "start", "stop", "query", "restore-preflight"])
    parser.add_argument("--lab", type=Path, default=DEFAULT_LAB)
    parser.add_argument("--boot", choices=["recovery", "normal"], default="recovery",
                        help="guest boot command to use with start")
    parser.add_argument("--input-control", action="store_true",
                        help="expose a local Unix QMP socket for emulated screen input")
    parser.add_argument("--diagnostic-keybag", action="store_true",
                        help="opt into the modified emulator's diagnostic keybag bypass")
    parser.add_argument("--virtual-dock", action="store_true",
                        help="enable the synthetic Tristar/SMC cable fixture and native USB test mode")
    parser.add_argument("--combined-usb", action="store_true",
                        help="use the cloned guest image with native USBMUX/HID/NCM configuration helper")
    parser.add_argument("--ncm-visible", action="store_true",
                        help="opt into the native ncm-no-hide boot diagnostic on the combined VM clone")
    parser.add_argument("--force", action="store_true",
                        help="allow SIGKILL if the disposable guest does not stop after SIGTERM")
    parser.add_argument("--usbmux-port", type=int, default=32226,
                        help="loopback virtual-device tunnel port for query")
    args = parser.parse_args()
    if args.force and args.action != "stop":
        parser.error("--force is only available for stop")
    if args.virtual_dock and (args.action != "start" or args.boot != "normal"):
        parser.error("--virtual-dock requires start --boot normal")
    if args.combined_usb and not args.virtual_dock:
        parser.error("--combined-usb requires --virtual-dock")
    if args.ncm_visible and not args.combined_usb:
        parser.error("--ncm-visible requires --combined-usb")
    if not 1 <= args.usbmux_port <= 65535:
        parser.error("--usbmux-port must be between 1 and 65535")
    lab = args.lab.resolve()
    pid = running_pid(lab)
    mode_file = lab / "ios-mode.txt"
    mode = mode_file.read_text().strip() if mode_file.exists() else "recovery"
    diagnostic_file = lab / "ios-diagnostic-keybag.txt"
    diagnostic = diagnostic_file.exists() and diagnostic_file.read_text().strip() == "1"
    dock_file = lab / "ios-virtual-dock.txt"
    virtual_dock = dock_file.exists() and dock_file.read_text().strip() == "1"
    combined_file = lab / "ios-combined-usb.txt"
    combined = combined_file.exists() and combined_file.read_text().strip() == "1"
    input_file = lab / "ios-input-control.txt"
    input_control = input_file.exists() and input_file.read_text().strip() == "1"
    visible_file = lab / "ios-ncm-visible.txt"
    ncm_visible = visible_file.exists() and visible_file.read_text().strip() == "1"
    if args.action == "status":
        print(json.dumps({"ios_pid": pid, "mode": mode, "diagnostic_keybag": diagnostic,
                          "virtual_dock": virtual_dock,
                          "combined_usb": combined,
                          "input_control": input_control,
                          "ncm_visible": ncm_visible,
                          "lab": str(lab)}))
    elif args.action == "start":
        if pid:
            if args.boot != mode:
                raise RuntimeError(f"guest is already running in {mode} mode; stop it before changing boot mode")
            if args.diagnostic_keybag != diagnostic:
                raise RuntimeError("guest is already running; stop it before changing diagnostic keybag mode")
            if args.virtual_dock != virtual_dock:
                raise RuntimeError("guest is already running; stop it before changing virtual dock mode")
            if args.combined_usb != combined:
                raise RuntimeError("guest is already running; stop it before changing USB profile")
            if args.input_control != input_control:
                raise RuntimeError("guest is already running; stop it before changing input control")
            if args.ncm_visible != ncm_visible:
                raise RuntimeError("guest is already running; stop it before changing NCM visibility")
            print(f"{mode.capitalize()} guest already running: {pid}")
            return 0
        command_file = "ios-command.json" if args.boot == "recovery" else "ios-normal-command.json"
        if args.combined_usb:
            command_file = "ios-normal-combined-usb-command.json"
        command = json.loads((lab / command_file).read_text())
        if command[0] != str(lab / "inferno/build/qemu-system-aarch64"):
            raise RuntimeError("unexpected emulator path in local launch record")
        if args.virtual_dock:
            if "-append" not in command:
                raise RuntimeError("normal guest launch record has no kernel arguments")
            position = command.index("-append") + 1
            command[position] += " force-usb-test-mode=1"
            if args.ncm_visible:
                command[position] += " ncm-no-hide=1"
        if args.input_control:
            if "-qmp" in command:
                raise RuntimeError("launch record already configures QMP")
            command += ["-qmp", f"unix:{lab / 'ios-input.sock'},server=on,wait=off"]
        check_guest_disks(lab, command)
        headless = any(option == "-display" and command[i + 1] == "none"
                       for i, option in enumerate(command[:-1]))
        child_env = dict(os.environ,
                         MHI2_IOS_VM_DIAGNOSTICS="1" if args.diagnostic_keybag else "0",
                         MHI2_IOS_VM_TRISTAR="1" if args.virtual_dock else "0",
                         MHI2_IOS_VM_TRISTAR_TRACE="0",
                         MHI2_IOS_VM_INPUT_CONTROL="1" if args.input_control else "0",
                         MHI2_IOS_VM_HEADLESS_DISPLAY="1" if headless else "0")
        with (lab / "ios-host.log").open("w") as log:
            child = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=log,
                                     stderr=subprocess.STDOUT, start_new_session=True,
                                     env=child_env)
        (lab / "ios.pid").write_text(f"{child.pid}\n")
        mode_file.write_text(args.boot + "\n")
        diagnostic_file.write_text("1\n" if args.diagnostic_keybag else "0\n")
        dock_file.write_text("1\n" if args.virtual_dock else "0\n")
        combined_file.write_text("1\n" if args.combined_usb else "0\n")
        input_file.write_text("1\n" if args.input_control else "0\n")
        visible_file.write_text("1\n" if args.ncm_visible else "0\n")
        print(f"Started {args.boot} guest: {child.pid}")
    elif args.action == "stop":
        stopped = stop_guest(lab, force=args.force)
        if stopped:
            print(f"Stopped {mode} guest: {stopped}")
        else:
            print("iOS guest is stopped")
    elif args.action == "query":
        return subprocess.run([str(lab / "query-restore")],
            env=dict(os.environ, USBMUXD_SOCKET_ADDRESS=f"127.0.0.1:{args.usbmux_port}"),
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
