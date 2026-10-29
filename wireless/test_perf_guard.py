"""Cancellation must not leave our profiler running outside the global lock."""
import os
from pathlib import Path
import subprocess
import sys
import unittest


class GuardTests(unittest.TestCase):
    def test_term_stops_owned_child_group(self):
        child = 'import os,time; print("OWNED_CHILD="+str(os.getpid()),flush=True); time.sleep(30)'
        process = subprocess.Popen(
            [sys.executable, '-m', 'wireless.perf_guard', '--', sys.executable, '-c', child],
            cwd=Path(__file__).resolve().parents[1], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            pid = None
            for line in process.stdout:
                if line.startswith('OWNED_CHILD='):
                    pid = int(line.split('=', 1)[1]);break
            self.assertIsNotNone(pid)
            process.terminate()
            process.communicate(timeout=8)
            with self.assertRaises(ProcessLookupError):
                os.killpg(pid, 0)
        finally:
            if process.poll() is None:
                process.kill();process.communicate(timeout=5)


if __name__ == '__main__':
    unittest.main()
