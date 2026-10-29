"""Protocol evidence and isolation of the native API/QNX byte adapter."""
import struct
import json
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch

from tests.carplay_lab.legacy_ios import usb_probe as wire
from tests.carplay_lab.common import mfi_sap
from tests.carplay_lab.vphone import wireless_native as native, wireless_link


class NativeDaemonTests(unittest.TestCase):
    def test_headless_paired_profile_allows_uart_capture_and_keeps_guards(self):
        builder=native.qnx_receiver
        with patch.object(builder,"running",return_value=123):
            with self.assertRaisesRegex(RuntimeError,"stop the owned QNX"):
                builder.build("unused",pairing=True,media=True,software_mfi=True,
                              paired_altscreen=True,capture_video=True,capture_only_altscreen=True)
        for options in ({"paired_altscreen":True}, {"capture_video":True}):
            with self.assertRaisesRegex(ValueError,"capture-only"):
                builder.build("unused",capture_only_altscreen=True,**options)

    def test_readiness_does_not_reuse_previous_dio_epoch(self):
        transport='[bridge] listening on 127.0.0.1:57342\n[native-api-relay] ready\n'
        stock='[dio-bridge] DNSServiceRegister status=0\n[native-mfi-sap-qemu] guarded request slot wrapped\n'
        first=transport+'[native-dio-owner] starting epoch=1 child=10\n'+stock
        self.assertTrue(native.receiver_ready(first))
        ended=first+'[native-dio-owner] ended epoch=1 wait_status=0\n'
        self.assertFalse(native.receiver_ready(ended))
        second=ended+'[native-dio-owner] starting epoch=2 child=11\n'
        self.assertFalse(native.receiver_ready(second))
        self.assertTrue(native.receiver_ready(second+stock))
        self.assertFalse(native.receiver_ready(stock))
    def test_recovery_refuses_unverified_or_replaced_receiver_before_guest_mutation(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(native.lab,'OUT',Path(directory)), \
             patch.object(native.qnx_receiver,'set_output'), patch.object(native.qnx_receiver,'running',return_value=123), \
             patch.object(native.lab,'rpc') as rpc:
            for video,pid in ((False,123),(True,124)):
                (Path(directory)/'wireless-native-daemon.json').write_text(json.dumps({'video_verified':video,'qnx_pid':pid}))
                with self.assertRaises(RuntimeError):native.recover()
            rpc.assert_not_called()

    def test_recovery_never_targets_a_similarly_named_or_other_uid_process(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(native.lab,'OUT',Path(directory)), \
             patch.object(native.qnx_receiver,'set_output'), patch.object(native.qnx_receiver,'running',return_value=123), \
             patch.object(native.lab,'profile',return_value={}), patch.object(native.lab,'require_machine'), \
             patch.object(native,'check_mfi_service'), patch.object(native.lab,'rpc') as rpc:
            (Path(directory)/'wireless-native-daemon.json').write_text(json.dumps({'video_verified':True,'qnx_pid':123}))
            for row in ({'name':'fairplayd','executable':'/usr/libexec/airplayd','uid':501},
                        {'name':'airplayd','executable':'/unowned/airplayd','uid':501},
                        {'name':'airplayd','executable':'/usr/libexec/airplayd','uid':0}):
                rpc.return_value={'result':{'processes':[row]}}
                with self.assertRaises(RuntimeError):native.recover()
            self.assertTrue(all(call.args[1]=='processes.list' for call in rpc.call_args_list))

    def test_invalid_recovery_duration_is_rejected_before_reading_state(self):
        for seconds in (True,0,151):
            with self.assertRaises(ValueError):native.recover(seconds)

    def test_bridge_abi_and_invalid_frames(self):
        self.assertEqual(native.frame(1), struct.pack("<BBHI",1,0,0,1))
        self.assertEqual(native.frame(2,b"abc"),struct.pack("<BBHI",2,0,3,1)+b"abc")
        for kind,data in ((0,b""),(4,b""),(2,b""),(1,b"x"),(3,b"x"),(2,bytes(5036))):
            with self.assertRaises(ValueError):native.frame(kind,data)

    def test_native_evidence_needs_actual_direction_and_messages(self):
        rows=[]
        for direction,ids in (("ios-to-qnx",(0xAA05,0x1D02,0x5702)),("qnx-to-ios",(0x5703,))):
            data=wire.IAP2_DETECT+b"".join(wire.link_frame(0x40,i+1,session=10,payload=wire.control_message(mid)) for i,mid in enumerate(ids))
            rows.extend({"direction":direction,"hex":bytes((byte,)).hex()} for byte in data)
        proof=native.observations(rows)
        self.assertTrue(proof["native_authentication_verified"])
        self.assertTrue(proof["native_identification_verified"])
        self.assertTrue(proof["native_wifi_response_sent"])
        self.assertFalse(proof["daemon_start_session_sent"])
        self.assertFalse(proof["native_carplay_availability_received"])
        self.assertFalse(native.observations([])["native_authentication_verified"])

    def test_control_fragmentation_retransmission_and_checksum(self):
        message=wire.control_message(0x4301)
        first=wire.link_frame(0x40,1,session=10,payload=message[:3])
        second=wire.link_frame(0x40,2,session=10,payload=message[3:])
        rows=[{"direction":"qnx-to-ios","hex":part.hex()} for part in (first,first,second)]
        proof=native.observations(rows)
        self.assertEqual(proof["control_messages"]["qnx-to-ios"],["0x4301"])
        broken=bytearray(second);broken[-1]^=1
        with self.assertRaises(ValueError):native.observations([{"direction":"qnx-to-ios","hex":broken.hex()}])

    def test_availability_and_bound_start_are_decoded_from_actual_messages(self):
        def packet(mid,parameters):
            return wire.link_frame(0x40,mid&255,session=10,payload=b'@@'+struct.pack('>HH',len(parameters)+6,mid)+parameters)
        availability=wire.parameter(1,wire.parameter(0,b'\x01')+wire.parameter(1,b'02:01:02:03:04:05\0'))
        start=wire.parameter(1,b'')+wire.parameter(2,(5000).to_bytes(4,'big'))+wire.parameter(4,b'actual-receiver-pi\0')
        proof=native.observations([{'direction':'ios-to-qnx','hex':packet(0x4300,availability).hex()},
                                   {'direction':'qnx-to-ios','hex':packet(0x4301,start).hex()}])
        self.assertEqual(proof['availability'],[{'wireless_available':True,'bluetooth_transport_id':'02:01:02:03:04:05'}])
        self.assertEqual(proof['start_sessions'][0]['pairing_id'],'actual-receiver-pi')
        self.assertEqual(proof['start_sessions'][0]['port'],5000)
        self.assertFalse(proof['native_authentication_verified'])

    def test_only_one_new_virtual_member_can_be_selected(self):
        before=wireless_link.topology("bridge100: flags=x\n member: vmenet0 flags=x\nvmenet0: flags=x\n")
        after=wireless_link.topology("bridge100: flags=x\n member: vmenet0 flags=x\n 42:5d:fd:99:3c:57 Vlan1 vmenet0 100 flags=x\nvmenet0: flags=x\nbridge101: flags=x\n member: vmenet2 flags=x\nvmenet2: flags=x\n")
        self.assertEqual(after['cache']['425dfd993c57'],('bridge100','vmenet0'))
        self.assertEqual(wireless_link.new_member(before,after),('bridge101','vmenet2'))
        with self.assertRaises(ValueError):wireless_link.new_member(before,before)
        after['names'].add('vmenet3')
        with self.assertRaises(ValueError):wireless_link.new_member(before,after)
        for bridge,member in (('en0','vmenet1'),('bridge100','en0'),('bridge100;id','vmenet1')):
            with self.assertRaises(ValueError):wireless_link.change(bridge,'addm',member)

    def test_darwin_snapshot_queries_virtual_bridge_cache_explicitly(self):
        from types import SimpleNamespace
        general='bridge0: flags=x\n member: en1 flags=x\nbridge100: flags=x\n member: vmenet0 flags=x\nvmenet0: flags=x\n'
        detail='bridge100: flags=x\n member: vmenet0 flags=x\n 42:5d:fd:99:3c:57 Vlan1 vmenet0 100 flags=x\n'
        with patch.object(wireless_link.subprocess,'run',side_effect=[SimpleNamespace(stdout=general),SimpleNamespace(stdout=detail)]) as run:
            self.assertEqual(wireless_link.snapshot()['cache']['425dfd993c57'],('bridge100','vmenet0'))
            self.assertEqual([call.args[0] for call in run.call_args_list],[['/sbin/ifconfig','-a'],['/sbin/ifconfig','bridge100']])

    def test_invalid_mfi_operations_never_sign(self):
        with patch.object(mfi_sap,"sign_digest") as signer:
            for request in (b"",bytes(32),b"\x04"+bytes(32),b"\x03"+bytes(31)+b"x"):
                with self.assertRaises(ValueError):mfi_sap.respond_request(request,None,b"cert")
            signer.assert_not_called()
            self.assertEqual(mfi_sap.respond_request(b"\x03"+bytes(32),None,b"cert"),b"cert")


class VirtualLinkRecoveryTests(unittest.TestCase):
    def setUp(self):
        self.record={"status":"moving","qnx_pid":123,"qnx_member":"vmenet2",
                     "source_bridge":"bridge101","target_bridge":"bridge100","vm_member":"vmenet0"}
        self.network={"names":{"vmenet0","vmenet2","bridge100","bridge101"},
                      "bridges":{"bridge100":{"vmenet0"},"bridge101":set()},"cache":{}}

    def restore(self, network=None, pid=123):
        with patch.object(wireless_link,"state",return_value=self.record), \
             patch.object(wireless_link.os,"geteuid",return_value=0), \
             patch.object(wireless_link.qnx_receiver,"running",return_value=pid), \
             patch.object(wireless_link,"snapshot",return_value=network or self.network), \
             patch.object(wireless_link,"change") as change, \
             patch.object(wireless_link,"save_state") as save:
            result=wireless_link.leave()
            return result,change.call_args_list,save.call_args_list

    def test_restore_detached_member_after_partial_join(self):
        result,changes,saved=self.restore()
        self.assertEqual(result['status'],'restored')
        self.assertEqual([row.args for row in changes],[('bridge101','addm','vmenet2')])
        self.assertEqual(len(saved),1)

    def test_restore_joined_member_without_touching_native_member(self):
        self.network['bridges']['bridge100'].add('vmenet2')
        _,changes,_=self.restore()
        self.assertEqual([row.args for row in changes],[('bridge100','deletem','vmenet2'),('bridge101','addm','vmenet2')])

    def test_restore_refuses_changed_process_or_physical_bridge(self):
        with self.assertRaises(RuntimeError):self.restore(pid=999)
        self.network['bridges']['bridge100'].add('en0')
        with self.assertRaises(RuntimeError):self.restore()

    def test_exited_qemu_with_removed_member_needs_no_network_mutation(self):
        self.network['names'].remove('vmenet2')
        result,changes,_=self.restore(pid=None)
        self.assertTrue(result['qnx_exited'])
        self.assertEqual(changes,[])
        self.assertEqual(result['status'],'restored')

    def test_restore_refuses_unexpected_membership(self):
        self.network['bridges']['bridge102']={'vmenet2'}
        with self.assertRaises(RuntimeError):self.restore()

    def test_failed_restore_preserves_pending_state(self):
        with patch.object(wireless_link,"state",return_value=self.record), \
             patch.object(wireless_link.os,"geteuid",return_value=0), \
             patch.object(wireless_link.qnx_receiver,"running",return_value=123), \
             patch.object(wireless_link,"snapshot",return_value=self.network), \
             patch.object(wireless_link,"change",side_effect=OSError('injected add failure')), \
             patch.object(wireless_link,"save_state") as save:
            with self.assertRaises(OSError):wireless_link.leave()
            save.assert_not_called()
            self.assertEqual(self.record['status'],'moving')


if __name__=="__main__":unittest.main()
