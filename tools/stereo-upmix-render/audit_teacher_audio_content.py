#!/usr/bin/env python3
"""Local, independent audio-content audit; not listening or reference-based SDR.

AST multi-label AudioSet scores and CLAP cosine similarities are not calibrated
stem-purity probabilities. Models receive audio only, never track/stem filenames.
Original levels and separately declared diagnostic gain views are both retained.
"""
import argparse
import gc
import hashlib
import importlib.metadata
import json
from pathlib import Path
import time
import numpy as np
from scipy import signal
import soundfile as sf
import torch

ROOT=Path(__file__).resolve().parents[2]
REVISIONS={'ast':('MIT/ast-finetuned-audioset-10-10-0.4593','f826b80d28226b62986cc218e5cec390b1096902'),
           'clap':('laion/clap-htsat-unfused','8fa0f1c6d0433df6e97c127f64b2a1d6c0dcda8a')}
# Same hypothesis set for every waveform, including mixtures and negative controls.
PROMPTS={
 'hi_hat':['The sound of a hi-hat playing a repeated rhythm.','A recording of isolated ticking hi-hat cymbals.'],
 'snare':['The sound of a snare drum playing a rhythm.','A recording of sharp snare drum hits.'],
 'kick':['The sound of a bass drum playing a rhythm.','A recording of deep kick drum beats.'],
 'cymbals':['The sound of ringing ride and crash cymbals.','A recording of sustained metallic cymbal crashes.'],
 'drum_kit':['The sound of a drum kit playing music.','A recording of multiple percussion instruments playing a beat.'],
 'lead_singing':['The sound of a solo singer singing a melody with words.','A recording of isolated lead vocals singing lyrics.'],
 'harmony_singing':['The sound of several singers singing vocal harmonies.','A recording of backing singers singing in harmony.'],
 'wordless_vocal':['The sound of a human voice singing a long wordless vowel.','A recording of sustained wordless singing, aah and ooh.'],
 'processed_vocal':['The sound of a repeated electronically processed vocal sample.','A recording of chopped and looped vocal samples in electronic music.'],
 'speech':['The sound of a person speaking words.','A recording of spoken speech.'],
 'synth':['The sound of a synthesizer playing electronic music.','A recording of electronic synthesizer notes and pads.'],
 'guitar':['The sound of an electric guitar playing music.','A recording of guitar strings playing chords or a melody.'],
 'piano':['The sound of a piano playing music.','A recording of piano keys playing chords and notes.'],
 'bass':['The sound of a bass guitar playing low notes.','A recording of a deep electronic bass line.'],
 'noise':['The sound of hissing static noise.','A recording of broadband noise and crackling.'],
 'music_mix':['The sound of a full music recording with multiple instruments.','A recording of a complete song with vocals and accompaniment.'],
}
AST_FOCUS=['Singing','Choir','Humming','Speech','Vocal music','A capella','Music',
    'Hi-hat','Cymbal','Snare drum','Bass drum','Drum kit','Drum machine','Percussion',
    'Synthesizer','Sampler','Piano','Guitar','Electric guitar','Bass guitar','Electronic music']

def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def rms(x):return float(np.sqrt(np.mean(np.square(x,dtype=np.float64))))
def db(x):return float(20*np.log10(max(x,1e-12)))

def describe_audio(x,mix):
    mid=(x[:,0]+x[:,1])*.5;side=(x[:,0]-x[:,1])*.5
    power=float(np.mean(x.astype(np.float64)**2))
    freq,psd=signal.welch(x,fs=48000,nperseg=2048,axis=0)
    psd=psd.sum(axis=1);total=float(psd.sum())+1e-30
    bands={f'{a}-{b}':float(psd[(freq>=a)&(freq<b)].sum()/total)
        for a,b in [(0,250),(250,1000),(1000,4000),(4000,8000),(8000,16000),(16000,24001)]}
    return dict(rms_dbfs=db(rms(x)),level_vs_mix_db=db(rms(x)/max(rms(mix),1e-12)),
        peak=float(np.max(abs(x))),side_energy_fraction=float(np.mean(side.astype(np.float64)**2)/(power+1e-30)),
        mono_retained_energy_fraction=float(np.mean(mid.astype(np.float64)**2)/(power+1e-30)),bands=bands)

def diagnostic_gain(x):
    # Never hide this per-window analysis gain in listening or teacher artifacts.
    return min(10.,.05/max(rms(x),1e-12),.95/max(float(np.max(abs(x))),1e-12))

def overlap(a,b):
    aa=float(np.sum(a.astype(np.float64)**2));bb=float(np.sum(b.astype(np.float64)**2))
    dot=float(np.sum(a.astype(np.float64)*b))
    return dict(cosine=dot/np.sqrt(aa*bb+1e-30),
        warning='Shared aligned waveform, NOT a leakage percentage or semantic verdict')

def gather(a):
    rows={}
    for manifest in a.manifests:
        for row in json.loads(manifest.read_text())['items']:rows[row['id']]=row
    jobs=[];clips=[]
    for audit in sorted(a.specialists.glob('*/audit.json')):
        rec=json.loads(audit.read_text());ident=rec['id']
        if a.ids and ident not in a.ids:continue
        row=rows[ident];mixpath=Path(row['stereo'])
        if digest(mixpath)!=rec['source_sha256']:raise ValueError('source changed')
        teacher=(a.base_teacher/ident/'teacher-provenance.json')
        if not teacher.exists():teacher=a.full_teacher/ident/'teacher-provenance.json'
        if digest(teacher)!=rec['base_teacher_sha256']:raise ValueError('base teacher changed')
        provenance=json.loads(teacher.read_text())
        paths={'mix':mixpath}
        expected={'mix':rec['source_sha256']}
        for name,h in provenance['output_sha256'].items():
            key='vocals' if '_(Vocals)_' in name else 'instrumental'
            paths[key]=teacher.parent/name;expected[key]=h
        for name,stem in rec['stems'].items():paths[name]=Path(stem['path']);expected[name]=stem['sha256']
        if a.direct_karaoke_root:
            direct_path=a.direct_karaoke_root/ident/'audit.json'
            if direct_path.exists():
                direct=json.loads(direct_path.read_text())
                if direct['source_sha256']!=rec['source_sha256'] or direct['original_audit_sha256']!=digest(audit):
                    raise ValueError('direct-input diagnostic provenance mismatch')
                for name,stem in direct['stems'].items():paths[name]=Path(stem['path']);expected[name]=stem['sha256']
        arrays={}
        for name,path in paths.items():
            if digest(path)!=expected[name]:raise ValueError('stem changed')
            x,rate=sf.read(path,always_2d=True,dtype='float32')
            if rate!=48000 or x.shape[1]!=2 or not np.isfinite(x).all():raise ValueError('invalid waveform')
            arrays[name]=x
        if len({x.shape for x in arrays.values()})!=1:raise ValueError('unaligned audio')
        arrays['mix_minus_hh']=arrays['mix']-arrays['hh']
        arrays['mix_minus_backing']=arrays['mix']-arrays['backing']
        core_start=row.get('core_offset_frames',0);core_frames=row.get('core_frames',row['frames'])
        arrays={k:x[core_start:core_start+core_frames] for k,x in arrays.items()}
        clip=dict(id=ident,title=row['title'],start=row.get('core_start_seconds',row.get('excerpt_start_seconds',0)),
            specialist_sha256=digest(audit),source_sha256=rec['source_sha256'],
            stems={},pair_overlap={f'{u}__{v}':overlap(arrays[u],arrays[v]) for u,v in
                [('hh','vocals'),('hh','demucs_vocals'),('hh','snare'),('backing','demucs_vocals'),('lead','backing')]})
        shared=min(1.,.95/max(float(np.max(abs(x))) for x in arrays.values()))
        for name,x in arrays.items():
            if a.stems and name not in a.stems:continue
            item=dict(sha256=expected.get(name),derived=name.startswith('mix_minus_'),
                metrics=describe_audio(x,arrays['mix']),windows=[])
            clip['stems'][name]=item
            for start in range(0,len(x),480000):
                window=x[start:min(start+480000,len(x))].mean(axis=1)
                if not len(window):continue
                w=dict(offset_seconds=start/48000,seconds=len(window)/48000,rms_dbfs=db(rms(window)),views={})
                item['windows'].append(w)
                if len(window)<48000:w['short_context_warning']='Final subsecond tail is zero-padded for inference; musical role unresolved'
                # Avoid presenting arbitrary labels on near-silence as evidence.
                if rms(window)<1e-4:w['abstain']='below -80 dBFS';continue
                for view,gain in [('shared_level',shared),('diagnostic_level',diagnostic_gain(window))]:
                    output=dict(gain=gain);w['views'][view]=output
                    padded=np.pad(window*gain,(0,max(0,48000-len(window))))
                    jobs.append((padded,output))
        clips.append(clip)
    # Sanity controls, not a music validation benchmark.
    rng=np.random.default_rng(1316);n=480000;t=np.arange(n)/48000
    pulse=np.exp(-30*(t%.5));noise=rng.standard_normal(n).astype(np.float32)
    controls={'silence':np.zeros(n,dtype=np.float32),'sine_440':(.08*np.sin(2*np.pi*440*t)).astype(np.float32),
        'synthetic_kick':(.12*np.sin(2*np.pi*70*t)*pulse).astype(np.float32),
        'synthetic_noise_hat':(signal.sosfilt(signal.butter(4,6000,fs=48000,btype='highpass',output='sos'),noise)*np.exp(-50*(t%.25))*.1).astype(np.float32)}
    control_results={}
    for name,x in controls.items():
        control_results[name]={};jobs.append((x,control_results[name]))
    return clips,jobs,control_results

def batch(items,n):
    for start in range(0,len(items),n):yield items[start:start+n]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--specialists',type=Path,default=ROOT/'build/reference-audio/v14-specialist-audit')
    p.add_argument('--manifests',type=Path,nargs='+',default=[ROOT/'build/reference-audio/v13-training-source/manifest.json',ROOT/'build/reference-audio/v14-full-source/manifest.json'])
    p.add_argument('--base-teacher',type=Path,default=ROOT/'build/reference-audio/v13-expanded-teacher')
    p.add_argument('--full-teacher',type=Path,default=ROOT/'build/reference-audio/v14-full-teacher')
    p.add_argument('--models',type=Path,default=ROOT/'build/hssr-audio-audit-models')
    p.add_argument('--direct-karaoke-root',type=Path);p.add_argument('--stems',nargs='+')
    p.add_argument('--ast-device',choices=['auto','cpu','mps'],default='auto')
    p.add_argument('--output',type=Path,required=True);p.add_argument('--ids',nargs='+');p.add_argument('--batch-size',type=int,default=4)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    torch.set_num_threads(2);torch.manual_seed(1316);np.random.seed(1316)
    clips,jobs,controls=gather(a)
    if a.ids and {c['id'] for c in clips}!=set(a.ids):raise ValueError('requested clips missing from audit')
    result=dict(schema=1,kind='independent-audio-content-diagnostics',clips=clips,controls=controls,
        models={},prompts=PROMPTS,package_versions={k:importlib.metadata.version(k) for k in ['torch','transformers','numpy','scipy']},
        limitations=['No human listening performed','No clean reference stems: no ground-truth purity/SDR',
            'AST sigmoid and CLAP cosine are uncalibrated for separated music',
            'Mono analysis may cancel side content; stereo energy metrics retained',
            'Diagnostic gain is analysis-only, not applied to teacher or playback',
            'Source identity does not establish musical foreground/background role'],training_approved=False)
    from transformers import ASTFeatureExtractor,ASTForAudioClassification,ClapModel,ClapProcessor
    from transformers.utils import logging as hf_logging
    hf_logging.disable_progress_bar()
    for stage in ['ast','clap']:
        directory=a.models/stage
        device='mps' if stage=='ast' and (a.ast_device=='mps' or (a.ast_device=='auto' and torch.backends.mps.is_available())) else 'cpu'
        result['models'][stage]=dict(repo=REVISIONS[stage][0],revision=REVISIONS[stage][1],
            files={f.name:digest(f) for f in directory.iterdir() if f.is_file()},device=device)
        began=time.monotonic()
        if stage=='ast':
            processor=ASTFeatureExtractor.from_pretrained(directory,local_files_only=True)
            model=ASTForAudioClassification.from_pretrained(directory,local_files_only=True,use_safetensors=True).to(device).eval()
            labels=[model.config.id2label[i] for i in range(model.config.num_labels)]
        else:
            processor=ClapProcessor.from_pretrained(directory,local_files_only=True)
            model=ClapModel.from_pretrained(directory,local_files_only=True,use_safetensors=False).eval()
            texts=[s for pair in PROMPTS.values() for s in pair]
            with torch.inference_mode():
                text_features=model.get_text_features(**processor(text=texts,padding=True,return_tensors='pt')).pooler_output
        for number,group in enumerate(batch(jobs,a.batch_size)):
            waveforms=[x for x,_ in group]
            with torch.inference_mode():
                if stage=='ast':
                    inputs=processor([signal.resample_poly(x,1,3).astype(np.float32) for x in waveforms],sampling_rate=16000,return_tensors='pt')
                    inputs={k:v.to(device) for k,v in inputs.items()}
                    scores=model(**inputs).logits.sigmoid().cpu().numpy()
                    for (_,output),values in zip(group,scores):
                        output['ast']=dict(top=[dict(label=labels[i],score=float(values[i])) for i in np.argsort(values)[-8:][::-1]],
                            focus={k:float(values[labels.index(k)]) for k in AST_FOCUS if k in labels})
                else:
                    inputs=processor(audio=waveforms,sampling_rate=48000,return_tensors='pt')
                    features=model.get_audio_features(**inputs).pooler_output
                    scores=(features@text_features.T).numpy()
                    for (_,output),values in zip(group,scores):
                        pairs=values.reshape(len(PROMPTS),2)
                        output['clap']=dict(cosine={k:float(v.mean()) for k,v in zip(PROMPTS,pairs)},
                            prompt_pair_spread={k:float(abs(v[0]-v[1])) for k,v in zip(PROMPTS,pairs)})
            if number%10==0:print(f'{stage}: {min((number+1)*a.batch_size,len(jobs))}/{len(jobs)} views; {time.monotonic()-began:.1f}s',flush=True)
        result['models'][stage]['seconds']=time.monotonic()-began
        (a.output/'results.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n')
        print(stage+' complete',flush=True)
        del model,processor;gc.collect()
    print(f'Audit complete: {len(clips)} clips; {len(jobs)} audio views; no training approval',flush=True)

if __name__=='__main__':main()
