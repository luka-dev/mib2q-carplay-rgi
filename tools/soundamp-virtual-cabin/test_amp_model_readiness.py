import copy
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
import soundfile as sf

from amp_model_readiness import (B_CASES, BANDS, GROUPS, CONFIG_CHANNELS, MOST_CHANNELS, acoustic_audit,
                                 audit, coverage_audit, fingerprint, model_gates)


class ReadinessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)

    def file(self, name, data):
        path = self.base / name
        path.write_text(json.dumps(data))
        return fingerprint(path)

    def coverage(self):
        ref = self.file('grid.json', {'complete': True, 'bands': BANDS,
                                     'tracks': {'song': {'variants': {'candidate': {}}}}})
        audit_ref = self.file('audit.json', {'fixture': True})
        scenario = {'audit_path': audit_ref['path'], 'bank_index': 7}
        scenario_ref = self.file('scenario.json', scenario)
        cell = {'groups': {k: {'power_dbfs': [-3.] * 7, 'bands_re_dry_L': [0.] * 7,
                               'peak_out_dbfs': -1.} for k in GROUPS},
                'peak_out_dbfs': [-3.] * 16, 'bands_re_dry_L': [[0.] * 7 for _ in range(16)],
                'whole_clip': {'peak_dbfs': -1., 'frames_above_0dbfs': 0,
                               'frames_above_minus1dbfs': 0, 'frames': 64}}
        return {'complete': True, 'grid': ref, 'names': ['candidate'],
                'scenarios': {'bank7': scenario}, 'scenario_models': {'bank7': {}},
                'common_input_audit': audit_ref, 'scenario_files': [scenario_ref],
                'bands': BANDS, 'frames_rendered': 64,
                'tracks': {'song': {'bank7': {v: {b: copy.deepcopy(cell) for b in B_CASES}
                                            for v in ('dry_bank1', 'candidate')}}}}

    def acoustic(self, domain='most_inputs'):
        capture = {'seat': 'driver', 'state': {'bank': 1, 'bass': 0},
                   'microphone_calibration': 'fixture', 'level_reference': 'fixed gain'}
        a = {'domain': domain, 'sample_rate': 48000,
             'channels': MOST_CHANNELS if domain == 'most_inputs' else CONFIG_CHANNELS,
             'capture': capture, 'provenance': {'kind': 'measured'},
             'response_hashes_verified': True, 'responses_validated': True,
             'stage_order': ['upmix_pcm', 'measured_brir'] if domain == 'most_inputs'
                            else ['upmix_pcm', 'amp_chain', 'measured_brir']}
        wav = self.base / 'fixture.wav'
        sf.write(wav, np.ones((1, 2)) * .1, 48000, subtype='FLOAT')
        manifest = {'schema_version': 1, 'sample_rate': 48000, 'domain': domain,
                    'channels': a['channels'], 'capture': capture,
                    'provenance': {'kind': 'measured', 'description': 'UNIT TEST fixture, not real cabin'},
                    'responses': {k: {'file': wav.name, 'sha256': fingerprint(wav)['sha256'], 'gain_db': 0.}
                                  for k in a['channels']}}
        a['provenance'] = manifest['provenance']
        a['manifest'] = self.file('manifest.json', manifest)
        evidence = {k: self.file(k + '.json', {'fixture': k}) for k in
                    ('capture_recording', 'state_record', 'microphone_calibration', 'config_order_mapping')}
        evidence.update(seat='driver', capture_state=capture['state'])
        return {'acoustics': a, 'model': None if domain == 'most_inputs' else {'fixture': True}}, evidence

    def test_defaults_do_not_certify_state_or_complete_model(self):
        from amp_dsp_chain import AmpChain
        result = audit(AmpChain().metadata())
        self.assertTrue(result['claims']['conditional_electrical_comparison_available'])
        self.assertFalse(result['claims']['all_electrical_scenarios_explicit'])
        self.assertFalse(result['claims']['actual_vehicle_state_verified'])
        self.assertFalse(result['claims']['full_cabin_fidelity_established'])
        self.assertIn('second_B_scenario_explicit', result['electrical']['omissions_or_hypotheses'])

    def test_explicit_hypotheses_are_not_live_proof(self):
        model = {'version': 'test', 'source_sha256': 'a' * 64}
        model.update({k: {'test_fixture': True} for k in ('upstream_scenario', 'first_b_scenario',
                                     'second_b_scenario', 'complex_scenario', 'runtime_scenario')})
        result = audit(model)
        self.assertTrue(result['claims']['all_electrical_scenarios_explicit'])
        self.assertFalse(result['claims']['actual_vehicle_state_verified'])

    def test_coverage_checks_missing_cell_despite_complete_flag(self):
        report = self.coverage()
        self.assertTrue(coverage_audit(report)['ready'])
        del report['tracks']['song']['bank7']['candidate']['B2_minus']
        result = coverage_audit(report)
        self.assertFalse(result['ready'])
        self.assertEqual(result['expected_cells'], 14)
        self.assertEqual(result['cells_present'], 13)

    def test_incomplete_empty_or_tampered_coverage_rejected(self):
        report = self.coverage()
        report['complete'] = False
        self.assertFalse(coverage_audit(report)['ready'])
        report['complete'] = True
        report['grid']['sha256'] = '0' * 64
        self.assertFalse(coverage_audit(report)['ready'])
        self.assertFalse(coverage_audit({})['ready'])

    def test_nonfinite_coverage_cell_rejected(self):
        report = self.coverage()
        report['tracks']['song']['bank7']['candidate']['B0']['peak_out_dbfs'][0] = float('nan')
        self.assertFalse(coverage_audit(report)['ready'])

    def test_fixture_evidence_never_certifies_full_fidelity(self):
        acoustic, evidence = self.acoustic()
        result = audit({}, acoustic=acoustic, evidence=evidence)
        self.assertTrue(result['claims']['measured_cabin_evidence_package_ready_for_review'])
        self.assertFalse(result['claims']['full_cabin_fidelity_established'])
        self.assertFalse(result['acoustic']['hardware_measurement_independently_verified'])

    def test_synthetic_or_double_processed_end_to_end_ir_rejected(self):
        report, evidence = self.acoustic()
        report['acoustics']['provenance']['kind'] = 'synthetic'
        self.assertFalse(acoustic_audit(report, evidence)['evidence_package_complete'])
        report['acoustics']['provenance']['kind'] = 'measured'
        report['acoustics']['stage_order'].insert(1, 'amp_chain')
        self.assertFalse(acoustic_audit(report, evidence)['gates']['no_duplicate_electronics'])

    def test_config_order_mapping_required_for_postamp(self):
        report, evidence = self.acoustic('post_amp_config_outputs')
        self.assertTrue(acoustic_audit(report, evidence)['evidence_package_complete'])
        del evidence['config_order_mapping']
        self.assertFalse(acoustic_audit(report, evidence)['evidence_package_complete'])

    def test_capture_mismatch_or_tampered_evidence_rejected(self):
        report, evidence = self.acoustic()
        evidence['seat'] = 'passenger'
        self.assertFalse(acoustic_audit(report, evidence)['evidence_package_complete'])
        evidence['seat'] = 'driver'
        Path(evidence['state_record']['path']).write_text('changed')
        self.assertFalse(acoustic_audit(report, evidence)['gates']['provenance_files_hash_match'])

    def test_ir_file_replaced_after_render_rejected(self):
        report, evidence = self.acoustic()
        self.assertTrue(acoustic_audit(report, evidence)['evidence_package_complete'])
        sf.write(self.base / 'fixture.wav', np.zeros((2, 2)), 48000, subtype='FLOAT')
        result = acoustic_audit(report, evidence)
        self.assertFalse(result['gates']['current_manifest_and_IRs_match_report'])
        self.assertIn('hash mismatch', result['response_revalidation']['reason'])

    def test_missing_scenario_and_common_audit_references_rejected(self):
        for key in ('scenario_files', 'common_input_audit'):
            report = self.coverage()
            del report[key]
            self.assertFalse(coverage_audit(report)['ready'], key)

    def test_duplicate_axes_and_unexpected_banks_variants_rejected(self):
        report = self.coverage()
        report['names'].append('candidate')
        self.assertFalse(coverage_audit(report)['ready'])
        report = self.coverage()
        report['tracks']['song']['bank10'] = {}
        self.assertFalse(coverage_audit(report)['ready'])
        report = self.coverage()
        report['tracks']['song']['bank7']['unknown'] = {}
        self.assertFalse(coverage_audit(report)['ready'])

    def test_scenario_reference_must_match_declared_scenario(self):
        report = self.coverage()
        report['scenarios']['bank7']['bank_index'] = 10
        self.assertFalse(coverage_audit(report)['ready'])

    def test_group_shape_and_whole_clip_required(self):
        for key in ('groups', 'whole_clip', 'bands_re_dry_L'):
            report = self.coverage()
            report['tracks']['song']['bank7']['candidate']['B0'][key] = {'placeholder': True}
            self.assertFalse(coverage_audit(report)['ready'], key)

    def test_extra_variant_requires_pinned_compatible_extra_report(self):
        report = self.coverage()
        grid_path = Path(report['grid']['path'])
        grid = json.loads(grid_path.read_text())
        grid['tracks']['song']['variants'] = {}
        grid_path.write_text(json.dumps(grid))
        report['grid'] = fingerprint(grid_path)
        self.assertFalse(coverage_audit(report)['ready'])
        extra = copy.deepcopy(grid)
        extra['tracks']['song']['variants']['candidate'] = {}
        report['extra_report'] = self.file('extra.json', extra)
        self.assertTrue(coverage_audit(report)['ready'])
        report['extra_report']['sha256'] = '0' * 64
        self.assertFalse(coverage_audit(report)['ready'])

    def test_model_presence_must_match_acoustic_domain(self):
        report, evidence = self.acoustic()
        report['model'] = {'version': 'bad-double-DSP'}
        self.assertFalse(acoustic_audit(report, evidence)['gates']['no_duplicate_electronics'])
        report, evidence = self.acoustic('post_amp_config_outputs')
        report['model'] = None
        self.assertFalse(acoustic_audit(report, evidence)['gates']['no_duplicate_electronics'])


if __name__ == '__main__':
    unittest.main()
