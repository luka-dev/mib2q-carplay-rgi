#!/usr/bin/env python3
"""Decrypt CarPlay type-103 APAP frames using Pioneer's exact contract.

The input is the hook capture format: consecutive frames prefixed by a
big-endian 32-bit length which includes the four-byte length field.  The
32-byte ``shk`` value from the type-103 SETUP is the ChaCha20-Poly1305 key.

Pioneer SPHERA 1.030010 uses an eight-byte nonce and stores the protected
body as::

    ciphertext || poly1305_tag[16] || nonce[8]

Audio packets authenticate APAP fixed-header bytes 4..11.  Extension/state
packets authenticate bytes 0..11.  This tool tries both official variants
and reports the one whose tag verifies.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

from Crypto.Cipher import ChaCha20_Poly1305


MAX_FRAME_SIZE = 1024 * 1024
LENGTH_PREFIX_SIZE = 4
APAP_FIXED_HEADER_SIZE = 15
SECURITY_OVERHEAD = 16 + 8

APAP_EXTENSION_NAMES = {
    2: "receiver_flag_unknown",
    4: "trim_start_media_time",
    5: "trim_end_media_time",
    7: "soundcheck_five_word_records",
    8: "audio_format_index",
    11: "soundcheck_three_words",
    12: "soundcheck_media_kind",
    17: "magic_cookie_id",
}


def parse_fixed_header(packet: bytes) -> dict[str, int]:
    if len(packet) < APAP_FIXED_HEADER_SIZE:
        raise ValueError("APAP packet is shorter than its 15-byte fixed header")
    return {
        # APAPDecodeFixedHeader passes these bit patterns to AirPlayMediaTimeMake,
        # whose value/timescale parameters are signed int64/int32.
        "media_time_value": int.from_bytes(packet[0:8], "big", signed=True),
        "media_time_scale": int.from_bytes(packet[8:12], "big", signed=True),
        "sequence_24": int.from_bytes(packet[12:15], "big"),
    }


def read_uleb128(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    shift = 0
    for _ in range(10):
        if offset >= len(data):
            raise ValueError("truncated ULEB128")
        byte = data[offset]
        offset += 1
        value |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return value, offset
        shift += 7
    raise ValueError("ULEB128 is longer than 10 bytes")


def decode_extension_value(field_id: int, payload: bytes) -> dict[str, object]:
    decoded: dict[str, object] = {"payload_hex": payload.hex()}
    if field_id == 8:
        if len(payload) not in (2, 4, 8):
            raise ValueError(
                f"audio_format_index field must be 2, 4, or 8 bytes, got {len(payload)}"
            )
        decoded["value"] = int.from_bytes(payload, "big")
    elif field_id == 12:
        if len(payload) != 1:
            raise ValueError(
                f"soundcheck_media_kind field must be 1 byte, got {len(payload)}"
            )
        decoded["value"] = int.from_bytes(payload, "big")
    elif field_id == 17:
        if len(payload) > 8:
            raise ValueError(
                f"magic_cookie_id field must be 0..8 bytes, got {len(payload)}"
            )
        decoded["value"] = int.from_bytes(payload, "big")
    elif field_id == 2:
        # The donor stores field 2, but its buffered consumer never reads it.
        # Preserve it without inventing a semantic interpretation.
        decoded["value"] = int.from_bytes(payload, "big")
    elif field_id in (4, 5):
        if len(payload) != 12:
            raise ValueError(
                f"trim media-time field {field_id} must be 12 bytes, got {len(payload)}"
            )
        decoded["media_time"] = {
            "value": int.from_bytes(payload[0:8], "big", signed=True),
            "scale": int.from_bytes(payload[8:12], "big", signed=True),
        }
    elif field_id == 7:
        if len(payload) % 20:
            raise ValueError(
                f"soundcheck record field must be a multiple of 20 bytes, got {len(payload)}"
            )
        decoded["records"] = [
            [
                int.from_bytes(payload[pos + word * 4 : pos + word * 4 + 4], "big")
                for word in range(5)
            ]
            for pos in range(0, len(payload), 20)
        ]
    elif field_id == 11:
        if len(payload) != 12:
            raise ValueError(
                f"soundcheck three-word field must be 12 bytes, got {len(payload)}"
            )
        decoded["words"] = [
            int.from_bytes(payload[pos : pos + 4], "big")
            for pos in range(0, 12, 4)
        ]
    return decoded


def parse_extension(plaintext: bytes) -> dict[str, object]:
    """Decode Pioneer's APAP ULEB128 TLV extension body.

    Field 2 is deliberately left named as unknown: the donor recognizes and
    stores it, but its buffered consumer does not read it.
    """

    fields: list[dict[str, object]] = []
    offset = 0
    terminated = False
    while offset < len(plaintext):
        field_id, offset = read_uleb128(plaintext, offset)
        if field_id == 0:
            terminated = True
            break
        length, offset = read_uleb128(plaintext, offset)
        end = offset + length
        if end > len(plaintext):
            raise ValueError(
                f"extension field {field_id} needs {length} byte(s), "
                f"only {len(plaintext) - offset} remain"
            )
        payload = plaintext[offset:end]
        offset = end
        item: dict[str, object] = {
            "id": field_id,
            "name": APAP_EXTENSION_NAMES.get(field_id, "unknown"),
            "length": length,
        }
        item.update(decode_extension_value(field_id, payload))
        fields.append(item)
    return {
        "fields": fields,
        "terminated": terminated,
        "trailing_hex": plaintext[offset:].hex(),
    }


def load_key(args: argparse.Namespace) -> bytes:
    if args.key_hex:
        try:
            key = bytes.fromhex(args.key_hex)
        except ValueError as exc:
            raise ValueError(f"invalid --key-hex value: {exc}") from exc
    else:
        raw = args.key_file.read_bytes()
        stripped = raw.strip()
        try:
            text = stripped.decode("ascii")
            key = bytes.fromhex(text)
        except (UnicodeDecodeError, ValueError):
            key = raw

    if len(key) != 32:
        raise ValueError(f"shk must be exactly 32 bytes, got {len(key)}")
    return key


def iter_frames(data: bytes):
    offset = 0
    index = 0
    while offset < len(data):
        if len(data) - offset < LENGTH_PREFIX_SIZE:
            raise ValueError(f"trailing {len(data) - offset} byte(s) at 0x{offset:x}")
        declared = int.from_bytes(data[offset : offset + 4], "big")
        if declared < LENGTH_PREFIX_SIZE + APAP_FIXED_HEADER_SIZE:
            raise ValueError(f"frame {index}: invalid inclusive length {declared}")
        if declared > MAX_FRAME_SIZE:
            raise ValueError(f"frame {index}: length {declared} exceeds {MAX_FRAME_SIZE}")
        end = offset + declared
        if end > len(data):
            raise ValueError(
                f"frame {index}: needs {declared} bytes at 0x{offset:x}, "
                f"only {len(data) - offset} remain"
            )
        yield index, offset, data[offset + LENGTH_PREFIX_SIZE : end]
        index += 1
        offset = end


def decrypt_variant(key: bytes, packet: bytes, aad: bytes) -> bytes:
    protected = packet[APAP_FIXED_HEADER_SIZE:]
    if len(protected) < SECURITY_OVERHEAD:
        raise ValueError("protected APAP body is shorter than its 24-byte overhead")
    ciphertext = protected[:-SECURITY_OVERHEAD]
    tag = protected[-SECURITY_OVERHEAD:-8]
    nonce = protected[-8:]
    cipher = ChaCha20_Poly1305.new(key=key, nonce=nonce)
    cipher.update(aad)
    return cipher.decrypt_and_verify(ciphertext, tag)


def decrypt_packet(key: bytes, packet: bytes) -> tuple[str, bytes]:
    candidates = (
        ("audio8", packet[4:12]),
        ("extension12", packet[:12]),
    )
    for name, aad in candidates:
        try:
            return name, decrypt_variant(key, packet, aad)
        except ValueError:
            pass
    raise ValueError("Poly1305 verification failed for audio8 and extension12 AAD")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path, help="inclusive-BE32 APAP capture")
    key_group = parser.add_mutually_exclusive_group(required=True)
    key_group.add_argument("--key-hex", help="32-byte SETUP shk as 64 hexadecimal digits")
    key_group.add_argument(
        "--key-file",
        type=Path,
        help="file containing raw 32-byte shk or its hexadecimal form",
    )
    output_group = parser.add_mutually_exclusive_group()
    output_group.add_argument(
        "--output",
        type=Path,
        help="write verified plaintext bodies consecutively to this file",
    )
    output_group.add_argument(
        "--output-dir",
        type=Path,
        help="write one plaintext body per frame plus manifest.tsv",
    )
    parser.add_argument(
        "--keep-going",
        action="store_true",
        help="report bad frames and continue instead of stopping at the first one",
    )
    args = parser.parse_args()

    try:
        key = load_key(args)
        capture = args.capture.read_bytes()
    except (OSError, ValueError) as exc:
        parser.error(str(exc))

    plaintext_parts: list[bytes] = []
    output_records: list[dict[str, object]] = []
    modes: Counter[str] = Counter()
    failures = 0
    total_plaintext = 0
    frame_count = 0

    try:
        for index, offset, packet in iter_frames(capture):
            frame_count += 1
            try:
                mode, plaintext = decrypt_packet(key, packet)
            except ValueError as exc:
                failures += 1
                print(f"frame {index} @ 0x{offset:x}: {exc}", file=sys.stderr)
                if not args.keep_going:
                    return 2
                continue
            modes[mode] += 1
            total_plaintext += len(plaintext)
            if args.output:
                plaintext_parts.append(plaintext)
            elif args.output_dir:
                header = parse_fixed_header(packet)
                extension = None
                extension_error = None
                if mode == "extension12":
                    try:
                        extension = parse_extension(plaintext)
                    except ValueError as exc:
                        extension_error = str(exc)
                output_records.append(
                    {
                        "index": index,
                        "capture_offset": offset,
                        "mode": mode,
                        "header_hex": packet[:APAP_FIXED_HEADER_SIZE].hex(),
                        **header,
                        "plaintext_size": len(plaintext),
                        "plaintext": plaintext,
                        "extension": extension,
                        "extension_error": extension_error,
                    }
                )
    except ValueError as exc:
        print(f"capture parse error: {exc}", file=sys.stderr)
        return 2

    if args.output:
        args.output.write_bytes(b"".join(plaintext_parts))
    elif args.output_dir:
        try:
            args.output_dir.mkdir(parents=True, exist_ok=False)
            manifest = [
                "index\toffset\tmode\tmedia_time_value\tmedia_time_scale\t"
                "sequence_24\theader_hex\tplaintext_size\tfile\textension_json"
            ]
            json_records = []
            for record in output_records:
                index = int(record["index"])
                mode = str(record["mode"])
                plaintext = bytes(record.pop("plaintext"))
                filename = f"{index:06d}_{mode}.bin"
                (args.output_dir / filename).write_bytes(plaintext)
                extension_json = json.dumps(
                    record["extension"], separators=(",", ":"), sort_keys=True
                )
                manifest.append(
                    f"{index}\t0x{int(record['capture_offset']):x}\t{mode}\t"
                    f"{record['media_time_value']}\t{record['media_time_scale']}\t"
                    f"{record['sequence_24']}\t{record['header_hex']}\t"
                    f"{len(plaintext)}\t{filename}\t{extension_json}"
                )
                json_record = dict(record)
                json_record["file"] = filename
                json_records.append(
                    json.dumps(json_record, separators=(",", ":"), sort_keys=True)
                )
            (args.output_dir / "manifest.tsv").write_text(
                "\n".join(manifest) + "\n", encoding="utf-8"
            )
            (args.output_dir / "manifest.jsonl").write_text(
                "\n".join(json_records) + "\n", encoding="utf-8"
            )
        except OSError as exc:
            print(f"output-dir error: {exc}", file=sys.stderr)
            return 2

    mode_text = ", ".join(f"{name}={count}" for name, count in sorted(modes.items()))
    print(
        f"frames={frame_count} verified={sum(modes.values())} failed={failures} "
        f"plaintext_bytes={total_plaintext} modes=[{mode_text}]"
    )
    if args.output:
        print(f"wrote {total_plaintext} bytes to {args.output}")
    elif args.output_dir:
        print(f"wrote {len(output_records)} frame bodies to {args.output_dir}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
