import copy
import unittest
from amp_music_report import GROUPS
from audit_amp_matrix_music_control import paired_deltas,verify_comparable


class MatrixMusicControlTests(unittest.TestCase):
    def test_paired_deltas_are_db_differences_not_amplitude_sums(self):
        a={'groups':{g:{'power_dbfs':[-20,-30]} for g in GROUPS}}
        b={'groups':{g:{'power_dbfs':[-23,-25]} for g in GROUPS}}
        self.assertEqual(paired_deltas(a,b),{g:[-3,5] for g in GROUPS})

    def test_comparison_rejects_changed_files_or_windows(self):
        old={'bands':[[20,60]],'group_channels':GROUPS,'variants':[['v',.4,0,0]],
             'files':{'x':{'sha256':'abc'}},'analysis':{'window_source_seconds':[5,55],
             'upmix_latency_frames':512,'alignment':'a','power':'p','reference':'BANK1'}}
        verify_comparable(old,copy.deepcopy(old))
        for change in ('hash','window','variant','keys'):
            new=copy.deepcopy(old)
            if change=='hash':new['files']['x']['sha256']='def'
            elif change=='window':new['analysis']['window_source_seconds']=[2,10]
            elif change=='variant':new['variants'][0][3]=9.5
            else:new['files']['y']=new['files']['x']
            with self.assertRaises(ValueError):verify_comparable(old,new)


if __name__=='__main__':unittest.main()
