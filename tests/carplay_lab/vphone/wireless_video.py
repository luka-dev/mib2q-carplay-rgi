"""Verify fresh native wireless AVC; bootstrap alone is never video evidence."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from tests.carplay_lab.common.extract_native_video import extract


def native_session(report: dict) -> dict:
    messages = [entry.get("message", "")
                for row in report.get("guest_logs", []) if row.get("process") == "airplayd"
                for entry in row.get("reply", {}).get("result", {}).get("entries", [])]
    starts = report.get("start_sessions", [])
    return {
        "bootstrap": all(report.get(key) is True for key in (
            "native_authentication_verified", "native_identification_verified",
            "native_wifi_response_sent", "daemon_start_session_sent")),
        "wireless_available": any(row.get("wireless_available") is True
                                  for row in report.get("availability", [])),
        "one_wireless_start": len(starts) == 1 and
            starts[0].get("wireless_attributes_present") is True and
            bool(starts[0].get("pairing_id")) and starts[0].get("port") == 5000,
        "native_wifi_eligible": any("[Session/WiFi/USB]" in m and "WiFi eligible: Y" in m for m in messages),
        "native_pair_verify": any("PairVerify client done" in m for m in messages),
        "native_endpoint_connected": any("Endpoint connected state changed: isConnected = true" in m for m in messages),
    }


def verify(report: dict, trace: bytes, directory: Path, stem: str) -> dict:
    """Caller supplies only bytes appended during this run, never an old capture."""
    checks = native_session(report)
    result = {"verified": False, "checks": checks,
              "trace_sha256": hashlib.sha256(trace).hexdigest()}
    try:
        if len(trace) > 67108864:
            raise ValueError("current-run QNX trace exceeds 64 MiB")
        if report.get("error") or report.get("observation_error") or not all(checks.values()):
            raise ValueError("current run lacks the complete native wireless session evidence")
        primary = decode_stream(trace, directory, stem, "[native-video-qemu] ", (800, 480))
        result.update(primary)
        result["primary_verified"] = primary["verified"]
        if report.get("paired_altscreen") is True:
            result["altscreen"] = decode_stream(trace, directory, stem + "-cluster",
                                                "[native-cluster-qemu] ", (1440, 542))
            result["verified"] = primary["verified"] and result["altscreen"]["verified"]
            if not result["verified"]:
                result["error"] = "both primary and AltScreen must independently decode"
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        result["error"] = str(error)
    return result


def decode_stream(trace: bytes, directory: Path, stem: str, marker: str, dimensions: tuple[int, int]) -> dict:
    result = {"verified": False}
    try:
        data, records = extract(trace.decode(errors="replace"), marker)
        if not shutil.which("ffprobe") or not shutil.which("ffmpeg"):
            raise ValueError("ffprobe and ffmpeg are required for independent video verification")
        path = directory / (stem + ".h264")
        path.write_bytes(data)
        result.update(records=records, bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), output=str(path))
        probe = subprocess.run(["ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0",
            "-show_entries", "stream=codec_name,profile,width,height,pix_fmt,nb_read_frames",
            "-of", "json", str(path)], capture_output=True, text=True, timeout=30, check=True)
        info = json.loads(probe.stdout)
        (directory / (stem + "-ffprobe.json")).write_text(json.dumps(info, indent=2) + "\n")
        streams = info.get("streams", [])
        if len(streams) != 1:
            raise ValueError("expected one native video stream")
        stream = streams[0]
        if stream.get("codec_name") != "h264" or (stream.get("width"), stream.get("height")) != dimensions or int(stream.get("nb_read_frames", 0)) < 1:
            raise ValueError(f"no decoded native {dimensions[0]}x{dimensions[1]} H.264 frames")
        decode = subprocess.run(["ffmpeg", "-hide_banner", "-v", "error", "-xerror", "-err_detect", "explode",
            "-i", str(path), "-f", "null", "-"], capture_output=True, text=True, timeout=30)
        (directory / (stem + "-decode.log")).write_text(decode.stderr)
        if decode.returncode or decode.stderr.strip():
            raise ValueError("independent H.264 decoder reported an error")
        image = directory / (stem + ".png")
        subprocess.run(["ffmpeg", "-hide_banner", "-v", "error", "-i", str(path), "-vf",
            f"select=eq(n\\,{int(stream['nb_read_frames'])-1})", "-frames:v", "1", "-update", "1", "-y", str(image)],
            capture_output=True, text=True, timeout=30, check=True)
        if not image.is_file():
            raise ValueError("decoder did not produce the final frame")
        result.update(verified=True, stream=stream, image=str(image))
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        result["error"] = str(error)
    return result
