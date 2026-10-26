#!/usr/bin/env python3
"""Read-only compatibility check for user-supplied MHI2Q firmware files."""

from __future__ import annotations

import argparse
import hashlib
import json
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROFILE_DIR = ROOT / "firmware/profiles"

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


def check_file(label: str, path: Path | None, expected: dict[str, object] | None) -> dict[str, object]:
    if path is None:
        return {"name": label, "status": "not-supplied"}
    if not path.is_file():
        return {"name": label, "status": "missing", "path": str(path)}
    actual = digest(path)
    if expected is None:
        actual.update(name=label, status="not-profiled")
        return actual
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
    return {"required_classes": "present" if not missing else "missing", "missing_classes": missing}


def load_profiles(explicit: Path | None, profile_dir: Path) -> list[tuple[Path, dict[str, object]]]:
    paths = [explicit] if explicit else sorted(profile_dir.glob("*.json"))
    if not paths:
        raise SystemExit("No firmware profiles found")
    return [(path, json.loads(path.read_text(encoding="utf-8"))) for path in paths]


def evaluate(profile_path: Path, profile: dict[str, object], supplied: dict[str, Path | None], jar_check: dict[str, object]) -> dict[str, object]:
    expected_files = profile.get("files", {})
    checks = [check_file(name, supplied[name], expected_files.get(name)) for name in supplied]
    bad = [item for item in checks if item["status"] in ("missing", "different-firmware", "not-profiled")]
    exact_count = sum(item["status"] == "validated" for item in checks)
    supplied_count = sum(item["status"] != "not-supplied" for item in checks)
    if jar_check["required_classes"] in ("missing", "invalid-jar"):
        result = "unsupported"
    elif bad:
        result = "native-verification-required" if profile.get("installable") is False and exact_count else "port-required"
    elif not supplied_count:
        result = "incomplete"
    elif profile.get("installable") is False:
        result = "java-port-ready"
    elif exact_count == supplied_count and supplied_count == len(supplied):
        result = "validated-profile"
    elif exact_count == supplied_count:
        result = "java-build-compatible"
    else:
        result = "incomplete"
    return {
        "profile_id": profile["id"],
        "profile": profile["label"],
        "profile_status": profile.get("status", "unknown"),
        "profile_path": str(profile_path),
        "result": result,
        "score": exact_count,
        "checks": checks,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description="Identify supplied files against repository firmware profiles. Nothing is modified.")
    ap.add_argument("--gal", type=Path)
    ap.add_argument("--receiver", type=Path, help="path to libautoreceiver.so")
    ap.add_argument("--lsd-jxe", type=Path)
    ap.add_argument("--stock-jar", type=Path, help="user-derived lsd.jar used for API checks")
    ap.add_argument("--compile-jar", type=Path, help="user-derived compile jar with InnerClasses restored")
    ap.add_argument("--profile", type=Path, help="compare only this profile; default is automatic profile detection")
    ap.add_argument("--profiles-dir", type=Path, default=PROFILE_DIR, help=argparse.SUPPRESS)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    supplied = {
        "gal": args.gal,
        "libautoreceiver.so": args.receiver,
        "lsd.jxe": args.lsd_jxe,
        "stock_jar": args.stock_jar,
        "compile_jar": args.compile_jar,
    }
    jar_check = inspect_jar(args.stock_jar)
    candidates = [evaluate(path, profile, supplied, jar_check) for path, profile in load_profiles(args.profile, args.profiles_dir)]
    rank = {"validated-profile": 5, "java-build-compatible": 4, "java-port-ready": 4, "native-verification-required": 3,
            "incomplete": 2, "port-required": 1, "unsupported": 0}
    candidates.sort(key=lambda item: (rank[item["result"]], item["score"]), reverse=True)
    selected = candidates[0]
    report = {
        **{key: value for key, value in selected.items() if key != "score"},
        "jar": jar_check,
        "other_profiles": [
            {"profile_id": item["profile_id"], "profile": item["profile"], "result": item["result"], "score": item["score"]}
            for item in candidates[1:]
        ],
        "installer_changes": [
            "adds files under /mnt/app/root/sq5_android_auto",
            "adds the selected profile JAR under /mnt/app/eso/hmi/lsd/jars",
            "edits only children.gal.exec/path in smartphone_integrator.json",
            "keeps verified backups and restores them on uninstall",
            "does not replace gal, libautoreceiver.so, lsd.jxe, or gal.json",
        ],
    }

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"Profile: {selected['profile']} ({selected['profile_status']})")
        for item in selected["checks"]:
            print(f"  {item['name']:<20} {item['status']}")
        print(f"  required Java APIs   {jar_check['required_classes']}")
        for name in jar_check.get("missing_classes", []):
            print(f"    missing: {name}")
        print(f"Result: {selected['result']}")
        if selected["result"] == "native-verification-required":
            print("The Java profile matches, but gal/libautoreceiver.so are not fully fingerprinted.")
            print("The installer must remain blocked until the native ABI and exact hashes are verified.")
        elif selected["result"] == "java-port-ready":
            print("The supplied Java inputs match this profile; native installation is still disabled.")
        elif selected["result"] == "java-build-compatible":
            print("The supplied inputs match this profile, but a full native install check was not requested.")
        elif selected["result"] == "port-required":
            print("The files are from another firmware. Port and validate its native and Java seams first.")
        elif selected["result"] == "validated-profile":
            print("All supplied files match this repository profile.")
        elif selected["result"] == "incomplete":
            print("Supply firmware files or derived JARs for a useful check.")

    return 0 if selected["result"] in ("validated-profile", "java-build-compatible", "java-port-ready") else 2 if selected["result"] == "incomplete" else 3


if __name__ == "__main__":
    raise SystemExit(main())
