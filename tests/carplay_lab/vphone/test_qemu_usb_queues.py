import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tests.carplay_lab.vphone.qemu_usb_queues import report
from tests.carplay_lab.vphone.sender_logs import summarize
from tests.carplay_lab.vphone.qemu_trace_build import instrument
from tests.carplay_lab.common import qnx_receiver


class USBQueueTests(unittest.TestCase):
    def evaluate(self, packets, bridge, native_footer=None):
        with tempfile.TemporaryDirectory() as d:
            a,b=Path(d)/'native',Path(d)/'bridge'
            native=[{'event':'begin','schema':1},*packets,
                    native_footer or {'event':'end','rows':len(packets),'truncated':False}]
            a.write_text(''.join(json.dumps(r)+'\n' for r in native))
            b.write_text(''.join(json.dumps(r)+'\n' for r in [
                {'event':'begin'},*bridge,{'event':'end','complete':True,'truncated':False}]))
            return report(a,b)

    def test_stages_and_wrapped_cumulative_ack(self):
        flow=['a'*32,'b'*32,51000,6030]
        def packet(stage,t,**updates):
            return {'event':'packet','stage':stage,'mono_ns':t*1000000,'flow':flow,
                    'seq':0xfffffff0,'ack':55,'flags':24,'payload_bytes':32,**updates}
        def host(direction,t,**updates):
            return {**packet('',t,**updates),'direction':direction,
                    'send_start_mono_ns':t*1000000,'read_mono_ns':t*1000000}
        reverse=[flow[1],flow[0],6030,51000]
        # Native buffering may reorder lines; joins use their original stamps.
        native=[packet('usb_complete',7),packet('socket_rx',2),packet('usb_accept',4),
                packet('usb_tx',15,flow=reverse,seq=55,ack=16,flags=16,payload_bytes=0)]
        v=self.evaluate(native,[host('rx',1),host('tx',16,flow=reverse,seq=55,ack=16,flags=16,payload_bytes=0)])
        p=v['ports']['6030']
        for stage,delay in [('bridge_to_qemu',1),('qemu_to_usb_accept',2),('usb_transfer',3),
                            ('usb_complete_to_ack',8),('ack_to_bridge',1)]:
            self.assertEqual(p[stage]['max_ms'],delay)
        self.assertEqual(v['unmatched_stages'],{})
        self.assertEqual(v['unacknowledged_delivered_segments'],0)

    def test_stale_ack_does_not_ack_new_data(self):
        f=['a','b',51000,6031]
        a={'event':'packet','stage':'usb_complete','mono_ns':5,'flow':f,'seq':100,'ack':0,'flags':24,'payload_bytes':50}
        b={**a,'stage':'usb_tx','mono_ns':7,'flow':['b','a',6031,51000],'seq':0,'ack':149,'flags':16,'payload_bytes':0}
        self.assertEqual(self.evaluate([a,b],[])['unacknowledged_delivered_segments'],1)

    def test_missing_stage_is_reported(self):
        row={'event':'packet','stage':'usb_complete','mono_ns':5,'flow':['a','b',1,6030],
             'seq':1,'ack':0,'flags':24,'payload_bytes':1}
        self.assertEqual(self.evaluate([row],[])['unmatched_stages'],{'usb_complete':1})

    def test_reset_is_not_delivery_ack(self):
        row={'event':'packet','stage':'usb_complete','mono_ns':5,'flow':['a','b',1,6030],
             'seq':1,'ack':0,'flags':24,'payload_bytes':1}
        reset={**row,'stage':'usb_tx','mono_ns':8,'flow':['b','a',6030,1],
               'seq':0,'ack':2,'flags':20,'payload_bytes':0}
        self.assertEqual(self.evaluate([row,reset],[])['unacknowledged_delivered_segments'],1)

    def test_identical_retransmissions_match_once_each(self):
        row={'event':'packet','stage':'socket_rx','mono_ns':1,'flow':['a','b',1,6030],
             'seq':1,'ack':0,'flags':24,'payload_bytes':1}
        native=[row,{**row,'mono_ns':2},
                {**row,'stage':'usb_accept','mono_ns':10},
                {**row,'stage':'usb_accept','mono_ns':20}]
        v=self.evaluate(native,[])['ports']['6030']['qemu_to_usb_accept']
        self.assertEqual(v['samples'],2)
        self.assertEqual(v['max_ms'],18/1e6)

    def test_incomplete_native_count_rejected(self):
        with self.assertRaises(ValueError):
            self.evaluate([],[],{'event':'end','rows':1})

    def test_instrumentation_refuses_changed_source(self):
        with self.assertRaises(ValueError):instrument('different QEMU source','socket')

    def test_diagnostic_binary_identity_cannot_escape_owned_build(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory).resolve()
            out=root/'build/vphone-lab/qnx-network'
            out.mkdir(parents=True)
            binary=out.parent/'qemu-net-trace/qemu-system-arm'
            record=out/'network-trace.json'
            record.write_text(json.dumps({'binary':str(binary)}))
            with patch.object(qnx_receiver,'ROOT',root),patch.object(qnx_receiver,'OUT',out):
                self.assertEqual(qnx_receiver.command('socket')[0],str(binary))
                record.write_text(json.dumps({'binary':'/tmp/foreign-qemu'}))
                with self.assertRaises(ValueError):qnx_receiver.command('socket')

    def test_native_sender_counters_keep_distinct_meanings(self):
        v=summarize({'result':{'entries':[
            {'date':'now','message':'fvdp_statsReport: stats/sec: | Submits 2 | Encodes 30 | IdlEncodes 28 | SinkODrops 7 |'},
            {'date':'now','message':'[0x1234] ### dropped 7 frames due to sink overflow'}]}})
        self.assertEqual(v['stats_per_second'][0]['counters'],{'Submits':2,'Encodes':30,'IdlEncodes':28,'SinkODrops':7})
        self.assertEqual(len(v['sink_overflow_notifications']),1)


if __name__=='__main__':unittest.main()
