#!/usr/bin/env python3
"""Cover every local song once with nonoverlapping cores and inference context.

Preserves source-level train/validation/heldout boundaries. Nothing is trained.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import numpy as np
import soundfile as sf
from prepare_local_stereo_stress_corpus import sha256, display_title, slug
from prepare_hssr_separator_supervision import split_sources

RATE=48000

def core_ranges(frames, seconds=20, minimum_tail=8):
    step=int(seconds*RATE);result=[]
    for start in range(0,frames,step):
        end=min(start+step,frames)
        if result and end-start<minimum_tail*RATE:
            result[-1]=(result[-1][0],end)
        else:result.append((start,end))
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--pcm-subtype',choices=['FLOAT','PCM_16'],default='FLOAT',
        help='FLOAT preserves decoded peaks above full scale; PCM_16 is legacy comparison only')
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    sources=[]
    for partition in ['training','heldout']:
        for path in sorted((a.source_root/partition).iterdir()):
            if path.suffix.lower() in ['.mp3','.wav','.flac','.m4a','.ogg','.opus']:
                sources.append((path.resolve(),sha256(path),partition))
    train_ids={h for _,h,g in sources if g=='training'}
    held_ids={h for _,h,g in sources if g=='heldout'}
    if not train_ids or train_ids&held_ids:raise ValueError('missing training or source leakage')
    validation=split_sources(list(train_ids),.2)
    items=[];tracks=[]
    for path,source_hash,group in sources:
        tags=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries',
            'format_tags=artist,title,album,genre','-of','json',str(path)]))
        tags={k.lower():v for k,v in tags.get('format',{}).get('tags',{}).items()}
        title=tags.get('title',display_title(path));artist=tags.get('artist','unknown')
        split='heldout' if group=='heldout' else 'validation' if source_hash in validation else 'train'
        with tempfile.TemporaryDirectory(prefix='hssr-full-decode-') as tmp:
            decoded=Path(tmp)/'full.wav'
            codec='pcm_f32le' if a.pcm_subtype=='FLOAT' else 'pcm_s16le'
            subprocess.run(['ffmpeg','-v','error','-nostdin','-i',str(path),'-map','0:a:0',
                '-ac','2','-ar',str(RATE),'-c:a',codec,str(decoded)],check=True)
            with sf.SoundFile(decoded) as f:
                total=len(f);ranges=core_ranges(total)
                for index,(start,end) in enumerate(ranges):
                    left=max(0,start-2*RATE);right=min(total,end+2*RATE)
                    f.seek(left);audio=f.read(right-left,dtype='float32' if a.pcm_subtype=='FLOAT' else 'int16',always_2d=True)
                    ident=f'{slug(title)}-{source_hash[:8]}-c{index:03d}'
                    dest=(a.output/ident/'stereo.wav').resolve();dest.parent.mkdir()
                    sf.write(dest,audio,RATE,subtype=a.pcm_subtype)
                    core=audio[start-left:end-left].astype(np.float64)/(1 if a.pcm_subtype=='FLOAT' else 32768)
                    rms=float(np.sqrt(np.mean(core*core)));peak=float(np.max(abs(core)))
                    items.append(dict(id=ident,source=str(path),source_sha256=source_hash,
                        artist=artist,title=title,source_tags=tags,partition=split,group=group,
                        stereo=str(dest),stereo_sha256=sha256(dest),frames=len(audio),
                        pcm_subtype=a.pcm_subtype,decoded_over_full_scale_fraction=float(np.mean(abs(core)>1)),
                        sample_rate=RATE,duration_seconds=len(audio)/RATE,
                        source_duration_seconds=total/RATE,excerpt_start_seconds=left/RATE,
                        core_start_seconds=start/RATE,core_offset_frames=start-left,
                        core_frames=end-start,core_rms=rms,core_peak=peak,
                        silence_candidate=rms<10**(-60/20),training_eligible=False,
                        review_status='unreviewed'))
        tracks.append(dict(source_sha256=source_hash,artist=artist,title=title,partition=split,
            frames=total,clips=len(ranges)))
        print(f'{split}: {title} — {len(ranges)} cores, {total/RATE:.1f}s',flush=True)
    result=dict(schema=1,kind='full-song-coverage-review-first',sample_rate=RATE,
        pcm_subtype=a.pcm_subtype,integer_clipping_avoided=a.pcm_subtype=='FLOAT',
        context_seconds=2,nominal_core_seconds=20,private_audio=True,
        unique_core_minutes=sum(i['core_frames'] for i in items)/RATE/60,
        training_policy='No automatic admission. Heldout never enters training; review gates are separate.',
        tracks=tracks,items=items)
    (a.output/'manifest.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n')
    print(f'complete {len(tracks)} songs / {len(items)} clips / {result["unique_core_minutes"]:.2f} core minutes')

if __name__=='__main__':main()
