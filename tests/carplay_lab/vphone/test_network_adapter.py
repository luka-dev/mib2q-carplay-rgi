"""Refusal boundaries for the VM-only loader's Mach-O mutation."""
import struct
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from contextlib import ExitStack
from tests.carplay_lab.vphone import network_adapter as adapter


def image():
    data=bytearray(512)
    struct.pack_into('<8I',data,0,0xFEEDFACF,0x100000C,0x80000002,6,1,152,0,0)
    struct.pack_into('<II',data,32,0x19,152)
    struct.pack_into('<I',data,32+64,1)
    struct.pack_into('<I',data,32+72+48,400)
    data[400:]=b'Z'*112
    return bytes(data)


class LoaderMutationTests(unittest.TestCase):
    def test_only_header_padding_and_counts_change(self):
        original=image();changed=adapter.weak_dependency(original,adapter.LOADER)
        self.assertEqual(len(original),len(changed))
        self.assertEqual(changed[400:],original[400:])
        self.assertEqual(changed[32:184],original[32:184])
        self.assertEqual(struct.unpack_from('<I',changed,16)[0],2)
        self.assertEqual(struct.unpack_from('<I',changed,184)[0],0x80000018)
        self.assertIn(adapter.LOADER.encode()+b'\0',changed[184:400])
        with self.assertRaises(ValueError):adapter.weak_dependency(changed,adapter.LOADER)

    def test_occupied_padding_and_short_segments_are_rejected(self):
        data=bytearray(image());data[184]=1
        with self.assertRaises(ValueError):adapter.weak_dependency(bytes(data),adapter.LOADER)
        data=bytearray(image());struct.pack_into('<I',data,36,8)
        with self.assertRaises(ValueError):adapter.weak_dependency(bytes(data),adapter.LOADER)

    def test_truncated_commands_and_other_dependency_paths_are_rejected(self):
        for value in (b'',image()[:60],bytes(512)):
            with self.assertRaises((ValueError,struct.error)):adapter.weak_dependency(value,adapter.LOADER)
        for path in ('/usr/lib/unowned.dylib',adapter.LOADER+'\0x',adapter.usb.GUEST_DIR+'/../unowned.dylib'):
            with self.assertRaises(ValueError):adapter.weak_dependency(image(),path)

    def test_wrong_architecture_or_load_commands_beyond_file_are_rejected(self):
        for field,value in ((4,0x1000007),(8,0),(12,2),(16,100),(20,65536)):
            data=bytearray(image());struct.pack_into('<I',data,field,value)
            with self.assertRaises(ValueError):adapter.weak_dependency(bytes(data),adapter.LOADER)


class InstallerRecoveryTests(unittest.TestCase):
    def context(self, directory):
        root=Path(directory);payload=root/'payload';payload.write_bytes(b'owned payload')
        record={'status':'installed','udid':'owned','original_sha256':'original','installed_sha256':'modified',
                'files':{'payload':hashlib.sha256(payload.read_bytes()).hexdigest()},'config':{'Enabled':True},
                'config_path':adapter.CONFIG}
        (root/adapter.RECORD).write_text(json.dumps(record))
        stack=ExitStack()
        stack.enter_context(patch.object(adapter,'OUT',root))
        stack.enter_context(patch.object(adapter.lab,'OUT',root))
        stack.enter_context(patch.object(adapter.lab,'profile',return_value={}))
        stack.enter_context(patch.object(adapter.lab,'require_machine',return_value={'udid':'owned'}))
        return root,record,stack

    def test_matching_live_install_is_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            root,record,stack=self.context(directory)
            with stack, patch.object(adapter.lab,'rpc',return_value={'result':{'libraries':[{'name':adapter.SYSTEM,'sha256':'modified'}]}}), \
                 patch.object(adapter.usb,'run') as probe, patch.object(adapter,'install_environment') as install:
                self.assertEqual(adapter.install()['status'],'installed')
                probe.assert_not_called();install.assert_not_called()

    def test_host_auto_sync_and_failed_probe_leave_recoverable_prepared_journal(self):
        with tempfile.TemporaryDirectory() as directory:
            root,record,stack=self.context(directory)
            with stack, patch.object(adapter.lab,'rpc',return_value={'result':{'libraries':[{'name':adapter.SYSTEM,'sha256':'original'}]}}), \
                 patch.object(adapter.usb,'run',side_effect=RuntimeError('probe failed')), patch.object(adapter,'install_environment') as install:
                with self.assertRaisesRegex(RuntimeError,'probe failed'):adapter.install()
                self.assertEqual(json.loads((root/adapter.RECORD).read_text())['status'],'prepared')
                install.assert_not_called()

    def test_changed_original_backup_is_refused_before_restore_mutates_guest(self):
        with tempfile.TemporaryDirectory() as directory:
            root,record,stack=self.context(directory)
            (root/('original-'+adapter.SYSTEM)).write_bytes(b'wrong backup')
            with stack, patch.object(adapter.lab,'rpc',return_value={'result':{'libraries':[{'name':adapter.SYSTEM,'sha256':'modified'}]}}), \
                 patch.object(adapter,'upload') as upload, patch.object(adapter,'install_environment') as install:
                with self.assertRaisesRegex(ValueError,'backup changed'):adapter.restore()
                upload.assert_not_called();install.assert_not_called()
                self.assertEqual(json.loads((root/adapter.RECORD).read_text())['status'],'installed')


if __name__=='__main__':unittest.main()
