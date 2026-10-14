"""Image-bound provenance, not current playback state."""
import hashlib
import json
from pathlib import Path
import re
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from audit_amp_level_control import level_control_parameters, first_b_level_control, RANGES
import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
IMAGE=Path(_WS + '/Firmwares/SoundAmp/'
           'uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin')
OUT=ROOT/'output/r2/soundamp/remaining-blocks-20260919'


@unittest.skipUnless(IMAGE.exists(),'fixed CodeFlash image required')
class LevelProvenanceTests(unittest.TestCase):
    def test_trim_ranges_and_override_inner_dispatch(self):
        r=level_control_parameters(IMAGE.read_bytes())
        self.assertEqual(r['normal_source']['trim_writers']['packet_lengths'],
                         {'single_field':[9,3,3],'composite':[23,5,5]})
        self.assertEqual(r['override']['mode_writer']['inner_function_id'],375)

    def test_b25_dispatch_and_instruction_pins(self):
        from audit_amp_output_bindings import loader_dm
        from extract_amp_sharc_app import parse_blocks
        from inspect_amp_visa import load_shortwords
        raw=(ROOT/'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        mem=load_shortwords(raw);dm=loader_dm(raw,parse_blocks(raw));data=IMAGE.read_bytes()
        r=first_b_level_control(data,dm,mem)
        self.assertEqual(r['pending_address'],'0xb9127')
        self.assertEqual(r['event_target'],'0xaa568')
        self.assertEqual(r['apply']['baseline_offset'],1090)
        self.assertEqual(r['apply']['curve_record_offsets'],[[773,782],[776,785],[779,788]])
        self.assertEqual(r,json.loads((OUT/'b_level_control.json').read_text()))
        dm[0xB913D+9]^=1
        with self.assertRaises(ValueError):first_b_level_control(data,dm,mem)

    def test_source_identity_and_rejected_mutation(self):
        data=IMAGE.read_bytes()
        report=level_control_parameters(data)
        self.assertEqual(report['level_word']['gp_relative'],-0x77F8)
        changed=bytearray(data);changed[0xC6980]^=1
        with self.assertRaises(ValueError):level_control_parameters(changed)

    def test_b_curve_targets_and_both_source_tables(self):
        from audit_amp_output_bindings import loader_dm
        from extract_amp_sharc_app import parse_blocks
        from inspect_amp_visa import load_shortwords
        raw=(ROOT/'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        mem=load_shortwords(raw);dm=loader_dm(raw,parse_blocks(raw));data=IMAGE.read_bytes()
        r=first_b_level_control(data,dm,mem)['curve_bindings']
        self.assertTrue(r['zero_curve_words'])
        self.assertEqual([v['effective_object_offset'] for v in r['bindings']],[421,592,676])
        self.assertEqual([v['channels'] for v in r['bindings']],[4,6,2])
        self.assertEqual(r['descriptor_layout']['level_slot'],6)
        self.assertEqual(r['baseline_effective_object_offset'],299+791)
        for row in r['bindings']:
            self.assertEqual(row['base_value']['loader_word'],row['base_value']['b01_word'])
            self.assertTrue(all(v['loader_word']==v['b01_word']=='0x0' for p in row['curve_records'] for v in p))
        mem[0x1314D3]^=1
        with self.assertRaises(ValueError):first_b_level_control(data,dm,mem)

    def test_exported_ranges_match_image_and_listings(self):
        data=IMAGE.read_bytes()
        report=json.loads((OUT/'level_control.json').read_text())
        self.assertEqual(report,level_control_parameters(data))
        for name,(a,b) in RANGES.items():
            self.assertEqual(report['ranges'][name]['sha256'],hashlib.sha256(data[a:b]).hexdigest())
            listing=(OUT/f'{name}.asm').read_text()
            self.assertIn(f'0x{a:08x}',listing)
            for invalid in ('invalid','truncated','UNKNOWN'):self.assertNotIn(invalid,listing)
            pc=a
            for line in listing.splitlines():
                match=re.match(r'0x([0-9a-f]+)\s+([0-9a-f]+)\s+',line)
                if not match:continue
                address=int(match[1],16);raw=bytes.fromhex(match[2])
                self.assertEqual(address,pc)
                self.assertEqual(raw,data[pc:pc+len(raw)])
                pc+=len(raw)
            self.assertEqual(pc,b)

    def test_request_ids_thresholds_and_conditional_examples(self):
        r=level_control_parameters(IMAGE.read_bytes())
        self.assertEqual(r['normal_source']['request_selectors'],[0x21,0x22,0x23,0x24,0x26,0x2A,0x32])
        self.assertEqual(r['normal_source']['request_source_indices'],[0,1,2,3,5,9,17])
        rows={v['raw_protocol_level']:v for v in r['settled_examples']['rows']}
        self.assertEqual(rows[70]['candidate_bank'],10)
        self.assertEqual(rows[70]['bank_55hz_design_gain_db'],6)
        self.assertEqual(rows[85]['candidate_bank'],7)
        self.assertEqual(rows[0]['candidate_bank'],15) # muted, not +20.5dB playback proof

    def test_nine_gain_slots_and_request_routing(self):
        r=json.loads((OUT/'b_level_control.json').read_text())['gain_controls']
        self.assertEqual([v['api'] for v in r['slots']],[19,18,22,21,23,24,25,27,29])
        self.assertEqual(r['slots'][7]['accepted_range'],[-200,400])
        self.assertEqual(r['slots'][1]['all_helper_descriptors'],[44,45,46,47,48,49,50,54,55,56,57,58])
        self.assertEqual(r['untouched'],{'51':[1]})
        self.assertEqual(r['api19_table'],[-100,-50,0,100])
        self.assertEqual(r['event_targets']['53'],'0xaa59a')

    def test_nine_slot_code_and_source_identity_fail_closed(self):
        from audit_amp_output_bindings import loader_dm
        from extract_amp_sharc_app import parse_blocks
        from inspect_amp_visa import load_shortwords
        from audit_amp_b_gain_controls import gain_controls
        raw=(ROOT/'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        mem=load_shortwords(raw);dm=loader_dm(raw,parse_blocks(raw));data=IMAGE.read_bytes()
        r=gain_controls(data,dm,mem)
        self.assertEqual(r,json.loads((OUT/'b_level_control.json').read_text())['gain_controls'])
        mem[0x12C618+1]=0x043e # corrupt slot argument, not a record value
        with self.assertRaises(ValueError):gain_controls(data,dm,mem)
        with self.assertRaises(ValueError):gain_controls(data[:-1],dm,mem)

    def test_api18_19_parameter_targets_and_eq_rebuild_bindings(self):
        r=json.loads((OUT/'b_level_control.json').read_text())['gain_controls']['parameter_controls']
        self.assertEqual([p['descriptor'] for p in r['apis'][0]['parameters']],list(range(44,59)))
        self.assertEqual([p['target_config_offset'] for p in r['apis'][0]['parameters']],
                         [35,79,122,147,198,249,293,377,316,317,420,307,312,391,396])
        self.assertEqual([p['descriptor'] for p in r['apis'][1]['parameters']],
                         [*range(44,51),*range(54,59)])
        self.assertEqual(r['delay_bindings']['object_offsets'],[615,616])
        self.assertEqual(r['delay_bindings']['channel_calls'],['0x12ebc9','0x12ec04'])
        self.assertEqual([(e['design_config_offset'],e['coefficient_config_offset'],e['channels'])
                         for e in r['eq_rebuilds']],[(305,295,6),(310,300,6),(389,379,2),(394,384,2)])
        for source in ('loader','b01'):
            for eq in r['eq_rebuilds']:
                self.assertEqual([v[source] for v in eq['design_words']],[20,100,0,0,1])
                self.assertEqual([v[source] for v in eq['coefficient_words']],[1,0,0,0,0])

    def test_api19_alternate_record_pairs_and_all_eq_curves_zero(self):
        r=json.loads((OUT/'b_level_control.json').read_text())['gain_controls']['parameter_controls']
        p={p['descriptor']:p for p in r['apis'][0]['parameters']}
        self.assertEqual({k:v['record_offsets'] for k,v in p[52]['variants'].items()},
            {'shadow16_eq5':[494,500],'shadow20_eq3':[427,433],'default':[533,578]})
        self.assertEqual({k:v['record_offsets'] for k,v in p[53]['variants'].items()},
            {'shadow16_eq5':[497,503],'shadow20_eq3':[430,436],'default':[536,581]})
        for row in r['apis']:
            for p in row['parameters']:
                if p['descriptor']<55:continue
                self.assertEqual(p['clamp'],[-100,20])
                self.assertTrue(all(v[s]==0 for pair in p['variants']['default']['records']
                                    for v in pair for s in ('loader','b01')))

    def test_api_parameter_proofs_reject_changed_code(self):
        from inspect_amp_visa import load_shortwords
        from audit_amp_b_parameter_controls import parameter_controls,PINS
        raw=(ROOT/'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        original=load_shortwords(raw)
        for a,_,_ in PINS:
            mem=original.copy();mem[a]^=1
            with self.subTest(pc=hex(a)):
                with self.assertRaises(ValueError):parameter_controls(mem,lambda offset:None)


if __name__=='__main__':unittest.main()
