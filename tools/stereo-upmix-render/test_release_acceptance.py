"""Synthetic ledger qualification only: no real listener or hardware evidence."""
from datetime import datetime,timezone
import copy
import json
import math
from pathlib import Path
import tempfile
import unittest

import release_acceptance as r


class ReleaseAcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.evidence=self.root/'evidence';self.evidence.mkdir()
        self.protocol,_=r.read(Path(__file__).with_name('release_acceptance_v2.json'))
        r.write_new(self.root/'p.json',self.protocol)
        (self.root/'spec.md').write_text('SYNTHETIC specification, not real qualification')
        self.freeze=r.seal(self.root/'p.json',self.root/'spec.md',self.root/'freeze')
        self.candidate='a'*64
        for name in ('measurement','build','workload'):
            (self.evidence/(name+'.json')).write_text(json.dumps({'schema':'synthetic-fixture','name':name}))
        self.domains={}
        for scope in {g['scope'] for g in self.protocol['gates']}:
            self.domains[scope]=self.domain(scope)

    def artifact(self,name):
        return dict(path=name,sha256=r.sha((self.evidence/name).read_bytes()))

    def domain(self,scope,cohort='loaded-a',filename=None):
        groups={}
        for g in self.protocol['gates']:
            if g['scope']!=scope:continue
            ids=[cohort] if scope=='TARGET' else [g['count_unit']+'-'+str(i) for i in range(g['min_count'])]
            groups[g['id']]=dict(unit=g['count_unit'],eligible_ids=ids,ineligible_ids=[],
                                 eligibility_method='SYNTHETIC fixed eligible units, not measurements')
        value=dict(schema=r.DOMAIN,scope=scope,protocol_sha256=self.freeze['protocol_sha256'],
                   candidate_sha256=self.candidate,gate_units=groups)
        if scope=='TARGET':
            value['target_context']=dict(cohort_id=cohort,target_identity='SYNTHETIC fixture',
                cpu='APQ8064/Krait',operating_system='QNX',emulated=False,
                build_manifest_sha256=self.artifact('build.json')['sha256'],
                workload_manifest_sha256=self.artifact('workload.json')['sha256'],
                sample_rate_hz=48000,callback_frames=1024,callback_period_seconds=1024/48000,
                sample_format='S16_LE',channels=['FL','FR','C','LFE','SL','SR'],
                representative_competing_load=True)
        if scope=='LISTENING':
            value['listening_context']=dict(verdict_unit='song_cluster',repeats_add_trials=False,
                ties_are_nonpreference=True,population_claim=False,listener_ids=['SYNTHETIC-only'],
                listener_count=1)
        filename=filename or ('domain-'+scope+'.json')
        (self.evidence/filename).write_text(json.dumps(value))
        return filename

    def gate(self,name):return next(g for g in self.protocol['gates'] if g['id']==name)

    def passing(self,g):
        if g['kind']=='bool':return g['threshold']
        if g['id']=='blind_listener_preference_ci95_lower':return .75
        return g['threshold']

    def report(self,name,value='DEFAULT',domain=None,**overrides):
        g=self.gate(name);domain=domain or self.domains[g['scope']]
        binding=self.artifact(domain)
        manifest=json.loads((self.evidence/domain).read_text())
        ids=manifest['gate_units'][name]['eligible_ids']
        record=dict(schema=r.EVIDENCE,gate_id=name,scope=g['scope'],state='measured',
            value=self.passing(g) if value=='DEFAULT' else value,
            sample_count=len(ids),eligible_unit_ids=ids,count_unit=g['count_unit'],
            method='SYNTHETIC ledger test, not real model/listener/target evidence',
            measurement_utc=datetime.now(timezone.utc).isoformat(),
            protocol_sha256=self.freeze['protocol_sha256'],
            metric_specification_sha256=self.freeze['metric_specification_sha256'],
            candidate_sha256=self.candidate,domain_manifest_sha256=binding['sha256'],
            domain_manifest=binding,evidence_origin={'TARGET':'actual-target','LISTENING':'real-listener',
                'CABIN':'actual-cabin'}.get(g['scope'],'computed'),
            artifacts=[self.artifact('measurement.json'),self.artifact('build.json'),
                       self.artifact('workload.json'),binding])
        if g.get('requires_positive_coherent_transfer'):record['coherent_sign']=1
        record.update(overrides)
        (self.evidence/(name+'.json')).write_text(json.dumps(record))
        return record

    def evaluate(self):return r.evaluate(self.root/'freeze',self.evidence,self.candidate)
    def result(self,name):return next(g for g in self.evaluate()['gates'] if g['id']==name)
    def fill(self):
        for g in self.protocol['gates']:self.report(g['id'])

    def test_missing_and_explicit_unknown_never_success(self):
        self.assertTrue(all(g['state']=='UNVERIFIED' for g in self.evaluate()['gates']))
        self.report('active_song_nmse_worst',None,state='blocked',reason='No truth')
        self.assertEqual(self.result('active_song_nmse_worst')['state'],'BLOCKED')
        self.assertFalse(self.evaluate()['installation_ready'])

    def test_synthetic_statuses_distinct_and_fixed_thresholds_unchanged(self):
        self.fill()
        self.assertTrue(self.evaluate()['acoustically_validated'])
        (self.evidence/'actual_cabin_front_rear_validation.json').unlink()
        self.assertTrue(self.evaluate()['installation_ready'])
        self.assertFalse(self.evaluate()['acoustically_validated'])
        self.report('active_song_nmse_worst',.81)
        self.assertFalse(self.evaluate()['installation_ready'])
        old,_=r.read(Path(__file__).with_name('release_acceptance_v1.json'))
        self.assertEqual([(g['id'],g['kind'],g['scope'],g['threshold'],g['min_count']) for g in old['gates']],
                         [(g['id'],g['kind'],g['scope'],g['threshold'],g['min_count']) for g in self.protocol['gates']])

    def test_negative_impossible_values_and_fractional_counts_fail(self):
        for name,value in [('fresh_silence_peak',-1),('active_song_nmse_worst',-1),
            ('native_parity_normalized_max_abs',-1),('target_underruns_or_missed_deadlines',-2.5),
            ('target_underruns_or_missed_deadlines',.5),('quantized_output_max_lsb',.5),
            ('target_full_path_p99_period_ratio',-.5),('blind_listener_preference_ci95_lower',2),
            ('support_route_coverage',1.1),('bass_energy_change_db_worst',-1)]:
            self.report(name,value);self.assertEqual(self.result(name)['state'],'FAIL',name)

    def test_timestamps_strict_after_freeze_audit_exception_reverifies(self):
        name='native_parity_normalized_max_abs'
        for timestamp in ('not-a-timestamp','2000-01-01T00:00:00Z','2999-01-01T00:00:00Z',
                          '2026-09-16T00:00:00','2026-09-16T00:00:00-04:00'):
            self.report(name,0.,measurement_utc=timestamp)
            self.assertEqual(self.result(name)['state'],'FAIL')
        self.report('data_provenance',True,measurement_utc='2000-01-01T00:00:00Z')
        self.assertEqual(self.result('data_provenance')['state'],'FAIL')
        self.report('data_provenance',True,measurement_utc='2000-01-01T00:00:00Z',
                    source_reverified_utc=datetime.now(timezone.utc).isoformat(),
                    source_reverified_reason='All bound source bodies re-read after freeze')
        self.assertEqual(self.result('data_provenance')['state'],'PASS')

    def test_domain_is_body_bound_and_counts_exact_unique_not_frames_as_songs(self):
        name='active_song_nmse_worst'
        for changes in ({'domain_manifest_sha256':'b'*64},{'domain_manifest':None},
                        {'eligible_unit_ids':['same']*12},{'sample_count':999},
                        {'count_unit':'audio_frame'},{'eligible_unit_ids':['different-'+str(i) for i in range(12)]}):
            self.report(name,.1,**changes);self.assertEqual(self.result(name)['state'],'FAIL')
        self.report(name,.1)
        (self.evidence/self.domains['FINAL_TEST']).write_text('{}')
        self.assertEqual(self.result(name)['state'],'FAIL')

    def test_zero_eligible_coverage_retained_unknown_not_pass(self):
        name='active_song_nmse_worst';path=self.evidence/self.domains['FINAL_TEST']
        value=json.loads(path.read_text());value['gate_units'][name].update(eligible_ids=[],ineligible_ids=['quiet-song'])
        path.write_text(json.dumps(value));self.report(name,0.)
        self.assertEqual(self.result(name)['state'],'UNVERIFIED')

    def test_all_target_metrics_share_same_loaded_build_workload_rate_period_cohort(self):
        self.fill()
        different=self.domain('TARGET','loaded-b','domain-TARGET-b.json')
        self.report('target_sustained_stress_seconds',7200.,domain=different)
        self.assertTrue(all(g['state']=='FAIL' for g in self.evaluate()['gates'] if g['scope']=='TARGET'))
        path=self.evidence/self.domains['TARGET'];value=json.loads(path.read_text())
        for key,bad in [('representative_competing_load',False),('callback_period_seconds',.001),
                        ('emulated',True),('cpu','x86'),('sample_format','FLOAT')]:
            changed=copy.deepcopy(value);changed['target_context'][key]=bad
            path.write_text(json.dumps(changed));self.report('algorithmic_latency_ms',20.)
            self.assertEqual(self.result('algorithmic_latency_ms')['state'],'FAIL')
        path.write_text(json.dumps(value))

    def test_listener_distinct_song_units_context_and_strict_wilson_boundary(self):
        name='blind_listener_preference_ci95_lower'
        self.report(name,.5);self.assertEqual(self.result(name)['state'],'FAIL')
        self.report(name,.5000000001);self.assertEqual(self.result(name)['state'],'PASS')
        path=self.evidence/self.domains['LISTENING'];value=json.loads(path.read_text())
        value['listening_context']['repeats_add_trials']=True;path.write_text(json.dumps(value))
        self.report(name,.8);self.assertEqual(self.result(name)['state'],'FAIL')

    def ratio(self,name,n,d,multiplier,**kw):
        return self.report(name,None,exact_log_ratio=dict(numerator=n,denominator=d,multiplier=multiplier),**kw)

    def test_exact_log_zero_leakage_pass_zero_retention_fail_without_epsilon(self):
        self.ratio('foreground_isolated_rear_db_worst',0.,1.,10)
        result=self.result('foreground_isolated_rear_db_worst')
        self.assertEqual(result['state'],'PASS');self.assertIsNone(result['value'])
        self.assertEqual(result['exact_case'],'exact_zero_numerator')
        self.ratio('support_retention_db_p10',0.,1.,20,coherent_sign=0)
        self.assertEqual(self.result('support_retention_db_p10')['state'],'FAIL')
        self.ratio('foreground_front_transfer_error_db_worst',0.,1.,20,coherent_sign=0)
        self.assertEqual(self.result('foreground_front_transfer_error_db_worst')['state'],'FAIL')
        self.ratio('bass_energy_change_db_worst',0.,1.,10)
        self.assertEqual(self.result('bass_energy_change_db_worst')['state'],'FAIL')

    def test_perfect_candidate_recovery_and_unknown_zero_zero(self):
        self.ratio('recovery_gain_db_median',1.,0.,10)
        self.assertEqual(self.result('recovery_gain_db_median')['state'],'PASS')
        self.ratio('recovery_gain_db_median',0.,0.,10)
        self.assertEqual(self.result('recovery_gain_db_median')['state'],'FAIL')
        self.assertEqual(self.result('recovery_gain_db_median')['value'],0.)
        self.ratio('foreground_isolated_rear_db_worst',0.,0.,10)
        self.assertEqual(self.result('foreground_isolated_rear_db_worst')['state'],'UNVERIFIED')
        self.report('foreground_isolated_rear_db_worst',-30.,
                    exact_log_ratio=dict(numerator=0.,denominator=0.,multiplier=10))
        self.assertEqual(self.result('foreground_isolated_rear_db_worst')['state'],'FAIL')
        json.dumps(self.evaluate(),allow_nan=False)

    def test_exact_ratio_finite_consistency_sign_and_restricted_gate(self):
        name='foreground_isolated_rear_db_worst'
        self.report(name,-30.,exact_log_ratio=dict(numerator=.001,denominator=1.,multiplier=10))
        self.assertEqual(self.result(name)['state'],'PASS')
        self.report(name,-31.,exact_log_ratio=dict(numerator=.001,denominator=1.,multiplier=10))
        self.assertEqual(self.result(name)['state'],'FAIL')
        self.ratio('native_parity_normalized_max_abs',0.,1.,10)
        self.assertEqual(self.result('native_parity_normalized_max_abs')['state'],'FAIL')
        self.report('support_retention_db_p10',0.,coherent_sign=-1)
        self.assertEqual(self.result('support_retention_db_p10')['state'],'FAIL')
        self.ratio('support_retention_db_p90',0.,1.,20,coherent_sign=1)
        self.assertEqual(self.result('support_retention_db_p90')['state'],'FAIL')

    def test_truthiness_nonfinite_wrong_origin_and_path_escape_fail(self):
        for value in (1,'true',None):
            self.report('data_provenance',value);self.assertEqual(self.result('data_provenance')['state'],'FAIL')
        self.report('active_song_nmse_worst',float('nan'))
        self.assertEqual(self.result('active_song_nmse_worst')['state'],'FAIL')
        self.report('algorithmic_latency_ms',20.,evidence_origin='computed')
        self.assertEqual(self.result('algorithmic_latency_ms')['state'],'FAIL')
        self.report('data_provenance',True,artifacts=[dict(path='../spec.md',sha256=r.sha(b'x'))])
        self.assertEqual(self.result('data_provenance')['state'],'FAIL')

    def test_nonobject_duplicate_json_and_mutated_freeze_fail_closed(self):
        name='active_song_nmse_worst';path=self.evidence/(name+'.json')
        for text in ('[]','{"value":0,"value":1}'):
            path.write_text(text);self.assertEqual(self.result(name)['state'],'FAIL')
        with self.assertRaises(ValueError):r.seal(self.root/'p.json',self.root/'spec.md',self.root/'freeze')
        (self.root/'freeze'/'metric-specification.md').write_text('relaxed after freeze')
        with self.assertRaises(ValueError):self.evaluate()

    def test_protocol_schema_domains_and_no_vacuous_release(self):
        r.validate_protocol(self.protocol)
        for change in ('oldschema','missingdomain','onlycabin'):
            altered=copy.deepcopy(self.protocol)
            if change=='oldschema':altered['schema']='upmix-release-protocol-v1'
            elif change=='missingdomain':
                next(g for g in altered['gates'] if g['kind']!='bool').pop('numeric_domain')
            else:altered['gates']=[g for g in altered['gates'] if g.get('acoustic_only')]
            with self.assertRaises(ValueError):r.validate_protocol(altered)


if __name__=='__main__':unittest.main()
