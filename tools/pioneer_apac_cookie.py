#!/usr/bin/env python3
"""Inspect the APAC magic-cookie envelope used by Pioneer AirPlay 480.7.4.

The donor accepts a direct ``dapa`` atom or an atom chain containing
``frma(apac)``, optional ``apac`` metadata and ``dapa``.  The APAC decoder is
configured with the payload beginning twelve bytes into the ``dapa`` atom.
Raw GlobalConfig payloads are also accepted here as an analysis convenience;
that raw form is not claimed to be a valid on-wire Pioneer cookie.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


SAMPLE_RATES = (
    96000,
    88200,
    64000,
    48000,
    44100,
    32000,
    24000,
    22050,
    16000,
    12000,
    11025,
    8000,
    7350,
)


def locate_dapa_payload(data: bytes) -> tuple[int, int, list[dict[str, object]]]:
    """Reproduce APACDecoder_GetCookieOffset's accepted atom grammar."""

    if len(data) >= 4 and data[:2] == b"\x08\x00":
        return 0, len(data), [{"type": "raw", "offset": 0, "size": len(data)}]

    offset = 0
    frma_seen = False
    atoms: list[dict[str, object]] = []
    while offset + 8 <= len(data):
        size = int.from_bytes(data[offset : offset + 4], "big")
        atom_type_raw = data[offset + 4 : offset + 8]
        atom_type = atom_type_raw.decode("ascii", errors="replace")
        if size < 8:
            raise ValueError(f"atom {atom_type!r} at 0x{offset:x} has size {size}")
        end = offset + size
        if end > len(data):
            raise ValueError(
                f"atom {atom_type!r} at 0x{offset:x} ends at 0x{end:x}, "
                f"past cookie size 0x{len(data):x}"
            )
        atoms.append({"type": atom_type, "offset": offset, "size": size})

        if atom_type_raw == b"frma":
            if frma_seen or size <= 11 or data[offset + 8 : offset + 12] != b"apac":
                raise ValueError("frma must occur once and contain the apac FourCC")
            frma_seen = True
        elif atom_type_raw == b"apac":
            if size <= 12 or data[offset + 8 : offset + 12] != b"\x00\x00\x00\x00":
                raise ValueError("apac atom must be larger than 12 bytes with a zero word at +8")
        elif atom_type_raw == b"dapa":
            if size <= 12:
                raise ValueError("dapa atom must contain a payload after its 12-byte header")
            return offset + 12, size - 12, atoms
        elif not frma_seen:
            raise ValueError(
                f"unknown atom {atom_type!r} is only accepted after a valid frma(apac)"
            )
        offset = end

    raise ValueError("no valid dapa atom was found")


def inspect_global_config(payload: bytes) -> dict[str, object]:
    if len(payload) < 6:
        raise ValueError("APAC dapa payload is shorter than sync + GlobalConfig word")
    sync = int.from_bytes(payload[:2], "big")
    # Configure consumes the 0x0800 sync word first.  The exported metadata
    # helpers are then called at cookie_offset + 2, i.e. on GlobalConfig.
    word = int.from_bytes(payload[2:6], "big")
    sample_rate_index = (word >> 15) & 0x3F
    channels = (word >> 1) & 0xFF
    frame_length = 1024 if word & 0x7E00 == 0 else 0
    return {
        "sync_word": f"0x{sync:04x}",
        "sync_valid": sync == 0x0800,
        "global_config_offset": 2,
        "config_word": f"0x{word:08x}",
        "sample_rate_index": sample_rate_index,
        "sample_rate": (
            SAMPLE_RATES[sample_rate_index]
            if sample_rate_index < len(SAMPLE_RATES)
            else None
        ),
        "frame_length": frame_length,
        "channels": channels,
        "payload_size": len(payload),
        "payload_sha256": hashlib.sha256(payload).hexdigest(),
    }


def inspect_cookie(data: bytes) -> dict[str, object]:
    offset, length, atoms = locate_dapa_payload(data)
    payload = data[offset : offset + length]
    return {
        "cookie_size": len(data),
        "payload_offset": offset,
        "payload_size": length,
        "atoms": atoms,
        "global_config": inspect_global_config(payload),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("cookie", type=Path, help="APAC magic-cookie CFData or raw dapa payload")
    parser.add_argument(
        "--extract",
        type=Path,
        help="write the located dapa decoder payload, including its 0x0800 sync word",
    )
    args = parser.parse_args()

    try:
        data = args.cookie.read_bytes()
        result = inspect_cookie(data)
        if args.extract:
            offset = int(result["payload_offset"])
            length = int(result["payload_size"])
            args.extract.write_bytes(data[offset : offset + length])
    except (OSError, ValueError) as exc:
        parser.error(str(exc))

    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
