#!/usr/bin/env python3
"""Exercise the shipping PF installer with stock MU1316 profiles and failures."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "deploy/smartphone_integrator/install_persistent_pf.sh"
STOCK = ROOT / "deploy/smartphone_integrator/pf_stock"
PROFILES = ("pf.conf", "pf.ecm0.conf", "pf.mlan0.conf", "pf.ppp0.conf")


def run_case(setup, verify, succeeds=True, fail_command=None):
    with tempfile.TemporaryDirectory(prefix="carplay-pf-") as tmp:
        base = Path(tmp)
        system = base / "system"
        etc = system / "etc"
        etc.mkdir(parents=True)
        commands = base / "commands"
        commands.mkdir()
        # No actual mount; unavailable QNX utilities must never be invoked.
        for name in ("mount", "sed", "awk", "find", "dirname", "tr"):
            command = commands / name
            command.write_text("#!/bin/sh\n" + ("exit 0\n" if name == "mount" else
                               'echo "$0" >> "$FORBIDDEN_LOG"\nexit 97\n'))
            command.chmod(0o755)
        if fail_command:
            command = commands / fail_command
            command.write_text("#!/bin/sh\nexit 98\n")
            command.chmod(0o755)
        setup(etc)
        env = dict(os.environ, SYS_ROOT=str(system),
                   PATH=str(commands) + os.pathsep + os.environ["PATH"],
                   FORBIDDEN_LOG=str(base / "forbidden"))
        result = subprocess.run(["/bin/sh", str(SCRIPT)], env=env,
                                capture_output=True, text=True, timeout=10)
        assert (result.returncode == 0) == succeeds, result.stdout + result.stderr
        assert not (base / "forbidden").exists(), "used unavailable QNX utility"
        assert not list(etc.glob("*.new")), "left temporary file behind"
        verify(etc, result)
        if succeeds:
            before = {p.name: p.read_bytes() for p in etc.iterdir()}
            again = subprocess.run(["/bin/sh", str(SCRIPT)], env=env,
                                   capture_output=True, text=True, timeout=10)
            assert again.returncode == 0, again.stdout + again.stderr
            assert before == {p.name: p.read_bytes() for p in etc.iterdir()}, "not idempotent"


def setup_stock(etc):
    for name in PROFILES:
        shutil.copy2(STOCK / name, etc / name)


def verify_stock(etc, _):
    for name in PROFILES:
        old = (STOCK / name).read_bytes()
        assert (etc / name).read_bytes() == old.replace(b" 6030,", b" 6030:6032,")
        assert (etc / (name + ".stock")).read_bytes() == old
        assert (etc / name).stat().st_mode == (STOCK / name).stat().st_mode


run_case(setup_stock, verify_stock)
for port_range in ("6030:6031", "6030:6032", "6030:6033"):
    original = (STOCK / "pf.conf").read_text().replace(" 6030,", " " + port_range + ",")
    # Unrelated interfaces and comments must be preserved, including 6033.
    original += "# 6030:6032 is unrelated\npass on ecm0 proto tcp to any port 6030:6033\n"

    def setup_range(etc):
        (etc / "pf.conf").write_text(original)

    def verify_range(etc, _):
        assert (etc / "pf.conf").read_text() == original.replace(" " + port_range + ",", " 6030:6032,")
        backup = etc / "pf.conf.stock"
        if port_range == "6030:6032":
            assert not backup.exists()
        else:
            assert backup.read_text() == original

    run_case(setup_range, verify_range)

for invalid in ("# only a comment: 6030:6032\n", (STOCK / "pf.conf").read_text() * 2):
    def setup_invalid(etc):
        (etc / "pf.conf").write_text(invalid)

    def verify_invalid(etc, _):
        assert (etc / "pf.conf").read_text() == invalid
        assert not (etc / "pf.conf.stock").exists()

    run_case(setup_invalid, verify_invalid, succeeds=False)

run_case(lambda _: None, lambda *_: None, succeeds=False)

def verify_backup_failure(etc, _):
    assert (etc / "pf.conf").read_bytes() == (STOCK / "pf.conf").read_bytes()
    assert not (etc / "pf.conf.stock").exists()

run_case(setup_stock, verify_backup_failure, succeeds=False, fail_command="cp")
run_case(setup_stock, lambda *_: None, succeeds=False, fail_command="cat")
print("PF installer: four stock profiles, range upgrades, idempotence, scoped matching, missing/duplicate rules and I/O failures PASS")
