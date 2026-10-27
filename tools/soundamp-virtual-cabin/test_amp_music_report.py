"""Report checks for power units and per-output source-window alignment."""
from pathlib import Path
import sys
import unittest
import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parent))
from amp_dsp_chain import AmpChain, SR
from amp_music_report import band_powers, measure, parse_variant


class MusicReportTests(unittest.TestCase):
    def test_integrated_psd_has_mean_square_units(self):
        t=np.arange(SR*2)/SR
        p=band_powers(.25*np.sin(2*np.pi*1000*t))
        self.assertAlmostEqual(p[4],.25**2/2,places=7)

    def test_window_is_aligned_after_full_clip_processing(self):
        c=AmpChain()
        c.bus_sos=[np.zeros((0,6)) for _ in range(8)]
        c.out_sos=[np.zeros((0,6)) for _ in range(16)]
        rng=np.random.default_rng(9)
        source=rng.normal(0,.1,12000)
        source[:1000]*=10  # A crop/reset error changes the measured window.
        x=np.zeros((12000,6));x[:,0]=source
        ref=band_powers(source[1000:8000])
        result=measure(c,x,ref,1000,8000,0)
        for o in [0,8,10,11,12,14]:
            bus=c.out_src[o]
            expected=20*np.log10(abs(c.M[bus,0]*c.pair_a[bus]*c.output_pair_a[c.route[o]]))
            np.testing.assert_allclose(result['bands_re_dry_L'][o],expected,atol=1e-9)
        with self.assertRaises(ValueError):
            measure(c,x,ref,1000,11500,0)

    def test_variant_validation_rejects_nonfinite_and_ambiguous_names(self):
        self.assertEqual(parse_variant('base:0.4:-3:0'),('base',.4,-3,0))
        for value in ['dry:0.4:0:0','../x:0.4:0:0','x:nan:0:0','x:0.4:0:inf','x:0.4:1:0']:
            with self.assertRaises(ValueError):
                parse_variant(value)


if __name__=='__main__':
    unittest.main()
