#!/usr/bin/env python3
"""Read-only MU1316 native evidence capture. Does not execute QNX code or contact HU."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess

REPO = Path(__file__).resolve().parents[1]
DEFAULT_FIRMWARE = REPO.parents[1] / "Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted"
MAIN_SHA256 = "bd4c042457d4b3c0b9b5f3bcf8ef1e64a30842435e7bc70b477237acaa5ef6ec"


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", type=Path, default=DEFAULT_FIRMWARE)
    parser.add_argument("--output", type=Path, default=REPO / "output/partial360-preflight/native")
    args = parser.parse_args()
    rabin, r2 = shutil.which("rabin2"), shutil.which("r2")
    require(rabin and r2, "Install radare2 or put existing r2/rabin2 on PATH")
    module = args.firmware / "app/eso/lib/libdm_modMain.so"
    blob = module.read_bytes()
    digest = hashlib.sha256(blob).hexdigest()
    require(digest == MAIN_SHA256, "Native module changed: re-audit offsets before using this preflight")
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {"module": str(module), "sha256": digest, "size": len(blob),
                "scope": "static characterization only; no hardware concurrency/readiness test"}
    for flag, label in (("-Ij", "info"), ("-sj", "symbols")):
        result = subprocess.run([rabin, flag, str(module)], capture_output=True, text=True, check=True)
        data = json.loads(result.stdout)
        (args.output / ("libdm_modMain-" + label + ".json")).write_text(json.dumps(data, indent=2) + "\n")
        if label == "info":
            info = data.get("info", data)
            require((info["arch"], info["bits"], info["endian"], info["baddr"]) == ("arm", 32, "little", 0),
                    "Unexpected ELF architecture/load base")
            manifest["format"] = "QNX ARM32 LE EABI5 shared ELF; static VA base 0"
        else:
            symbols = {s["name"]: s for s in (data if isinstance(data, list) else data["symbols"])}
    table = symbols["_ZN17CCaptureInterface16m_captureConfigsE"]
    require(table["size"] == 7 * 56 and table["paddr"] == table["vaddr"] == 0xa0aa8,
            "Unexpected capture-profile table")
    profiles = [struct.unpack_from("<14i", blob, table["paddr"] + i * 56) for i in range(7)]
    ids = [p[0] for p in profiles]
    require(ids == [17, 27, 27, 47, 48, 49, -125], "Capture profile IDs changed")
    manifest["capture_table"] = {"va": hex(table["vaddr"]), "file_offset": hex(table["paddr"]),
                                 "record_bytes": 56, "displayable_ids": ids}
    config_paths = [
        "ifs2/ifs_coreservices3/etc/eso/production/displaymanager.json",
        "system/etc/eso/production/displaymanager.json",
        "system/etc/eso/presets/displaymanager.json",
    ]
    manifest["configs"] = []
    for relative in config_paths:
        path = args.firmware / relative
        raw = path.read_bytes()
        config = json.loads(re.sub(r"(?m)^\s*#.*$", "", raw.decode()))
        defaults = config["default"]
        manifest["configs"].append({"path": relative, "sha256": hashlib.sha256(raw).hexdigest(),
                                     "rvc": defaults.get("rvc"), "zfas": defaults.get("zfas")})
    production = manifest["configs"][0]
    require(production["zfas"]["source"] == {"x": 0, "y": 0, "w": 1024, "h": 480},
            "IFS TopView source default changed")
    require(production["rvc"]["source"] == {"x": 0, "y": 48, "w": 702, "h": 384},
            "IFS RVC source default changed")
    selections = {
        "cropping-disassembly.txt": [
            "_ZN17CCaptureInterface11setCroppingERKi2T4IiES3_",
            "_ZN21CDSIDisplaymanagement14evtSetCroppingEP12CSetCropping"],
        "context-capture-disassembly.txt": [
            "_ZN15CContextManager20preContextSwitchHookERKN4util9SharedPtrINS_14SContextSwitchENS0_20SharedPtrDefaultBaseEEE",
            "_ZN15CContextManager21postContextSwitchHookERKN4util9SharedPtrINS_14SContextSwitchENS0_20SharedPtrDefaultBaseEEE",
            "_ZN17CCaptureInterface18isDisplayableReadyERKi",
            "_ZN17CCaptureInterface14startCapturingERKiNS_14ECaptureOriginE"],
        "component-ownership-disassembly.txt": [
            "_ZN21CDSIDisplaymanagement17evtStartComponentEP18CDSIStartComponent",
            "_ZN17CCaptureInterface13stopCapturingENS_14ECaptureOriginE"],
        "rvc-context-disassembly.txt": ["_ZN17CCaptureInterface16evtSetRVCEnabledEP20CCapSetRVCEnabledEvt"],
    }
    manifest["functions"] = []
    for filename, names in selections.items():
        commands = []
        for name in names:
            symbol = symbols[name]
            manifest["functions"].append({"name": name, "va": hex(symbol["vaddr"]),
                                           "file_offset": hex(symbol["paddr"]), "size": symbol["size"]})
            commands.extend(["s " + hex(symbol["vaddr"]), "af", "pdf"])
        result = subprocess.run([r2, "-q", "-e", "scr.color=0", "-e", "bin.relocs.apply=true",
                                 "-e", "bin.demangle=true", "-c", "; ".join(commands), str(module)],
                                capture_output=True, text=True, check=True)
        require("CCaptureInterface" in result.stdout, "Missing native disassembly: " + filename)
        (args.output / filename).write_text(result.stdout)
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("PASS MU1316 fingerprint, ARM32 layout, capture table and distinct RVC/TopView config defaults")
    print("Capture-profile displayables:", ids, "(59 is not in this table; this does not prove coexistence)")
    print("Native evidence:", args.output / "manifest.json")


if __name__ == "__main__":
    main()
