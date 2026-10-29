import copy
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
import soundfile as sf

import audit_upmix_end_to_end as runner


class FakeChain:
    calls = []

    def __init__(self, **kwargs):
        self.kwargs = kwargs

    def metadata(self):
        return {'version': 'TEST ONLY mock electronics', 'kwargs': self.kwargs}

    def process(self, pcm, **kwargs):
        self.calls.append((copy.deepcopy(self.kwargs), copy.deepcopy(kwargs), pcm.copy()))
        out = np.tile(pcm[:, :1], (1, 16)) * 4
        return {'out': out}


class EndToEndRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.source = self.base / 'Song.wav'
        self.audio = np.zeros((4096, 2), dtype=np.int16)
        self.audio[:2048, 0] = (np.sin(np.arange(2048) * 2 * np.pi * 100 / 48000) * 25000).astype(np.int16)
        self.audio[:, 1] = self.audio[:, 0] // 2
        sf.write(self.source, self.audio, 48000, subtype='PCM_16')
        self.renderer = self.base / 'renderer'
        self.renderer.write_bytes(b'test-only mocked binary')
        self.renderer.chmod(0o700)
        self.model = self.base / 'model'
        self.guard = self.base / 'guard'
        self.model.write_bytes(b'test main')
        self.guard.write_bytes(b'test guard')
        self.args = runner.parser().parse_args([
            '--renderer', str(self.renderer), '--model', str(self.model), '--guard', str(self.guard),
            '--out', str(self.base / 'report'), '--input', str(self.source), '--seconds', str(2048 / 48000),
            '--tail-seconds', str(512 / 48000), '--variant', 'test:float:.3:0:0:1', '--scenario', 'baseline_v7'])
        FakeChain.calls = []

    def fake_render(self, cmd, **kwargs):
        source, rate = sf.read(cmd[-2], dtype='int16', always_2d=True)
        pcm = np.zeros((len(source), 6), dtype=np.int16)
        pcm[512:, :2] = source[:-512]
        sf.write(cmd[-1], pcm, rate, subtype='PCM_16')
        return SimpleNamespace(returncode=0, stdout='mocked C', stderr='minimum-guard-gain=1 total-frames=' + str(len(source)))

    def execute(self, renderer=None):
        with patch.object(runner, 'AmpChain', FakeChain), \
             patch.object(runner, 'source_provenance', return_value=[]), \
             patch.object(runner.subprocess, 'run', side_effect=renderer or self.fake_render), \
             patch.object(runner, 'make_test_scenario', side_effect=lambda **kw: {'parameters': kw}), \
             patch.object(runner, 'scenario_manifest', side_effect=lambda kw: copy.deepcopy(kw)):
            return runner.run(self.args)

    def test_complete_grid_preserves_overs_without_wavs(self):
        report = self.execute()
        self.assertTrue(report['complete'])
        self.assertEqual(report['expected_rows'], 2)
        self.assertEqual(report['completed_rows'], 2)
        track = report['tracks']['Song']
        row = track['scenarios']['baseline_v7']['rows']['test']
        self.assertIsNone(row['file'])
        self.assertFalse(row['output_wav_retained'])
        self.assertGreater(row['metrics']['outputs']['whole_clip']['peak_dbfs'], 0)
        self.assertEqual(row['output_arrays']['measurement_float32']['shape'], [2560, 16])
        self.assertEqual(row['output_arrays']['measurement_float32']['dtype'], '<f4')
        self.assertEqual(row['metrics']['analysis']['programme_window_frames'], [0, 2048])
        self.assertEqual(row['metrics']['analysis']['tail_window_frames'], [2048, 2560])
        self.assertEqual(report['summary']['baseline_v7']['test']['tracks'], 1)
        for delta in row['delta_vs_dry_bank1'].values():
            np.testing.assert_array_equal(delta, np.zeros(7))
        pcm = runner.read_pcm6(track['pcm6']['dry_bank1']['file']['path'], 2560)
        np.testing.assert_array_equal(pcm[:512], 0)
        np.testing.assert_array_equal(pcm[512:, :2], self.audio[:2048] / 32768)
        np.testing.assert_array_equal(pcm[:, 2:], 0)
        padded, _ = sf.read(track['source_padded']['path'], dtype='int16')
        np.testing.assert_array_equal(padded[:2048], self.audio[:2048])
        np.testing.assert_array_equal(padded[2048:], 0)
        self.assertFalse(list(self.args.out.rglob('*_16ch.wav')))

    def test_saved_float_outputs_match_hashes_and_preserve_overs(self):
        self.args.save_output_wavs = True
        report = self.execute()
        row = report['tracks']['Song']['scenarios']['baseline_v7']['rows']['test']
        self.assertEqual(sf.info(row['file']['path']).subtype, 'FLOAT')
        audio, _ = sf.read(row['file']['path'], always_2d=True)
        self.assertGreater(abs(audio).max(), 1)
        self.assertEqual(runner.array_fingerprint(audio, '<f4'), row['output_arrays']['measurement_float32'])
        runner.verify(row['file'])

    def test_full_scenarios_explicit_reset_and_independent_probes(self):
        self.args.scenario = ['full_zero_aux', 'full_aux_probe']
        report = self.execute()
        self.assertEqual(report['completed_rows'], 4)
        self.assertEqual(len(FakeChain.calls), 4)
        for index, (parameters, kwargs, pcm) in enumerate(FakeChain.calls):
            self.assertEqual(parameters['parameters']['input_eq_bank'], 7)
            self.assertEqual(parameters['parameters']['adaptive_enable'], int(index >= 2))
            self.assertEqual(kwargs['second_b_auxiliary'].shape, (2560, 3))
            self.assertEqual(kwargs['complex_auxiliary'].shape, (2560,))
            np.testing.assert_array_equal(kwargs['complex_auxiliary'][2048:], 0)
            np.testing.assert_array_equal(kwargs['second_b_auxiliary'][2048:], 0)
            if index < 2:
                np.testing.assert_array_equal(kwargs['complex_auxiliary'], 0)
            else:
                self.assertGreater(abs(kwargs['complex_auxiliary']).max(), .009)
                self.assertLessEqual(abs(kwargs['second_b_auxiliary']).max(), .003)
        np.testing.assert_array_equal(FakeChain.calls[2][1]['complex_auxiliary'], FakeChain.calls[3][1]['complex_auxiliary'])
        self.assertFalse(report['tracks']['Song']['scenarios']['full_aux_probe']['auxiliary']['music_donors'])

    def test_float_and_s16_flags_are_distinct_fresh_processes(self):
        self.args.variant += ['integer:s16:.2:-1:-6:.5']
        commands = []
        def render(cmd, **kwargs):
            commands.append(cmd)
            return self.fake_render(cmd, **kwargs)
        report = self.execute(render)
        self.assertEqual(len(commands), 2)
        self.assertIn('--float-render', commands[0])
        self.assertNotIn('--float-render', commands[1])
        self.assertEqual(report['completed_rows'], 3)

    def test_retained_scenarios_select_binding_without_changing_probe_contract(self):
        self.args.scenario = ['full_retained_zero_aux', 'full_retained_aux_probe']
        report = self.execute()
        self.assertTrue(report['complete'])
        self.assertEqual(report['completed_rows'],4)
        for index,(parameters,kwargs,pcm) in enumerate(FakeChain.calls):
            self.assertEqual(parameters['parameters']['first_b_spectral'],'retained_calibration')
            self.assertEqual(parameters['parameters']['adaptive_enable'],int(index>=2))
            old_name = 'full_zero_aux' if index<2 else 'full_aux_probe'
            expected,metadata=runner.auxiliary(old_name,2048,512)
            for key in expected:np.testing.assert_array_equal(kwargs[key],expected[key])

    def test_historical_full_scenario_remains_neutral(self):
        self.args.scenario=['full_zero_aux']
        self.execute()
        for parameters,kwargs,pcm in FakeChain.calls:
            self.assertEqual(parameters['parameters']['first_b_spectral'],'neutral_diagnostic')

    def test_renderer_failure_keeps_incomplete_report_and_log(self):
        def fail(cmd, **kwargs):
            return SimpleNamespace(returncode=2, stdout='', stderr='synthetic failure')
        with self.assertRaises(RuntimeError):
            self.execute(fail)
        report = json.loads((self.args.out / 'report.json').read_text())
        self.assertFalse(report['complete'])
        self.assertEqual(report['completed_rows'], 0)
        self.assertEqual(report['error']['type'], 'RuntimeError')
        self.assertIn('synthetic failure', next(self.args.out.rglob('*.log')).read_text())

    def test_nonfinite_output_fails_incomplete(self):
        def bad_process(self, pcm, **kw):
            return {'out': np.full((len(pcm), 16), np.nan)}
        with patch.object(FakeChain, 'process', bad_process), self.assertRaises(ValueError):
            self.execute()
        self.assertFalse(json.loads((self.args.out / 'report.json').read_text())['complete'])

    def test_source_change_mid_render_invalidates_completion(self):
        def mutate(cmd, **kw):
            result = self.fake_render(cmd, **kw)
            sf.write(self.source, self.audio * 0, 48000, subtype='PCM_16')
            return result
        with self.assertRaisesRegex(ValueError, 'file changed'):
            self.execute(mutate)
        self.assertFalse(json.loads((self.args.out / 'report.json').read_text())['complete'])

    def test_wrong_renderer_framecount_rejected(self):
        def wrong(cmd, **kw):
            sf.write(cmd[-1], np.zeros((9, 6)), 48000, subtype='PCM_16')
            return SimpleNamespace(returncode=0, stdout='', stderr='')
        with self.assertRaisesRegex(ValueError, 'exactly requested frames'):
            self.execute(wrong)

    def test_invalid_variants(self):
        for value in ('x:float:0:0:0', 'dry_bank1:float:0:0:0:1', '../x:float:0:0:0:1',
                      'x:unknown:0:0:0:1', 'x:float:nan:0:0:1', 'x:float:.6:0:0:1',
                      'x:float:.3:1:0:1', 'x:float:.3:0:-25:1', 'x:float:.3:0:0:1.1'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                runner.variant(value)

    def test_duplicate_and_missing_axes_fail_before_output(self):
        original = copy.deepcopy(self.args)
        for key, value in (('variant', self.args.variant * 2), ('scenario', ['baseline_v7'] * 2),
                           ('input', [self.source] * 2), ('variant', []), ('scenario', []),
                           ('seconds', float('nan')), ('seconds', 1), ('tail_seconds', 0)):
            self.args = copy.deepcopy(original)
            setattr(self.args, key, value)
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                runner.validate(self.args)
            self.assertFalse(self.args.out.exists())

    def test_existing_output_never_overwritten(self):
        self.args.out.mkdir()
        sentinel = self.args.out / 'keep'
        sentinel.write_text('important')
        with self.assertRaisesRegex(ValueError, 'already exists'):
            runner.run(self.args)
        self.assertEqual(sentinel.read_text(), 'important')

    def test_non_pcm16_or_wrong_rate_rejected(self):
        for subtype, rate in (('FLOAT', 48000), ('PCM_16', 44100)):
            sf.write(self.source, self.audio, rate, subtype=subtype)
            with self.assertRaisesRegex(ValueError, 'stereo48k PCM16'):
                runner.validate(self.args)

    def test_build_provenance_hash_binding(self):
        path = self.base / 'build.json'
        record = {'renderer': runner.fingerprint(self.renderer),
                  'models': {'main_model': runner.fingerprint(self.model), 'guard_model': runner.fingerprint(self.guard)},
                  'sources': [runner.fingerprint(self.renderer)]}
        path.write_text(json.dumps(record))
        result = runner.build_provenance(path, self.renderer, self.model, self.guard)
        self.assertTrue(result['listed_file_hashes_verified'])
        self.assertFalse(result['binary_source_build_identity_attested'])
        self.guard.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'hashes differ'):
            runner.build_provenance(path, self.renderer, self.model, self.guard)


if __name__ == '__main__':
    unittest.main()
