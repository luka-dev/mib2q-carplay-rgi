#!/usr/bin/env python3
"""Exercise the review-only rc.local insertion and configuration wrapper."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile

from build_launch_candidate import ANCHOR, LAUNCH, OUT, RC_LOCAL, REQUIRED


HERE = Path(__file__).resolve().parent
subprocess.run(["python3", str(HERE / "build_launch_candidate.py")], check=True)
source = RC_LOCAL.read_text()
candidate = (OUT / "rc.local.wireless.offline.sh").read_text()
assert candidate.replace(LAUNCH, "") == source
assert candidate.count(ANCHOR) == 1
example = (OUT / "wireless_carplay.env.example").read_text()
assert all(example.count(f"export {name}=''") == 1 for name in REQUIRED)
assert 'dio_manager.json' in example
assert all(f'export MHI2Q_CP_{field}=' not in example
           for field in ('NAME', 'MANUFACTURER', 'FIRMWARE'))

with tempfile.TemporaryDirectory(prefix="mhi2q-launch-") as temp:
    home = Path(temp)
    wrapper = HERE / "wireless_dio_supervisor.sh"
    env = dict(os.environ, H=str(home))
    missing = subprocess.run(["sh", str(wrapper)], env=env, capture_output=True, text=True)
    assert missing.returncode == 78
    assert "missing configuration" in missing.stderr
    (home / "wireless_carplay.env").write_text("export MHI2Q_CP_NAME='Unit Test'\n")
    supervisor = home / "wireless_dio_supervisor_bin"
    supervisor.write_text("#!/bin/sh\nprintf '%s\\n' \"$MHI2Q_CP_NAME\"\n")
    supervisor.chmod(0o755)
    launched = subprocess.run(["sh", str(wrapper)], env=env, capture_output=True, text=True)
    assert launched.returncode == 0, launched.stderr
    assert launched.stdout.strip() == "Unit Test"

print("offline rc.local insertion, config guard and supervisor environment passed")
