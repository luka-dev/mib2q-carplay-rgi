#!/usr/bin/env python3
"""Verify exact-match and changed-firmware decisions without shipping firmware."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile


REPO = Path(__file__).resolve().parents[1]
CHECKER = REPO / "scripts/check_firmware.py"


def entry(path):
    data = path.read_bytes()
    return {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def main():
    with tempfile.TemporaryDirectory(prefix="mib2q-aa-firmware-") as td:
        root = Path(td)
        files = {}
        for name in ("gal", "libautoreceiver.so", "lsd.jxe"):
            path = root / name
            path.write_bytes(("test " + name).encode("ascii"))
            files[name] = path
        for key, name in (("stock_jar", "lsd.jar"), ("compile_jar", "lsd_ic.jar")):
            path = root / name
            with zipfile.ZipFile(path, "w") as jar:
                for cls in __import__("runpy").run_path(str(CHECKER))["REQUIRED_CLASSES"]:
                    jar.writestr(cls, b"test-only")
            files[key] = path

        profile = {
            "id": "synthetic_test", "label": "synthetic test profile", "status": "test-only",
            "files": {name: entry(path) for name, path in files.items()},
        }
        profile_path = root / "profile.json"
        profile_path.write_text(json.dumps(profile), encoding="utf-8")
        args = [
            sys.executable, str(CHECKER), "--profile", str(profile_path),
            "--gal", str(files["gal"]), "--receiver", str(files["libautoreceiver.so"]),
            "--lsd-jxe", str(files["lsd.jxe"]), "--stock-jar", str(files["stock_jar"]),
            "--compile-jar", str(files["compile_jar"]), "--json",
        ]
        before = {path: entry(path) for path in files.values()}
        exact = subprocess.run(args, text=True, capture_output=True)
        assert exact.returncode == 0, exact.stdout + exact.stderr
        assert json.loads(exact.stdout)["result"] == "validated-profile"
        assert {path: entry(path) for path in files.values()} == before

        files["gal"].write_bytes(files["gal"].read_bytes() + b"changed")
        changed = subprocess.run(args, text=True, capture_output=True)
        assert changed.returncode == 3, changed.stdout + changed.stderr
        report = json.loads(changed.stdout)
        assert report["result"] == "port-required"
        assert next(item for item in report["checks"] if item["name"] == "gal")["status"] == "different-firmware"

    print("firmware checker tests: PASS")


if __name__ == "__main__":
    main()
