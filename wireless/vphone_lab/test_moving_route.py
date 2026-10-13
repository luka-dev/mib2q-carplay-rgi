import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch

from wireless.vphone_lab import moving_route as route


class MovingRouteTests(unittest.TestCase):
    def test_invalid_duration_does_not_contact_guest(self):
        with patch.object(route.lab, 'require_machine') as owned:
            for value in [True, 1, 61, 2.5, '40']:
                with self.assertRaises(ValueError): route.run(value)
            owned.assert_not_called()

    def test_host_location_sync_is_rejected_before_mutation(self):
        with patch.object(route.lab, 'require_machine', return_value={'syncsHostLocation': True}), patch.object(route.lab, 'rpc') as rpc:
            with self.assertRaises(RuntimeError): route.run(2)
            rpc.assert_not_called()

    def test_path_is_bounded_and_guest_is_left_stationary(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(route.lab,'OUT',Path(directory)), \
                patch.object(route.lab,'require_machine',return_value={'syncsHostLocation': False}), \
                patch.object(route.lab,'rpc',return_value={'ok':True}) as rpc, patch.object(route.time,'sleep'):
            result=route.run(2)
            self.assertEqual(len(result['events']),3)
            self.assertEqual(rpc.call_args.args[1],'location.set')
            self.assertEqual(rpc.call_args.args[2]['speed'],0)
            self.assertFalse(result['host_location_modified'])
            self.assertTrue(result['complete'])
            self.assertEqual(result['events'][0]['params']['latitude'],route.START[0])
            self.assertEqual(result['events'][-1]['params']['longitude'],route.END[1])

    def test_partial_failure_still_saves_handoff_and_stops_simulated_motion(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(route.lab,'OUT',Path(directory)), \
                patch.object(route.lab,'require_machine',return_value={'syncsHostLocation': False}), \
                patch.object(route.lab,'rpc',side_effect=[{'ok':True},RuntimeError('guest disconnected'),{'ok':True}]) as rpc, patch.object(route.time,'sleep'):
            with self.assertRaises(RuntimeError): route.run(2)
            self.assertTrue((Path(directory)/'moving-route.json').is_file())
            self.assertEqual(rpc.call_args.args[2]['speed'],0)

if __name__ == '__main__' : unittest.main()
