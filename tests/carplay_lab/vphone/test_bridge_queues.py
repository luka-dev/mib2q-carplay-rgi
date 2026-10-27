import json
from pathlib import Path
import struct
import tempfile
import unittest

from tests.carplay_lab.vphone.bridge_queues import Journal, report, transport


def packet(seq, ack=0, reverse=False, data=b'x'):
    src,dst=(6030,49152) if reverse else (49152,6030)
    segment=struct.pack('>HHIIBBHHH',src,dst,seq,ack,0x50,0x18,65535,0,0)+data
    addresses=bytes(range(16,32))+bytes(range(16)) if reverse else bytes(range(32))
    return bytes(12)+b'\x86\xdd'+struct.pack('>IHBB',0x60000000,len(segment),6,64)+addresses+segment


class QueueTests(unittest.TestCase):
    def test_stages_and_qnx_ack_use_one_host_monotonic_clock(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'queues.jsonl'
            trace=Journal(path,{'clock':'host'})
            trace.packet('rx',packet(100),1000000000,1020000000,10,1000010,2000010)
            trace.packet('tx',packet(900,ack=101,reverse=True,data=b''),1030000000,1030000000,5000010,5000020,5000030)
            trace.check(0,1000000);trace.close(True)
            result=report(path);stats=result['ports']['6030']
            self.assertEqual(stats['capture_to_read']['max_ms'],20)
            self.assertEqual(stats['user_queue']['max_ms'],1)
            self.assertEqual(stats['send_call']['max_ms'],1)
            self.assertEqual(stats['qnx_ack_observed']['max_ms'],3)
            self.assertEqual(result['unacknowledged_screen_segments'],0)
            self.assertTrue(result['complete'])
            self.assertNotIn('encrypted',path.read_text())

    def test_wrapping_sequence_and_stale_ack_do_not_ack_future_data(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'queues.jsonl';trace=Journal(path,{})
            trace.packet('rx',packet(0xffffffff,data=b'ab'),0,0,1,2,3)
            trace.packet('tx',packet(90,ack=0xffffffff,reverse=True,data=b''),0,0,4,5,6)
            trace.packet('tx',packet(90,ack=1,reverse=True,data=b''),0,0,10,11,12)
            trace.close(True)
            result=report(path)
            self.assertEqual(result['ports']['6030']['qnx_ack_observed']['samples'],1)
            self.assertEqual(result['unacknowledged_screen_segments'],0)

    def test_negative_wall_age_is_reported_as_unusable(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'queues.jsonl';trace=Journal(path,{})
            trace.packet('rx',packet(100),20,10,1,2,3);trace.close(False)
            result=report(path)
            self.assertEqual(result['negative_wall_clock_age_samples'],1)
            self.assertFalse(result['complete'])
            self.assertNotIn('capture_to_read',result['ports']['6030'])

    def test_incomplete_and_regressing_traces_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'queues.jsonl';trace=Journal(path,{})
            trace.packet('rx',packet(100),0,0,4,3,2);trace.close(True)
            with self.assertRaises(ValueError):report(path)
            path.write_text(json.dumps({'event':'begin'})+'\n')
            with self.assertRaises(ValueError):report(path)

    def test_tcp_ack_field_and_non_tcp_filter(self):
        self.assertEqual(transport(packet(100,ack=42))['ack'],42)
        self.assertEqual(transport(bytes(14)),{})

if __name__=='__main__':unittest.main()
