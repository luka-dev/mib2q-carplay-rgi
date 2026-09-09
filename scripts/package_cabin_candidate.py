#!/usr/bin/env python3
"""Create a private, review-only candidate bundle. Never contacts the vehicle."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
V11_SHA = "ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reports", type=Path, nargs="+", required=True)
    parser.add_argument("--screen", type=Path, required=True)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--profile", choices=["float", "quiet-bass", "cabin"], default="float")
    args = parser.parse_args()
    model = args.model.resolve()
    payload = model.read_bytes()
    screen = json.loads(args.screen.read_text())
    if not screen.get("offline_screen_passed") or screen.get("model_sha256") != digest(model):
        raise RuntimeError("candidate has no matching passing offline screen")
    expected_build = {
        "hook": digest(ROOT / "build/libcarplay_hook.so"),
        "apac": digest(ROOT / "build/libcarplay_apac.so"),
        "renderer": digest(args.renderer),
    }
    if (screen.get("build_sha256") != expected_build or
            screen.get("profile") != args.profile or
            screen.get("renderer_sha256") != expected_build["renderer"]):
        raise RuntimeError("screen is stale or does not bind this renderer, ARM libraries and profile")
    provided_reports = {str(p.resolve()): digest(p) for p in args.reports}
    if not screen.get("report_sha256") or any(
            provided_reports.get(str(Path(p).resolve())) != sha
            for p, sha in screen["report_sha256"].items()):
        raise RuntimeError("screen's evidence report is missing or changed")
    if len(payload) != 469760:
        raise RuntimeError("wrong model size")
    header = struct.unpack_from("<16I", payload)
    if (header[:10] != (0x52535348, 0x314F454E, 1, 64, 368, 160, 264, 449280, 5104, 469696)
            or any(header[11:]) or header[10] != zlib.crc32(payload[64:])):
        raise RuntimeError("invalid model header/CRC")
    rollback_model = ROOT / "build/hssr-selection-v11/hssr_model.bin"
    if digest(rollback_model) != V11_SHA:
        raise RuntimeError("V11 rollback hash mismatch")
    startup = ROOT / "deploy/smartphone_integrator/carplay_startup.sh"
    startup_text = startup.read_text()
    marker = "export AUDIO_STEREO_UPMIX=on\n"
    if startup_text.count(marker) != 1 or "export AUDIO_FLOAT_RENDER=" in startup_text:
        raise RuntimeError("startup template changed; review required")
    sources = {
        "hooks/libcarplay_hook.so": ROOT / "build/libcarplay_hook.so",
        "hooks/libcarplay_apac.so": ROOT / "build/libcarplay_apac.so",
        "hooks/hssr_model.bin": model,
        "rollback/hssr_model.bin": rollback_model,
        "rollback/libcarplay_hook.so": ROOT / "build/rollback-before-v12/libcarplay_hook.so",
        "rollback/libcarplay_apac.so": ROOT / "build/rollback-before-v12/libcarplay_apac.so",
        "rollback/carplay_startup.sh": startup,
        "reports/offline-screen.json": args.screen.resolve(),
    }
    for index, report in enumerate(args.reports):
        sources[f"reports/{index:02d}-{report.name}"] = report.resolve()
    for source in sources.values():
        if not source.is_file():
            raise RuntimeError(f"missing input: {source}")
        if source.suffix == ".so":
            elf = source.read_bytes()[:20]
            if elf[:6] != b"\x7fELF\x01\x01" or struct.unpack_from("<H", elf, 18)[0] != 40:
                raise RuntimeError(f"not a little-endian ARM32 library: {source}")
    args.output.mkdir(parents=True, exist_ok=False)
    for name, source in sources.items():
        target = args.output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    candidate_startup = args.output / "carplay_startup.sh"
    profile_flag = (f"export AUDIO_QUIET_LFE={int(args.profile == 'quiet-bass')}\n"
                    f"export AUDIO_CABIN_RENDER={int(args.profile == 'cabin')}\n")
    candidate_startup.write_text(startup_text.replace(marker,
        marker + "# Experimental linked float/matrix guard, independent of Sound Check enable.\n"
        "export AUDIO_FLOAT_RENDER=1\n" + profile_flag))
    candidate_startup.chmod(startup.stat().st_mode)
    manifest = {
        "kind": "private-cabin-upmix-candidate", "installed": False,
        "vehicle_approved": False, "model_sha256": digest(model),
        "profile": args.profile, "build_sha256": expected_build,
        "separation_quality_approved": False,
        "offline_screen_passed": True,
        "screen_kind": screen.get("kind", "trained-model-non-regression"),
        "new_trained_model_selected": screen.get("new_trained_model_selected", True),
        "operating_assumptions": {"3D": "MMI OFF, not physical DSP bypass",
            "focus": "All (not live confirmed)", "surround": "minimum, numeric value unconfirmed"},
        "limitations": ["Offline candidate; listening and live deadline/level checks required.",
            "Known input matrix only; unknown complex DSP/EQ/acoustics are not emulated.",
            "19 drivers / 16 circuits; rear-door woofer and tweeter are not independent channels.",
            "Sample-peak guard, not a true-peak limiter.",
            "MUSDB-derived/private development model; no commercial clearance implied."],
        "sha256": {str(path.relative_to(args.output)): digest(path)
                   for path in sorted(args.output.rglob("*")) if path.is_file()},
    }
    (args.output / "README.txt").write_text(
        "LOCAL EXPERIMENTAL BUNDLE - NOT INSTALLED\n"
        "Review manifest.json and reports before any vehicle transfer.\n"
        "hooks/: candidate ARM/QNX libraries and selected model.\n"
        "carplay_startup.sh: existing startup with AUDIO_FLOAT_RENDER=1 added.\n"
        "AUDIO_SOUNDCHECK_LIMITER=off does NOT disable the independent float guard.\n"
        "rollback/: previous local libraries, V11 and original startup template;\n"
        "these are local build backups, not a verified snapshot of the vehicle.\n"
        "Selected rendering profile is recorded in manifest.json; no quality promotion implied.\n"
        "Legacy render: set AUDIO_CABIN_RENDER=0, AUDIO_QUIET_LFE=0, AUDIO_FLOAT_RENDER=0.\n"
        "Exact stereo widening: AUDIO_STEREO_UPMIX=off (normal Sound Check still applies).\n"
        "The existing SSH deployment script intentionally remains pinned to V11.\n"
        "No automatic installer is included.\n")
    manifest["sha256"]["README.txt"] = digest(args.output / "README.txt")
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(args.output.resolve())


if __name__ == "__main__":
    main()
