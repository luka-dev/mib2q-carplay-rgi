"""API18/19 finite scalar targets; never an actual-car preset."""
import copy
import json
import unittest
from amp_b_control import api18_19_parameter_targets,zero_gain_eq_coefficients,curve_contribution
from test_amp_b_control import OUT,decoded_curve


class BParameterControlTests(unittest.TestCase):
    def setUp(self):
        self.binding=json.loads((OUT/'b_level_control.json').read_text())['gain_controls']['parameter_controls']

    def run_api(self,api,x,*,source='b01',selectors=None,state=None):
        return api18_19_parameter_targets(self.binding,source=source,api=api,coordinate=x,
            contributions={d:[0]*9 for d in range(44,59)} if state is None else state,
            shadow_selectors={16:0,20:0} if selectors is None else selectors)

    def test_b01_api19_delay_and_gain_targets(self):
        for x,delay,gain44 in [(-100,162,.7),(-50,162,.7),(0,96,1),(100,19,1.2)]:
            r=self.run_api(19,x)
            self.assertAlmostEqual(r['targets'][44],gain44,places=6)
            for d in (52,53):self.assertEqual(r['targets'][d],delay)
            self.assertAlmostEqual(r['targets'][48],1.778279423713684+(-.9 if x<0 else .2 if x>0 else 0),places=6)

    def test_api18_additional_gain_and_untouched_banks(self):
        state={d:[.01*i for i in range(9)] for d in range(44,59)};saved=copy.deepcopy(state)
        r=self.run_api(18,9,state=state)
        self.assertEqual(state,saved)
        for d in (51,52,53):
            self.assertNotIn(d,r['targets']);self.assertEqual(r['contributions'][d],state[d])
        self.assertAlmostEqual(r['targets'][47],.8912509083747864+sum(state[47])-.01-.5)

    def test_shadow_selector_priority_and_variants(self):
        for selectors,expected in [({16:5,20:3},'shadow16_eq5'),
                                    ({16:4,20:3},'shadow20_eq3'),({16:4,20:2},'default')]:
            r=self.run_api(19,100,selectors=selectors)
            self.assertEqual(r['curve_variants'][52],expected)
            self.assertEqual(r['curve_variants'][53],expected)
        # Real tables currently match; alter a copy to ensure selection is used.
        p=self.binding['apis'][0]['parameters'][8]
        p['variants']['shadow20_eq3']['records'][1][1]['b01']=-50
        self.assertEqual(self.run_api(19,100,selectors={16:4,20:3})['targets'][52],46)
        self.assertEqual(self.run_api(19,100,selectors={16:5,20:3})['targets'][52],19)

    def test_zero_gain_eq_matches_both_images_across_controls(self):
        for source in ('loader','b01'):
            for api in (18,19):
                for x in (-200,-100,-50,-9,0,9,100,200):
                    r=self.run_api(api,x,source=source)
                    for eq in self.binding['eq_rebuilds']:
                        actual=zero_gain_eq_coefficients(r['eq_design_records'][eq['coefficient_config_offset']])
                        self.assertEqual(actual,[v[source] for v in eq['coefficient_words']])

    def test_nonzero_prior_eq_is_not_silently_identity(self):
        state={d:[0]*9 for d in range(44,59)};state[55][8]=6
        r=self.run_api(19,100,state=state)
        self.assertEqual(r['eq_design_records'][295][2],6)
        with self.assertRaises(ValueError):zero_gain_eq_coefficients(r['eq_design_records'][295])

    def test_other_slots_survive_and_clamps_differ(self):
        state={d:[0]*9 for d in range(44,59)}
        state[44][8]=10;state[52][8]=400;state[55][8]=-150;state[56][8]=50
        r=self.run_api(19,0,state=state)
        self.assertEqual([r['targets'][d] for d in (44,52,55,56)],[5,288,-100,20])

    def test_all_extracted_contributions_against_decoded_kernel(self):
        for api in self.binding['apis']:
            for p in api['parameters']:
                for variant in p['variants'].values():
                    for source in ('loader','b01'):
                        records=[[v[source] for v in point] for point in variant['records']]
                        for x in (-200,-50,-9,0,9,100,200):
                            _,state=decoded_curve(x,records,0,[0]*9)
                            self.assertAlmostEqual(state[6],curve_contribution(x,records),places=5)

    def test_rejects_missing_state_and_invalid_domains(self):
        for api,x in [(25,0),(18,201),(19,float('nan'))]:
            with self.assertRaises(ValueError):self.run_api(api,x)
        with self.assertRaises(ValueError):self.run_api(18,0,selectors={16:5.0,20:3})
        with self.assertRaises(ValueError):self.run_api(19,0,state={44:[0]*9})
        with self.assertRaises(ValueError):self.run_api(18,0,source='current-car')
        for record in ([20,100,0,3,1],[20,100,float('nan'),0,1],[20,100,1,0,1]):
            with self.assertRaises(ValueError):zero_gain_eq_coefficients(record)


if __name__=='__main__':unittest.main()
