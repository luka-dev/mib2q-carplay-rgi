import json
import struct
import unittest

import numpy as np
from numpy.testing import assert_allclose, assert_array_equal

from amp_b_bound_scenario import retained_config
from amp_b_spatial_binding import (ROOT, bind_effective_spatial, build_effective_spatial,
                                    spatial_binding_evidence)


class SpatialBindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory=ROOT/'assets/soundamp/remaining-blocks-20260920-v3'
        cls.report=json.loads((directory/'b_config_control.json').read_text())
        cls.maps=json.loads((directory/'b_mode_control.json').read_text())['pointer_maps']
        cls.edges=spatial_binding_evidence()['edges']

    def args(self,source='b01',bank=1,variant='20:default'):
        pointers={int(k):v for k,v in self.maps[variant].items()}
        pointers.update({int(k):v for k,v in self.maps['16:default'].items()})
        return dict(effective_config=retained_config(self.report,source=source,bank=bank),
                    pointers=pointers,edges=self.edges,assume_successful_fresh_initialization=True)

    def test_raw_retained_bank_offset_selectors_and_constructor_constants(self):
        bound=bind_effective_spatial(**self.args()); a=bound['constructor']
        raw=self.report['sources']['b01']['banks'][0]['words'][3:31]
        self.assertEqual(struct.pack('<28f',*a['params']),struct.pack('<28I',*raw))
        self.assertEqual((a['eigen_selector'],a['balance_enabled'],a['gate_selector']), (1,True,0))
        self.assertEqual((a['low_band'],a['high_band']),(0,73))
        self.assertEqual(struct.pack('<2f',a['balance_slope'],a['balance_offset']),
                         struct.pack('<2I',0x40a00001,0xc0600001))
        self.assertTrue(bound['enabled']);self.assertFalse(bound['metadata']['actual_vehicle_state_proven'])

    def test_gate_is_one_other_histories_zero_and_state_independent(self):
        x,b=build_effective_spatial(**self.args()); y,_=build_effective_spatial(**self.args())
        self.assertEqual(b['constructor']['gate_history'],1.)
        for key in ('left_history','right_history','cross_history','diffuse_history'):
            assert_array_equal(b['constructor'][key],np.zeros(74))
        self.assertEqual(b['constructor']['minimum_history'],0)
        self.assertEqual(b['constructor']['mean_history'],0)
        left=np.linspace(.1,1,513)+.2j;right=left[::-1]*.7-.1j
        first=x.process(left,right,enabled=True)
        replay=y.process(left,right,enabled=True)
        assert_allclose(first,replay,atol=0,rtol=0)
        second=x.process(left,right,enabled=True)
        self.assertGreater(np.max(np.abs(np.asarray(first)-second)),1e-6)

    def test_ramp_supplied_step_not_endpoint_interpolation(self):
        ref,b=build_effective_spatial(**self.args());p=b['constructor']['params']
        # Firmware changes low index0 to1, then iteratively uses p8 through72.
        expected=np.empty(74);expected[0]=p[4]
        v=p[4]
        # 130409 loads previous,13040A increments BEFORE first output store.
        for i in range(1,73):v+=p[8];expected[i]=v
        expected[73]=p[5]
        assert_allclose(ref.ramp,expected,atol=0,rtol=0)
        self.assertGreater(ref.ramp[72],ref.ramp[73])

    def test_all_retained_sources_banks_pointer_variants_are_valid_distinct(self):
        for source in ('loader','b01'):
            for bank in range(1,5):
                for variant in ('20:default','20:3'):
                    ref,b=build_effective_spatial(**self.args(source,bank,variant))
                    out=ref.process(np.zeros(513,dtype=complex),np.zeros(513,dtype=complex),enabled=b['enabled'])
                    assert_array_equal(out,np.zeros((2,513)))
        a=bind_effective_spatial(**self.args())['constructor']['params']
        b=bind_effective_spatial(**self.args(variant='20:3'))['constructor']['params']
        self.assertAlmostEqual(a[27],.2);self.assertEqual(b[27],1.)
        self.assertEqual(bind_effective_spatial(**self.args(bank=4))['constructor']['eigen_selector'],0)

    def test_float32_half_conversion_bypass_and_gate_equality(self):
        a=self.args();offset=a['pointers'][1166]
        a['effective_config'][offset]=float(np.nextafter(np.float32(.5),np.float32(0)))
        a['effective_config'][offset+3]=2.
        ref,b=build_effective_spatial(**a)
        self.assertTrue(b['enabled']) # float32(.49999997+.5) rounds to1
        self.assertEqual(ref.gate_selector,2)
        a['effective_config'][offset]=0.
        ref,b=build_effective_spatial(**a)
        left=np.arange(513)+1j; right=2*left
        assert_array_equal(ref.process(left,right,enabled=b['enabled']),[left,right])
        self.assertEqual(ref.controls.gate,1.)

    def test_fail_closed_invalid_histories_snapshot_pointers_and_map(self):
        cases=[]
        a=self.args();a['assume_successful_fresh_initialization']=False;cases.append(a)
        a=self.args();a['pointers'][1166]=820;cases.append(a)
        a=self.args();a['pointers'].pop(1156);cases.append(a)
        a=self.args();a['edges']=list(range(75));cases.append(a)
        a=self.args();a['effective_config'][3+6]=74;cases.append(a)
        a=self.args();a['effective_config'][3+1]=-1;cases.append(a)
        a=self.args();a['effective_config'][3+11]=float('nan');cases.append(a)
        for args in cases:
            with self.assertRaises(ValueError):bind_effective_spatial(**args)


if __name__=='__main__':unittest.main()
