#!/usr/bin/env python3
"""Verify every source/core, PCM reconstruction, partition boundaries and quality flags."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import numpy as np
from scipy import signal
import soundfile as sf
from teacher_audit_policy import coverage_report

def file_hash(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rough_fingerprint(audio):
    """Review-only coarse spectral/time similarity, NOT robust audio identification."""
    scale=32768 if np.issubdtype(audio.dtype,np.integer) else 1
    mono=signal.resample_poly(audio.mean(axis=1).astype(np.float32)/scale,1,4)
    freq,_,power=signal.spectrogram(mono,fs=12000,nperseg=1024,noverlap=512)
    edges=np.geomspace(60,6001,25)
    features=[]
    for a,b in zip(edges[:-1],edges[1:]):
        bins=(freq>=a)&(freq<b)
        features.append(power[bins].mean(axis=0) if bins.any() else power[np.argmin(abs(freq-(a+b)*.5))])
    features=np.stack(features)
    features=np.log1p(features*1e5)
    indices=np.linspace(0,features.shape[1],129).astype(int)
    pooled=np.stack([features[:,a:max(a+1,b)].mean(axis=1) for a,b in zip(indices[:-1],indices[1:])],axis=1)
    pooled-=pooled.mean(axis=1,keepdims=True)
    if not np.isfinite(pooled).all():raise ValueError('invalid fingerprint')
    return pooled.flatten()/max(float(np.linalg.norm(pooled)),1e-12)

def inspect(manifest):
    coverage=coverage_report(manifest);groups={}
    for row in manifest['items']:groups.setdefault(row['source_sha256'],[]).append(row)
    songs=[];clips=[];fingerprints=[]
    for source_hash,rows in groups.items():
        rows.sort(key=lambda r:r['core_start_seconds']);source=Path(rows[0]['source'])
        if file_hash(source)!=source_hash:raise ValueError(f'source changed: {source}')
        core_hash=hashlib.sha256();shape_total=0
        for row in rows:
            path=Path(row['stereo']);info=sf.info(path)
            if file_hash(path)!=row['stereo_sha256']:raise ValueError(f'clip changed: {row["id"]}')
            subtype=row.get('pcm_subtype','PCM_16');is_float=subtype=='FLOAT'
            x,rate=sf.read(path,dtype='float32' if is_float else 'int16',always_2d=True)
            if rate!=48000 or x.shape!=(row['frames'],2) or info.subtype!=subtype or not np.isfinite(x).all():raise ValueError('clip format mismatch')
            start=row['core_offset_frames'];core=x[start:start+row['core_frames']]
            core_hash.update(core.astype('<f4' if is_float else '<i2',copy=False).tobytes());shape_total+=len(core)
            floating=core.astype(np.float64)/(1 if is_float else 32768);power=float(np.mean(floating**2))
            side=(floating[:,0]-floating[:,1])*.5
            clips.append(dict(id=row['id'],technical_pass=True,partition=row['partition'],frames=len(core),
                rms_dbfs=float(10*np.log10(max(power,1e-24))),
                pcm_subtype=subtype,integer_clipping_avoided=is_float,
                peak=float(abs(floating).max()),over_full_scale_fraction=float(np.mean(abs(floating)>1)),
                full_scale_sample_fraction=float(np.mean(abs(floating)>=1 if is_float else (core==32767)|(core==-32768))),
                side_energy_fraction=float(np.mean(side**2)/(power+1e-30)),
                exact_zero_fraction=float(np.mean(core==0)),training_approved=False))
        # Independently decode the complete original and compare the concatenated cores bit-for-bit.
        raw=subprocess.check_output(['ffmpeg','-v','error','-nostdin','-i',str(source),'-map','0:a:0',
            '-ac','2','-ar','48000','-c:a','pcm_f32le' if is_float else 'pcm_s16le','-f','f32le' if is_float else 's16le','-'])
        pcm_hash=hashlib.sha256(raw).hexdigest()
        if pcm_hash!=core_hash.hexdigest() or len(raw)!=shape_total*(8 if is_float else 4):raise ValueError('core reconstruction differs from original decode')
        fingerprints.append(rough_fingerprint(np.frombuffer(raw,dtype='<f4' if is_float else '<i2').reshape(-1,2)))
        songs.append(dict(title=rows[0]['title'],source_sha256=source_hash,decoded_pcm_sha256=pcm_hash,
            partition=rows[0]['partition'],clips=len(rows),frames=shape_total,exact_core_reconstruction=True))
        print(f'verified {len(songs)}/{len(groups)} {rows[0]["title"]}',flush=True)
    pairs=[];exact=[]
    for i,a in enumerate(songs):
        for j,b in enumerate(songs[:i]):
            score=float(fingerprints[i]@fingerprints[j])
            pair=dict(a=a['title'],b=b['title'],partitions=[a['partition'],b['partition']],spectral_time_cosine=score,
                length_ratio=min(a['frames'],b['frames'])/max(a['frames'],b['frames']))
            if a['decoded_pcm_sha256']==b['decoded_pcm_sha256']:exact.append(pair)
            if a['partition']!=b['partition']:pairs.append(pair)
    return dict(schema=1,coverage=coverage,songs=songs,clips=clips,exact_decoded_duplicates=exact,
        highest_cross_partition_similarity=sorted(pairs,key=lambda p:-p['spectral_time_cosine'])[:10],
        duplicate_check_limit='Exact PCM duplicates detected; coarse similarity only prioritizes review, not a comprehensive near-duplicate guarantee',
        technical_pass=not any(len(set(p['partitions']))>1 for p in exact),
        integer_clipping_avoided=all(r['integer_clipping_avoided'] for r in clips),semantic_validation_complete=False,
        training_approved=False)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--manifest',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.output.exists():raise ValueError('fresh output required')
    result=inspect(json.loads(a.manifest.read_text()));result['manifest_sha256']=file_hash(a.manifest)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2,ensure_ascii=False,allow_nan=False)+'\n')
    print('Full source integrity complete; semantic teacher review separate',flush=True)

if __name__=='__main__':main()
