"""Static control contract. Not a MOST capture or full DSP execution test."""
import hashlib
import re
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from audit_amp_output_bindings import complex_process_mask, hmi_3d_mode_to_hu_level

import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
JAVA = Path(_WS + '/Tools/jxe2jar/out/MU1316-vf')
HU = Path(_WS + '/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/'
          'MHI2Q_US_AUG22_P5087_MU1316/RE/MIBRoot/MIBRoot')


class ControlContractTests(unittest.TestCase):
    def test_distinct_numbering_domains(self):
        self.assertEqual([hmi_3d_mode_to_hu_level(x) for x in (1, 2, 4, 8)],
                         [0, 1, 2, 3])
        for invalid in (0, 3, 16):
            with self.assertRaises(ValueError):
                hmi_3d_mode_to_hu_level(invalid)

    def test_complex_masks_and_precedence(self):
        self.assertEqual(complex_process_mask(0, 0), 2)
        self.assertEqual(complex_process_mask(1, 0), 1)
        self.assertEqual(complex_process_mask(2, 0), 0xF4BA)
        self.assertEqual(complex_process_mask(3, 0), 0xF4BA)
        self.assertEqual(complex_process_mask(4, 0), 0xFFF7FE5)
        self.assertEqual(complex_process_mask(7, 1), 0xDFF75E5)
        # Sideband changes only these stages, not an entire processing bypass.
        delta = complex_process_mask(4, 0) ^ complex_process_mask(4, 1)
        self.assertEqual([i for i in range(32) if delta & (1 << i)], [9, 11, 25])

    @unittest.skipUnless(JAVA.exists(), 'local MU1316 Java decompilation required')
    def test_actual_java_enum_and_ui_mapping(self):
        constants = (JAVA / 'org/dsi/ifc/audio/Constants.java').read_text()
        values = [int(re.search(r'MODE3D_3D_' + label + r'\s*=\s*(\d+)', constants)[1])
                  for label in ('OFF', 'LOW', 'MID', 'HIGH')]
        self.assertEqual(values, [1, 2, 4, 8])
        choice = (JAVA / 'de/audi/tone/app/sound/ThreeDMode.java').read_text()
        self.assertIn('MAP_HMI_TO_DSI = new int[]{1, 2, 4, 8}', choice)
        self.assertIn('this.this$0.dsiSound.setThreeDMode(j)', choice)

    @unittest.skipUnless(HU.exists(), 'local MU1316 MIBRoot required')
    def test_actual_native_table_and_dispatch_bytes(self):
        data = HU.read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(),
                         'e6e710d7467be935c3fea933ef9068a66786c392f3f7a0e78155ad9a1063acd1')
        table = struct.unpack_from('<9I', data, 0xDDEE50 - 0x100000)
        self.assertEqual(table, (0, 1, 2, 4, 8, 16, 32, 64, 128))
        for mode in (1, 2, 4, 8):
            self.assertEqual(table.index(mode) - 1, hmi_3d_mode_to_hu_level(mode))
        # ARM mov r3,#0x22; sub ip,r0,#1; sxth ip,ip; str ip,[sp].
        for va, expected in [(0x1EB1D0, '2230a0e3'), (0x1EB1F0, '01c040e2'),
                             (0x1EB1FC, '7cc0bfe6'), (0x1EB200, '00c08de5')]:
            self.assertEqual(data[va - 0x100000:va - 0x100000 + 4].hex(), expected)


if __name__ == '__main__':
    unittest.main()
