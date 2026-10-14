"""Regression checks for the fixed-image, read-only amplifier audit."""

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import audit_amp_output_bindings as audit

import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
IMAGE = Path(_WS + "/Firmwares/SoundAmp/"
             "uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin")


class PureAuditTests(unittest.TestCase):
    def test_ram_copy_translation(self):
        self.assertEqual(audit.source_offset(-0x132FC), 0x102448)
        self.assertEqual(audit.source_offset(-0x12977), 0x102DCD)
        self.assertEqual(audit.source_offset(-0x138F8, 1), 0x1003AC)
        with self.assertRaises(ValueError):
            audit.source_offset(0)

    def test_reject_wrong_image(self):
        with self.assertRaises(ValueError):
            audit.audit(b"not an amp image")

    def test_level_zero_is_not_processing_bypass(self):
        self.assertEqual([audit.processing_row(i, True, True) for i in range(4)], [1,2,3,4])
        self.assertEqual(audit.processing_row(0, False, True), 0)
        self.assertEqual(audit.processing_row(0, True, False), 0)
        with self.assertRaises(ValueError):
            audit.processing_row(4, True, True)

    def test_tcb_batches_and_identity_do_not_consume_coefficients(self):
        rows = audit.output_eq_layout([17,0,12,5])
        self.assertEqual([r['start_record'] for r in rows], [0,17,17,29])
        self.assertEqual([r['tcb_stage_counts'] for r in rows], [[12,5],[1],[12],[5]])
        self.assertTrue(rows[1]['separate_identity_stage'])
        with self.assertRaises(ValueError):
            audit.output_eq_layout([145])


@unittest.skipUnless(IMAGE.exists(), "local original J525 image required")
class ImageAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = IMAGE.read_bytes()
        cls.result = audit.audit(cls.data)

    def test_permutations_and_routes(self):
        r = self.result
        self.assertEqual(r["split_modes"][0], r["split_modes"][1])
        p, q = r["v850_config_to_sharc_slot"], r["sharc_slot_to_v850_config"]
        self.assertEqual([q[p[i]] for i in range(16)], list(range(16)))
        self.assertEqual([c["ten_bus_source_modes"][0] for c in r["channels"]],
                         [0, 1, 9, 8, 8, 9, 7, 6, 4, 5, 2, 2, 0, 1, 3, 3])

    def test_counts_and_packing(self):
        r = self.result
        self.assertEqual(r["output_eq_count_sum"], 143)
        self.assertEqual(sum(r["input_bus_eq_counts"]), 55)
        assigned = [f["index"] for c in r["channels"] for f in c["packed_eq_records_inferred"]]
        self.assertEqual(sorted(assigned), list(range(143)))
        self.assertEqual(r["channels"][14]["eq_count"], 0)
        self.assertEqual(r["channels"][15]["eq_count"], 0)

    def test_schedule_and_matrix_node(self):
        r = self.result
        self.assertEqual(r["graph_schedule_count"], 79)
        nodes = {n["descriptor"]: n for n in r["graph_nodes"]}
        self.assertEqual(len(nodes), 79)
        self.assertEqual(len(nodes["0xc4aed"]["port_ids"]), 16)
        self.assertEqual(nodes["0xc4aed"]["process_visa_address_candidate"], "0x1269bc")
        self.assertEqual(len(nodes["0xc4df1"]["port_ids"]), 26)
        self.assertEqual(len(nodes["0xc4e6a"]["port_ids"]), 41)

    def test_frequency_lookup(self):
        for selector in (62, 100, 240, 255, 870, 1100, 4500, 6000):
            self.assertAlmostEqual(audit.frequency_lookup(self.data, selector)
                                   ["frequency_hz_from_cot"], selector, places=3)

    def test_dynamic_parameters_and_neutral_channel_trims(self):
        d = self.result['dynamic_stage_research']
        self.assertEqual(d['pre_complex_gain_writer']['neutral_table1_tenths_db'], [0]*10)
        self.assertEqual(d['pre_complex_gain_writer']['neutral_table2_tenths_db'], [0]*10)
        a, b = d['limiters']
        self.assertEqual([r['threshold_static'] for r in a['records_config_order']], [100]*12+[1]*4)
        self.assertEqual(a['loader_mode'], 0)
        self.assertAlmostEqual(b['records_config_order'][12]['threshold_static'], .59, places=6)
        self.assertEqual(b['records_config_order'][14]['threshold_static'], .5)
        self.assertEqual(d['first_b']['frames'], 512)
        self.assertEqual(d['second_b']['source_input_ids'], [8,9,10])

    def test_post_eq_binds_only_low_frequency_outputs(self):
        for e in self.result['known_electronic_stages']['post_output_eq']:
            self.assertEqual(e['counts_config_order'], [0]*12+[2]*4)
            self.assertEqual(e['packed_source_record_order'], [0,1,2,3,6,7,4,5])

    def test_proof_boundary_and_b01_copy(self):
        for channel in self.result["channels"]:
            self.assertEqual(channel["physical_candidate"]["status"],
                             "inference_not_direct_pinout_proof")
        self.assertEqual(self.result["b01_copy"]["source"], "0x12000c")
        self.assertEqual(self.result["b01_copy"]["words"], 3562)

    def test_all_five_gain_rows_and_exact_mute_sentinel(self):
        r = self.result
        self.assertEqual(len(r['ten_bus_pair_gain_rows_db']), 5)
        self.assertEqual(r['level_parameter_values'], [-100.0,-50.0,0.0,100.0])
        self.assertEqual(r['gain_lookup_minus100_db'], 0.0)
        self.assertAlmostEqual(r['gain_lookup_minus99_db'], 10**(-99/20), places=11)
        self.assertEqual(r['ten_bus_pair_gain_rows_db'][1][-4:], [-100,-99,-100,-99])

    def test_known_electronic_stages_are_extracted_with_units_and_order(self):
        k=self.result['known_electronic_stages']
        self.assertEqual(k['input_matrix']['file_offset'],'0x101bf4')
        self.assertAlmostEqual(k['input_matrix']['linear'][0][3],10**(-99/20),places=11)
        self.assertEqual([d['samples'] for d in k['pre_eq_delays']],
                         [[1632]*8,[87,87,125,204,403,403,367,367]])
        m=k['output_pair_mixer']
        self.assertEqual(m['a_file_offset'],'0x10032c')
        self.assertEqual(m['a_codes'][3],47)
        self.assertAlmostEqual(m['a_linear'][3],10**(9.4/20),places=6)
        self.assertEqual(m['b_linear'][0],0)
        self.assertAlmostEqual(m['b_linear'][3],10**(6/20),places=6)
        self.assertEqual(len(k['row_eq']['rows']),5)
        records=k['row_eq']['rows'][1]+[r for bank in k['post_output_eq'] for r in bank['records']]
        self.assertEqual(len(records),32)
        self.assertTrue(all(r['type_code']==1 and r['gain_linear']==1 for r in records))

    def test_profile_one_uses_its_own_matrix_and_mixer(self):
        k=audit.audit(self.data,profile=1)['known_electronic_stages']
        self.assertEqual(k['input_matrix']['db'][3][3],4)
        self.assertEqual(k['output_pair_mixer']['a_codes'][1],20)
        self.assertEqual(k['output_pair_mixer']['a_file_offset'],'0x10069c')


if __name__ == "__main__":
    unittest.main()
