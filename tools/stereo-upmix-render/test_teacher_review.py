import copy
import io
import json
from pathlib import Path
import tempfile
import unittest
import numpy as np
import soundfile as sf
from teacher_review_server import ReviewStore,digest,ROOT
from teacher_audit_policy import coverage_report,triage,reviewed_candidates

class ReviewTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.store=ReviewStore(self.root)
        self.mix=self.root/'mix.wav';self.stem=self.root/'hh.wav'
        sf.write(self.mix,np.ones((4800,2),np.float32)*.8,48000,subtype='FLOAT')
        sf.write(self.stem,np.ones((4800,2),np.float32)*.2,48000,subtype='FLOAT')
        self.store.registry={'x':dict(paths={'mix':self.mix,'hh':self.stem},
            hashes={'mix':digest(self.mix),'hh':digest(self.stem)},revision='r',seconds=.1)}
        self.note=dict(id='x',stem='hh',revision='r',decision='good',notes='test fixture only',position=.03)
    def test_common_gain(self):
        a,_=sf.read(io.BytesIO(self.store.media('x','mix','r')))
        b,_=sf.read(io.BytesIO(self.store.media('x','hh','r')))
        self.assertLess(abs(a[0,0]/b[0,0]-4),.002)
        self.assertLessEqual(np.max(abs(a)),.401)
    def test_cached_changed_audio_rejected(self):
        self.store.media('x','hh','r')
        sf.write(self.stem,np.zeros((4800,2)),48000,subtype='FLOAT')
        with self.assertRaises(ValueError):self.store.media('x','hh','r')
        with self.assertRaises(ValueError):self.store.annotate(self.note)
    def test_notes_durable_not_approved(self):
        r=self.store.annotate(self.note)
        self.assertFalse(r['training_approved'])
        self.assertEqual(json.loads(self.store.notes.read_text())['stem_sha256'],digest(self.stem))
    def test_invalid_notes(self):
        for change in [dict(revision='old'),dict(position=1),dict(position=float('nan')),
                       dict(stem='../../elsewhere'),dict(decision='approved'),dict(extra=True)]:
            with self.assertRaises(ValueError):self.store.annotate(dict(self.note,**change))
        with self.assertRaises(ValueError):self.store.annotate([])
    def test_heldout_and_unreviewed_never_candidates(self):
        r=self.store.annotate(self.note)
        items=[dict(id='x',revision='r',partition='heldout')]
        self.assertEqual(reviewed_candidates(items,[r],['hh']),[])
        items[0]['partition']='train'
        self.assertEqual(reviewed_candidates(items,[r],['hh','backing']),[])
        self.assertEqual(len(reviewed_candidates(items,[r],['hh'])),1)
    def test_lead_collapse_flag(self):
        flags=triage(dict(stems={'lead':{'rms':.00002},'backing':{'rms':.085}},qa={}))
        self.assertTrue(flags)
        self.assertIn('Есть ли здесь лид-партия?',flags[0])
        self.assertNotIn('исчез',flags[0])
    def test_real_full_coverage(self):
        path=ROOT/'build/reference-audio/v14-full-source/manifest.json'
        if not path.exists():self.skipTest('private full corpus not available')
        data=json.loads(path.read_text());r=coverage_report(data)
        self.assertEqual((r['songs'],r['clips']),(50,587))
        broken=copy.deepcopy(data);broken['items'][0]['core_frames']-=1
        with self.assertRaises(ValueError):coverage_report(broken)

if __name__=='__main__':unittest.main()
