#!/usr/bin/env python3
"""Report explicit settled pre-B bank scenarios, never label them live playback."""
import argparse
import json
from pathlib import Path
import numpy as np
from amp_b_upstream import BInputPathFixedReference, select_level_bank


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('audit',type=Path)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    audit=json.loads(a.audit.read_text())
    frequencies=[20,30,55,80,120,250,4500,10000]
    rows=[]
    for params in audit['input_path_profiles']:
        for index in range(16):
            model=BInputPathFixedReference(params,eq_family='tone',
                shared_gains_tenths_db=[0]*4,bank_index=index,delay_samples=0)
            response=model.response(frequencies)
            rows.append(dict(model.scenario,gain_db=(20*np.log10(abs(response[:,0]))).tolist()))
    params=audit['input_path_profiles'][0]
    examples=[{'signed_level_integer':v,'candidate_bank':select_level_bank(v*(1<<24),
                params['bank_eq']['level_selector']['thresholds'])} for v in (0,-10,-20,-30,-40,-50,-61)]
    result={'source_sha256':audit['source_sha256'],
        'scope':'calibrated writer targets, tone controls zero, explicitly selected effective bank, zero delay, settled response BEFORE B; not speaker output, not vehicle readback',
        'frequencies_hz':frequencies,'level_examples':examples,'scenarios':rows,
        'warning':'candidate bank needs enable flag, commit gate, event delivery and smoothing completion; disabling bank control selects0, not neutral7'}
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'scenarios':len(rows),'live_state_proven':False,'output':str(a.output)}))


if __name__=='__main__':main()
