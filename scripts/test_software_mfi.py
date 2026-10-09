#!/usr/bin/env python3
"""Offline host MFi crypto/control tests and production-isolation checks."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "wireless/daemon/Cargo.toml"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qnx-cross", action="store_true",
                        help="also build the ordinary QNX daemon in the local toolchain image")
    parser.add_argument("--qnx-qemu", action="store_true",
                        help="also run software MFi QNX/QEMU bootstrap and stock210 lifecycle tests")
    parser.add_argument("--out", type=Path, default=ROOT / "build/software-mfi-validation")
    options = parser.parse_args()
    out = options.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    report = {"status": "running", "hardware_tested": False, "checks": []}
    sources = ["wireless/daemon/Cargo.toml", "wireless/daemon/Cargo.lock",
               "wireless/daemon/build.rs", "wireless/daemon/src/lib.rs",
               "wireless/daemon/src/control.rs", "wireless/daemon/src/mfi.rs",
               "wireless/daemon/src/main.rs", "wireless/daemon/src/software_mfi_test.rs",
               "wireless/daemon/tests/software_mfi.rs", "scripts/test_software_mfi.py"]
    sources += ["wireless/daemon/probes/" + name for name in (
        "phone_qemu_probe.rs", "mock_mfi_qemu.c", "software_mfi_fixture.rs",
        "software_mfi_fixture_main.rs", "software_mfi_fixture.h", "software_mfi_phone.rs",
        "software_mfi_verify_qemu.c", "software_mfi_qemu.py", "qemu_daemon_bridge.py")]
    sources += ["tests/software_mfi_fixture_test.c", "wireless/receiver_bridge/qemu_capture.py",
                "wireless/receiver_bridge/stock210_lifecycle_fixture.py"]
    report["source_sha256"] = {
        path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in sources
    }
    env = dict(os.environ, CARGO_TARGET_DIR=str(out / "host-target"))

    def run(name: str, command: list[str], *, expected_failure: str | None = None,
            process_env: dict[str, str] | None = None, timeout: int = 300) -> str:
        print(name, flush=True)
        start = time.monotonic()
        log = out / f"{name}.log"
        with log.open("w") as stream:
            completed = subprocess.run(command, cwd=ROOT, env=process_env or env,
                                       stdout=stream, stderr=subprocess.STDOUT,
                                       timeout=timeout, check=False)
        text = log.read_text()
        passed = (completed.returncode == 0 if expected_failure is None
                  else completed.returncode != 0 and expected_failure in text)
        check = {"name": name, "passed": passed, "log": str(log),
                 "seconds": round(time.monotonic() - start, 3)}
        counts = re.findall(r"test result: ok\. (\d+) passed; (\d+) failed", text)
        if counts:
            check["passed_tests"] = sum(int(passed) for passed, _ in counts)
        report["checks"].append(check)
        if not passed:
            raise RuntimeError(f"{name} failed; see {log}")
        return text

    try:
        for executable in ["cargo", "rustc", "openssl", "clang"]:
            if not shutil.which(executable):
                raise RuntimeError(f"missing host test tool: {executable}")
        report["openssl"] = run("openssl-version", ["openssl", "version"]).strip()
        cargo = ["cargo", "--offline", "--manifest-path", str(MANIFEST)]
        run("default-tests", [cargo[0], "test", *cargo[1:], "--all-targets"])
        run("default-build", [cargo[0], "build", *cargo[1:], "--bin", "mhi2q-wireless-daemon"])
        binary = out / "host-target/debug/mhi2q-wireless-daemon"
        data = binary.read_bytes()
        markers = [b"NOT_A_REAL_MFI_IDENTITY", b"mhi2q-test-only-mfi",
                   b"synthetic MFi OpenSSL", b"mfiq_verify"]
        if any(marker in data for marker in markers):
            raise RuntimeError("synthetic provider marker in default daemon")
        report["checks"].append({"name": "default-binary-isolation", "passed": True,
                                 "sha256": hashlib.sha256(data).hexdigest()})
        run("software-mfi-tests", [cargo[0], "test", *cargo[1:], "--all-targets",
                                   "--features", "software-mfi-test,qemu-probe"])
        run("fixture-sanitizer-build", ["clang", "-std=c11", "-O1", "-g",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-Wall", "-Wextra", "-Werror",
            str(ROOT / "tests/software_mfi_fixture_test.c"), "-o", str(out / "fixture-test")])
        run("fixture-sanitizer-fuzz", [str(out / "fixture-test")])
        guard = out / "target-guard"
        run("compile-target-guard", ["rustc", "--edition=2024",
                                      str(MANIFEST.parent / "build.rs"), "-o", str(guard)])
        diagnostic = "offline host-only synthetic identity provider"
        for target_os, family in [("nto", "unix"), ("windows", "windows")]:
            guard_env = dict(env, CARGO_FEATURE_SOFTWARE_MFI_TEST="1",
                             CARGO_CFG_TARGET_OS=target_os, CARGO_CFG_TARGET_FAMILY=family)
            run(f"reject-{target_os}-test-provider", [str(guard)],
                expected_failure=diagnostic, process_env=guard_env)
        if options.qnx_cross or options.qnx_qemu:
            qnx_build = [
                "docker", "run", "--rm", "--platform=linux/amd64",
                "-v", f"{ROOT.parents[1]}:/src", "-w", "/src/Patches/mhi2-carplay",
                "qnx65-armv7-toolchain:latest", "/opt/rust/qnx-cargo", "build",
                "--manifest-path", "/src/Patches/mhi2-carplay/wireless/daemon/Cargo.toml",
                "--bin", "mhi2q-wireless-daemon"]
            run("qnx-default-build", qnx_build)
            qnx_binary = MANIFEST.parent / "target/armv7-unknown-nto-qnx650/release/mhi2q-wireless-daemon"
            qnx_data = qnx_binary.read_bytes()
            if any(marker in qnx_data for marker in markers):
                raise RuntimeError("synthetic provider marker in QNX daemon")
            report["checks"].append({"name": "qnx-binary-isolation", "passed": True,
                                     "sha256": hashlib.sha256(qnx_data).hexdigest()})
            run("qnx-cargo-reject-test-provider", [*qnx_build, "--features", "software-mfi-test"],
                expected_failure=diagnostic)
        if options.qnx_qemu:
            base = ["python3", str(ROOT / "wireless/daemon/probes/qemu_daemon_bridge.py"),
                    "--phone-probe", "--control-probe"]
            run("qemu-legacy-mock-regression", base)
            for name, flags in [
                ("repeat", ["--repeat-phone-qemu"]),
                ("retry", ["--mfi-retry-qemu"]),
                ("timeout-recovery", ["--repeat-phone-qemu", "--mfi-timeout-recovery-qemu"]),
                ("tamper", ["--software-mfi-tamper-qemu"]),
            ]:
                run(f"qemu-software-mfi-{name}", [*base, "--software-mfi-qemu", *flags])
            run("stock210-software-mfi-lifecycle", [
                "python3", str(ROOT / "wireless/receiver_bridge/qemu_capture.py"),
                "--wireless-hook-qemu", "--wireless-media-qemu", "--wireless-lifecycle-qemu",
                "--mesh", "--start-experiment", "--seed-display", "--skip-hid-qemu",
                "--mdnsd", "--mock-mac-qemu", "--interface-lo0-qemu", "--software-mfi-qemu"],
                timeout=600)
        report["status"] = "passed"
    except (RuntimeError, OSError, subprocess.SubprocessError) as error:
        report["status"] = "failed"
        report["error"] = str(error)
    finally:
        if report["status"] == "passed":
            changed = [path for path, digest in report["source_sha256"].items()
                       if hashlib.sha256((ROOT / path).read_bytes()).hexdigest() != digest]
            if changed:
                report["status"] = "failed"
                report["error"] = f"sources changed during validation; rerun: {changed}"
        summary = out / "summary.json"
        summary.write_text(json.dumps(report, indent=2) + "\n")
        print(f"{report['status']}: {summary}", flush=True)
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
