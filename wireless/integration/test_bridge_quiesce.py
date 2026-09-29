#!/usr/bin/env python3
"""Host check: BUSY triggers one wired drain; only QUIESCE=OK permits TERM."""

from __future__ import annotations

import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import time


SOURCE = Path(__file__).with_name("dio_owner_exec.c")


def wait_for(predicate, seconds: float = 4) -> None:
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError("timed out")


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="mhi2q-quiesce-") as name:
        root = Path(name)
        helper = root / "dio_owner_exec"
        subprocess.run(["cc", "-std=c11", "-Os", "-Wall", "-Wextra", "-Werror",
                        "-o", str(helper), str(SOURCE)], check=True)
        script = root / "owner.sh"
        script.write_text("#!/bin/sh\necho \"start $MHI2Q_DIO_OWNER_MODE\" >> \"$EVENTS\"\n"
                          "trap 'echo stop >> \"$EVENTS\"; exit 0' TERM\n"
                          "while :; do sleep 1; done\n")
        script.chmod(0o755)
        events = root / "events"
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            port = listener.getsockname()[1]
            listener.listen(2)
            listener.settimeout(5)
            env = dict(os.environ, EVENTS=str(events),
                       MHI2Q_DIO_OWNER_LOCK=str(root / "owner.lock"),
                       MHI2Q_DIO_WIRED_PRIORITY_LOCK=str(root / "priority.lock"),
                       MHI2Q_REQUIRE_BRIDGE_QUIESCE="1",
                       MHI2Q_OWNER_BRIDGE_PORT=str(port), MHI2Q_CP_AIRPLAY_PORT="5000")
            owner = subprocess.Popen([str(helper), "wireless", str(script)], env=env,
                                     stderr=subprocess.DEVNULL)
            wired = None
            try:
                wait_for(lambda: events.exists() and "start wireless" in events.read_text())
                replies = [3, 0, 0]
                requests: list[bytes] = []
                allow_ok = threading.Event()

                def serve() -> None:
                    for status in replies:
                        client, _ = listener.accept()
                        with client:
                            request = b""
                            while len(request) < 16:
                                request += client.recv(16 - len(request))
                            requests.append(request)
                            if len(requests) == 3:
                                allow_ok.wait(timeout=2)
                            client.sendall(b"CPRA\x02" + bytes([status, 0, 0]))

                thread = threading.Thread(target=serve, daemon=True)
                thread.start()
                wired = subprocess.Popen([str(helper), "wired", str(script)], env=env,
                                         stderr=subprocess.DEVNULL)
                wait_for(lambda: len(requests) >= 3)
                assert owner.poll() is None and "stop" not in events.read_text()
                allow_ok.set()
                wait_for(lambda: "start wired" in events.read_text())
                assert owner.wait(timeout=2) == 0
                assert events.read_text().index("stop") < events.read_text().index("start wired")
                thread.join(timeout=1)
                assert len(requests) == 3
                assert [item[5] for item in requests] == [3, 4, 3]
                assert all(item[:5] == b"CPRX\x02" and int.from_bytes(item[12:14], "little") == 5000
                           for item in requests)
                print("bridge quiesce: BUSY drains once; QUIESCE OK precedes wired SIGTERM")
            finally:
                for child in (wired, owner):
                    if child and child.poll() is None:
                        child.terminate()
                        try:
                            child.wait(timeout=2)
                        except subprocess.TimeoutExpired:
                            child.kill()
                            child.wait()


if __name__ == "__main__":
    main()
