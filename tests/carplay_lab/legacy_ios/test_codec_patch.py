from pathlib import Path
import hashlib
import struct
import tempfile
import unittest
from unittest.mock import patch
import codec_patch


class CodecPatchTests(unittest.TestCase):
    def test_call_targets_and_conditional_authenticated_tail_call(self):
        for offset, before, after in codec_patch.PATCHES[:2]:
            instruction, = struct.unpack('<I', after)
            self.assertEqual(instruction >> 26, 37)  # BL, not an unlinked B.
            immediate = instruction & 0x3ffffff
            if immediate & 0x2000000: immediate -= 0x4000000
            self.assertEqual(0x180000000 + offset + immediate * 4, codec_patch.CAVE)
            self.assertEqual(before, bytes.fromhex('09093fd7'))
        words = struct.unpack('<7I',codec_patch.PATCHES[2][2])
        page = (((words[0] >> 5) & 0x7ffff) << 2) | ((words[0] >> 29) & 3)
        if page & (1 << 20): page -= 1 << 21
        address = (codec_patch.CAVE & ~0xfff) + page * 4096 + ((words[1] >> 10) & 0xfff)
        self.assertEqual(address, codec_patch.CALLBACK)
        self.assertEqual(words[2],0xeb10023f)  # CMP x17,x16
        self.assertEqual(words[3],0x54000061)  # B.NE to the original BRAA.
        self.assertEqual(words[-1],0xd71f0909)
        self.assertEqual(words[4],0x12863da0)  # kCMPropertyNotSupported

    def test_unknown_cache_is_never_modified(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)/'cache';p.write_bytes(b'unknown')
            with self.assertRaises(ValueError): codec_patch.patch_cache(p)
            self.assertEqual(p.read_bytes(), b'unknown')

    def test_apply_revert_idempotence_and_verification_rollback(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)/'cache'
            original = b'headerABCDmiddleEFGHtrailer'
            changed = b'header1234middle5678trailer'
            edits = ((6,b'ABCD',b'1234'),(16,b'EFGH',b'5678'))
            p.write_bytes(original)
            old, new = hashlib.sha256(original).hexdigest(), hashlib.sha256(changed).hexdigest()
            with patch.multiple(codec_patch, CACHE_BYTES=len(original), PATCHES=edits,
                                ORIGINAL_SHA256=old, PATCHED_SHA256=new):
                codec_patch.patch_cache(p);self.assertEqual(p.read_bytes(),changed)
                codec_patch.patch_cache(p);self.assertEqual(p.read_bytes(),changed)
                codec_patch.patch_cache(p,revert=True);self.assertEqual(p.read_bytes(),original)
                with patch.object(codec_patch,'cache_sha256',side_effect=[old,'mismatch']):
                    with self.assertRaises(ValueError):codec_patch.patch_cache(p)
                self.assertEqual(p.read_bytes(),original)
