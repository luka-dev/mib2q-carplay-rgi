#!/usr/bin/env python3
"""Count architecture options with training-only protected output rows skipped.

No model training/selection; these dense counts exclude device/runtime costs.
"""
import argparse
import json
from pathlib import Path
from unified_upmix_net import UnifiedUpmixNet


def main(a):
    rows=[]
    for width in (24,28,32):
        net=UnifiedUpmixNet(width=width)
        full=net.cost();pieces=full['dense_macs_per_frame'].copy()
        pieces['four_source_projection']=pieces.pop('four_source_projection')//4
        selected=sum(pieces.values())*full['fps']
        rows.append(dict(width=width,depth=2,local=16,global_width=16,decoder=16,training_parameters=full['parameters'],
            all_source_dense_macs_s=full['dense_macs_per_second'],eligible_and_scene_dense_macs_s=selected,
            dense_screen_150m=selected<=150e6,model_status='default24 trained; wider options have no checkpoint, fitting, generalization or C port'))
    result=dict(scope='deterministic architecture dense-operation count only; no GPU/DEV/data or recipe change',options=rows,
        output_policy='same jointly trained four-source network, use only eligible output plus all scene heads at inference; protected training heads share the trunk but unused terminal rows can be skipped',
        limits='No device measurement. Excludes FFT/iFFT, norms/nonlinearities, features, DSP, memory/scheduler. Current C format/structs accept24 only; wider options require explicit runtime/state/export updates. Width32 margin is small and not a full-chain fit claim. Wider architecture is not selected by this table.')
    if a.out.exists():raise FileExistsError('Preserve prior cost table')
    a.out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);main(p.parse_args())
