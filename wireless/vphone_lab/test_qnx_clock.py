import struct
import unittest
from unittest.mock import patch
from wireless.lab_common import qnx_clock


class QNXClockTests(unittest.TestCase):
    def test_unknown_startup_is_rejected(self):
        for data in (b'',b'\x7fELF'+bytes(4096)):
            with self.assertRaises(ValueError): qnx_clock.corrected(data)

    def test_pinned_arm_constants_and_other_bytes_preserved(self):
        fixture=bytearray(1024)
        fixture[0xf4:0xfc]=bytes.fromhex('d50009e3f70440e3')
        struct.pack_into('<Q',fixture,0x138,24000000)
        with patch.object(qnx_clock.hashlib,'sha256') as digest:
            digest.return_value.hexdigest.return_value=qnx_clock.STARTUP_SHA256
            out=qnx_clock.corrected(bytes(fixture))
            self.assertEqual(out[:0xf4],fixture[:0xf4])
            self.assertEqual(out[0xfc:0x138],fixture[0xfc:0x138])
            self.assertEqual(out[0x140:],fixture[0x140:])
            lo,hi=struct.unpack_from('<II',out,0xf4)
            def imm(word):return (word&4095)|((word>>4)&0xf000)
            self.assertEqual(imm(lo)|(imm(hi)<<16),16000000)
            self.assertEqual(struct.unpack_from('<Q',out,0x138)[0],62500000)
            fixture[0xf4]=0
            with self.assertRaises(ValueError):qnx_clock.corrected(bytes(fixture))

if __name__=='__main__':unittest.main()
