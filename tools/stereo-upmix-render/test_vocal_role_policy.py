import unittest
from vocal_role_policy import rear_permission, validate_role_fields
from vocal_role_policy import POLICY
from prepare_vocal_role_review import interval_candidates
from finish_v15_candidate import role_contract_ready


class RolePolicyTests(unittest.TestCase):
    def note(self):return dict(source_sha256='source',stem_sha256='stem',reviewer='human-local',
        decision='good',role='backing',role_confirmed=True,lead_contamination='absent',interval_start=2.,interval_end=12.)
    def permission(self,n):return rear_permission(n,'source','stem')['allow']
    def test_good_separation_without_role_is_not_backing(self):
        self.assertFalse(self.permission({'decision':'good','stem':'backing'}))
        self.assertFalse(self.permission(None))
    def test_only_verified_clean_backing_is_candidate(self):
        n=self.note();self.assertTrue(self.permission(n))
        self.assertFalse(rear_permission(n,'source','stem')['training_approved'])
        for field,value in [('role_confirmed',False),('lead_contamination','unknown'),('lead_contamination','present'),
            ('decision','unsure'),('reviewer','model'),('source_sha256','old'),('stem_sha256','old')]:
            with self.subTest(field=field,value=value):self.assertFalse(self.permission(dict(n,**{field:value})))
    def test_front_anchor_roles_never_get_rear_permission(self):
        for role in ('lead','co_lead','lead_double','vocal_hook','vocal_reverb','mixed_vocal','unknown','non_vocal'):
            with self.subTest(role=role):self.assertFalse(self.permission(dict(self.note(),role=role)))
    def test_interval_bounds_and_finite_values(self):
        entry=dict(seconds=24,core_start=2,core_end=22)
        validate_role_fields(self.note(),entry)
        for start,end in [(0,10),(2,24),(10,10),(float('nan'),12),(2,float('inf')),(True,12)]:
            with self.subTest(start=start,end=end):
                with self.assertRaises(ValueError):validate_role_fields(dict(self.note(),interval_start=start,interval_end=end),entry)
        with self.assertRaises(ValueError):validate_role_fields({'role':'backing'},entry)
    def test_absent_interval_does_not_approve_whole_clip(self):
        n=self.note();del n['interval_end'];self.assertFalse(self.permission(n))
    def test_legacy_success_does_not_authorize_role_heldout(self):
        self.assertFalse(role_contract_ready({'improved_validation':True}))
        self.assertEqual(interval_candidates([],{'partition':'heldout'},{}),[])
        self.assertFalse(role_contract_ready(dict(role_policy=POLICY,role_targets_validated=True)))
    def test_overlapping_lead_review_blocks_backing_candidate(self):
        row=dict(id='x',partition='train',stereo_sha256='source',frames=24*48000,core_offset_frames=2*48000,core_frames=20*48000)
        record={'analysis':{'stems':{'backing':{'sha256':'stem'}}}}
        note=dict(self.note(),id='float--x',stem='backing',role_policy=POLICY)
        self.assertEqual(len(interval_candidates([note],row,record)),1)
        lead=dict(note,role='lead',interval_start=10,interval_end=15)
        self.assertEqual(interval_candidates([note,lead],row,record),[])


if __name__=='__main__':unittest.main()
