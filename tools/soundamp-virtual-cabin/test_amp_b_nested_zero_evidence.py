"""Independent fixed-zero-cursor evidence for12D10E requested zero delays.

No allocator or changing-delay claim; buffers explicitly provide >=1 word.
"""
import unittest
import numpy as np
from amp_b_decorrelator import NestedAllpass


def literal_loop(x,n1,g1,n2,g2):
    a=np.zeros(max(1,n1));b=np.zeros(max(1,n2));i=j=0;y=[]
    for sample in x:
        # Parallel compute/store clauses read their old operands.
        d1,d2=a[i],b[j]
        u=sample-g2*d2;v=u-g1*d1;w=d1+g1*v
        a[i]=v;b[j]=w;y.append(d2+g2*u)
        i+=1
        if n1<=i:i=0 #12D191/195, requestedN rather than allocated bufferlen
        j+=1
        if n2<=j:j=0 #12D19D/1A0
    return np.asarray(y)


class NestedZeroEvidenceTests(unittest.TestCase):
    def test_zero_requested_cursor_loop_matches_explicit_one_word_reference(self):
        x=np.random.default_rng(86).normal(0,.01,1001)
        for n1,n2 in ((0,0),(0,96),(79,0)):
            expected=literal_loop(x,n1,.2,n2,-.3)
            actual=NestedAllpass(max(1,n1),.2,max(1,n2),-.3).process(x)
            np.testing.assert_allclose(actual,expected,atol=1e-16)

    def test_retained_bank_record_zero_inner_gain_has_97_not96_sample_feedback(self):
        x=np.zeros(300);x[0]=1
        y=literal_loop(x,0,0,96,-.3)
        expected=np.zeros_like(x);expected[0]=-.3
        expected[97]=.91;expected[194]=.273;expected[291]=.0819
        np.testing.assert_allclose(y,expected,atol=1e-16)


if __name__=='__main__':unittest.main()
