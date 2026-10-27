import struct
import unittest
import zlib
import json
from pathlib import Path
import shutil
import tempfile
from unittest import mock

from prepare_combined_usb import gpt_view, trustcache, decode_trustcache, initialize, save_gpt_recovery, recover_gpt, detach_owned_image


def im4p(payload):
    def tlv(tag, data):
        return bytes((tag, len(data))) + data
    return tlv(0x30, tlv(0x16, b"IM4P") + tlv(0x16, b"rtsc") + tlv(0x16, b"test") + tlv(4, payload))


def fixture():
    size = 2 * 1024 * 1024
    head, tail = bytearray(65536), bytes(65536)
    header = bytearray(92)
    header[:8] = b"EFI PART"
    struct.pack_into("<II", header, 8, 0x10000, 92)
    struct.pack_into("<QQQQ", header, 24, 1, size // 4096 - 1, 6, size // 4096 - 6)
    struct.pack_into("<QII", header, 72, 2, 128, 128)
    table = bytearray(16384)
    table[:16] = bytes(range(1, 17))
    struct.pack_into("<QQ", table, 32, 6, size // 4096 - 6)
    struct.pack_into("<I", header, 88, zlib.crc32(table))
    struct.pack_into("<I", header, 16, zlib.crc32(header))
    head[4096:4188], head[8192:24576] = header, table
    return bytes(head), tail, size


class ProvisionTests(unittest.TestCase):
    def test_failed_mount_metadata_can_be_recovered_without_touching_filesystems(self):
        head, tail, size = fixture()
        writes, start, end = gpt_view(head, tail, size)
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp); image = base / 'root'
            original = bytearray(b'X'*size)
            original[:start] = head[:start]; original[end:] = tail[-(size-end):]
            image.write_bytes(original)
            save_gpt_recovery(base,image,head,tail,start,end)
            with image.open('r+b') as stream:
                for offset,data in writes: stream.seek(offset); stream.write(data)
            recover_gpt(base,image)
            self.assertEqual(image.read_bytes(),original)
            self.assertFalse(json.loads((base/'native-gpt-recovery.json').read_text())['pending'])
            save_gpt_recovery(base,image,head,tail,start,end)
            (base/'native-gpt-head.bin').write_bytes(b'damaged')
            with self.assertRaisesRegex(ValueError,'checksum'): recover_gpt(base,image)
            self.assertEqual(image.read_bytes(),original)

    def test_busy_owned_mount_retries_before_force_and_reports_failure(self):
        with mock.patch('prepare_combined_usb.subprocess.run') as run, mock.patch('prepare_combined_usb.time.sleep'):
            run.side_effect=[mock.Mock(returncode=16)]*3+[mock.Mock(returncode=0)]
            detach_owned_image('/dev/disk-owned')
            self.assertEqual(run.call_args.args[0],['hdiutil','detach','/dev/disk-owned','-force'])
        with mock.patch('prepare_combined_usb.subprocess.run') as run, mock.patch('prepare_combined_usb.time.sleep'):
            run.side_effect=[mock.Mock(returncode=16)]*3+[__import__('subprocess').CalledProcessError(16,'detach')]
            with self.assertRaises(__import__('subprocess').CalledProcessError): detach_owned_image('/dev/disk-owned')
    def test_bounded_im4p_decode_rejects_wrong_types_and_lengths(self):
        payload = struct.pack("<I", 1) + bytes(20)
        container = im4p(payload)
        self.assertEqual(decode_trustcache(container), payload)
        for damaged in (container[:-1], container + b"x", container.replace(b"rtsc", b"krnl"),
                        b"\x30\x80", b"\x30\x85" + bytes(5), b"x" * 1048577):
            with self.assertRaises(ValueError):
                decode_trustcache(damaged)

    def test_initialization_copies_all_drives_and_preserves_base_profile(self):
        with tempfile.TemporaryDirectory() as temp:
            lab = Path(temp)
            source, base = lab / "original", lab / "combined-usb"
            source.mkdir()
            base.mkdir()
            cache = lab / "source.trustcache"
            payload = struct.pack("<I", 1) + bytes(20)
            cache.write_bytes(im4p(payload))
            command = ["qemu", "-M", f"t8030,trustcache={cache},ticket=test"]
            for name in ("root", "sep_nvram", "sep_ssc"):
                (source / name).write_bytes(name.encode())
                command += ["-drive", f"file={source / name},format=raw"]
            profile = lab / "ios-normal-command.json"
            profile.write_text(json.dumps(command))
            def copy(args, **_):
                self.assertEqual(args[:2], ["cp", "-c"])
                shutil.copy2(args[2], args[3])
            with mock.patch("prepare_combined_usb.check_guest_disks"), \
                    mock.patch("prepare_combined_usb.subprocess.run", side_effect=copy):
                initialize(lab, base)
            self.assertEqual(json.loads(profile.read_text()), command)
            self.assertEqual((base / "trustcache.raw").read_bytes(), payload)
            for name in ("root", "sep_nvram", "sep_ssc"):
                self.assertEqual((base / "disks" / name).read_bytes(), name.encode())
            updated = json.loads((lab / "ios-normal-combined-usb-command.json").read_text())
            self.assertNotIn(str(source), " ".join(updated))
            with self.assertRaisesRegex(RuntimeError, "new clone"):
                initialize(lab, base)

    def test_gpt_view_preserves_entire_filesystem(self):
        head, tail, size = fixture()
        writes, start, end = gpt_view(head, tail, size)
        self.assertEqual((start, end), (24576, size - 20480))
        for offset, data in writes:
            self.assertTrue(offset + len(data) <= start or offset >= end)
        table = dict(writes)[1024]
        self.assertEqual(struct.unpack_from("<QQ", table, 32), (48, end // 512 - 1))
        for offset in (512, size - 512):
            header = bytearray(dict(writes)[offset][:92])
            checksum = struct.unpack_from("<I", header, 16)[0]
            struct.pack_into("<I", header, 16, 0)
            self.assertEqual(zlib.crc32(header), checksum)

    def test_corrupted_gpt_is_rejected_before_writes(self):
        head, tail, size = fixture()
        for offset in (4096, 4108, 4136, 8224):
            changed = bytearray(head)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                gpt_view(bytes(changed), tail, size)
        with self.assertRaises(ValueError):
            gpt_view(head, bytes(8192), size)

    def test_trustcache_addition_is_sorted_idempotent_and_bounded(self):
        old = struct.pack("<I", 1) + bytes(16) + struct.pack("<I", 1) + bytes.fromhex("ff" * 20) + b"\2\0"
        added = trustcache(old, "01" * 20)
        self.assertEqual(struct.unpack_from("<I", added, 20)[0], 2)
        self.assertEqual(added[24:44], b"\1" * 20)
        self.assertEqual(trustcache(added, "01" * 20), added)
        for payload, digest in ((old[:-1], "01" * 20), (old, "01"), (old, "gg" * 20)):
            with self.assertRaises(ValueError):
                trustcache(payload, digest)
