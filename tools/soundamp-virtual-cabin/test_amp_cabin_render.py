import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import sys

import numpy as np
import soundfile as sf

from amp_cabin_render import DOMAINS, SR, convolve_outputs, load_transfer, main, render, validate_manifest
from amp_dsp_chain import AmpChain


class CabinBridgeTests(unittest.TestCase):
    def fixture(self, directory, domain='most_inputs'):
        root=Path(directory);responses={}
        for i,name in enumerate(DOMAINS[domain]):
            h=np.zeros((3,2));h[0,0]=i+1;h[2,1]=-(i+1)
            path=root/(name+'.wav');sf.write(path,h,SR,subtype='DOUBLE')
            responses[name]=dict(file=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),gain_db=0)
        m=dict(schema_version=1,sample_rate=SR,domain=domain,channels=DOMAINS[domain],responses=responses,
               provenance=dict(kind='synthetic',description='test only, not cabin measurement'),
               capture=dict(seat='synthetic',state='fixture',microphone_calibration='none: synthetic',level_reference='arbitrary test units'))
        path=root/'manifest.json';path.write_text(json.dumps(m));return path,m

    def test_most_coherent_sum_sign_delay_and_full_tail(self):
        with tempfile.TemporaryDirectory() as tmp:
            path,_=self.fixture(tmp);x=np.zeros((7,6));x[6]=1
            y,r=render(x,path)
            expected=np.zeros((9,2));expected[6,0]=21;expected[8,1]=-21
            np.testing.assert_allclose(y,expected,atol=1e-13)
            self.assertIsNone(r['model']);self.assertEqual(r['fidelity'],'synthetic fixture only')
            self.assertFalse(r['acoustics']['hardware_verified'])
            self.assertGreater(r['peak_dbfs'],0) # no clipping/normalization

    def test_end_to_end_domain_rejects_double_dsp(self):
        with tempfile.TemporaryDirectory() as tmp:
            path,_=self.fixture(tmp);x=np.zeros((5,6))
            for kwargs in (dict(chain=AmpChain()),dict(electronic_tail_frames=0),dict(stage_inputs={'first_b':np.zeros((5,10))})):
                with self.assertRaises(ValueError):render(x,path,**kwargs)

    def test_post_amp_uses_all_config_outputs_in_order(self):
        with tempfile.TemporaryDirectory() as tmp:
            path,_=self.fixture(tmp,'post_amp_config_outputs');x=np.zeros((1000,6));x[0]=[.1,.2,.3,.4,.5,.6]
            c=AmpChain();y,r=render(x,path,chain=c,electronic_tail_frames=300)
            electrical=c.process(np.pad(x,((0,300),(0,0))),capture_stages=False)['out']
            summed=electrical@np.arange(1,17);expected=np.zeros((1302,2))
            expected[:1300,0]=summed;expected[2:,1]=-summed
            np.testing.assert_allclose(y,expected,atol=1e-13)
            self.assertEqual(r['acoustics']['stage_order'],['upmix_pcm','amp_chain','measured_brir'])
            self.assertFalse(r['electronic_tail_fully_settled_proven'])

    def test_post_amp_requires_explicit_chain_and_tail(self):
        with tempfile.TemporaryDirectory() as tmp:
            path,_=self.fixture(tmp,'post_amp_config_outputs');x=np.zeros((4,6))
            for kwargs in ({},dict(chain=AmpChain()),dict(chain=AmpChain(),electronic_tail_frames=True),dict(chain=AmpChain(),electronic_tail_frames=-1)):
                with self.assertRaises(ValueError):render(x,path,**kwargs)

    def test_hash_channel_capture_and_provenance_validation(self):
        with tempfile.TemporaryDirectory() as tmp:
            path,m=self.fixture(tmp)
            for key,value in (('domain','logical_buses'),('channels',list(reversed(DOMAINS['most_inputs']))),('capture',{}),('provenance',{}),('sample_rate',44100)):
                wrong=dict(m);wrong[key]=value;path.write_text(json.dumps(wrong))
                with self.assertRaises(ValueError):validate_manifest(path)
            path.write_text(json.dumps(m));ir=Path(tmp)/'L.wav';sf.write(ir,np.ones((3,2)),SR,subtype='DOUBLE')
            with self.assertRaisesRegex(ValueError,'hash mismatch'):load_transfer(path)

    def test_relative_response_paths_and_explicit_gain(self):
        with tempfile.TemporaryDirectory() as tmp:
            path,m=self.fixture(tmp);m['responses']['L']['gain_db']=-6;path.write_text(json.dumps(m))
            irs,meta=load_transfer(path)
            self.assertAlmostEqual(irs['L'][0,0],10**(-6/20))
            self.assertTrue(Path(meta['responses']['L']['path']).is_absolute())

    def test_malformed_audio_and_silent_manifest_fail(self):
        with tempfile.TemporaryDirectory() as tmp:
            path,m=self.fixture(tmp)
            for x in (np.zeros((0,6)),np.zeros((2,5)),np.full((2,6),np.nan)):
                with self.assertRaises(ValueError):render(x,path)
            for name,record in m['responses'].items():
                file=Path(tmp)/record['file'];sf.write(file,np.zeros((2,2)),SR,subtype='DOUBLE')
                record['sha256']=hashlib.sha256(file.read_bytes()).hexdigest()
            path.write_text(json.dumps(m))
            with self.assertRaisesRegex(ValueError,'silent'):load_transfer(path)

    def test_unequal_ir_lengths_and_coherent_cancellation(self):
        x=np.ones((4,2));a=np.ones((1,2));b=np.zeros((5,2));b[0]=-1
        np.testing.assert_allclose(convolve_outputs(x,[a,b]),np.zeros((8,2)),atol=1e-14)

    def test_complex_audio_and_invalid_manifest_types_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path,m=self.fixture(tmp)
            with self.assertRaises(ValueError):render(np.ones((3,6),complex),path)
            for value in ([],dict(m,provenance=None),dict(m,capture=[]),dict(m,responses=None),dict(m,domain=[])):
                path.write_text(json.dumps(value))
                with self.assertRaises(ValueError):validate_manifest(path)
        with self.assertRaises(ValueError):convolve_outputs(np.ones((3,1),complex),[np.ones((1,2))])

    def test_cli_float_artifact_preserves_overrange_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as tmp:
            path,_=self.fixture(tmp);root=Path(tmp);source=root/'input.wav';target=root/'output.wav'
            sf.write(source,np.ones((10,6)),SR,subtype='FLOAT')
            argv=['amp_cabin_render',str(source),str(target),'--manifest',str(path)]
            with patch.object(sys,'argv',argv):main()
            audio,_=sf.read(target);self.assertGreater(np.max(abs(audio)),1)
            report=json.loads(target.with_suffix('.wav.report.json').read_text())
            self.assertTrue(report['complete']);self.assertIsNone(report['model'])
            self.assertEqual(report['output']['sha256'],hashlib.sha256(target.read_bytes()).hexdigest())
            with patch.object(sys,'argv',argv),self.assertRaises(SystemExit):main()


if __name__=='__main__':unittest.main()
