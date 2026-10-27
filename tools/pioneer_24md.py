#!/usr/bin/env python3
"""Inspect and decrypt Pioneer's 24MD update container.

The public RSA modulus can be recovered from two firmware packages signed by
the same RSA-2048 key.  No private key or brute force is involved: for each
known PKCS#1 v1.5 encoded header digest, n divides (signature**e - message).
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import math
import struct
import zlib
from pathlib import Path

try:
    import gmpy2
except ImportError:
    gmpy2 = None


HEADER_SIZE = 0x100
RSA_BLOCK_SIZE = 0x100
KEY_BLOCK_OFFSET = 0x200
PAYLOAD_OFFSET = 0x300
PUBLIC_EXPONENT = 65537
RAW_MAGIC = b"\x5a\xa5\xa5\x5a"
RAW_HEADER_SIZE = 0x200
RAW_ENTRY_SIZE = 0x200

DER_PREFIXES = {
    "md5": bytes.fromhex("3020300c06082a864886f70d020505000410"),
    "sha1": bytes.fromhex("3021300906052b0e03021a05000414"),
    "sha224": bytes.fromhex("302d300d06096086480165030402040500041c"),
    "sha256": bytes.fromhex("3031300d060960864801650304020105000420"),
    "sha384": bytes.fromhex("3041300d060960864801650304020205000430"),
    "sha512": bytes.fromhex("3051300d060960864801650304020305000440"),
}


def exact_power(base: int, exponent: int) -> int:
    if gmpy2 is not None:
        return gmpy2.mpz(base) ** exponent
    return pow(base, exponent)


def greatest_common_divisor(first: int, second: int) -> int:
    if gmpy2 is not None:
        return gmpy2.gcd(first, second)
    return math.gcd(first, second)


def read_container(path: Path) -> bytes:
    data = path.read_bytes()
    if len(data) < PAYLOAD_OFFSET or data[:4] != b"24MD":
        raise ValueError(f"{path}: not a 24MD container")
    header_size = struct.unpack_from("<I", data, 4)[0]
    declared_size = struct.unpack_from("<I", data, 0x0C)[0]
    if header_size != HEADER_SIZE:
        raise ValueError(f"{path}: unsupported header size 0x{header_size:x}")
    if declared_size != len(data):
        raise ValueError(
            f"{path}: declared size 0x{declared_size:x} != actual 0x{len(data):x}"
        )
    return data


def c_string(data: bytes, offset: int, size: int) -> str:
    return data[offset : offset + size].split(b"\0", 1)[0].decode("ascii", "replace")


def print_info(path: Path) -> None:
    data = read_container(path)
    payload_size = len(data) - PAYLOAD_OFFSET
    print(f"path:          {path}")
    print(f"size:          0x{len(data):x} ({len(data)})")
    print(f"model:         {c_string(data, 0x14, 0x20)}")
    print(f"build:         {c_string(data, 0x34, 0x10)}")
    print(f"region:        {c_string(data, 0x44, 0x08)}")
    print(f"version bytes: {data[0x48:0x50].hex(' ')}")
    print(f"payload:       0x{payload_size:x}; AES aligned={payload_size % 16 == 0}")
    print(f"sha256:        {hashlib.sha256(data).hexdigest()}")


def emsa_pkcs1_v15(header: bytes, algorithm: str, encoding: str) -> bytes:
    digest = hashlib.new(algorithm, header).digest()
    if encoding == "digest-info":
        encoded_digest = DER_PREFIXES[algorithm] + digest
    elif encoding == "raw-digest":
        # Pioneer uses the same private-key/type-1 operation both for the
        # header digest and for the raw AES-256 key block.  Some builders may
        # therefore pad the digest directly instead of using RSA_sign's DER
        # DigestInfo wrapper.
        encoded_digest = digest
    else:
        raise ValueError(f"unsupported digest encoding: {encoding}")
    padding_size = RSA_BLOCK_SIZE - len(encoded_digest) - 3
    if padding_size < 8:
        raise ValueError("digest is too large for RSA block")
    return b"\x00\x01" + b"\xff" * padding_size + b"\x00" + encoded_digest


def candidate_modulus(
    first: bytes,
    second: bytes,
    algorithm: str,
    encoding: str,
    first_signature_power: int,
    second_signature_power: int,
) -> int:
    first_message = int.from_bytes(
        emsa_pkcs1_v15(first[:HEADER_SIZE], algorithm, encoding), "big"
    )
    second_message = int.from_bytes(
        emsa_pkcs1_v15(second[:HEADER_SIZE], algorithm, encoding), "big"
    )
    first_multiple = first_signature_power - first_message
    second_multiple = second_signature_power - second_message
    return greatest_common_divisor(first_multiple, second_multiple)


def validate_modulus(
    modulus: int, data: bytes, algorithm: str, encoding: str, byteorder: str
) -> bool:
    signature = int.from_bytes(data[HEADER_SIZE:KEY_BLOCK_OFFSET], byteorder)
    expected = int.from_bytes(
        emsa_pkcs1_v15(data[:HEADER_SIZE], algorithm, encoding), "big"
    )
    return pow(signature, PUBLIC_EXPONENT, modulus) == expected


def normalize_modulus(
    candidate: int,
    first: bytes,
    second: bytes,
    algorithm: str,
    encoding: str,
    byteorder: str,
) -> int | None:
    if candidate <= 1:
        return None
    if candidate.bit_length() == RSA_BLOCK_SIZE * 8:
        tests = (first, second)
        if all(
            validate_modulus(candidate, item, algorithm, encoding, byteorder)
            for item in tests
        ):
            return candidate

    # The GCD is usually n, but can be a small multiple of n.  Strip only
    # small factors and require exact RSA verification before accepting it.
    reduced = candidate
    for factor in range(2, 10000):
        while reduced % factor == 0 and reduced.bit_length() > RSA_BLOCK_SIZE * 8:
            reduced //= factor
        if reduced.bit_length() <= RSA_BLOCK_SIZE * 8:
            break
    if reduced.bit_length() == RSA_BLOCK_SIZE * 8:
        tests = (first, second)
        if all(
            validate_modulus(reduced, item, algorithm, encoding, byteorder)
            for item in tests
        ):
            return reduced
    return None


def recover_modulus(first_path: Path, second_path: Path) -> tuple[int, str, str, str]:
    first = read_container(first_path)
    second = read_container(second_path)
    for byteorder in ("big", "little"):
        first_signature = int.from_bytes(
            first[HEADER_SIZE:KEY_BLOCK_OFFSET], byteorder
        )
        second_signature = int.from_bytes(
            second[HEADER_SIZE:KEY_BLOCK_OFFSET], byteorder
        )
        print(f"calculating RSA powers for {byteorder} byte order ...", flush=True)
        first_signature_power = exact_power(first_signature, PUBLIC_EXPONENT)
        second_signature_power = exact_power(second_signature, PUBLIC_EXPONENT)
        for encoding in ("raw-digest", "digest-info"):
            for algorithm in DER_PREFIXES:
                print(
                    f"trying {algorithm}/{encoding}/{byteorder} ...", flush=True
                )
                candidate = candidate_modulus(
                    first,
                    second,
                    algorithm,
                    encoding,
                    first_signature_power,
                    second_signature_power,
                )
                modulus = normalize_modulus(
                    candidate, first, second, algorithm, encoding, byteorder
                )
                if modulus is not None:
                    return modulus, algorithm, encoding, byteorder
    raise RuntimeError("no shared RSA modulus found for the supported digest schemes")


def rsa_public_decode(block: bytes, modulus: int, byteorder: str) -> bytes:
    value = int.from_bytes(block, byteorder)
    decoded = pow(value, PUBLIC_EXPONENT, modulus)
    return decoded.to_bytes(RSA_BLOCK_SIZE, "big")


def pkcs1_payload(decoded: bytes) -> bytes:
    if not decoded.startswith(b"\x00\x01"):
        raise ValueError(f"unexpected RSA block prefix: {decoded[:16].hex()}")
    separator = decoded.find(b"\x00", 2)
    if separator < 10 or any(value != 0xFF for value in decoded[2:separator]):
        raise ValueError("invalid PKCS#1 v1.5 type-1 padding")
    return decoded[separator + 1 :]


def unwrap_aes_key(data: bytes, modulus: int, byteorder: str) -> tuple[bytes, bytes]:
    decoded = rsa_public_decode(
        data[KEY_BLOCK_OFFSET:PAYLOAD_OFFSET], modulus, byteorder
    )
    payload = pkcs1_payload(decoded)
    if len(payload) != 32:
        raise ValueError(
            f"RSA key payload has {len(payload)} bytes instead of an AES-256 key: "
            f"{payload.hex()}"
        )
    return payload, decoded


def decrypt_container(
    input_path: Path, modulus: int, byteorder: str, output_path: Path
) -> None:
    try:
        from Crypto.Cipher import AES
        from Crypto.Util.Padding import unpad
    except ImportError as error:
        raise RuntimeError("pycryptodome is required for AES decryption") from error

    data = read_container(input_path)
    key, decoded_key_block = unwrap_aes_key(data, modulus, byteorder)
    plaintext = AES.new(key, AES.MODE_CBC, iv=b"\0" * 16).decrypt(data[PAYLOAD_OFFSET:])
    try:
        plaintext = unpad(plaintext, 16)
    except ValueError:
        # Some update builders may AES-align the gzip stream without PKCS#7.
        plaintext = plaintext.rstrip(b"\0")
    if not plaintext.startswith(b"\x1f\x8b"):
        raise ValueError(f"decrypted payload is not gzip: {plaintext[:16].hex()}")
    output_path.write_bytes(plaintext)
    print(f"RSA block:     {decoded_key_block.hex()}")
    print(f"AES-256 key:   {key.hex()}")
    print(f"gzip output:   {output_path} ({len(plaintext)} bytes)")
    print(f"gzip sha256:   {hashlib.sha256(plaintext).hexdigest()}")
    with gzip.open(output_path, "rb") as stream:
        prefix = stream.read(16)
    print(f"raw prefix:    {prefix.hex(' ')}")


def read_raw_layout(path: Path) -> tuple[dict[str, int | str], list[dict[str, int | str]]]:
    file_size = path.stat().st_size
    with path.open("rb") as stream:
        header = stream.read(RAW_HEADER_SIZE)
        if len(header) != RAW_HEADER_SIZE or header[:4] != RAW_MAGIC:
            raise ValueError(f"{path}: not a Pioneer raw update image")
        declared_size = struct.unpack_from("<I", header, 4)[0]
        if declared_size != file_size:
            raise ValueError(
                f"{path}: declared size 0x{declared_size:x} != actual 0x{file_size:x}"
            )
        count = struct.unpack_from("<I", header, 0x78)[0]
        table_size = struct.unpack_from("<I", header, 0x7C)[0]
        if table_size != count * RAW_ENTRY_SIZE:
            raise ValueError(
                f"{path}: table size 0x{table_size:x} does not match {count} entries"
            )
        table = stream.read(table_size)
    data_offset = RAW_HEADER_SIZE + table_size
    entries: list[dict[str, int | str]] = []
    for index in range(count):
        item = table[index * RAW_ENTRY_SIZE : (index + 1) * RAW_ENTRY_SIZE]
        relative_offset = struct.unpack_from("<I", item, 0x74)[0]
        target_offset = struct.unpack_from("<I", item, 0x7C)[0]
        size = struct.unpack_from("<I", item, 0x84)[0]
        header_checksum = struct.unpack_from("<I", item, 0x8C)[0]
        absolute_offset = data_offset + relative_offset
        if absolute_offset + size > file_size:
            raise ValueError(
                f"{path}: entry {index} extends beyond the raw image"
            )
        entries.append(
            {
                "index": index,
                "name": c_string(item, 0, 0x10),
                "filename": c_string(item, 0x14, 0x3C),
                "date": c_string(item, 0x50, 0x20),
                "relative_offset": relative_offset,
                "absolute_offset": absolute_offset,
                "target_offset": target_offset,
                "size": size,
                "header_checksum": header_checksum,
            }
        )
    image = {
        "size": file_size,
        "name": c_string(header, 0x18, 0x40),
        "date": c_string(header, 0x58, 0x20),
        "count": count,
        "data_offset": data_offset,
    }
    return image, entries


def print_raw_info(path: Path) -> None:
    image, entries = read_raw_layout(path)
    print(f"path:          {path}")
    print(f"size:          0x{image['size']:x} ({image['size']})")
    print(f"image:         {image['name']}")
    print(f"date:          {image['date']}")
    print(f"data offset:   0x{image['data_offset']:x}")
    print(f"partitions:    {image['count']}")
    for item in entries:
        print(
            f"[{item['index']}] {item['name']}: {item['filename']} "
            f"file=0x{item['absolute_offset']:x}+0x{item['size']:x} "
            f"target=0x{item['target_offset']:x} "
            f"header-check={item['header_checksum']:08x}"
        )


def extract_raw(path: Path, output_dir: Path) -> None:
    _, entries = read_raw_layout(path)
    output_dir.mkdir(parents=True, exist_ok=True)
    with path.open("rb") as source:
        for item in entries:
            filename = Path(str(item["filename"])).name
            destination = output_dir / f"{item['index']:02d}_{item['name']}_{filename}"
            source.seek(int(item["absolute_offset"]))
            remaining = int(item["size"])
            checksum = 0
            with destination.open("wb") as target:
                while remaining:
                    chunk = source.read(min(4 * 1024 * 1024, remaining))
                    if not chunk:
                        raise EOFError(f"short read while extracting {destination}")
                    target.write(chunk)
                    checksum = zlib.crc32(chunk, checksum)
                    remaining -= len(chunk)
            print(
                f"[{item['index']}] {destination} ({item['size']} bytes) "
                f"zlib-crc32={checksum:08x} "
                f"header-check={int(item['header_checksum']):08x}"
            )


def extract_wrapped_ext4(path: Path, output_path: Path) -> None:
    wrapper_size = 0x200
    superblock_offset = wrapper_size + 0x400
    with path.open("rb") as source:
        source.seek(superblock_offset)
        superblock = source.read(0x400)
        if len(superblock) != 0x400 or superblock[0x38:0x3A] != b"\x53\xef":
            raise ValueError(f"{path}: no ext4 superblock behind 0x200-byte wrapper")
        blocks_lo = struct.unpack_from("<I", superblock, 0x04)[0]
        log_block_size = struct.unpack_from("<I", superblock, 0x18)[0]
        blocks_hi = struct.unpack_from("<I", superblock, 0x150)[0]
        block_size = 1024 << log_block_size
        filesystem_size = ((blocks_hi << 32) | blocks_lo) * block_size
        if wrapper_size + filesystem_size > path.stat().st_size:
            raise ValueError(f"{path}: ext4 size extends beyond wrapped image")
        source.seek(wrapper_size)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        remaining = filesystem_size
        digest = hashlib.sha256()
        with output_path.open("wb") as target:
            while remaining:
                chunk = source.read(min(4 * 1024 * 1024, remaining))
                if not chunk:
                    raise EOFError(f"short read while extracting {output_path}")
                target.write(chunk)
                digest.update(chunk)
                remaining -= len(chunk)
    print(f"ext4 output:   {output_path} ({filesystem_size} bytes)")
    print(f"ext4 sha256:   {digest.hexdigest()}")


def parse_modulus(value: str) -> int:
    path = Path(value)
    if path.is_file():
        value = path.read_text().strip()
    return int(value.removeprefix("0x"), 16)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    info_parser = subparsers.add_parser("info", help="inspect a 24MD container")
    info_parser.add_argument("container", type=Path)

    recover_parser = subparsers.add_parser(
        "recover-modulus", help="recover shared RSA modulus from two containers"
    )
    recover_parser.add_argument("first", type=Path)
    recover_parser.add_argument("second", type=Path)
    recover_parser.add_argument("--output", type=Path)

    decrypt_parser = subparsers.add_parser("decrypt", help="unwrap AES key and decrypt")
    decrypt_parser.add_argument("container", type=Path)
    decrypt_parser.add_argument("--modulus", required=True)
    decrypt_parser.add_argument("--byteorder", choices=("big", "little"), default="big")
    decrypt_parser.add_argument("--output", required=True, type=Path)

    raw_info_parser = subparsers.add_parser(
        "raw-info", help="inspect a decrypted Pioneer raw update image"
    )
    raw_info_parser.add_argument("image", type=Path)

    extract_raw_parser = subparsers.add_parser(
        "extract-raw", help="extract partitions from a decrypted raw image"
    )
    extract_raw_parser.add_argument("image", type=Path)
    extract_raw_parser.add_argument("--output-dir", required=True, type=Path)

    ext4_parser = subparsers.add_parser(
        "extract-ext4", help="remove Pioneer's 0x200-byte wrapper from a rootfs"
    )
    ext4_parser.add_argument("image", type=Path)
    ext4_parser.add_argument("--output", required=True, type=Path)

    args = parser.parse_args()
    if args.command == "info":
        print_info(args.container)
    elif args.command == "recover-modulus":
        modulus, algorithm, encoding, byteorder = recover_modulus(
            args.first, args.second
        )
        modulus_hex = f"{modulus:0512x}"
        print(f"digest:        {algorithm}")
        print(f"encoding:      {encoding}")
        print(f"byte order:    {byteorder}")
        print(f"RSA modulus:   {modulus_hex}")
        if args.output:
            args.output.write_text(modulus_hex + "\n")
            print(f"saved:         {args.output}")
    elif args.command == "decrypt":
        decrypt_container(
            args.container,
            parse_modulus(args.modulus),
            args.byteorder,
            args.output,
        )
    elif args.command == "raw-info":
        print_raw_info(args.image)
    elif args.command == "extract-raw":
        extract_raw(args.image, args.output_dir)
    elif args.command == "extract-ext4":
        extract_wrapped_ext4(args.image, args.output)


if __name__ == "__main__":
    main()
