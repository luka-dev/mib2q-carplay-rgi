#!/usr/bin/env python3
"""Exec/PID/env and diagnostic failure regressions for the actual launcher C.

Host stand-in records only explicit test variables, never the full environment.
Native SI startup is a separate QNX/QEMU check.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SI = Path(__file__).resolve().parent


class StartupTest(unittest.TestCase):
    def test_exec_profiles_and_unavailable_diagnostics(self):
        with tempfile.TemporaryDirectory(prefix="si-startup-") as directory:
            root = Path(directory)
            child = root / "smartphone_integrator"
            child.write_text("""#!/usr/bin/env python3
import json, os, sys
print(json.dumps({'pid':os.getpid(), 'argv':sys.argv[1:],
 'factory':os.getenv('IPL_CONFIG_DIR_SMARTPHONE_INTEGRATOR'),
 'library':os.getenv('LD_LIBRARY_PATH'),
 'preload':os.getenv('LD_PRELOAD'),
 'hook':{k:v for k,v in os.environ.items() if k=='MHI2Q_SI_HOOK' or k.startswith('MHI2Q_SI_HOOK_')},
 'nearby':os.getenv('MHI2Q_SI_HOOKED')}))
""")
            child.chmod(0o755)
            args = ["-sm-name", "smartphone_integrator", "-sm-id", "353",
                    "-sm-time", "123", "-sm-restart", "0", "space in one arg"]
            env = {**os.environ, "IPL_CONFIG_DIR_SMARTPHONE_INTEGRATOR": "/factory/config",
                   "LD_LIBRARY_PATH": "/factory/lib", "MHI2Q_SI_HOOK_CHILD_ENVS": "BAD=wireless",
                   "MHI2Q_SI_HOOK_ROOT": "0", "MHI2Q_SI_HOOK_FUTURE": "stale",
                   "MHI2Q_SI_HOOKED": "retain"}
            # macOS SIP strips LD_* when launching its protected interpreters;
            # use an ordinary compiled child for an exact execve env check.
            source = root / "record.c"
            source.write_text(r'''
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(void) {
 printf("PRELOAD=%s\nLIBRARY=%s\n",getenv("LD_PRELOAD"),getenv("LD_LIBRARY_PATH"));
 return 0;
}
''')
            record = root / "record"
            subprocess.run(["cc", str(source), "-o", str(record)], check=True)
            for stage2 in (0, 1):
                for available in (False, True):
                    with self.subTest(stage2=stage2, available=available):
                        log = root / ("log" if available else "missing/log")
                        pid = root / ("pid" if available else "missing/pid")
                        binary = root / "launcher"
                        subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                                        f'-DSI_STARTUP_DIR="{root}"', f'-DSI_STARTUP_LOG="{log}"',
                                        f'-DSI_STARTUP_PID="{pid}"', f"-DSI_STARTUP_STAGE2={stage2}",
                                        str(SI / "si_startup.c"), "-o", str(binary)], check=True)
                        if log.exists(): log.unlink()
                        process = subprocess.Popen([str(binary), *args], env=env, text=True,
                                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                        stdout, stderr = process.communicate(timeout=10)
                        self.assertEqual(process.returncode, 0, stderr)
                        lines = log.read_text().splitlines() if available else stdout.splitlines()
                        state = json.loads(next(line for line in lines if line.startswith("{")))
                        self.assertEqual(state["pid"], process.pid)
                        self.assertEqual(state["argv"], args)
                        self.assertEqual(state["factory"], "/factory/config")
                        self.assertEqual(state["nearby"], "retain")
                        self.assertEqual(state["hook"], {
                            "MHI2Q_SI_HOOK": "1", "MHI2Q_SI_HOOK_VERBOSE": "1",
                            "MHI2Q_SI_HOOK_TRACK_JOBS": "1", "MHI2Q_SI_HOOK_WIRELESS": str(stage2)})
                        if available: self.assertEqual(int(pid.read_text()), process.pid)
                        else: self.assertIn("optional log unavailable", stderr)
                        # Execute a native child to verify loader variables even on macOS.
                        original = child.read_bytes()
                        child.write_bytes(record.read_bytes())
                        if log.exists(): log.unlink()
                        run = subprocess.run([str(binary)], env=env, text=True,
                                             capture_output=True, timeout=10)
                        self.assertEqual(run.returncode, 0, run.stderr)
                        output = log.read_text() if available else run.stdout
                        self.assertIn(f"PRELOAD={root}/libsi_adapter.so", output)
                        self.assertIn("LIBRARY=/factory/lib", output)
                        child.write_bytes(original)

    def test_missing_candidate_does_not_run_another_si(self):
        with tempfile.TemporaryDirectory(prefix="si-startup-missing-") as directory:
            root = Path(directory)
            binary = root / "launcher"
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            f'-DSI_STARTUP_DIR="{root}"', f'-DSI_STARTUP_LOG="{root}/missing/log"',
                            f'-DSI_STARTUP_PID="{root}/missing/pid"',
                            str(SI / "si_startup.c"), "-o", str(binary)], check=True)
            run = subprocess.run([str(binary)], text=True, capture_output=True, timeout=10)
            self.assertEqual(run.returncode, 127)
            self.assertIn("exec candidate", run.stderr)


if __name__ == "__main__":
    unittest.main()
