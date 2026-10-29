import copy
import json
from pathlib import Path
import unittest
from unittest.mock import patch

import audit_upmix_end_to_end as base
import audit_upmix_end_to_end_batch as batch
import test_audit_upmix_end_to_end as fixtures


class BatchRunnerTests(unittest.TestCase):
    def setUp(self):
        fixtures.EndToEndRunnerTests.setUp(self)
        second = self.base / 'Second.wav'
        second.write_bytes(self.source.read_bytes())
        self.args.input.append(second)
        self.args.jobs = 2

    def execute(self, mutate=None, failure=None):
        def launch(args, index, source):
            if source.stem == failure:
                raise RuntimeError('synthetic worker failure')
            child = copy.deepcopy(args)
            child.input = [source]
            child.out = args.out / f'shard{index}'
            report = base.run(child)
            if mutate:
                mutate(report, source.stem)
                base.checkpoint(child.out, report)
            log = args.out / f'shard{index}.log'
            log.write_text('mock launch')
            return child.out / 'report.json', {'command': batch.command(args, source, child.out), 'log': base.fingerprint(log)}
        with patch.object(base, 'AmpChain', fixtures.FakeChain), \
             patch.object(base, 'source_provenance', return_value=[]), \
             patch.object(base.subprocess, 'run', side_effect=lambda cmd, **kw: fixtures.EndToEndRunnerTests.fake_render(self, cmd, **kw)), \
             patch.object(batch, 'launch', side_effect=launch):
            return batch.run(self.args)

    def test_two_shards_complete_and_preserve_original_reports(self):
        report = self.execute()
        self.assertTrue(report['complete'])
        self.assertEqual(report['completed_rows'], 4)
        self.assertEqual(report['expected_rows'], 4)
        self.assertEqual(list(report['tracks']), ['Song', 'Second'])
        self.assertEqual(report['summary']['baseline_v7']['test']['tracks'], 2)
        self.assertEqual(report['batch']['effective_jobs'], 2)
        self.assertEqual(len(report['batch']['shards']), 2)
        base.verify(report['batch']['wrapper'])
        for name, item in report['batch']['shards'].items():
            base.verify(item['report'])
            original = json.loads(Path(item['report']['path']).read_text())
            self.assertEqual(list(original['tracks']), [name])
            self.assertEqual(original['completed_rows'], 2)

    def test_child_failure_keeps_partial_aggregate(self):
        with self.assertRaisesRegex(RuntimeError, 'shards failed'):
            self.execute(failure='Second')
        report = json.loads((self.args.out / 'report.json').read_text())
        self.assertFalse(report['complete'])
        self.assertEqual(report['completed_rows'], 2)
        self.assertIn('Second', report['batch']['errors'])

    def test_missing_row_with_false_complete_claim_rejected(self):
        def mutate(report, name):
            if name == 'Second':
                del report['tracks'][name]['scenarios']['baseline_v7']['rows']['test']
        with self.assertRaises(RuntimeError):
            self.execute(mutate=mutate)
        report = json.loads((self.args.out / 'report.json').read_text())
        self.assertFalse(report['complete'])
        self.assertIn('extra/missing row', report['batch']['errors']['Second']['message'])

    def test_provenance_difference_rejected(self):
        def mutate(report, name):
            if name == 'Second':
                report['provenance']['renderer']['sha256'] = '0' * 64
        with self.assertRaises(RuntimeError):
            self.execute(mutate=mutate)
        report = json.loads((self.args.out / 'report.json').read_text())
        self.assertIn('parameters/provenance', report['batch']['errors']['Second']['message'])

    def test_command_preserves_all_base_flags_but_not_jobs(self):
        self.args.save_output_wavs = True
        self.args.build_provenance = self.base / 'build.json'
        cmd = batch.command(self.args, self.source, self.base / 'child')
        self.assertIn('--save-output-wavs', cmd)
        self.assertIn('--build-provenance', cmd)
        self.assertNotIn('--jobs', cmd)
        parsed = base.parser().parse_args(cmd[2:])
        self.assertEqual(parsed.input, [self.source.resolve()])
        self.assertEqual(parsed.variant, self.args.variant)
        self.assertEqual(parsed.scenario, self.args.scenario)

    def test_jobs_duplicate_inputs_existing_output_fail_before_launch(self):
        original = copy.deepcopy(self.args)
        for key, value in (('jobs', 0), ('jobs', True), ('input', [self.source, self.source])):
            self.args = copy.deepcopy(original)
            setattr(self.args, key, value)
            with patch.object(batch, 'launch') as launch, self.assertRaises(ValueError):
                batch.run(self.args)
            launch.assert_not_called()
            self.assertFalse(self.args.out.exists())
        self.args = original
        self.args.out.mkdir()
        with self.assertRaisesRegex(ValueError, 'already exists'):
            batch.run(self.args)


if __name__ == '__main__':
    unittest.main()
