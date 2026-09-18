import unittest
import numpy as np
import torch

from q2_local_guidance_model import Q2Config,Q2LocalGuidanceModel
from q2_training_core import prepare_case,objective,forward_chunk,predict_case,source_pcm,fit_scalars,pcm_metrics


def row():
    rng=np.random.default_rng(77);s=(rng.normal(size=(6144,2,2))*.01).astype('f4')
    return dict(id='fixture',song_id='fixture',artist='procedural',partition='TRAIN',sample_rate=48000,
                mixture=s.sum(1),targets=s,classes=('a','b'),core_start=1024,core_stop=6144,
                reconstruction_known=np.ones(2,bool),detection_known=np.ones(2,bool))


class Q2TrainingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.threads=torch.get_num_threads();torch.set_num_threads(2)
    @classmethod
    def tearDownClass(cls):torch.set_num_threads(cls.threads)

    def test_core_masks_and_losses_backward(self):
        config=Q2Config(width=12,band_hidden=24,classes=('a','b'))
        r=row();case=prepare_case(r,config);model=Q2LocalGuidanceModel(config)
        out,_=forward_chunk(model,case,0,case['original'].shape[1])
        loss,parts=objective(out,case,0,case['original'].shape[1]);loss.backward()
        self.assertTrue(torch.isfinite(loss))
        self.assertFalse(case['valid'][:7].any())
        self.assertTrue(all(p.grad is None or torch.isfinite(p.grad).all() for p in model.parameters()))
        self.assertEqual(set(parts),{'source','magnitude','activity','descriptor','uncertainty','total'})

    def test_chunk_cadence_and_original_only_modes(self):
        config=Q2Config(width=12,band_hidden=24,classes=('a','b'))
        case=prepare_case(row(),config);model=Q2LocalGuidanceModel(config)
        state=None
        for start,stop in ((0,7),(7,16),(16,27)):
            out,state=forward_chunk(model,case,start,stop,state)
            if case['valid'][start:stop].any():objective(out,case,start,stop)
        case['partition']='DEV'
        with self.assertRaises(ValueError):forward_chunk(model,case,0,4,arm='oracle_train')

    def test_scalar_is_train_only_and_quiet_unknown_not_pass(self):
        r=row();alpha=fit_scalars([r]);metrics=pcm_metrics(r['targets'],r,alpha)
        self.assertTrue(all(v==0 for f in metrics['families'] for v in f['nmse']))
        r['targets'][:,0]=0
        metrics=pcm_metrics(r['targets'],r,alpha)
        self.assertEqual(metrics['families'][0]['nmse'],[None,None])
        r['partition']='DEV'
        with self.assertRaises(ValueError):fit_scalars([r])

    def test_synthesis_original_targets_and_zero_forward(self):
        config=Q2Config(width=12,band_hidden=24,classes=('a','b'));r=row();case=prepare_case(r,config)
        torch.testing.assert_close(source_pcm(case['targets'],case),torch.from_numpy(r['targets']),atol=2e-8,rtol=1e-5)
        model=Q2LocalGuidanceModel(config).eval()
        pred=predict_case(model,case,chunk_frames=7)
        self.assertEqual(pred.shape,case['targets'].shape)
        self.assertFalse(pred.any())


if __name__=='__main__':unittest.main()
