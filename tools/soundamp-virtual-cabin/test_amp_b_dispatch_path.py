"""Guard against mistaking mode5's prep branch for a spectral bypass."""
import sys
import unittest

from amp_b_mode5_schedule import ROOT,LOADER,mode5_schedule_evidence


class Mode5SpectralDispatchTests(unittest.TestCase):
    def test_mode5_preparation_rejoins_spectral_call_on_successful_path(self):
        mode5_schedule_evidence()  # Pins the complete131208..13163D range.
        sys.path.insert(0,str(ROOT/'tools'))
        from inspect_amp_visa import load_shortwords,decode
        mem=load_shortwords(LOADER.read_bytes())
        expected={
            0x131286:'R2=DM(+1128,I3)',
            0x13129c:'IF EQ JUMP 0x13158c',
            0x13158e:'COMP(R0,R2) || R1=DM(-22,I6)',
            0x131591:'IF EQ JUMP 0x1315d3',
            0x131624:'JUMP 0x131594 (DB)',
            0x131627:'R1=DM(-24,I6)',
            0x131629:'I7=MODIFY(I7,+2)',
            0x1315b5:'CJUMP 0x12e80e (DB); R2=I6, I6=I7',
        }
        for pc,op in expected.items():self.assertEqual(decode(mem,pc)[1],op)
        # The prep's delayed branch rejoins below its own selector test.
        # Until the spectral call, the real continuation has no intervening
        # jump/return that could skip it. Failed callees are outside this test.
        pc=0x131594
        while pc<0x1315b5:
            size,op,_=decode(mem,pc)
            self.assertNotIn('JUMP',op);self.assertNotIn('RTS',op)
            pc+=size
        self.assertEqual(pc,0x1315b5)


if __name__=='__main__':unittest.main()
