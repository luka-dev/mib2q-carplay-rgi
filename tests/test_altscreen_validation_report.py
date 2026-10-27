import json
import importlib.util
import os
import shlex
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
RUNNER = ROOT / "scripts/altscreen_validation.py"


class ReportContract(unittest.TestCase):
    def test_initial_snapshot_failure_replaces_stale_pass(self):
        spec = importlib.util.spec_from_file_location("alt_validation", RUNNER)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out / "summary.json").write_text('{"status":"passed","stale":true}')
            module.RunReport(out, [], [out / "missing.c"])
            saved = json.loads((out / "summary.json").read_text())
            self.assertEqual(saved["status"], "failed")
            self.assertNotIn("stale", saved)
            self.assertEqual(saved["checks"]["asan_ubsan"]["status"], "not_run")

    def test_malformed_compiler_configuration_finishes_failed_not_running(self):
        with tempfile.TemporaryDirectory() as directory:
            result, report = self.invoke(Path(directory), CC='"')
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(report["status"], "failed")
            self.assertEqual(report["checks"]["asan_ubsan"]["status"], "not_run")

    def test_removed_source_invalidates_success_and_still_finishes_report(self):
        spec = importlib.util.spec_from_file_location("alt_validation", RUNNER)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            source = out / "source.c"
            source.write_text("initial source")
            report = module.RunReport(out, [], [source])
            source.unlink()
            self.assertEqual(report.finish("passed"), "failed")
            saved = json.loads((out / "summary.json").read_text())
            self.assertTrue(saved["source_changed_during_run"])
            self.assertEqual(saved["status"], "failed")

    def invoke(self, out, **environment):
        env = dict(os.environ, ALTSCREEN_TEST_OUT=str(out), **environment)
        for key in [
            "ALTSCREEN_TSAN",
            "ALTSCREEN_LINUX",
            "ALTSCREEN_QNX",
            "ALTSCREEN_REGRESSION",
        ]:
            env[key] = "0"
        result = subprocess.run(
            [
                sys.executable,
                str(RUNNER),
                "--suites",
                "wire",
                "--skip-analysis",
                "--skip-fuzz",
                "--skip-report-contract",
            ],
            cwd=ROOT,
            env=env,
            capture_output=True,
            text=True,
            timeout=30,
        )
        return result, json.loads((out / "summary.json").read_text())

    def test_compiler_failure_replaces_stale_pass_and_marks_remaining_checks(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out / "summary.json").write_text('{"status":"passed","stale":true}')
            result, report = self.invoke(out, CC="/definitely/missing/alt-compiler")
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(report["status"], "failed")
            self.assertNotIn("stale", report)
            self.assertEqual(report["checks"]["asan_ubsan"]["status"], "failed")
            self.assertEqual(report["checks"]["fuzz"]["status"], "not_run")
            self.assertEqual(report["checks"]["qnx"]["status"], "not_run")
            self.assertTrue(report["source_sha256"])
            self.assertFalse((out / "summary.json.tmp").exists())

    def test_partial_success_does_not_claim_optional_or_unselected_checks(self):
        with tempfile.TemporaryDirectory() as directory:
            result, report = self.invoke(
                Path(directory),
                CC="clang",
                FUZZ_CC="/definitely/missing/skipped-fuzzer",
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(report["status"], "passed")
            suites = report["checks"]["asan_ubsan"]["suites"]
            self.assertEqual(suites["wire"]["status"], "passed")
            self.assertEqual(suites["async"]["status"], "not_run")
            self.assertEqual(report["host_sanitized_scenarios"], 1)
            for name in [
                "tsan",
                "linux",
                "qnx",
                "regression",
                "hardware",
                "static_analysis",
            ]:
                self.assertEqual(report["checks"][name]["status"], "not_run")

    def test_failed_compile_is_attributed_to_the_attempted_suite(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            compiler = out / "compiler.py"
            compiler.write_text(
                "import sys\nif '--version' in sys.argv:\n print('test compiler')\n sys.exit(0)\nsys.exit(7)\n"
            )
            result, report = self.invoke(
                out, CC=shlex.join([sys.executable, str(compiler)])
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(
                report["checks"]["asan_ubsan"]["suites"]["wire"]["status"], "failed"
            )
            self.assertEqual(
                report["checks"]["asan_ubsan"]["suites"]["async"]["status"], "not_run"
            )


if __name__ == "__main__":
    unittest.main()
