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
          (0x125F36, 0x126092, 156), (0x12610B, 0x12618C, 68),
          (0x12879C, 0x128910, 172), (0x12823C, 0x1283F6, 208),
          (0x126A95, 0x126BAE, 127), (0x124DF3, 0x12504D, 243),
          (0x124393, 0x12440B, 50), (0x12512E, 0x1251A9, 48),
          (0x12DD7D, 0x12DE3B, 82), (0x12DE3B, 0x12DFA8, 161),
          (0x12DD16, 0x12DD67, 34), (0x131208, 0x13163D, 477),
          (0x12677D, 0x12682B, 82))


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

    def test_gain_and_dynamics_ops(self):
        # ADI rev 2.4 tables 12-4, 12-11 and multifunction table 12-17.
        for code, expected in [(0x0b0330, 'F3=ABS F3'),
                               (0x0e0330, 'F3=F3 COPYSIGN F0'),
                               (0x5d1258, 'F1=F1*F5, F2=ABS F10'),
                               (0x2c82d1, 'R2=BTGL R13 BY R1')]:
            self.assertEqual(visa.compute(code), expected)

    def test_log_and_exp_ops(self):
        # ADI rev 2.4, table 12-4; LOGB extracts an integer exponent.
        for code, expected in [(0x0c1140, 'R1=LOGB F4'),
                               (0x0bdcc4, 'F12=SCALB F12 BY R4'),
                               (0x0bd024, 'F0=SCALB F2 BY R4'),
                               (0x0cd020, 'R0=TRUNC F2'),
                               (0x204282, 'R2=ASHIFT R8 BY R2')]:
            self.assertEqual(visa.compute(code), expected)

    def test_register_bit_set_clear(self):
        # ADI programming rev2.4 register shifter BSET/BCLR, table12-11.
        self.assertEqual(visa.compute(0x2c0221), 'R2=BSET R2 BY R1')
        self.assertEqual(visa.compute(0x2c4221), 'R2=BCLR R2 BY R1')

    def test_reciprocal_square_root_seed_ops(self):
        # ADI rev2.4 table12-4, opcode11000101; seed, not exact sqrt.
        for code, expected in [(0x0c5240,'F2=RSQRTS F4'),
                               (0x0c5120,'F1=RSQRTS F2'),
                               (0x0c5610,'F6=RSQRTS F1')]:
            self.assertEqual(visa.compute(code),expected)

    def test_zero_fill_is_full_instruction(self):
        self.check_words([0, 0, 0], 'NOP48')
        self.check_words([1], 'NOP')
        with self.assertRaises(ValueError):
            visa.decode({0:0xffff, 1:0xffff, 2:0xffff},0)

    def test_fft_parallel_dual_and_triple_operations(self):
        # ADI rev2.4 dual add/sub and multiply-dual-add/sub encodings.
        for code, expected in [(0x0fb189, 'F1=F8+F9, F11=F8-F9'),
                (0x7fea45, 'F14=F1*F4, F10=F9+F13, F15=F9-F13'),
                (0x288240, 'R2=LEFTZ R4')]:
            self.assertEqual(visa.compute(code), expected)

    def test_mantissa_extract_is_integer_and_not_gain(self):
        # ADI rev2.4 section11-35: unsigned magnitude 1.31, AV tests infinity.
        self.assertEqual(visa.compute(0x0ad410), 'R4=MANT F1')
        self.assertEqual(visa.compute(0x0ad140), 'R1=MANT F4')
        # Section11-33: rounds floating extended precision, not integer FLOAT.
        self.assertEqual(visa.compute(0x0a5040), 'F0=RND F4')

    def test_noise_integer_accumulator_and_coefficient_distance_ops(self):
        # ADI rev2.4 tables12-4/5/7/8/10, not guessed from generator output.
        for code,expected in [(0x114000,'MRF=0'),(0x410000,'MR0F=R0'),
                (0x400000,'R0=MR0F'),(0x1f4083,'MRF=MRF-R8*R3 (SSI)'),
                (0x174002,'MRF=R0*R2 (SSI)'),(0x101000,'R0=SAT MRF (SI)'),
                (0x092c12,'F12=ABS(F1-F2)')]:
            self.assertEqual(visa.compute(code),expected)


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
