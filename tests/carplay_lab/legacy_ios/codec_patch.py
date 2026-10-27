"""Opt-in, reversible software-fallback property experiment for iOS 18A5351d."""
from __future__ import annotations
import hashlib
import os
from pathlib import Path

CACHE_BYTES = 2052489216
ORIGINAL_SHA256 = 'af1001abe5b075afd337bf110562746c3e006e9d93951e163fc5daae062aeab8'
PATCHED_SHA256 = '86f1dd01fc9331e7fcc3c3ce8a91a85be37ca1060146ef746f60b04ea98032b6'
CALLBACK = 0x1a9fec5d8
CAVE = 0x1aa1e53b8
# Two property-query calls and 28 bytes of verified RX segment padding.
# The thunk compares the authenticated/stripped callback in x17 with our own
# getter. Self query returns kCMPropertyNotSupported; other callbacks tail-call
# the original BRAA with its original authentication discriminator.
PATCHES = (
    (0x29fec8d8, bytes.fromhex('09093fd7'), bytes.fromhex('b8e20794')),
    (0x29fec91c, bytes.fromhex('09093fd7'), bytes.fromhex('a7e20794')),
    (0x2a1e53b8, bytes(28), bytes.fromhex('30f0fff0106217913f0210eb61000054a03d8612c0035fd609091fd7')),
)


def cache_sha256(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def patch_cache(path: Path, *, revert: bool = False) -> dict:
    """Avoid the recursive supported-properties query in the software encoder.

    Codec creation/encoding and dispatch_once checks are unchanged. Other
    property callbacks retain pointer authentication and are called normally.
    This is an opt-in VM experiment, not a production encoder implementation.
    """
    if path.is_symlink() or not path.is_file() or path.stat().st_size != CACHE_BYTES:
        raise ValueError('expected the pinned regular iOS cache')
    source_hash, target_hash = (PATCHED_SHA256, ORIGINAL_SHA256) if revert else (ORIGINAL_SHA256, PATCHED_SHA256)
    before = cache_sha256(path)
    if before not in (source_hash, target_hash):
        raise ValueError('cache SHA-256 does not match either pinned state')
    edits = [(off, new, old) if revert else (off, old, new) for off, old, new in PATCHES]
    with path.open('r+b') as stream:
        for offset, source, target in edits:
            stream.seek(offset)
            if stream.read(len(source)) != (target if before == target_hash else source):
                raise ValueError('codec instruction does not match the pinned state')
        if before != target_hash:
            try:
                for offset, source, target in edits:
                    stream.seek(offset)
                    if stream.write(target) != len(target):
                        raise OSError('short instruction write')
                stream.flush()
                os.fsync(stream.fileno())
                if cache_sha256(path) != target_hash:
                    raise ValueError('patched cache SHA-256 mismatch')
            except Exception:
                for offset, source, target in edits:
                    stream.seek(offset)
                    stream.write(source)
                stream.flush()
                os.fsync(stream.fileno())
                raise
    return {'experiment': 'conditional-software-properties', 'enabled': not revert,
            'callback': hex(CALLBACK), 'thunk': hex(CAVE),
            'before_sha256': before, 'after_sha256': target_hash,
            'patches': [{'file_offset': hex(off), 'original_bytes': old.hex(), 'patched_bytes': new.hex()}
                        for off, old, new in PATCHES]}
