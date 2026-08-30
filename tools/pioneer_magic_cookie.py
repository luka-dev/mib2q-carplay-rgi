#!/usr/bin/env python3
"""Inspect or build the binary-plist body of AirPlay SETMAGICCOOKIE.

Pioneer AirPlay 480.7.4 receives codec configuration after stream-103 SETUP
on the existing RTSP connection.  The request body is a binary property list:

    {"magicCookieID": <signed int64>, "magicCookie": <CFData>}

This host tool validates captures and extracts the exact CFData passed to the
AAC/APAC converter.  It does not inspect the codec-specific cookie payload;
use pioneer_apac_cookie.py on an extracted APAC cookie.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import plistlib
from pathlib import Path


INT64_MIN = -(1 << 63)
INT64_MAX = (1 << 63) - 1


def decode_request(data: bytes) -> tuple[int, bytes, dict[str, object]]:
    try:
        root = plistlib.loads(data)
    except Exception as exc:
        raise ValueError(f"invalid property list: {exc}") from exc
    if not isinstance(root, dict):
        raise ValueError("SETMAGICCOOKIE root is not a dictionary")

    cookie_id = root.get("magicCookieID")
    cookie = root.get("magicCookie")
    if isinstance(cookie_id, bool) or not isinstance(cookie_id, int):
        raise ValueError("magicCookieID is not an integer")
    if not INT64_MIN <= cookie_id <= INT64_MAX:
        raise ValueError("magicCookieID is outside signed int64")
    if not isinstance(cookie, bytes):
        raise ValueError("magicCookie is not binary data")
    if not cookie:
        raise ValueError("magicCookie is empty")

    summary: dict[str, object] = {
        "body_size": len(data),
        "body_sha256": hashlib.sha256(data).hexdigest(),
        "magic_cookie_id": cookie_id,
        "magic_cookie_size": len(cookie),
        "magic_cookie_sha256": hashlib.sha256(cookie).hexdigest(),
        "plist_format": "binary" if data.startswith(b"bplist00") else "non-binary",
        "extra_keys": sorted(str(key) for key in root if key not in {
            "magicCookieID", "magicCookie"
        }),
    }
    return cookie_id, cookie, summary


def inspect_command(args: argparse.Namespace) -> int:
    data = args.body.read_bytes()
    _, cookie, summary = decode_request(data)
    if args.extract:
        if args.extract.exists():
            raise ValueError(f"extract path already exists: {args.extract}")
        args.extract.write_bytes(cookie)
    if args.show_hex:
        summary["magic_cookie_hex"] = cookie.hex()
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


def build_command(args: argparse.Namespace) -> int:
    if not INT64_MIN <= args.cookie_id <= INT64_MAX:
        raise ValueError("magicCookieID is outside signed int64")
    if args.output.exists():
        raise ValueError(f"output already exists: {args.output}")
    cookie = args.cookie.read_bytes()
    if not cookie:
        raise ValueError("magic cookie is empty")
    body = plistlib.dumps(
        {"magicCookieID": args.cookie_id, "magicCookie": cookie},
        fmt=plistlib.FMT_BINARY,
        sort_keys=True,
    )
    args.output.write_bytes(body)
    _, _, summary = decode_request(body)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    inspect_parser = subparsers.add_parser("inspect", help="validate a captured request body")
    inspect_parser.add_argument("body", type=Path, help="binary-plist RTSP request body")
    inspect_parser.add_argument(
        "--extract", type=Path, help="write magicCookie CFData to this new path"
    )
    inspect_parser.add_argument(
        "--show-hex", action="store_true", help="include the complete cookie in JSON"
    )
    inspect_parser.set_defaults(handler=inspect_command)

    build_parser = subparsers.add_parser("build", help="build a deterministic test body")
    build_parser.add_argument("cookie_id", type=int, help="signed int64 magicCookieID")
    build_parser.add_argument("cookie", type=Path, help="raw codec magic-cookie data")
    build_parser.add_argument("output", type=Path, help="new binary-plist output path")
    build_parser.set_defaults(handler=build_command)

    args = parser.parse_args()
    try:
        return int(args.handler(args))
    except (OSError, ValueError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    raise SystemExit(main())
