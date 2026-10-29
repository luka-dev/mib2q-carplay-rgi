"""Stress oracle must expose lateness/loss, never count drained frames as live fps."""
import asyncio
import struct
import unittest
from .media_stress import SharedLink,annexb,pacing,run

class MediaStressTests(unittest.TestCase):
    def test_pacing_and_drained_tail(self):
        rows=[(i+1,int(i/30*1e9),int((i/30+.01)*1e9)) for i in range(90)]
        self.assertTrue(pacing(rows,90,3)['pacing_pass'])
        late=[(seq,due,got+2_000_000_000) for seq,due,got in rows]
        r=pacing(late,90,3)
        self.assertFalse(r['pacing_pass']);self.assertLess(r['source_window_fps'],30)
        self.assertEqual(r['late_to_100ms_buffer'],90)
        self.assertFalse(pacing(rows[:-1],90,3)['pacing_pass'])
        with self.assertRaises(ValueError):pacing(rows+rows[:1],90,3)
        with self.assertRaises(ValueError):pacing([(1,100,0)],90,3)

    def test_avcc_bounds(self):
        self.assertEqual(annexb(struct.pack('!I',2)+b'\x65a'),b'\0\0\0\1\x65a')
        for bad in (b'\0',bytes(4),struct.pack('!I',100)+b'x'):
            with self.assertRaises(ValueError):annexb(bad)

    def test_playout_deadlines_not_raw_arrival_gap(self):
        rows=[(1,0,0),(2,33_333_333,110_000_000)]
        result=pacing(rows,2,2/30)
        self.assertGreater(result['gap_ms']['max'],100)
        self.assertTrue(result['pacing_pass']) # both ready by source PTS +100ms

    def test_shared_capacity_and_outage(self):
        link=SharedLink(40,0)
        self.assertAlmostEqual(link.reserve(0,500000),.1)
        self.assertAlmostEqual(link.reserve(0,500000),.2) # same cap, not 40 per stream
        stalled=SharedLink(40,0,.2)
        self.assertGreaterEqual(stalled.reserve(2.01,1000),.19)
        self.assertAlmostEqual(link.reserve(10,1000),0) # bounded burst credit
        for invalid in (0,float('nan'),float('inf'),101):
            with self.assertRaises(ValueError):SharedLink(invalid,0)

    def test_run_bounds(self):
        for invalid in (True,0,31,float('inf')):
            with self.assertRaises(ValueError):run(invalid)

if __name__=='__main__':unittest.main()
