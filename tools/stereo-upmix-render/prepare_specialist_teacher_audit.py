#!/usr/bin/env python3
"""Hierarchical teacher estimates for HUMAN audit, never automatic training truth.

Demucs6 -> drums -> DrumSep (hh/kick/snare/toms/ride/crash).
RoFormer all-vocals -> karaoke lead estimate -> backing/residual estimate.
Native-rate, shared-gain FLOAT files; raw disagreement/residuals stay visible.
"""
import argparse
import gc
import importlib.metadata
import json
import logging
from pathlib import Path
import random
import time
import numpy as np
from scipy.signal import resample_poly
import soundfile as sf
import torch
from prepare_hssr_native_teacher import digest, valid_pair, output_pair, POLICY

MODELS={'demucs':'htdemucs_6s.yaml','drumsep':'MDX23C-DrumSep-aufr33-jarredou.ckpt',
        'karaoke':'mel_band_roformer_karaoke_aufr33_viperx_sdr_10.1956.ckpt'}

def restore(x,frames,gain):
    x=np.asarray(x)
    if x.ndim!=2 or x.shape[0]!=2 or not np.isfinite(x).all():raise ValueError('invalid native stem')
    y=resample_poly(x.T.astype(np.float64),160,147,axis=0)
    if len(y)<frames:raise ValueError('truncated stem')
    return (y[:frames]/gain).astype(np.float32)

def stats(x):
    return dict(frames=len(x),peak=float(np.max(abs(x))),rms=float(np.sqrt(np.mean(x.astype(np.float64)**2))))

def relative_error(reference,estimate):
    return float(10*np.log10((np.sum((reference.astype(np.float64)-estimate)**2)+1e-20)/
        (np.sum(reference.astype(np.float64)**2)+1e-20)))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest',type=Path,required=True);p.add_argument('--teacher-root',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--model-dir',type=Path,required=True)
    p.add_argument('--ids',nargs='+',required=True)
    a=p.parse_args();all_items={r['id']:r for r in json.loads(a.manifest.read_text())['items']}
    if not set(a.ids)<=all_items.keys():raise ValueError('unknown audit ID')
    items=[all_items[i] for i in a.ids];a.output.mkdir(parents=True,exist_ok=True)
    torch.set_num_threads(2);torch.manual_seed(13);random.seed(13);np.random.seed(13)
    from audio_separator.separator import Separator
    from audio_separator.separator.architectures.demucs_separator import get_demucs_model
    states={}
    for item in items:
        d=a.output/item['id']
        if d.exists():raise ValueError(f'use new audit IDs/root; already exists {d}')
        identity=json.loads((a.teacher_root/item['id']/'teacher-provenance.json').read_text())['identity']
        if (identity['policy']!=POLICY or identity['source_sha256']!=digest(item['stereo'])
                or not valid_pair(a.teacher_root/item['id'],identity)):raise ValueError('invalid base teacher')
        d.mkdir()
        states[item['id']]=dict(schema=1,id=item['id'],source_sha256=digest(item['stereo']),
            base_teacher_sha256=digest(a.teacher_root/item['id']/'teacher-provenance.json'),
            policy='hierarchical-native44k1-shared-gain-float48-review-only-v1',
            audio_separator=importlib.metadata.version('audio-separator'),torch=importlib.metadata.version('torch'),
            training_eligible=False,human_review='unreviewed',stems={},models={},qa={})
    for stage,model_name in MODELS.items():
        print('stage '+stage,flush=True)
        separator=Separator(log_level=logging.WARNING,model_file_dir=str(a.model_dir),sample_rate=44100,
            demucs_params={'shifts':0,'overlap':.25},mdxc_params={'overlap':4,'batch_size':1})
        separator.load_model(model_name);engine=separator.model_instance
        if stage=='demucs':
            engine.demucs_model_instance=get_demucs_model(name='htdemucs_6s',repo=a.model_dir)
            engine.demucs_model_instance.to(separator.torch_device).eval()
            if int(engine.demucs_model_instance.samplerate)!=44100:raise ValueError('wrong Demucs sample rate')
            model_files=[a.model_dir/model_name,a.model_dir/'5c90dfd2-34c22ccb.th']
        else:
            if int(engine.model_data_cfgdict.audio.sample_rate)!=44100:raise ValueError('wrong native rate')
            config='config_drumsep_mdx23c.yaml' if stage=='drumsep' else 'mel_band_roformer_karaoke_aufr33_viperx_sdr_10.1956_config.yaml'
            model_files=[a.model_dir/model_name,a.model_dir/config]
        model_identity={x.name:digest(x) for x in model_files}
        for item in items:
            started=time.monotonic();d=a.output/item['id'];record=states[item['id']]
            if stage=='demucs':input_path=Path(item['stereo'])
            elif stage=='drumsep':input_path=d/'drums.wav'
            else:input_path=output_pair(a.teacher_root/item['id'])[0]
            source,rate=sf.read(input_path,dtype='float32',always_2d=True)
            if rate!=48000 or source.shape[1]!=2:raise ValueError('invalid stage input')
            native=resample_poly(source,147,160,axis=0).astype(np.float32)
            gain=min(1.,.9/max(float(np.max(abs(native))),1e-12))
            if stage=='demucs':
                array=engine.demix_demucs((native*gain).T)
                names=['bass','drums','other','demucs_vocals','guitar','piano']
                if len(array)!=6:raise ValueError('unexpected Demucs stem count')
                result=dict(zip(names,array))
            else:
                result=engine.demix(mix=(native*gain).T,override_model_segment_size=False)
                if stage=='karaoke':result={'lead':result['Vocals']}
                elif set(result)!=set(['kick','snare','toms','hh','ride','crash']):raise ValueError('unexpected drum taxonomy')
            outputs={name:restore(x,len(source),gain) for name,x in result.items()}
            if stage=='karaoke':
                outputs['backing']=(source.astype(np.float64)-outputs['lead']).astype(np.float32)
                record['qa']['backing_is_complement_not_ground_truth']=True
                record['qa']['lead_plus_backing_max_error']=float(np.max(abs(source-outputs['lead']-outputs['backing'])))
            else:
                estimate=sum(x.astype(np.float64) for x in outputs.values())
                record['qa'][stage+'_relative_residual_db']=relative_error(source,estimate)
                if stage=='drumsep':outputs['drum_residual']=(source-estimate).astype(np.float32)
            for name,x in outputs.items():
                if not np.isfinite(x).all():raise ValueError('nonfinite teacher output')
                target=d/(name+'.wav');sf.write(target,x,48000,subtype='FLOAT')
                record['stems'][name]=dict(path=str(target.resolve()),sha256=digest(target),**stats(x),
                    parent=stage,common_input_gain=gain)
            if stage=='demucs':
                rv,_=sf.read(output_pair(a.teacher_root/item['id'])[0],dtype='float32')
                record['qa']['vocal_teacher_disagreement_db']=relative_error(rv,outputs['demucs_vocals'])
            record['models'][stage]=dict(files=model_identity,device=str(separator.torch_device),
                overlap=4 if stage!='demucs' else .25,shifts=0 if stage=='demucs' else None)
            # Partial stages are auditable on disk; UI publishes only complete sets.
            (d/'progress.json').write_text(json.dumps(record,indent=2)+'\n')
            if stage=='karaoke':
                record['qa']['technical_checks_passed']=all(s['frames']==len(source) for s in record['stems'].values())
                record['qa']['semantic_accuracy_verified']=False
                (d/'audit.json').write_text(json.dumps(record,indent=2)+'\n')
            print(f'{stage} {item["id"]}: {time.monotonic()-started:.1f}s',flush=True)
        del engine,separator;gc.collect()
        if torch.backends.mps.is_available():torch.mps.empty_cache()
    print('specialist review ready; no training admission',flush=True)

if __name__=='__main__':main()
