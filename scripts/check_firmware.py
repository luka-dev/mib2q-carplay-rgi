#!/usr/bin/env python3
"""Read-only compatibility check for user-supplied MHI2Q firmware files."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROFILE_PATH = ROOT / "firmware/profiles/mhi2q_us_aug22_p3639_mu0918.json"

REQUIRED_CLASSES = (
    "de/audi/tghu/navi/app/cluster/ClusterService.class",
    "de/audi/tghu/fwhmi/DisplayManagerMIB2High.class",
    "de/esolutions/hmi/widgets/audi/evo/high/widgets/CombiMapController.class",
    "de/audi/app/terminalmode/smartphone/androidauto2/AndroidAuto2ListenerDistributor.class",
    "de/audi/app/terminalmode/smartphone/androidauto2/nav/AndroidAuto2NavHandler.class",
    "de/audi/app/terminalmode/smartphone/androidauto2/AndroidAuto2KeyEventsController.class",
    "de/audi/app/combi/bap/app/audio/AppConnectorTerminalMode.class",
    "de/audi/app/terminalmode/ExternalEventsListener.class",
    "de/audi/app/earlyfunc/evo/parking/ParkingSystemControllerComponentEvo.class",
    "de/audi/app/earlyfunc/core/parking/ParkingPartialPopupHandler.class",
    "de/audi/audio/context/AudioDrawerContextImpl.class",
    "de/esolutions/hmi/widgets/audi/evo/high/PartialPopupManagerEvoHigh.class",
)


def digest(path: Path) -> dict[str, object]:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for block in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(block)
    return {"path": str(path), "size": path.stat().st_size, "sha256": h.hexdigest()}


def check_file(label: str, path: Path | None, expected: dict[str, object]) -> dict[str, object]:
    if path is None:
        return {"name": label, "status": "not-supplied"}
    if not path.is_file():
        return {"name": label, "status": "missing", "path": str(path)}
    actual = digest(path)
    exact = actual["size"] == expected["size"] and actual["sha256"] == expected["sha256"]
    actual.update(name=label, status="validated" if exact else "different-firmware")
    return actual


def inspect_jar(path: Path | None) -> dict[str, object]:
    if path is None or not path.is_file():
        return {"required_classes": "not-checked", "missing_classes": []}
    try:
        with zipfile.ZipFile(path) as jar:
            names = set(jar.namelist())
    except (OSError, zipfile.BadZipFile) as exc:
        return {"required_classes": "invalid-jar", "error": str(exc), "missing_classes": []}
    missing = [name for name in REQUIRED_CLASSES if name not in names]
    return {
        "required_classes": "present" if not missing else "missing",
        "missing_classes": missing,
    }


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Compare private firmware inputs with the on-car validated MU0918 profile. Nothing is modified."
    )
    ap.add_argument("--gal", type=Path)
    ap.add_argument("--receiver", type=Path, help="path to libautoreceiver.so")
    ap.add_argument("--lsd-jxe", type=Path)
    ap.add_argument("--stock-jar", type=Path, help="user-derived lsd.jar used for API checks")
    ap.add_argument("--compile-jar", type=Path, help="user-derived compile jar with InnerClasses restored")
    ap.add_argument(
        "--profile", type=Path, default=DEFAULT_PROFILE_PATH,
        help="validated profile JSON to compare against (default: MU0918 profile)",
    )
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    profile = json.loads(args.profile.read_text(encoding="utf-8"))
    supplied = {
        "gal": args.gal,
        "libautoreceiver.so": args.receiver,
        "lsd.jxe": args.lsd_jxe,
        "stock_jar": args.stock_jar,
        "compile_jar": args.compile_jar,
    }
    checks = [check_file(name, supplied[name], profile["files"][name]) for name in supplied]
    jar_check = inspect_jar(args.stock_jar)
    bad = [c for c in checks if c["status"] in ("missing", "different-firmware")]
    supplied_count = sum(c["status"] != "not-supplied" for c in checks)
    exact_count = sum(c["status"] == "validated" for c in checks)

    if jar_check["required_classes"] in ("missing", "invalid-jar"):
        result = "unsupported"
    elif bad:
        result = "port-required"
    elif supplied_count and exact_count == supplied_count:
        result = "validated-profile"
    else:
        result = "incomplete"

    report = {
        "profile": profile["label"],
        "result": result,
        "checks": checks,
        "jar": jar_check,
        "installer_changes": [
            "adds files under /mnt/app/root/sq5_android_auto",
            "adds /mnt/app/eso/hmi/lsd/jars/mib2q_android_auto_cluster.jar",
            "edits only children.gal.exec/path in smartphone_integrator.json",
            "keeps verified backups and restores them on uninstall",
            "does not replace gal, libautoreceiver.so, lsd.jxe, or gal.json",
        ],
    }

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"Profile: {profile['label']}")
        for item in checks:
            print(f"  {item['name']:<20} {item['status']}")
        print(f"  required Java APIs   {jar_check['required_classes']}")
        for name in jar_check.get("missing_classes", []):
            print(f"    missing: {name}")
        print(f"Result: {result}")
        if result == "port-required":
            print("The files are from another firmware. Do not install this native hook yet;")
            print("port its ABI addresses and regenerate/review the three firmware Java seams first.")
        elif result == "validated-profile":
            print("All supplied files match the configuration tested on the SQ5.")
        elif result == "incomplete":
            print("Supply gal, libautoreceiver.so, lsd.jxe and the two user-derived jars for a full check.")

    return 0 if result == "validated-profile" else 2 if result == "incomplete" else 3


if __name__ == "__main__":
    raise SystemExit(main())
