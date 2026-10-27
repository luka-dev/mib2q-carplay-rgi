#!/usr/bin/env python3
"""Exercise wireless-only install/rollback against disposable filesystem trees."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile


REPO = Path(__file__).resolve().parents[2]
ARCHIVE = REPO / "wireless/out/mhi2q-wireless-offline-review.tar"
REPLACED = {
    "/mnt/app/eso/bin/apps/btstack": "reference/btstack.stock",
    "/mnt/app/eso/lib/libairplay.so": "reference/libairplay.stock.so",
    "/mnt/system/etc/pf.conf": "reference/pf.conf.wired",
    "/mnt/system/etc/pf.ecm0.conf": "reference/pf.ecm0.conf.wired",
    "/mnt/system/etc/pf.mlan0.conf": "reference/pf.mlan0.conf.wired",
    "/mnt/system/etc/pf.ppp0.conf": "reference/pf.ppp0.conf.wired",
    "/mnt/app/root/hooks/carplay_startup.sh": "reference/carplay_startup.wired.sh",
    "/mnt/app/root/rc.local": "reference/rc.local.before.sh",
}
OWNED = (
    "/mnt/app/root/hooks/mhi2q-wireless-daemon",
    "/mnt/app/root/hooks/dio_owner_exec",
    "/mnt/app/root/hooks/wireless_dio_supervisor_bin",
    "/mnt/app/root/hooks/libdio_receiver_bridge.so",
    "/mnt/app/root/hooks/wireless_dio_supervisor.sh",
    "/mnt/app/root/hooks/wireless_carplay.env",
)


def run(script: Path, action: str, stage: Path, target: Path, success: bool,
        fail_path: str = "") -> str:
    env = dict(os.environ, MHI2Q_STAGE=str(stage), MHI2Q_TARGET_ROOT=str(target))
    if fail_path:
        env["MHI2Q_TEST_FAIL_PATH"] = fail_path
    result = subprocess.run(["sh", str(script), action], env=env, text=True,
                            capture_output=True)
    assert (result.returncode == 0) == success, (action, result.stdout, result.stderr)
    return result.stdout + result.stderr


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--archive", type=Path, default=ARCHIVE)
args = parser.parse_args()

with tempfile.TemporaryDirectory(prefix="mhi2q-install-") as temp:
    temp = Path(temp)
    stage, target = temp / "stage", temp / "target"
    stage.mkdir()
    target.mkdir()
    with tarfile.open(args.archive) as archive:
        archive.extractall(stage, filter="data")
    script = stage / "integration/install_wireless_candidate.sh"
    assert script.is_file()
    for path, reference in REPLACED.items():
        destination = target / path.lstrip("/")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes((stage / reference).read_bytes())
    # The QNX daemon cannot run on the host; substitute only in this throwaway
    # stage and update its checksum so the same shell verification still runs.
    fake = stage / "bin/mhi2q-wireless-daemon"
    fake.write_text("#!/bin/sh\n[ \"$1\" = --check-config ] && [ \"$MHI2Q_CP_NAME\" = Test ]\n")
    fake.chmod(0o755)
    owner = stage / "bin/dio_owner_exec"
    subprocess.run(["cc", "-std=c11", "-Os", "-Wall", "-Wextra", "-Werror",
                    "-o", str(owner),
                    str(REPO / "wireless/integration/dio_owner_exec.c")], check=True)
    sums = stage / "checksums.txt"
    lines = sums.read_text().splitlines()
    for local, name in ((fake, "bin/mhi2q-wireless-daemon"),
                        (owner, "bin/dio_owner_exec")):
        lines = [f"{hashlib.sha256(local.read_bytes()).hexdigest()} {name}"
                 if line.endswith(f" {name}") else line for line in lines]
    sums.write_text("\n".join(lines) + "\n")
    config = stage / "integration/wireless_carplay.env"
    assert "missing" in run(script, "preflight", stage, target, False)
    config.write_text("export MHI2Q_CP_NAME=Test\n")
    run(script, "preflight", stage, target, True)
    assert "persistent filesystem" in run(script, "install", stage, target, False)
    (target / "mnt/persist").mkdir(parents=True)
    assert all((target / path.lstrip("/")).read_bytes() == (stage / ref).read_bytes()
               for path, ref in REPLACED.items())
    assert "rolled back" in run(script, "install", stage, target, False,
                                "/mnt/system/etc/pf.ecm0.conf")
    assert all((target / path.lstrip("/")).read_bytes() == (stage / ref).read_bytes()
               for path, ref in REPLACED.items())
    assert all(not (target / path.lstrip("/")).exists() for path in OWNED)
    assert all(not (target / (path.lstrip("/") + ".wireless-prev")).exists()
               for path in REPLACED)
    run(script, "install", stage, target, True)
    assert all((target / path.lstrip("/")).exists() for path in OWNED)
    assert all((target / (path.lstrip("/") + ".wireless-prev")).is_file()
               for path in REPLACED)
    assert "existing backup" in run(script, "install", stage, target, False)
    rc = target / "mnt/app/root/rc.local"
    rc.write_text("another owner's edit\n")
    assert "changed since installation" in run(script, "rollback", stage, target, False)
    rc.write_bytes((stage / "integration/rc.local.wireless.candidate.sh").read_bytes())
    unit_config = target / "mnt/app/root/hooks/wireless_carplay.env"
    unit_config.write_text("export MHI2Q_CP_NAME=ChangedAfterInstall\n")
    run(script, "rollback", stage, target, True)
    assert all((target / path.lstrip("/")).read_bytes() == (stage / ref).read_bytes()
               for path, ref in REPLACED.items())
    assert all(not (target / path.lstrip("/")).exists() for path in OWNED
               if not path.endswith("wireless_carplay.env"))
    assert unit_config.read_text() == "export MHI2Q_CP_NAME=ChangedAfterInstall\n"
    assert all(not (target / (path.lstrip("/") + ".wireless-prev")).exists()
               for path in REPLACED)
    payload = stage / "pf/pf.conf"
    payload.write_bytes(payload.read_bytes() + b"\n")
    assert "checksum mismatch" in run(script, "preflight", stage, target, False)

print("offline install, idempotence guard, tamper rejection and rollback passed")
