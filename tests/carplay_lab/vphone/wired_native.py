"""Run wired native video against an isolated snapshot and private EHCI QEMU."""
import argparse
import base64
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import signal
import socket
import struct
import subprocess
import threading
import time
from types import SimpleNamespace

from . import lab, ethernet, carplay_input, mfi_probe, moving_route, usb
from tests.carplay_lab.common import qnx_receiver, mfi_sap

ROOT = Path(__file__).resolve().parents[3]
SHARED = ROOT / "build/vphone-lab"
BASE = ROOT / "build/wired-altscreen-30fps-re"
OUT = BASE / "native"


def command(transport="socket"):
    if transport != "socket":
        raise ValueError("this receiver uses only the isolated NCM socket bridge")
    settings = json.loads((OUT / "qnx-network/build-record.json").read_text())
    backend = settings.get("native_wait_backend", "poll")
    if backend not in ("poll", "kqueue"):
        raise ValueError("unknown private wait backend")
    record = json.loads((BASE / f"qemu-highspeed-{backend}/build.json").read_text())
    binary = Path(record["binary"])
    if hashlib.sha256(binary.read_bytes()).hexdigest() != record["sha256"]:
        raise ValueError("private QEMU fingerprint changed")
    return [str(binary), "-M", "virt", "-m", "2048", "-cpu", "cortex-a15,cntfrq=62500000",
            "-device", f"loader,file={OUT / 'qnx-network/qemu_start_debug.bin'},addr=0x40200000,force-raw=on,cpu-num=0",
            "-display", "none", "-serial", "stdio", "-monitor", "none",
            "-netdev", "socket,id=n0,listen=127.0.0.1:32501", "-device", "usb-net,netdev=n0",
            "-netdev", "user,id=mfi,net=10.0.3.0/24,hostfwd=tcp:127.0.0.1:33503-10.0.3.15:32503",
            "-device", "usb-net,netdev=mfi,mac=52:54:00:12:34:57"]


def configure():
    OUT.mkdir(parents=True, exist_ok=True)
    if not (OUT / "machine.json").exists():
        shutil.copyfile(SHARED / "machine.json", OUT / "machine.json")
    lab.OUT = OUT
    qnx_receiver.set_output(OUT / "qnx-network")
    # Process-local adapter: existing ownership checks compare this exact command.
    qnx_receiver.command = command
    # vm list costs several seconds on this host; avoid repeating it for every
    # fragment of the bounded iAP2 RPC exchange. Each RPC still goes through
    # Launchpad; cache the initial ownership check for this bounded experiment.
    require = lab.require_machine
    machine_lock = threading.Lock()
    cached = {}
    def checked_machine(settings, state=None):
        with machine_lock:
            if time.monotonic() >= cached.get("until", 0):
                cached["machine"] = require(settings, state)
                cached["until"] = time.monotonic() + 180
            machine = cached["machine"]
            if state and machine.get("state") != state:
                raise RuntimeError(f"expected {state} guest")
            return machine
    lab.require_machine = checked_machine
    create = socket.create_connection
    def input_connection(address, *args, **kwargs):
        if address == ("127.0.0.1", 32503):
            address = ("127.0.0.1", 33503)
        return create(address, *args, **kwargs)
    carplay_input.socket = SimpleNamespace(create_connection=input_connection)


@contextmanager
def shared_lock(name):
    with (SHARED / name).open("a+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield


def start(backend="poll"):
    if qnx_receiver.running():
        raise RuntimeError("this private receiver is already running")
    image = qnx_receiver.OUT / "qemu_start_debug.bin"
    record = json.loads((qnx_receiver.OUT / "build-record.json").read_text())
    if hashlib.sha256(image.read_bytes()).hexdigest() != record["output_sha256"]:
        raise ValueError("private QNX image fingerprint changed")
    record["native_wait_backend"] = backend
    (qnx_receiver.OUT / "build-record.json").write_text(json.dumps(record, indent=2) + "\n")
    for port in (32501, 32502, 33503):
        with socket.socket() as check:
            check.bind(("127.0.0.1", port))
    trace = qnx_receiver.OUT / "network-trace.jsonl"
    trace.write_text("")
    environment = dict(os.environ, MHI2Q_QEMU_NET_TRACE=str(trace))
    args = command()
    with (qnx_receiver.OUT / "serial.log").open("w") as serial, (qnx_receiver.OUT / "qemu-host.log").open("w") as host:
        child = subprocess.Popen(args, stdin=subprocess.DEVNULL, stdout=serial, stderr=host,
                                 env=environment, start_new_session=True)
    (qnx_receiver.OUT / "qnx.pid").write_text(str(child.pid) + "\n")
    (qnx_receiver.OUT / "transport.txt").write_text("socket\n")
    lab.save("receiver-command.json", {"pid": child.pid, "command": args})
    return {"qnx_pid": child.pid}


def stop():
    pid = qnx_receiver.running()
    if pid:
        os.kill(pid, signal.SIGTERM)
        deadline = time.monotonic() + 8
        while qnx_receiver.running() == pid:
            if time.monotonic() > deadline:
                raise RuntimeError("owned receiver did not stop")
            time.sleep(.1)
    return {"stopped_pid": pid}


def signer(seconds, errors):
    try:
        key, certificate = mfi_sap.fixture_key()
        end = time.monotonic() + seconds
        with socket.socket() as listener:
            listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listener.bind(("127.0.0.1", 32502))
            listener.listen(2)
            listener.settimeout(.5)
            print("wired native MFi-SAP ready", flush=True)
            while time.monotonic() < end:
                try:
                    connection, _ = listener.accept()
                except socket.timeout:
                    continue
                with connection:
                    connection.settimeout(5)
                    if not qnx_receiver.running():
                        raise RuntimeError("owned receiver stopped during MFi request")
                    data = mfi_sap.respond_request(mfi_sap.read_exact(connection, 33), key, certificate)
                    connection.sendall(struct.pack(">I", len(data)) + data)
                    print("wired native MFi-SAP response", len(data), flush=True)
    except Exception as error:
        errors.append(str(error))


def serve(seconds):
    errors = []
    with shared_lock(".ethernet.lock"):
        worker = threading.Thread(target=signer, args=(seconds, errors), daemon=True)
        worker.start()
        result = ethernet.run(seconds, trace_queues=True)
        worker.join(timeout=2)
        result["signer_errors"] = errors
        lab.save("serve-result.json", result)
        if errors:
            raise RuntimeError("; ".join(errors))
        return result


def session(seconds):
    # The wire exchange uses PyUSB's exception classes even with the API peer.
    # Fail before creating an endpoint if the laboratory venv is not in use.
    import usb.core  # noqa: F401
    if not qnx_receiver.running():
        raise RuntimeError("private receiver is not running")
    serial = (qnx_receiver.OUT / "serial.log").read_text(errors="replace")
    keys = re.findall(r"receiver-edpk=([0-9a-f]{64})(?![0-9a-f])", serial)
    if not keys:
        raise RuntimeError("private receiver public key is not ready")
    with shared_lock(".usb.lock"):
        settings = lab.profile()
        lab.require_machine(settings, "running")
        lab.client("guest", "unlock", *lab.machine_arguments(settings))
        original_peer = mfi_probe.NativeAPIPeer
        class TimedPeer(original_peer):
            def __init__(self, settings, incoming, log):
                super().__init__(settings, incoming, log)
                deadline = time.monotonic() + 10
                while True:
                    try:
                        lab.rpc(settings, "files.read", {"path": log, "limit": 1024})
                        break
                    except RuntimeError:
                        if time.monotonic() >= deadline:
                            raise
                        time.sleep(.2)
            def read(self, endpoint, size, timeout):
                started = time.monotonic()
                result = super().read(endpoint, size, max(timeout, 15000) if timeout >= 4000 else timeout)
                packet = mfi_probe.wire.parse_link_frame(bytes(result[2:]))
                payload = packet["payload"]
                message = hex(int.from_bytes(payload[4:6], "big")) if payload.startswith(b"@@") and len(payload) >= 6 else None
                print(json.dumps({"iap_receive_seconds": time.monotonic()-started,
                                  "control_message": message, "payload_bytes": len(payload)}), flush=True)
                return result
        mfi_probe.NativeAPIPeer = TimedPeer
        return mfi_probe.run(identify=True, start_carplay=True,
                             public_key=bytes.fromhex(keys[-1]), hold_seconds=seconds)


def restore():
    settings = lab.profile()
    result = {}
    with shared_lock(".usb.lock"):
        original = json.loads((OUT / "airplay-preferences-original.json").read_text())["result"]["values"]
        current = lab.rpc(settings, "settings.get", {"domain": "com.apple.airplay", "key": "encoderBWFixed"})["result"]
        if current.get("exists") and current.get("value") != 2000000:
            raise RuntimeError("encoderBWFixed changed outside this experiment; retain the restore journal")
        if "encoderBWFixed" in original:
            value = original["encoderBWFixed"]
            lab.rpc(settings, "settings.set", {"domain": "com.apple.airplay", "key": "encoderBWFixed", "value": value["value"], "type": value["type"]})
        else:
            lab.rpc(settings, "settings.delete", {"domain": "com.apple.airplay", "key": "encoderBWFixed"})
        result["preference"] = lab.rpc(settings, "settings.get", {"domain": "com.apple.airplay", "key": "encoderBWFixed"})
        map_backups = ("maps-preferences-original.json", "maps-container-key-original.json")
        if any((OUT / name).exists() for name in map_backups):
            # Stop the app before restoring its cached display-rate preference.
            result["maps_stopped_for_preference_reload"] = lab.rpc(settings, "apps.terminate", {"bundle_id": "com.apple.Maps"})
        for name in map_backups:
            maps_backup = OUT / name
            if not maps_backup.exists():
                continue
            old = json.loads(maps_backup.read_text())["result"]
            request = {"domain": old["domain"], "key": "VKNavDisplayRate"}
            now = lab.rpc(settings, "settings.get", request)["result"]
            if now.get("exists") and now.get("value") not in (2, old.get("value")):
                raise RuntimeError("VKNavDisplayRate changed outside this experiment")
            if old.get("exists"):
                lab.rpc(settings, "settings.set", dict(request, value=old["value"], type=old["type"]))
            else:
                lab.rpc(settings, "settings.delete", request)
            result[name] = lab.rpc(settings, "settings.get", request)
            if old["domain"].startswith("/"):
                # cfprefsd's successful readback can precede the actual file flush.
                deadline = time.monotonic() + 30
                while True:
                    reply = lab.rpc(settings, "files.read", {"path": old["domain"], "binary": True, "limit": 32768})["result"]
                    if reply.get("truncated"):
                        raise RuntimeError("Maps preference file exceeds restoration verification bound")
                    values = plistlib.loads(base64.b64decode(reply["content"]))
                    exists = "VKNavDisplayRate" in values
                    if exists == old.get("exists", False) and (not exists or values["VKNavDisplayRate"] == old["value"]):
                        result["maps_container_file_verified"] = True
                        break
                    if time.monotonic() >= deadline:
                        raise RuntimeError("Maps container preference restoration has not flushed")
                    time.sleep(1)
        # The generic helper can restore the interface tree while keeping its
        # runtime AllowMultipleCreates flag. Removing that flag was rejected by
        # this guest (0xe00002e2); retain and report it rather than reset the VM.
        result["usb_restore"] = usb.run("restore")["status"]
        probe = usb.run("probe")["log"]["result"]["content"]
        description = plistlib.loads(probe[probe.index("<?xml"):].encode())
        saved = json.loads((OUT / "usb-original.json").read_text())["description"]
        result["usb_description_matches_original"] = description == saved
        remaining = {}
        if "AllowMultipleCreates" not in saved and description.get("AllowMultipleCreates") is True:
            remaining["AllowMultipleCreates"] = description.pop("AllowMultipleCreates")
        result["usb_interfaces_restored"] = description == saved
        result["remaining_runtime_properties"] = remaining
        if not result["usb_interfaces_restored"]:
            raise RuntimeError("USB description differs beyond the recorded runtime flag")
        if remaining:
            result["runtime_property_removal"] = "rejected by guest USB API (0xe00002e2); VM kept running"
        lab.rpc(settings, "files.remove", {"path": usb.GUEST_DIR + "/location-route"})
    result["pending"] = False
    lab.save("restore-result.json", result)
    lab.save("pending-restore.json", {"pending": False, "result": "restore-result.json"})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("start", "stop", "status", "serve", "session", "snapshot", "timing", "clock", "tap", "motion", "logs", "restore"))
    parser.add_argument("--seconds", type=int, default=120)
    parser.add_argument("--channel", choices=("primary", "cluster"), default="cluster")
    parser.add_argument("--fresh", action="store_true")
    parser.add_argument("--backend", choices=("poll", "kqueue"), default="poll")
    parser.add_argument("--x", type=int)
    parser.add_argument("--y", type=int)
    args = parser.parse_args()
    configure()
    if args.action == "start": result = start(args.backend)
    elif args.action == "stop": result = stop()
    elif args.action == "status": result = {"qnx_pid": qnx_receiver.running()}
    elif args.action == "serve": result = serve(args.seconds)
    elif args.action == "session": result = session(args.seconds)
    elif args.action == "snapshot": result = carplay_input.snapshot(args.channel, fresh=args.fresh)
    elif args.action == "timing": result = carplay_input.timing(args.channel)
    elif args.action == "clock": result = carplay_input.clock(3, .5)
    elif args.action == "tap": result = carplay_input.tap(args.x, args.y)
    elif args.action == "motion": result = moving_route.run(args.seconds)
    elif args.action == "restore": result = restore()
    else:
        result = lab.rpc(lab.profile(), "logs.syslog", {"seconds": min(args.seconds, 45), "process": "airplayd", "max_lines": 1500})
        lab.save("airplayd-current.json", result)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
