#!/usr/bin/env python3
"""Same karaoke teacher on original mix vs previous all-vocals-only input.

An input-distribution diagnostic, not a new automatically accepted vocal teacher.
"""
import argparse
import json
import logging
from pathlib import Path
import time
import numpy as np
from scipy.signal import resample_poly
import soundfile as sf
import torch
from prepare_specialist_teacher_audit import restore,stats,MODELS
from prepare_hssr_native_teacher import digest,output_pair

ROOT=Path(__file__).resolve().parents[2]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--manifest',type=Path,default=ROOT/'build/reference-audio/v13-training-source/manifest.json')
    p.add_argument('--specialists',type=Path,default=ROOT/'build/reference-audio/v14-specialist-audit')
    p.add_argument('--teacher-root',type=Path,default=ROOT/'build/reference-audio/v13-expanded-teacher')
    p.add_argument('--ids',nargs='+')
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    base=ROOT/'build/reference-audio';models=ROOT/'build/hssr-teacher-models'
    rows={r['id']:r for r in json.loads(a.manifest.read_text())['items']}
    existing=sorted(a.specialists.glob('*/audit.json'))
    existing=[p for p in existing if p.parent.name in rows and (not a.ids or p.parent.name in a.ids)]
    if a.ids and {p.parent.name for p in existing}!=set(a.ids):raise ValueError('missing requested specialist')
    torch.set_num_threads(2);torch.manual_seed(13);np.random.seed(13)
    from audio_separator.separator import Separator
    separator=Separator(log_level=logging.WARNING,model_file_dir=str(models),sample_rate=44100,
        mdxc_params={'overlap':4,'batch_size':1})
    separator.load_model(MODELS['karaoke']);engine=separator.model_instance
    if int(engine.model_data_cfgdict.audio.sample_rate)!=44100:raise ValueError('unexpected native rate')
    for oldpath in existing:
        began=time.monotonic();old=json.loads(oldpath.read_text());row=rows[old['id']]
        for name,h in old['models']['karaoke']['files'].items():
            if digest(models/name)!=h:raise ValueError('karaoke model/config changed')
        if digest(row['stereo'])!=old['source_sha256']:raise ValueError('source changed')
        mix,rate=sf.read(row['stereo'],dtype='float32',always_2d=True)
        provenance=a.teacher_root/old['id']/'teacher-provenance.json'
        if digest(provenance)!=old['base_teacher_sha256']:raise ValueError('base teacher changed')
        vox,_=sf.read(output_pair(a.teacher_root/old['id'])[0],dtype='float32',always_2d=True)
        if rate!=48000 or mix.shape!=vox.shape:raise ValueError('invalid source')
        native=resample_poly(mix,147,160,axis=0).astype(np.float32)
        gain=min(1.,.9/max(float(np.max(abs(native))),1e-12))
        raw=engine.demix(mix=(native*gain).T,override_model_segment_size=False)['Vocals']
        lead=restore(raw,len(mix),gain)
        outputs={'lead_from_mix':lead,'vocal_residual_from_mix':(vox.astype(np.float64)-lead).astype(np.float32)}
        record=dict(id=old['id'],source_sha256=old['source_sha256'],original_audit_sha256=digest(oldpath),
            model=old['models']['karaoke'],common_input_gain=gain,input='original stereo mix',
            warning='Residual is all-vocals minus direct lead, not guaranteed backing-only or instrument-free',
            training_approved=False,stems={})
        d=a.output/old['id'];d.mkdir()
        for name,x in outputs.items():
            path=d/(name+'.wav');sf.write(path,x,48000,subtype='FLOAT')
            record['stems'][name]=dict(path=str(path.resolve()),sha256=digest(path),**stats(x))
        (d/'audit.json').write_text(json.dumps(record,indent=2)+'\n')
        print(old['id'],dict(old_lead_rms=old['stems']['lead']['rms'],direct_lead_rms=stats(lead)['rms']),
            f'{time.monotonic()-began:.1f}s',flush=True)

if __name__=='__main__':main()
