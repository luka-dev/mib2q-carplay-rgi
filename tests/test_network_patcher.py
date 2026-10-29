import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch
from scripts import network_patcher as patcher

class NetworkPatcherTests(unittest.TestCase):
    def test_wifi_only_stage_and_verify(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / 'network'
            result = patcher.stage(out)
            self.assertEqual(result['components'], ['wifi'])
            self.assertEqual(result['status'], 'verified')
            self.assertFalse((out / 'SDIO-boot-inputs').exists())
            self.assertFalse((out / 'PF-examples-unactivated').exists())
            self.assertFalse((out / 'PF-configured-unactivated').exists())
            self.assertEqual(set(patcher.regular_files(out)),
                             patcher.selected_files() | {'network-manifest.json'})

    def test_changed_driver_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / 'network'; patcher.stage(out)
            (out / 'wifi/runtime/driver.so').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'files changed'):
                patcher.verify_stage(out)

    def test_disabled_component_rejected_even_with_updated_file_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / 'network'; patcher.stage(out)
            unwanted = out / 'PF-configured-unactivated/pf.conf'
            unwanted.parent.mkdir(); unwanted.write_text('pass all')
            manifest = out / 'network-manifest.json'
            data = json.loads(manifest.read_text())
            data['files'][str(unwanted.relative_to(out))] = patcher.sha(unwanted)
            manifest.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, 'only selected Wi-Fi'):
                patcher.verify_stage(out)

    def test_old_multi_component_stage_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / 'network'; patcher.stage(out)
            manifest = out / 'network-manifest.json'; data = json.loads(manifest.read_text())
            data['schema_version'] = 1; manifest.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, 'no longer selected'):
                patcher.verify_stage(out)

    def test_disabled_options_rejected_before_creating_output(self):
        for kwargs in ({'recipe': Path('old.build')}, {'phones': ['192.168.50.2']},
                       {'pf_input': Path('old-pf')}, {'dynamic_phone': True},
                       {'link_mbps': 30}, {'control_evidence': Path('old.json')}):
            with self.subTest(kwargs=kwargs), tempfile.TemporaryDirectory() as directory:
                out = Path(directory) / 'network'
                with self.assertRaisesRegex(ValueError, 'disabled; Wi-Fi only'):
                    patcher.stage(out, **kwargs)
                self.assertFalse(out.exists())

    def test_existing_output_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory); marker = out / 'owner-file'; marker.write_text('preserve')
            with self.assertRaises(FileExistsError): patcher.stage(out)
            self.assertEqual(marker.read_text(), 'preserve')

    def test_wrong_stack_before_output(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / 'network'; stack = Path(directory) / 'io-pkt'; stack.write_bytes(b'wrong')
            with self.assertRaisesRegex(ValueError, 'unsupported io-pkt'):
                patcher.stage(out, stack=stack)
            self.assertFalse(out.exists())

    def test_do_not_stage_within_source_resources(self):
        out = patcher.ROOT / 'deploy/wifi/runtime/nested-output'
        with self.assertRaisesRegex(ValueError, 'overlaps project sources'): patcher.stage(out)
        self.assertFalse(out.exists())

    def test_failed_copy_removes_only_new_output(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / 'network'; marker = Path(directory) / 'owner'; marker.write_text('keep')
            with patch.object(patcher.shutil, 'copyfile', side_effect=OSError('copy failed')):
                with self.assertRaises(OSError): patcher.stage(out)
            self.assertFalse(out.exists()); self.assertEqual(marker.read_text(), 'keep')

    def test_retired_resources_not_needed(self):
        with tempfile.TemporaryDirectory() as directory:
            cloned = Path(directory) / 'source'
            shutil.copytree(patcher.ROOT / 'deploy/wifi/runtime', cloned / 'deploy/wifi/runtime')
            helper = cloned / 'deploy/binpatch/wifi_runtime.py'; helper.parent.mkdir(parents=True)
            shutil.copyfile(patcher.ROOT / 'deploy/binpatch/wifi_runtime.py', helper)
            with patch.object(patcher, 'ROOT', cloned):
                self.assertEqual(patcher.status()['status'], 'wifi-only-resources-verified')
                self.assertFalse(patcher.status()['sdio']['selected'])

if __name__ == '__main__': unittest.main()
