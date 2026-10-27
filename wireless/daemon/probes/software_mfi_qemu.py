"""Prepare public software-MFi vectors and check actual QNX phone verification."""
from __future__ import annotations

from hashlib import sha256
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]
AUDI = ROOT.parents[1]
DAEMON = ROOT / "wireless/daemon"
FW_APP = AUDI / ("Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/"
                 "advanced/MU1316-appimg/armle")
GUEST_FIXTURE = "/proc/boot/software_mfi.fixture"


def prepare(work: Path, manifest: Path) -> dict:
    """Generate a fresh identity on the host; copy only public vectors to IFS."""
    target = ROOT / "build/software-mfi-qemu-host-target"
    subprocess.run(["cargo", "build", "--offline", "--manifest-path",
                    str(DAEMON / "Cargo.toml"), "--target-dir", str(target),
                    "--features", "software-mfi-test", "--bin", "software-mfi-fixture"],
                   check=True, capture_output=True, text=True, timeout=120)
    fixture = work / "software_mfi.fixture"
    generated = subprocess.run([str(target / "debug/software-mfi-fixture"), str(fixture)],
                               check=True, capture_output=True, text=True, timeout=30)
    data = fixture.read_bytes()
    assets = {"software_mfi.fixture": fixture,
              "libcrypto.so.2": FW_APP / "usr/lib/libcrypto.so.2"}
    source = manifest.read_text()
    # Lifecycle supervisor passes the preload only to its ordinary daemon.
    preloads = re.findall(r"^    (?:LD_PRELOAD|MHI2Q_CP_DAEMON_PRELOAD)=\S*/libmock_mfi_qemu\.so\n",
                          source, re.MULTILINE)
    if len(preloads) != 1:
        raise RuntimeError("software MFi requires a unique QEMU daemon preload")
    # The combined lifecycle starts the independent media client before the
    # supervisor. Set the public-fixture environment before either child starts,
    # so phones spawned by that client inherit the same identity as the daemon.
    console = "    reopen /dev/ser1\n"
    if source.count(console) != 1:
        raise RuntimeError("cannot locate early QEMU fixture environment")
    source = source.replace(console, console + f"    MHI2Q_MFI_QEMU_FIXTURE={GUEST_FIXTURE}\n", 1)
    entries = []
    for name, path in assets.items():
        if path != work / name:
            shutil.copyfile(path, work / name)
        destination = f"/proc/boot/{name}="
        source = "\n".join(line for line in source.splitlines()
                           if not line.startswith(destination)) + "\n"
        entries.append(f"/proc/boot/{name}=/work/{name}")
    # Keep assets before the long factory plugin tail in the QEMU IFS.
    lines = source.splitlines()
    insertion = next((i for i, line in enumerate(lines)
                      if line.startswith("/proc/boot/ifconfig=")), None)
    if insertion is None:
        raise RuntimeError("cannot find QEMU boot binary asset section")
    lines.insert(insertion, "[perms=+r,+x]\n" + "\n".join(entries))
    manifest.write_text("\n".join(lines) + "\n")
    result = {"fixture_sha256": sha256(data).hexdigest(),
              "certificate_bytes": int.from_bytes(data[8:10], "big"),
              "challenge_cases": int.from_bytes(data[10:12], "big"),
              "private_key_exported": False,
              "factory_libcrypto_sha256": sha256(assets["libcrypto.so.2"].read_bytes()).hexdigest()}
    print(generated.stdout.strip(), flush=True)
    return result


def validate(output: str, log: Path, fixture: dict, *, sessions: int = 1,
             tampered: bool = False) -> None:
    checks = {"synthetic_certificate_seen": "[phone-qemu] software certificate verified=1" in output,
              "combined_i2c": "combined SENDRECV command=0xc0140507" in output,
              "rsa_signature_read": "[mfi-qemu] software signature read=128" in output,
              "no_dummy_signature": "[mfi-qemu] signature read=20" not in output}
    if tampered:
        checks["actual_signature_rejected"] = "[phone-qemu] software signature rejected=1" in output
        checks["aa04_closes_bridge"] = "[phone-qemu] software AA04 closed bridge=1" in output
        checks["session_gates_closed"] = not any(marker in output for marker in (
            "[phone-qemu] wifi=1", "[phone-qemu] CarPlayStartSession=1",
            "[phone-qemu] receiver START=1", "[phone-qemu] full daemon path=1"))
    else:
        for case in range(fixture["challenge_cases"]):
            marker = f"[phone-qemu] software RSA signature verified case={case} bytes=128"
            checks[f"verified_case_{case}"] = output.count(marker) == sessions
        checks["verifier_negative_controls"] = output.count(
            "[phone-qemu] software tamper checks rejected=1") == sessions * fixture["challenge_cases"]
        checks["session_completed"] = output.count("[phone-qemu] full daemon path=1") == sessions
    passed = all(checks.values())
    summary = dict(fixture, status="passed" if passed else "failed", checks=checks,
                   sessions=sessions, tampered=tampered, hardware_tested=False, log=str(log))
    log.with_suffix(".json").write_text(json.dumps(summary, indent=2) + "\n")
    if not passed:
        raise RuntimeError(f"software MFi QEMU failed: {checks}; log: {log}")
    print(f"Software MFi actual QNX RSA verification passed; {log.with_suffix('.json')}", flush=True)
