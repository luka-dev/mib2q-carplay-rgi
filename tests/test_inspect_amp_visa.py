"""Static decoding regression vectors; not a hardware execution claim."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import inspect_amp_visa as visa

LOADER = ROOT / 'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'
RANGES = ((0x12694C, 0x1269BC, 54), (0x128910, 0x128974, 48),
          (0x1284D6, 0x128753, 289), (0x126DB2, 0x127852, 1133),
          (0x127852, 0x12796D, 129), (0x12796D, 0x1279ED, 59),
          (0x1279ED, 0x127A4D, 46), (0x127A80, 0x127ABA, 27),
          (0x127B3B, 0x127B75, 27), (0x126092, 0x12610B, 65),
          (0x1283F6, 0x128453, 45), (0x12621D, 0x1262DA, 93),
          (0x125F36, 0x126092, 156), (0x12610B, 0x12618C, 68))


class EncodingTests(unittest.TestCase):
    def check_words(self, words, expected, size=None):
        result = visa.decode(dict(enumerate(words)), 0)
        self.assertEqual(result[1], expected)
        self.assertEqual(result[0], len(words) if size is None else size)

    def test_compressed_immediate_and_indirect_load(self):
        self.check_words([0x9184, 0x00a8], 'DM(I0,M6)=168')
        self.check_words([0x9208, 0x0a0d], 'I4=DM(+13,I1)')
        self.check_words([0x9d08, 0x0179], 'DM(-7,I6)=R2')
        self.check_words([0x0fa7, 0xffff], 'M7=-0x1')

    def test_short_compute_and_move(self):
        self.check_words([0xc142], 'R4-=R2')
        self.check_words([0x9010], 'R0=DM(I0,M0)')
        self.check_words([0x9ff0], 'DM(I7,M7)=R0')

    def test_parallel_multiply_and_add(self):
        self.check_words([0x687e, 0x1093, 0x0802], 'F8=F0*F2 || R1=DM(I4,+2)')
        self.check_words([0x6afe, 0x1608, 0x1c89], 'F12=F8+F9 || DM(I5,+2)=R12')

    def test_complex_handler_compute_ops(self):
        # ADI rev 2.4 tables 12-3, 12-4, 12-11; image words below.
        for code, expected in [(0x2cc0c4, 'BTST R12 BY R4'),
                               (0x022220, 'R2=-R2'),
                               (0x0e2440, 'F4=MAX(F4,F0)'),
                               (0x0e1404, 'F4=MIN(F0,F4)'),
                               (0x089001, 'F0=(F0+F1)/2')]:
            self.assertEqual(visa.compute(code), expected)

    def test_modified_index_destination_and_bitfield(self):
        self.check_words([0x043e, 0xa63f], 'I4=MODIFY(I2,M4)')
        self.check_words([0x023e, 0x2010, 0xc02d], 'R2=FEXT R13 BY 0:19')

    def test_zero_fill_is_full_instruction(self):
        self.check_words([0, 0, 0], 'NOP48')
        self.check_words([1], 'NOP')
        with self.assertRaises(ValueError):
            visa.decode({0:0xffff, 1:0xffff, 2:0xffff},0)


@unittest.skipUnless(LOADER.exists(), 'extracted local J525 loader required')
class ImageVisaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mem = visa.load_shortwords(LOADER.read_bytes())

    def test_complete_ranges_and_branch_alignment(self):
        for start, end, count in RANGES:
            pc = start
            instructions = {}
            targets = []
            while pc < end:
                n, text, target = visa.decode(self.mem, pc)
                self.assertNotIn('UNKNOWN', text, hex(pc))
                instructions[pc] = text
                if target is not None:
                    targets.append(target)
                pc += n
            self.assertEqual(pc, end)
            self.assertEqual(len(instructions), count)
            for target in targets:
                if start <= target < end:
                    self.assertIn(target, instructions)

    def test_zero_loader_gap_is_live_loop_end(self):
        self.assertEqual(visa.decode(self.mem, 0x12699A)[:2], (3, 'NOP48'))
        self.assertEqual(visa.decode(self.mem, 0x126974)[2], 0x12699A)

    def test_runtime_abi_modifier_constants(self):
        expected = ['M15=-0x1','M7=-0x1','M14=0x1','M6=0x1','M13=0x0','M5=0x0']
        self.assertEqual([visa.decode(self.mem, 0x125A2A+i*2)[1] for i in range(6)], expected)

    def test_coefficient_reorder_loads_and_strides(self):
        starts = [0x125D21,0x125D2B,0x125D35,0x125D3F,0x125D49]
        self.assertEqual([visa.decode(self.mem,a)[1] for a in starts],
                         [f'R0=DM(+{i},I0)' for i in [2,3,0,4,1]])
        self.assertEqual(visa.decode(self.mem,0x125D57)[1], 'I0=MODIFY(I0,+5)')
        self.assertEqual(visa.decode(self.mem,0x125D5A)[1], 'I4=MODIFY(I4,+4)')


if __name__ == '__main__':
    unittest.main()
