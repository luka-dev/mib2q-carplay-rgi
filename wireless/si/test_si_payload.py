#!/usr/bin/env python3
"""Transport byte-exact ELF payloads; reject damaged or oversized input."""
import hashlib
from pathlib import Path
import subprocess
import tempfile
import zlib

SI = Path(__file__).resolve().parent


def main():
    with tempfile.TemporaryDirectory(prefix="si-payload-") as temp:
        work = Path(temp)
        exe = work / "unpack"
        subprocess.run(["cc", "-std=c99", "-D_POSIX_C_SOURCE=200809L", "-Wall", "-Wextra",
                        "-Werror", "-fsanitize=address,undefined", str(SI / "si_payload.c"),
                        "-lz", "-o", str(exe)], check=True)
        raw = (b"\x7fELF" + bytes(range(256))) * 4000
        source, dest = work / "payload", work / "elf"
        packed = zlib.compress(raw, 9)
        source.write_bytes(packed)
        args = [str(source), str(dest), str(len(raw)), f"{zlib.crc32(raw):08x}"]

        def run(extra, ok):
            result = subprocess.run([str(exe), *extra], capture_output=True, text=True)
            assert (result.returncode == 0) == ok, result.stdout + result.stderr

        run(args, True)
        assert hashlib.sha256(dest.read_bytes()).digest() == hashlib.sha256(raw).digest()
        # A rerun must preserve an existing file, including if its content differs.
        dest.write_bytes(b"preserve")
        run(args, False)
        assert dest.read_bytes() == b"preserve"
        dest.unlink()
        run([*args[:3], "0"], False)
        assert not dest.exists()
        run([*args[:2], str(len(raw) - 1), args[3]], False)
        run([*args[:2], str(8 * 1024 * 1024 + 1), args[3]], False)
        run([*args[:2], "-1", args[3]], False)
        for bad in (b"", packed[:8], packed[:-1], b"\x7fELF" + packed[4:]):
            source.write_bytes(bad)
            run(args, False)
            assert not dest.exists()
        source.write_bytes(packed)
        second = work / "second"
        run([*args, str(source), str(second), args[2], args[3]], True)
        assert dest.read_bytes() == raw == second.read_bytes()
    print("SI payload transport: byte-exact ELF, corruption/size/existing-file controls PASS (ASan/UBSan)")


if __name__ == "__main__":
    main()
