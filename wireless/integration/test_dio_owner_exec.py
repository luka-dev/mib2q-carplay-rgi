#!/usr/bin/env python3
"""Host process test of the exact fcntl handoff implementation."""

from __future__ import annotations

import os
from contextlib import ExitStack
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import tempfile
import threading
import time


SOURCE = Path(__file__).with_name("dio_owner_exec.c")
WRAPPER = SOURCE.parent / "out/carplay_startup.stock210.offline.sh"


def wait_for(predicate, seconds=8):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        result = predicate()
        if result:
            return result
        time.sleep(0.05)
    raise AssertionError("timed out waiting for process event")


def stop(process):
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)


def main():
    subprocess.run(['python3', str(SOURCE.with_name('build_wrapper_candidate.py')),
                    '--stock210', '--property-handoff'], check=True)
    with tempfile.TemporaryDirectory(prefix="mhi2q-dio-owner-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        binary = root / "dio_owner_exec"
        subprocess.run(["cc", "-std=c11", "-Os", "-Wall", "-Wextra", "-Werror",
                        "-o", str(binary), str(SOURCE)], check=True)
        script = root / "fake_dio.sh"
        script.write_text("""#!/bin/sh
echo "$MHI2Q_DIO_OWNER_MODE start $$" >> "$EVENTS"
echo "$MHI2Q_DIO_OWNER_MODE preload $LD_PRELOAD apac ${AUDIO_SPATIAL_CODEC_LIB:-}" >> "$EVENTS"
if [ "${IGNORE_TERM:-0}" = 1 ]; then
    trap '' TERM
else
    trap 'echo "$MHI2Q_DIO_OWNER_MODE stop $$" >> "$EVENTS"; exit 0' TERM
fi
while :; do sleep 1; done
""")
        script.chmod(0o755)
        events = root / "events"
        env = dict(os.environ, MHI2Q_DIO_OWNER_LOCK=str(root / "owner.lock"),
                   MHI2Q_DIO_WIRED_PRIORITY_LOCK=str(root / "priority.lock"),
                   EVENTS=str(events))

        def lines():
            return events.read_text().splitlines() if events.exists() else []

        def launch(mode, extra=None):
            process = subprocess.Popen([str(binary), mode, str(script)],
                                    env=env | (extra or {}),
                                    stdout=subprocess.DEVNULL,
                                    stderr=subprocess.PIPE)
            cleanup.callback(stop, process)
            return process

        wireless = launch("wireless")
        wait_for(lambda: any(x == f"wireless start {wireless.pid}" for x in lines()))
        wired = launch("wired")
        wait_for(lambda: any(x == f"wired start {wired.pid}" for x in lines()))
        assert wireless.wait(timeout=3) == 0
        first = lines()
        assert first.index(f"wireless stop {wireless.pid}") < first.index(f"wired start {wired.pid}")
        deferred = launch("wireless")
        assert deferred.wait(timeout=2) == 75
        assert len([x for x in lines() if x.startswith("wireless start")]) == 1
        stop(wired)

        resumed = launch("wireless")
        wait_for(lambda: any(x == f"wireless start {resumed.pid}" for x in lines()))
        stop(resumed)

        stubborn = launch("wireless", {"IGNORE_TERM": "1"})
        wait_for(lambda: any(x == f"wireless start {stubborn.pid}" for x in lines()))
        before = time.monotonic()
        takeover = launch("wired")
        wait_for(lambda: any(x == f"wired start {takeover.pid}" for x in lines()), seconds=7)
        assert time.monotonic() - before < 7
        assert stubborn.wait(timeout=2) == -signal.SIGKILL
        stop(takeover)

        # Exercise the generated SI wrapper's trampoline and generation
        # publication boundary, with a harmless local dio_manager stand-in.
        hooks = root / "hooks"
        apps = root / "apps"
        hooks.mkdir()
        apps.mkdir()
        shutil.copy2(binary, hooks / "dio_owner_exec")
        shutil.copy2(WRAPPER, hooks / "carplay_startup.sh")
        (hooks / "carplay_startup.sh").chmod(0o755)
        shutil.copy2(script, apps / "dio_manager")
        for filename in ("libcarplay_hook.so", "libdio_receiver_bridge.so",
                         "carplay_processes.sh", "libcarplay_wireless_stock210.so",
                         "libcarplay_wireless_apac.so"):
            (hooks / filename).touch()
        monitor = hooks / "carplay_monitor.sh"
        monitor.write_text("#!/bin/sh\necho \"monitor $MHI2Q_DIO_OWNER_MODE $$\" >> \"$EVENTS\"\n")
        monitor.chmod(0o755)
        (hooks / "wireless_carplay.env").write_text("# explicit fixture configuration\n")
        integration_env = env | {"H": str(hooks), "DIODIR": str(apps),
                                 "WLOG": str(root / "wrapper.log"),
                                 "OWNER_FILE": str(root / "generation"),
                                 "MHI2Q_CP_WIRELESS_HOOK": str(hooks / 'libcarplay_wireless_stock210.so'),
                                 "MHI2Q_CP_WIRELESS_APAC": str(hooks / 'libcarplay_wireless_apac.so')}
        listener = socket.socket()
        cleanup.callback(listener.close)
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(8)
        integration_env['MHI2Q_OWNER_BRIDGE_PORT'] = str(listener.getsockname()[1])
        requests = []

        def quiesce():
            client, _ = listener.accept()
            with client:
                client.settimeout(3)
                request = b''
                while len(request) < 16:
                    data = client.recv(16 - len(request))
                    if not data:
                        return
                    request += data
                requests.append(request)
                client.sendall(b'CPRA\x02\x00\x00\x00')

        bridge = threading.Thread(target=quiesce, daemon=True)
        bridge.start()
        # Wireless startup must not run mv/rm for owner or USB/NCM metadata.
        # Keep only sleep for the harmless fake dio's loop.
        path_tools = root / "path_tools"
        path_tools.mkdir()
        os.symlink(shutil.which("sleep"), path_tools / "sleep")
        wireless = subprocess.Popen([str(hooks / "dio_owner_exec"), "wireless",
                                     str(hooks / "carplay_startup.sh")],
                                    env=integration_env | {
                                        "MHI2Q_DIO_PUBLISH_OWNER": "1",
                                        "PATH": str(path_tools),
                                    }, stderr=subprocess.PIPE)
        cleanup.callback(stop, wireless)
        wait_for(lambda: any(x == f"wireless start {wireless.pid}" for x in lines()))
        assert (root / "generation").read_text().strip() == str(wireless.pid)
        wired = subprocess.Popen([str(hooks / "carplay_startup.sh")],
                                 env=integration_env, stderr=subprocess.PIPE)
        cleanup.callback(stop, wired)
        wait_for(lambda: any(x == f"wired start {wired.pid}" for x in lines()))
        assert (root / "generation").read_text().strip() == str(wired.pid)
        # fork precedes exec, but the background monitor can be scheduled
        # after the wired child's first log; wait for both observations.
        wait_for(lambda: any(x.startswith("monitor wired ") for x in lines()))
        timeline = lines()
        assert f'wireless preload {hooks}/libcarplay_wireless_stock210.so:{hooks}/libdio_receiver_bridge.so apac {hooks}/libcarplay_wireless_apac.so' in timeline
        bridge.join(timeout=1)
        assert len(requests) == 1 and requests[0][:6] == b'CPRX\x02\x03'
        wired_preload = next(line for line in timeline if line.startswith(f'wired preload {hooks}/'))
        assert wired_preload.startswith(f'wired preload {hooks}/libcarplay_hook.so apac ')
        assert 'wireless_apac' not in wired_preload
        wired_monitor = next(i for i, line in enumerate(timeline) if line.startswith("monitor wired "))
        assert timeline.index(f"wireless stop {wireless.pid}") < wired_monitor
        assert timeline.index(f"wireless stop {wireless.pid}") < timeline.index(f"wired start {wired.pid}")
        assert wireless.wait(timeout=3) == 0
        stop(wired)
        si_wireless=subprocess.Popen([str(hooks / "carplay_startup.sh")],
            env=integration_env|{"MHI2Q_DIO_OWNER_MODE":"wireless","MHI2Q_CP_SI_EPOCH":"1234",
                                 "MHI2Q_CP_SI_SESSION":"7","MHI2Q_CP_SI_LINK":"3"},
            stderr=subprocess.PIPE)
        cleanup.callback(stop,si_wireless)
        wait_for(lambda: f"wireless start {si_wireless.pid}" in lines())
        wait_for(lambda: any(x.startswith("monitor wireless ") for x in lines()))
        assert (root/"generation").read_text().strip()==str(si_wireless.pid)
        stop(si_wireless)
        print("dio owner host handoff: orderly takeover, wired priority, restart, SIGKILL deadline passed")


if __name__ == "__main__":
    main()
