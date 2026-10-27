#!/usr/bin/env python3
"""Offline AltScreen checks with atomic, run-scoped evidence. Never deploys."""

from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
SUITES = {
    "command_policy": ["tests/altscreen_command_policy_test.c"],
    "command_worker": ["tests/altscreen_command_worker_test.c"],
    "command_reversal": ["tests/altscreen_command_reversal_test.c"],
    "command_age": ["tests/altscreen_command_age_test.c"],
    "command_refresh": ["tests/altscreen_command_refresh_test.c"],
    "wire": ["tests/altscreen_wire_test.c", "altscreen_render/frame_io.c"],
    "control_wire": ["tests/altscreen_control_wire_test.c"],
    "policy": ["tests/altscreen_policy_test.c"],
    "io_batch": ["tests/altscreen_io_batch_test.c"],
    "recovery": ["tests/altscreen_recovery_test.c"],
    "health_wire": ["tests/altscreen_health_wire_test.c"],
    "negotiation": ["tests/altscreen_negotiation_test.c"],
    "idle_clock": ["tests/altscreen_idle_clock_test.c"],
    "control": ["tests/altscreen_control_test.c", "altscreen_render/frame_io.c"],
    "deadline": ["tests/altscreen_deadline_test.c"],
    "async": ["tests/altscreen_async_test.c", "altscreen_render/frame_io.c"],
    "forward": ["tests/altscreen_av_test.c"],
    "queue": ["tests/altscreen_queue_policy_test.c", "altscreen_render/frame_io.c"],
    "worker": ["tests/altscreen_worker_test.c", "altscreen_render/frame_io.c"],
    "readiness": ["tests/altscreen_readiness_test.c", "altscreen_render/frame_io.c"],
    "sink": ["tests/altscreen_sink_test.c"],
    "mailbox": ["tests/altscreen_mailbox_test.c"],
    "timing": ["tests/altscreen_timing_test.c"],
    "pace": ["tests/altscreen_pace_trace_test.c"],
    "present": ["tests/altscreen_present_test.c"],
}
TSAN_SUITES = [
    "command_worker",
    "command_reversal",
    "command_age",
    "command_refresh",
    "async",
    "control",
    "worker",
    "negotiation",
    "idle_clock",
    "mailbox",
]
LINUX_SUITES = ["command_worker", "command_reversal", "command_age", "command_refresh", "idle_clock", "negotiation", "async", "control"]
ANALYSIS = [
    "hook/altscreen/altscreen_hook.c",
    "hook/altscreen/altscreen_av.c",
    "altscreen_render/main.c",
    "altscreen_render/frame_io.c",
    "altscreen_render/sink_omx_qnx.c",
    "altscreen_render/screen_egl_present.c",
]
FUZZ = {
    "command": "tests/altscreen_command_fuzz.c",
    "parser": "tests/altscreen_parser_fuzz.c",
    "control": "tests/altscreen_control_fuzz.c",
}
CHECKS = [
    "asan_ubsan",
    "report_contract",
    "hostcheck",
    "static_analysis",
    "fuzz",
    "tsan",
    "linux",
    "qnx",
    "regression",
    "hardware",
]


def source_files():
    paths = set(
        ANALYSIS + ["hook/altscreen/altscreen_hook.c", "altscreen_render/altctl.c"]
    )
    for directory in ["common", "hook/altscreen", "altscreen_render"]:
        paths.update(str(p.relative_to(ROOT)) for p in (ROOT / directory).glob("*.h"))
    paths.update(str(p.relative_to(ROOT)) for p in (ROOT / "tests").glob("altscreen_*"))
    paths.update(
        [
            "scripts/altscreen_validation.py",
            "scripts/test_altscreen_pacing.sh",
            "scripts/compare_altscreen_static.py",
            "scripts/run_tests.sh",
            "tests/test_altscreen_validation_report.py",
        ]
    )
    return sorted(p for p in paths if (ROOT / p).is_file())


def hashes(paths, missing_ok=False):
    result = {}
    for p in paths:
        try:
            result[str(p)] = hashlib.sha256((ROOT / p).read_bytes()).hexdigest()
        except OSError:
            if not missing_ok:
                raise
            result[str(p)] = None
    return result


class RunReport:
    def __init__(self, out, selected, source_paths):
        self.out = out
        self.path = out / "summary.json"
        self.sources = source_paths
        self.data = {
            "schema_version": 1,
            "status": "running",
            "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "scope": "offline source, synthetic inputs and hardware fakes; no HU operations",
            "selected_suites": selected,
            "source_sha256": hashes(source_paths, missing_ok=True),
            "checks": {k: {"status": "not_run"} for k in CHECKS},
            "artifacts_sha256": {},
        }
        if any(value is None for value in self.data["source_sha256"].values()):
            self.data["status"] = "failed"
            self.data["error"] = "Initial source snapshot is unavailable"
        self.save()  # Replace any stale PASS before probing tools or compiling.

    def save(self):
        temporary = self.path.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(self.data, indent=2) + "\n")
        os.replace(temporary, self.path)

    def begin(self, name):
        self.data["checks"][name] = {"status": "running"}
        self.data["current_check"] = name
        self.save()

    def passed(self, name, **details):
        self.data["checks"][name] = {"status": "passed", **details}
        self.save()

    def finish(self, status, error=None):
        changed = hashes(self.sources, missing_ok=True) != self.data["source_sha256"]
        if status == "passed" and changed:
            status, error = (
                "failed",
                "Source files changed during validation; rerun required",
            )
        if error:

            def fail_running(value):
                if isinstance(value, dict):
                    if value.get("status") == "running":
                        value["status"] = status
                    for child in value.values():
                        fail_running(child)

            fail_running(self.data["checks"])
            self.data["error"] = str(error)
            current = self.data.get("current_check")
            if current and self.data["checks"][current]["status"] == "running":
                self.data["checks"][current]["status"] = status
        self.data["status"] = status
        self.data["source_changed_during_run"] = changed
        self.data["finished_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        self.data.pop("current_check", None)
        self.save()
        return status


def command(args, log, timeout=300, env=None):
    with log.open("a") as stream:
        subprocess.run(
            [str(x) for x in args],
            cwd=ROOT,
            stdout=stream,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=True,
            env=env,
        )


def compiler_flags(sanitizer):
    return [
        "-std=gnu99",
        "-O1",
        "-g",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-unused-but-set-variable",
        "-Ihook",
        "-Icommon",
        "-fno-omit-frame-pointer",
        f"-fsanitize={sanitizer}",
    ]


def suite_flags(name):
    if name in ("command_worker", "command_reversal", "command_age", "command_refresh"):
        strip = (
            "-Wl,-dead_strip" if platform.system() == "Darwin" else "-Wl,--gc-sections"
        )
        return [
            "-ffunction-sections",
            "-fdata-sections",
            "-Wno-unused-parameter",
            strip,
        ]
    if name in ["sink", "mailbox"]:
        return ["-Itoolchain/qnx65-abi/include/OMX"]
    if name == "present":
        strip = (
            "-Wl,-dead_strip" if platform.system() == "Darwin" else "-Wl,--gc-sections"
        )
        return [
            "-ffunction-sections",
            "-fdata-sections",
            "-Ialtscreen_render/hostcheck",
            strip,
        ]
    return []


def run_suite(name, out, compiler, sanitizer):
    log, exe = out / f"{name}.log", out / name
    log.write_text("")
    command(
        compiler
        + compiler_flags(sanitizer)
        + suite_flags(name)
        + SUITES[name]
        + (["-ldl"] if name in ("command_worker", "command_reversal", "command_age", "command_refresh") else [])
        + ["-lpthread", "-lm", "-o", str(exe)],
        log,
    )
    command([exe], log, timeout=30)
    print(f"{name} {sanitizer} PASS", flush=True)
    return {
        "status": "passed",
        "log": str(log),
        "binary_sha256": hashes([exe])[str(exe)],
    }


def env_flag(name):
    return os.environ.get(name, "0") == "1"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--suites", nargs="+", choices=SUITES)
    ap.add_argument("--skip-analysis", action="store_true")
    ap.add_argument("--skip-fuzz", action="store_true")
    for name in ["tsan", "linux", "qnx", "regression"]:
        ap.add_argument(
            "--" + name,
            action="store_true",
            default=env_flag("ALTSCREEN_" + name.upper()),
        )
    ap.add_argument(
        "--skip-report-contract", action="store_true", help=argparse.SUPPRESS
    )
    args = ap.parse_args(argv)
    out = Path(
        os.environ.get("ALTSCREEN_TEST_OUT", ROOT / "build/altscreen-pacing-validation")
    ).resolve()
    out.mkdir(parents=True, exist_ok=True)
    selected = args.suites or list(SUITES)
    report = RunReport(out, selected, source_files())
    try:
        if report.data["status"] == "failed":
            raise RuntimeError(report.data["error"])
        cc = shlex.split(os.environ.get("CC", "clang"))
        fuzz_cc = shlex.split(os.environ.get("FUZZ_CC", ""))
        if not fuzz_cc:
            homebrew = Path("/opt/homebrew/opt/llvm/bin/clang")
            fuzz_cc = (
                [str(homebrew)]
                if platform.system() == "Darwin" and homebrew.exists()
                else cc
            )
        report.begin("asan_ubsan")
        # Tool identity is itself run-scoped; failures cannot retain old results.
        versions = {}
        for name, compiler in [("cc", cc)] + (
            [] if args.skip_fuzz else [("fuzz_cc", fuzz_cc)]
        ):
            versions[name] = subprocess.check_output(
                compiler + ["--version"], cwd=ROOT, text=True, timeout=15
            ).splitlines()[0]
        report.data["compilers"] = versions
        suites = {name: {"status": "not_run"} for name in SUITES}
        for name in selected:
            suites[name] = {"status": "running", "log": str(out / f"{name}.log")}
            report.data["checks"]["asan_ubsan"]["suites"] = suites
            report.save()
            suites[name] = run_suite(name, out, cc, "address,undefined")
            report.data["checks"]["asan_ubsan"]["suites"] = suites
            report.save()
        report.passed("asan_ubsan", suites=suites, count=len(selected))
        report.data["host_sanitized_scenarios"] = len(selected)

        if not args.skip_report_contract:
            report.begin("report_contract")
            log = out / "report_contract.log"
            log.write_text("")
            command(
                [
                    sys.executable,
                    "-m",
                    "unittest",
                    "discover",
                    "-s",
                    "tests",
                    "-p",
                    "test_altscreen_validation_report.py",
                ],
                log,
                timeout=60,
            )
            count = re.search(r"Ran (\d+) tests?", log.read_text())
            if not count or int(count[1]) == 0:
                raise RuntimeError("No validation-report contract tests ran")
            report.passed("report_contract", log=str(log), count=int(count[1]))

        if not args.skip_analysis:
            report.begin("hostcheck")
            log = out / "hostcheck.log"
            log.write_text("")
            command(["make", "-C", "altscreen_render", "hostcheck"], log)
            report.passed("hostcheck", log=str(log))
            report.begin("static_analysis")
            logs = {}
            for source in ANALYSIS:
                flags = [
                    "--analyze",
                    "-std=gnu99",
                    "-Ihook",
                    "-Icommon",
                    "-DENABLE_LOGGING=0",
                ]
                if source.endswith(("sink_omx_qnx.c", "screen_egl_present.c")):
                    flags += [
                        "-D__QNX__",
                        "-DPLATFORM_QNX",
                        "-Ialtscreen_render/hostcheck",
                        "-Itoolchain/qnx65-abi/include/OMX",
                    ]
                if source == "hook/altscreen/altscreen_hook.c":
                    flags[flags.index("-DENABLE_LOGGING=0")] = "-DENABLE_LOGGING=1"
                if source == "hook/altscreen/altscreen_av.c":
                    flags += [
                        "-Xanalyzer",
                        "-analyzer-disable-checker",
                        "-Xanalyzer",
                        "unix.BlockInCriticalSection",
                    ]
                log = out / (Path(source).name + ".analysis.log")
                log.write_text("")
                command(
                    cc + flags + [source, "-Xanalyzer", "-analyzer-output=text"], log
                )
                if re.search(r"warning:|error:", log.read_text()):
                    raise RuntimeError(f"Static analyzer diagnostics: {log}")
                logs[source] = str(log)
            report.passed(
                "static_analysis",
                logs=logs,
                exception="Only forwarder unix.BlockInCriticalSection excluded: nonblocking I/O is fault-tested",
            )
            print("Clang static analysis and QNX API hostcheck PASS", flush=True)

        if not args.skip_fuzz:
            report.begin("fuzz")
            campaigns = {}
            seconds = int(os.environ.get("FUZZ_SECONDS", "30"))
            if seconds < 1:
                raise ValueError("FUZZ_SECONDS must be positive")
            for name, source in FUZZ.items():
                corpus = out / f"{name}-corpus"
                corpus.mkdir(exist_ok=True)
                artifacts = out / f"{name}-artifacts"
                artifacts.mkdir(exist_ok=True)
                seeds = {
                    "hello": bytes.fromhex("4156324800000003"),
                    "ack": bytes.fromhex("415632410000000700000001"),
                    "status": bytes.fromhex("415632530000000700000001"),
                    "idr": bytes.fromhex("000000026544"),
                    "config": bytes.fromhex("0164001fffe1000267110100026822"),
                    "boundary": b"\xff" * 40,
                    "expired-selection": bytes([0, 0, 1, 0, 2, 0, 255, 0, 1]),
                }
                for seed, data in seeds.items():
                    (corpus / seed).write_bytes(data)
                exe, log = out / f"{name}_fuzz", out / f"{name}_fuzz.log"
                log.write_text("")
                command(
                    fuzz_cc
                    + compiler_flags("fuzzer,address,undefined")
                    + [source, "-lpthread", "-lm", "-o", str(exe)],
                    log,
                )
                command(
                    [
                        exe,
                        corpus,
                        "-max_len=2048",
                        f"-max_total_time={seconds}",
                        "-timeout=5",
                        "-rss_limit_mb=512",
                        f"-artifact_prefix={artifacts}/",
                    ],
                    log,
                    timeout=seconds + 30,
                )
                done = re.search(r"Done (\d+) runs in (\d+) second", log.read_text())
                if not done:
                    raise RuntimeError(f"Fuzz completion missing: {log}")
                campaigns[name] = {
                    "status": "passed",
                    "runs": int(done[1]),
                    "seconds": int(done[2]),
                    "log": str(log),
                    "binary_sha256": hashes([exe])[str(exe)],
                }
                report.data["checks"]["fuzz"]["campaigns"] = campaigns
                report.save()
                print(f"{name} fuzz PASS: {done[1]} inputs", flush=True)
            report.passed(
                "fuzz",
                campaigns=campaigns,
                total_runs=sum(x["runs"] for x in campaigns.values()),
            )

        if args.tsan:
            report.begin("tsan")
            directory = out / "tsan"
            directory.mkdir(exist_ok=True)
            suites = {name: {"status": "not_run"} for name in TSAN_SUITES}
            for name in TSAN_SUITES:
                suites[name] = {
                    "status": "running",
                    "log": str(directory / f"{name}.log"),
                }
                report.data["checks"]["tsan"]["suites"] = suites
                report.save()
                suites[name] = run_suite(name, directory, cc, "thread")
                report.save()
            report.passed("tsan", suites=suites)
        if args.linux:
            report.begin("linux")
            directory = out / "linux"
            directory.mkdir(exist_ok=True)
            suites = {}
            for name in LINUX_SUITES:
                exe = directory / name
                log = directory / f"{name}.log"
                log.write_text("")
                suites[name] = {"status": "running", "log": str(log)}
                report.data["checks"]["linux"]["suites"] = suites
                report.save()
                build = (
                    ["gcc"]
                    + compiler_flags("address,undefined")
                    + (
                        [
                            "-ffunction-sections",
                            "-fdata-sections",
                            "-Wno-unused-parameter",
                            "-Wl,--gc-sections",
                        ]
                        if name in ("command_worker", "command_reversal", "command_age", "command_refresh")
                        else []
                    )
                    + SUITES[name]
                    + (["-ldl"] if name in ("command_worker", "command_reversal", "command_age", "command_refresh") else [])
                    + ["-lpthread", "-lm", "-o", str(exe)]
                )
                # The toolchain image defaults PATH to the ARM assembler; native
                # Linux tests must use the image's host assembler instead.
                script = (
                    "set -e\nexport PATH=/usr/bin:/bin\ncd /src\n"
                    + shlex.join(build)
                    + "\n"
                    + shlex.quote(str(exe))
                )
                # Host absolute output path is mapped through /src in Docker.
                script = script.replace(str(ROOT), "/src")
                command(
                    [
                        "docker",
                        "run",
                        "--rm",
                        "--platform=linux/amd64",
                        "-v",
                        f"{ROOT}:/src",
                        "qnx65-armv7-toolchain:latest",
                        "bash",
                        "-c",
                        script,
                    ],
                    log,
                )
                suites[name] = {
                    "status": "passed",
                    "log": str(log),
                    "binary_sha256": hashes([exe])[str(exe)],
                }
            report.passed("linux", suites=suites)
        if args.qnx:
            report.begin("qnx")
            directory = out / "qnx"
            directory.mkdir(exist_ok=True)
            hook_log = directory / "hook.log"
            hook_log.write_text("")
            relative = (
                str(directory.relative_to(ROOT))
                if directory.is_relative_to(ROOT)
                else None
            )
            if relative is None:
                raise ValueError("--qnx output must be inside the repository workspace")
            command(
                [
                    "bash",
                    "-c",
                    'source scripts/build_hook_common.sh\nOUTDIR_REL="$1" carplay_build_hook',
                    "build-hook",
                    relative,
                ],
                hook_log,
                timeout=300,
            )
            render_log = directory / "renderers.log"
            render_log.write_text("")
            command(["bash", "scripts/build_renderers.sh"], render_log)
            artifacts = [
                directory / "libcarplay_hook.so",
                directory / "libcarplay_apac.so",
                ROOT / "build/altscreen_render",
            ]
            report.data["artifacts_sha256"].update(hashes(artifacts))
            report.passed(
                "qnx",
                logs=[str(hook_log), str(render_log)],
                gates="ARM, 26 exact hook exports, no emutls, compiler-only constructors",
            )
        if args.regression:
            report.begin("regression")
            log = out / "regression.log"
            log.write_text("")
            command(["bash", "scripts/run_tests.sh"], log, timeout=900)
            skipped = [
                line.strip() for line in log.read_text().splitlines() if "SKIP" in line
            ]
            report.passed("regression", log=str(log), skipped=skipped)
        status = report.finish("passed")
        if status != "passed":
            raise RuntimeError(report.data["error"])
        print(f"AltScreen offline validation PASS ({report.path})", flush=True)
        return 0
    except KeyboardInterrupt:
        report.finish("interrupted", "Interrupted")
        return 130
    except Exception as exc:
        report.finish("failed", exc)
        print(f"AltScreen validation FAILED: {exc} ({report.path})", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
