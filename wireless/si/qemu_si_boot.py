#!/usr/bin/env python3
"""Boot the SI launcher through real QNX Service Manager in its own VM.

Uses an unchanged SI/adapter/launcher review payload. --legacy-wrapper allows
an explicit saved script as an A/B input. Fault injection makes only optional
log/PID files unopenable. This does not simulate the vehicle's USB hardware.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import sys
import zlib

import qemu_stage1 as q


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--payload-dir", type=Path, required=True)
    parser.add_argument("--workdir", type=Path, required=True)
    parser.add_argument("--legacy-wrapper", type=Path)
    parser.add_argument("--missing-diagnostics", action="store_true")
    parser.add_argument("--timeout", type=int, default=100)
    args = parser.parse_args()
    payload_dir = args.payload_dir.resolve()
    manifest = json.loads((payload_dir / "manifest.json").read_text())
    if digest(q.REPO / "wireless/si/si_startup.c") != manifest["source_sha256"]:
        raise ValueError("launcher source changed; rebuild the review payload")
    if digest(payload_dir / "si_startup_config.h") != manifest["config_sha256"]:
        raise ValueError("launcher configuration changed")
    stage2 = manifest["abi"] == 2
    for name, record in manifest["files"].items():
        if digest(payload_dir / name) != record["sha256"]:
            raise ValueError(f"review payload changed: {name}")
    q.WORK = args.workdir.resolve()
    q.WORK.relative_to(q.REPO / "build")
    q.WORK.mkdir(parents=True, exist_ok=True)
    (q.WORK / "boot-receipt.json").unlink(missing_ok=True)
    q.build_guest_and_ifs(candidate_override=str(payload_dir / "smartphone_integrator"),
                         service_manager=True, stage2=stage2)
    work = q.WORK
    source = "/src/" + str(work.relative_to(q.REPO))
    payloads = json.loads((work / "payloads.json").read_text())
    if payloads["adapter"]["sha256"] != manifest["files"]["libsi_adapter.so"]["sha256"]:
        raise ValueError("base fixture adapter differs from review payload")
    build = (work / "si_stage1.build").read_text()
    directory = manifest["installed_directory"]
    # The payload remains in QNX4. Map only its fixed production directory.
    build += (f'\n[type=dir] {Path(directory).parent.as_posix()}\n'
              f'[type=link] {directory}=/ramdisk\n'
              '[type=link] /bin/sh=/proc/boot/sh\n')
    if args.legacy_wrapper:
        if stage2:
            parser.error("legacy wrapper A/B uses ABI 1")
        wrapper = args.legacy_wrapper.read_bytes()
        if args.missing_diagnostics:
            # Explicit path fault; every other byte is the saved HU wrapper.
            if wrapper.count(b"/tmp/si-stage1.log") != 1:
                raise ValueError("legacy wrapper log path changed")
            wrapper = wrapper.replace(b"/tmp/si-stage1.log", b"/missing/si-stage1.log")
        (work / "si_wrapper.sh").write_bytes(wrapper)
        log_path = "/tmp/si-stage1.log"
    else:
        launcher_path = payload_dir / "si_startup"
        if args.missing_diagnostics:
            # QNX shared-memory namespace creation can bypass procmgr aliases;
            # use an explicit unavailable directory instead of relying on one.
            launcher_path = work / "si_startup_no_diagnostics"
            q.docker('cd /src && arm-unknown-nto-qnx6.5.0eabi-gcc '
                     '-std=c99 -Os -Wall -Wextra -Werror '
                     f'-include {shlex.quote("/src/" + str((payload_dir / "si_startup_config.h").relative_to(q.REPO)))} '
                     + shlex.quote('-DSI_STARTUP_LOG="/missing/si-startup.log"') + ' '
                     + shlex.quote('-DSI_STARTUP_PID="/missing/si-stage1.pid"') + ' '
                     + 'wireless/si/si_startup.c -o ' + shlex.quote(source + '/si_startup_no_diagnostics'))
        launcher = launcher_path.read_bytes()
        packed = zlib.compress(launcher, 9)
        (work / "startup.zlib").write_bytes(packed)
        payloads["startup"] = {"size": len(launcher), "crc32": f"{zlib.crc32(launcher):08x}",
                               "sha256": hashlib.sha256(launcher).hexdigest(),
                               "packed_sha256": hashlib.sha256(packed).hexdigest()}
        record = payloads["startup"]
        build += f'\n/etc/startup.zlib={source}/startup.zlib\n'
        line = (f'    /proc/boot/si_payload /etc/startup.zlib /ramdisk/si_startup '
                f'{record["size"]} {record["crc32"]}\n')
        build = build.replace('    display_msg "--- mprotect-on-text probe ---"',
                              line + '    display_msg "--- mprotect-on-text probe ---"')
        framework = work / "framework.json"
        text = framework.read_text()
        assert text.count('"exec":"/proc/boot/si_wrapper.sh"') == 1
        framework.write_text(text.replace('"exec":"/proc/boot/si_wrapper.sh"',
                                         '"exec":"/ramdisk/si_startup"'))
        log_path = "/tmp/si-startup.log"
    if not args.missing_diagnostics:
        build = build.replace('    display_msg "=== SLOG DUMP ==="',
                              f'    /proc/boot/cat {log_path}\n    display_msg "=== SLOG DUMP ==="')
    (work / "si_stage1.build").write_text(build)
    (work / "payloads.json").write_text(json.dumps(payloads, indent=2) + "\n")
    q.docker(f'cd {shlex.quote(source)} && '
             'export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH && '
             'mkifs si_stage1.build si_stage1.bin')
    q.verify_payload_delivery(source, payloads)
    log = q.run_qemu(args.timeout)
    result = q.evaluate(log, stage2=stage2, no_adapter=False)
    expected_block = bool(args.legacy_wrapper and args.missing_diagnostics)
    # A failed arbitrary profile must never be accepted as fault reproduction.
    redirection_error = any(word in log for word in
                            ("cannot create", "Is a directory", "cannot open", "can't create"))
    ok = result == 0 if not expected_block else (
        result != 0 and "CLIENT: STATUS published=1" not in log and redirection_error)
    receipt = {
        "status": "PASS" if ok else "FAIL", "native_acceptance": "PASS" if result == 0 else "FAIL",
        "abi": manifest["abi"], "legacy_wrapper": bool(args.legacy_wrapper),
        "legacy_wrapper_sha256": digest(args.legacy_wrapper) if args.legacy_wrapper else None,
        "missing_diagnostics": args.missing_diagnostics, "expected_startup_block": expected_block,
        "diagnostic_path_fault_injected": args.missing_diagnostics,
        "production_launcher_bytes": not args.legacy_wrapper and not args.missing_diagnostics,
        "payloads": payloads, "ifs_sha256": digest(work / "si_stage1.bin"),
        "log_sha256": digest(work / "qemu_stage1.log"),
        "sources": {name: digest(q.REPO / "wireless/si" / name) for name in
                    ("si_startup.c", "qemu_si_boot.py", "qemu_stage1.py")},
        "not_proven": ["HU failure cause", "vehicle USB connect/disconnect", "HMI/phone media"],
    }
    (work / "boot-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(f'SI boot regression: {receipt["status"]}; native startup {receipt["native_acceptance"]}')
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
