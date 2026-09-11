import unittest
import numpy as np
import torch
from train_v15_streaming import content_gate, admitted_windows, core_mask, routing_loss, assert_frozen, normalized_inputs
from train_hssr import HssrNetwork, freeze_except_routing_head
import hssr_runtime_training as runtime


class StreamingTests(unittest.TestCase):
    def test_double_features_become_runtime_float(self):
        x=normalized_inputs(np.array([[20.,-20.,.5]],dtype=np.float64),np.zeros(3),np.ones(3))
        self.assertEqual(x.dtype,torch.float32)
        torch.testing.assert_close(x,torch.tensor([[6.,-6.,.5]]))
        with self.assertRaises(ValueError):normalized_inputs(np.array([[np.nan]]),0,1)

    def window(self):
        v=dict(clap={'cosine':dict(lead_singing=.1,harmony_singing=.1,wordless_vocal=.1,
            processed_vocal=.1,speech=.1,guitar=.5,music_mix=.7)},ast={'focus':{'Singing':.01}})
        return dict(seconds=10,offset_seconds=0,views={'shared_level':v,'diagnostic_level':v})
    def test_two_view_nonvocal_gate(self):
        w=self.window();self.assertTrue(content_gate(w,True));self.assertFalse(content_gate(w,False))
        w['views'].pop('shared_level');self.assertFalse(content_gate(w,True))
    def test_short_and_nan_abstain(self):
        w=self.window();w['seconds']=.2;self.assertFalse(content_gate(w,True))
        w=self.window();w['views']['shared_level']['ast']['focus']['Singing']=float('nan')
        self.assertFalse(content_gate(w,True))
    def test_heldout_rejected_without_reading_targets(self):
        self.assertEqual(admitted_windows({'partition':'heldout'},'mixture'),[])
    def test_context_and_tail_excluded(self):
        row={'core_offset_frames':96000};select=np.arange(3,9000,4)
        mask=core_mask(row,select,[dict(offset_seconds=0,seconds=10)])
        self.assertTrue(mask.any())
        times=select*128/48000-2
        self.assertGreaterEqual(times[mask].min(),0)
        self.assertLess(times[mask].max(),10)
        self.assertFalse(mask[-1])
    def test_embedding_and_chunk_parity(self):
        torch.set_num_threads(2);torch.manual_seed(17);m=HssrNetwork()
        x=torch.randn(1,9,368)
        with torch.no_grad():
            logits,_=runtime.forward(m,x)
            emb,_=runtime.forward(m,x,return_embeddings=True)
            a,state=runtime.forward(m,x[:,:4],return_embeddings=True)
            b,_=runtime.forward(m,x[:,4:],state,return_embeddings=True)
            torch.testing.assert_close(emb,torch.cat([a,b],1),rtol=0,atol=0)
            actual=torch.nn.functional.linear(emb,runtime.weight(m.head.weight),m.head.bias)
            torch.testing.assert_close(logits,actual,atol=1e-6,rtol=1e-5)
    def test_optimizer_preserves_frozen_rows(self):
        torch.manual_seed(7);m=HssrNetwork();initial={k:v.detach().clone() for k,v in m.state_dict().items()}
        freeze_except_routing_head(m);opt=torch.optim.AdamW(m.head.parameters(),lr=1e-4,weight_decay=0)
        pred=m.head(torch.randn(8,160));target=torch.zeros(8,32,2)
        self.assertIsNone(routing_loss(pred,target,torch.zeros(8,dtype=torch.bool)))
        loss=routing_loss(pred,target,torch.ones(8,dtype=torch.bool));loss.backward();opt.step()
        assert_frozen(m,initial)
        self.assertFalse(torch.equal(m.head.weight,initial['head.weight']))


if __name__=='__main__':unittest.main()
