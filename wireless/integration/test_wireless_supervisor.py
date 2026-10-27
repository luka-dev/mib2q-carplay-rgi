#!/usr/bin/env python3
"""Host process test for wireless dio/daemon handoff and IE cleanup."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time


HERE = Path(__file__).resolve().parent


def wait_for(predicate, seconds: float = 8) -> None:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.05)
    raise AssertionError("timed out waiting for supervisor event")


def stop(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=4)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=2)


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="mhi2q-wireless-supervisor-") as directory:
        root = Path(directory)
        hooks = root / "hooks"
        hooks.mkdir()
        for source, target, extra in (
            ("dio_owner_exec.c", "dio_owner_exec", []),
            ("wireless_dio_supervisor.c", "wireless_dio_supervisor_bin", []),
            ("supervisor_fake_child.c", "carplay_startup.sh", []),
        ):
            subprocess.run(["cc", "-std=c11", "-Os", "-Wall", "-Wextra", "-Werror",
                            "-o", str(hooks / target), str(HERE / source), *extra], check=True)
        shutil.copy2(hooks / "carplay_startup.sh", hooks / "mhi2q-wireless-daemon")
        shutil.copy2(hooks / "carplay_startup.sh", hooks / "carplay_monitor.sh")
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1", 0))
            port = reserve.getsockname()[1]
        events = root / "events"
        env = dict(os.environ, H=str(hooks), EVENTS=str(events),
                   MHI2Q_DIO_OWNER_LOCK=str(root / "owner.lock"),
                   MHI2Q_DIO_WIRED_PRIORITY_LOCK=str(root / "priority.lock"),
                   MHI2Q_REQUIRE_BRIDGE_QUIESCE="1",
                   MHI2Q_OWNER_BRIDGE_PORT=str(port), MHI2Q_OWNER_READY_MS="3000",
                   MHI2Q_OWNER_RETRY_MS="200")

        def lines() -> list[str]:
            return events.read_text().splitlines() if events.exists() else []

        supervisor = subprocess.Popen([str(hooks / "wireless_dio_supervisor_bin")],
                                      env=env, stderr=subprocess.DEVNULL)
        wired: subprocess.Popen[bytes] | None = None
        try:
            wait_for(lambda: len([x for x in lines() if x.startswith("daemon start ")]) == 1)
            first = lines()
            assert len([x for x in first if x.startswith("wireless start ")]) == 1
            wait_for(lambda: len([x for x in lines() if x.startswith("monitor start ")]) == 1)

            wired = subprocess.Popen([str(hooks / "dio_owner_exec"), "wired",
                                      str(hooks / "carplay_startup.sh")], env=env,
                                     stderr=subprocess.DEVNULL)
            wait_for(lambda: len([x for x in lines() if x.startswith("clear ")]) == 1)
            timeline = lines()
            assert next(i for i, x in enumerate(timeline) if x.startswith("bridge quiesce ")) < \
                   next(i for i, x in enumerate(timeline) if x.startswith("wireless stop "))
            assert next(i for i, x in enumerate(timeline) if x.startswith("wireless stop ")) < \
                   next(i for i, x in enumerate(timeline) if x.startswith("wired start "))
            assert next(i for i, x in enumerate(timeline) if x.startswith("daemon stop ")) < \
                   next(i for i, x in enumerate(timeline) if x.startswith("clear "))
            assert len([x for x in timeline if x.startswith("monitor stop ")]) == 1
            time.sleep(0.5)
            assert len([x for x in lines() if x.startswith("wireless start ")]) == 1
            stop(wired)
            wait_for(lambda: len([x for x in lines() if x.startswith("daemon start ")]) == 2)
            stop(supervisor)
            wait_for(lambda: len([x for x in lines() if x.startswith("clear ")]) == 2)
            assert len([x for x in lines() if x.startswith("daemon stop ")]) == 2
            assert len([x for x in lines() if x.startswith("wireless stop ")]) == 2
            assert len([x for x in lines() if x.startswith("bridge quiesce ")]) == 2
            assert len([x for x in lines() if x.startswith("monitor stop ")]) == 2
            print("wireless supervisor: daemon follows owner, wired handoff cleans IE, restart passed")
        finally:
            if wired is not None:
                stop(wired)
            stop(supervisor)


if __name__ == "__main__":
    main()
