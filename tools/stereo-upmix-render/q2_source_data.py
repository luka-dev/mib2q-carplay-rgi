"""Q2 metadata index and bounded original-source crops; never rear-role labels.

Index creation reads JSON/stat metadata only, including sealed TEST identities.
The reader refuses TEST PCM. All audio crops retain common origin, unit gain and
declared zero extension of shorter originals, never independent normalization.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import time

import numpy as np
import soundfile as sf

CLASSES=('lead_vocal','backing_vocal','other_vocal','bass','drums',
         'high_percussion','percussion','guitar','keys','strings_winds',
         'other_plucked','other_unknown')
SCHEMA='q2-original-source-index-v1'
CATALOG_SHA='ecac5c4d4477167456da372fb5ff095e45a8a76652cbe9d69e0922e46f44510d'
PARTITIONS={'train':'TRAIN','validation':'DEV','heldout':'TEST'}

def require(value,message):
    if not value:raise ValueError(message)

def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def signature(path):
    s=Path(path).stat();return (s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns)

def contained(root,relative):
    p=Path(relative)
    require(not p.is_absolute() and p.parts and '..' not in p.parts,'Unsafe relative source path')
    result=Path(root)/p
    require(result.is_file() and not result.is_symlink(),'Missing/symlink source')
    require(result.resolve().is_relative_to(Path(root).resolve()),'Source escapes root')
    return result

def source_class(track):
    stem,kind=track['stem'],track['track_type']
    if stem=='vocals':
        if kind in ('lead_male_singer','lead_female_singer'):return 0
        if kind=='background_vocals':return 1
        return 2
    if stem=='bass':return 3
    if stem=='drums':return 5 if kind in ('hi_hat','cymbals') else 4
    return {'percussion':6,'guitar':7,'piano':8,'other_keys':8,
            'bowed_strings':9,'wind':9,'other_plucked':10,'other':11}.get(stem)

def known_mask(tracks):
    """Conservative completeness for detector labels; zeros need not be absent."""
    known=np.ones(len(CLASSES),dtype=bool)
    if any(t['has_bleed'] is not False or t['isolated_source_candidate'] is not True
           or source_class(t) is None for t in tracks):known[:]=False
    if any(t['stem']=='drums' and t['track_type'] in
           ('full_acoustic_drumkit','drum_machine','overheads') for t in tracks):
        known[[5,6]]=False
    if any(t['stem']=='vocals' and source_class(t)==2 for t in tracks):known[[0,1]]=False
    if any(t['stem']=='other' for t in tracks):known[:11]=False
    return known

def admission_reasons(tracks,quarantine):
    reasons=[]
    if any(t['has_bleed'] is not False or t['isolated_source_candidate'] is not True for t in tracks):reasons.append('bleed_or_unknown')
    if any(source_class(t) is None for t in tracks):reasons.append('unmapped_source_category')
    if quarantine:reasons.append('duplicate_quarantine')
    return reasons

def _validate_rows(rows):
    ids=set();artists={p:set() for p in PARTITIONS.values()};groups={p:set() for p in PARTITIONS.values()}
    for row in rows:
        require(row['song_id'] not in ids and row['partition'] in artists,'Duplicate song/partition')
        ids.add(row['song_id']);artists[row['partition']].add(row['artist_key']);groups[row['partition']].add(row['group_id'])
        require(row['sample_rate']==44100 and 0<row['common_frames']<=row['frames'],'Invalid native timeline')
        require(len(row['class_known'])==len(CLASSES) and all(type(v)is bool for v in row['class_known']),'Invalid known mask')
    for a,b in (('TRAIN','DEV'),('TRAIN','TEST'),('DEV','TEST')):
        require(not artists[a]&artists[b] and not groups[a]&groups[b],'Artist/duplicate group crosses split')

def build_index(catalog_path,source_root,output,*,expected_catalog_sha256=CATALOG_SHA,
                min_free_bytes=50*1024**3,max_seconds=120):
    """Metadata only. Generic other is a track-identity head, not noise truth.

    Retain every original identity, including ineligible/TEST. No activity filter.
    Index is not a new dataset split or a role/quality approval.
    """
    start=time.monotonic();catalog_path=Path(catalog_path).absolute();source_root=Path(source_root).resolve(strict=True);output=Path(output)
    require(not output.exists() and output.parent.is_dir(),'Fresh index file in existing owner required')
    require(math.isfinite(max_seconds) and max_seconds>0 and type(min_free_bytes)is int and min_free_bytes>=0,'Invalid limits')
    require(shutil.disk_usage(output.parent).free>=min_free_bytes,'Disk reserve')
    c_sig=signature(catalog_path);require(digest(catalog_path)==expected_catalog_sha256,'Catalog SHA mismatch')
    catalog=json.loads(catalog_path.read_text());require(signature(catalog_path)==c_sig,'Catalog changed')
    parents={s['id']:s['id'] for s in catalog['songs']}
    def find(k):
        while parents[k]!=k:k=parents[k]
        return k
    for group in catalog.get('duplicate_groups',[]):
        ids={r['song'] for r in group};require(ids<=parents.keys(),'Unknown duplicate member')
        roots={find(k) for k in ids}
        if roots:
            root=min(roots)
            for key in roots:parents[key]=root
    rows=[];metadata_signatures={}
    for song in catalog['songs']:
        require(time.monotonic()-start<max_seconds,'Metadata time cap')
        p=contained(source_root,song['metadata_path']);sig=signature(p)
        require(digest(p)==song['metadata_sha256'] and signature(p)==sig,'Source metadata SHA changed')
        metadata=json.loads(p.read_text());require(signature(p)==sig,'Metadata changed during parse')
        metadata_signatures[p]=sig
        tracks=song['tracks'];require(tracks and len({t['id'] for t in tracks})==len(tracks),'Duplicate/empty source list')
        actual={}
        for stem in metadata['stems']:
            for t in stem['tracks']:
                require(t['id'] not in actual,'Duplicate original metadata source')
                actual[t['id']]=(stem['stemName'],t['trackType'],t.get('has_bleed'))
        require(actual=={t['id']:(t['stem'],t['track_type'],t['has_bleed']) for t in tracks},'Original metadata labels differ')
        require(len({t['path'] for t in tracks})==len(tracks),'Duplicate source path')
        native=all(t['header']['sample_rate']==44100 and t['header']['channels']==2 for t in tracks)
        require(native,'Index v1 only native44100 stereo; no silent conversion')
        require(all(type(t['header']['frames'])is int and t['header']['frames']>0 for t in tracks),'Invalid source frames')
        for t in tracks:
            require(len(t['sha256'])==64,'Missing original body hash')
            require(contained(source_root,t['path']).stat().st_size==t['file_bytes'],'Original byte size differs')
        reasons=admission_reasons(tracks,song.get('cross_partition_duplicate_quarantine'))
        rows.append(dict(song_id=song['id'],artist=song['artist'],artist_key=song['artist_key'],title=song['title'],
            genre=song.get('genre'),partition=PARTITIONS[song['partition']],historical_partition=song['partition'],
            group_id='moises-'+find(song['id']),sample_rate=44100,frames=max(t['header']['frames'] for t in tracks),
            common_frames=min(t['header']['frames'] for t in tracks),tracks=tracks,
            metadata_path=song['metadata_path'],metadata_sha256=song['metadata_sha256'],
            class_known=known_mask(tracks).tolist(),reconstruction_known=[not reasons]*len(CLASSES),
            fully_mapped_q1_subset=not reasons and all(t['stem']!='other' for t in tracks),
            complete_reconstruction_candidate=not reasons,exclusion_reasons=reasons,
            role_status='UNKNOWN',rear_role_approved=False))
    _validate_rows(rows)
    require(digest(catalog_path)==expected_catalog_sha256 and signature(catalog_path)==c_sig,'Catalog changed after index')
    for p,sig in metadata_signatures.items():require(signature(p)==sig,'Metadata changed after index')
    result=dict(schema=SCHEMA,classes=list(CLASSES),catalog_path=str(catalog_path),catalog_sha256=expected_catalog_sha256,
        source_root=str(source_root),builder_sha256=digest(__file__),split_seed=catalog['split_seed'],rows=rows,
        source_bodies_hashed=False,source_pcm_decoded=False,test_pcm_accessed=False,
        tail_policy='common origin; zero extension after declared original end; no tail truncation',
        source_identity_not_audible_absence=True,noise_positive_coverage='UNKNOWN',role_approved=False,car_approved=False)
    with output.open('x') as f:json.dump(result,f,indent=2);f.write('\n')
    return output

def load_index(path,*,expected_sha256=None):
    path=Path(path).absolute();require(path.is_file() and not path.is_symlink(),'Missing/symlink index')
    require(path.stat().st_size<=8*1024**2,'Oversized index');sig=signature(path);body=digest(path)
    if expected_sha256 is not None:require(body==expected_sha256,'Index SHA differs')
    index=json.loads(path.read_text());require(signature(path)==sig,'Index changed')
    require(index['schema']==SCHEMA and index['classes']==list(CLASSES) and index['car_approved']is False,'Index contract differs')
    _validate_rows(index['rows']);return index

def array_digest(array):return hashlib.sha256(np.ascontiguousarray(array).tobytes()).hexdigest()

def load_bundle(manifest_path,*,expected_sha256=None,max_bytes=256*1024**2):
    """Portable bounded TRAIN-only qualification arrays, not full-corpus reset."""
    path=Path(manifest_path).absolute();sig=signature(path)
    if expected_sha256 is not None:require(digest(path)==expected_sha256,'Bundle manifest SHA differs')
    manifest=json.loads(path.read_text())
    require(manifest['schema']=='q2-source-fit-input-v1' and manifest['complete'] is True
        and manifest['classes']==list(CLASSES) and manifest['car_approved'] is False,'Bundle contract differs')
    require(len(manifest['cases'])==4 and len({r['song_id'] for r in manifest['cases']})==4,'Fixed four-song bundle')
    rows=[];used=0;signatures={}
    for spec in manifest['cases']:
        require(spec['partition']=='TRAIN','Qualification bundle must stay historicalTRAIN')
        artifact=spec['artifact'];p=contained(path.parent,artifact['path']);signatures[p]=signature(p)
        used+=artifact['decoded_bytes'];require(used<=max_bytes,'Bundle decoded byte cap')
        require(p.stat().st_size==artifact['bytes'] and digest(p)==artifact['sha256'],'Bundle body differs')
        import zipfile
        with zipfile.ZipFile(p) as archive:require(sum(i.file_size for i in archive.infolist())<=artifact['decoded_bytes']+16384,'Oversized archive expansion')
        with np.load(p,allow_pickle=False) as archive:arrays={k:archive[k] for k in archive.files}
        require(set(arrays)==set(artifact['arrays']),'Bundle array inventory differs')
        for key,value in arrays.items():
            require(artifact['arrays'][key]==dict(shape=list(value.shape),dtype=value.dtype.str,sha256=array_digest(value)),'Bundle array header/SHA differs')
            require(np.isfinite(value).all(),'Nonfinite bundle array')
        n=spec['frames'];require(arrays['mixture'].shape==(n,2) and arrays['targets'].shape==(n,len(CLASSES),2)
            and arrays['residual'].shape==(n,2),'Bundle audio geometry differs')
        require(all(arrays[k].dtype==np.float32 for k in ('mixture','targets','residual')),'Bundle FLOAT required')
        require(all(arrays[k].shape==(len(CLASSES),) and arrays[k].dtype==bool for k in ('reconstruction_known','detection_known')),'Bundle known-mask differs')
        require(0<=spec['core_start']<spec['core_stop']==n and spec['sample_rate']==48000,'Bundle core/rate differs')
        rows.append(dict(spec,**arrays,class_known=arrays['detection_known']))
    require(signature(path)==sig and all(signature(p)==s for p,s in signatures.items()),'Bundle changed during load')
    return manifest,rows

class SourceReader:
    """Per-process bounded crop reader; first access fully hashes each used WAV.

    Native and polyphase crops use whole-song coordinates. No TEST access and no
    mutable disk cache. `exposure` counts requested content, not successful fit.
    """
    def __init__(self,index_path,*,expected_sha256=None,max_crop_seconds=10.,max_rss_crop_bytes=256*1024**2):
        require(math.isfinite(max_crop_seconds) and 0<max_crop_seconds<=120,'Crop duration cap')
        self.path=Path(index_path).absolute();self.signature=signature(self.path);self.sha256=digest(self.path)
        self.index=load_index(self.path,expected_sha256=expected_sha256);self.root=Path(self.index['source_root'])
        self.rows={r['song_id']:r for r in self.index['rows']};self.verified={};self.exposure={}
        self.max_seconds=max_crop_seconds;self.max_bytes=max_rss_crop_bytes
        catalog=Path(self.index['catalog_path']);self.catalog_signature=signature(catalog)
        require(digest(catalog)==self.index['catalog_sha256'],'Bound original catalog changed')
        originals={r['id']:r for r in json.loads(catalog.read_text())['songs']}
        require(set(originals)==set(self.rows),'Index catalog song set differs')
        for song,row in self.rows.items():
            original=originals[song]
            require(row['tracks']==original['tracks'] and row['metadata_sha256']==original['metadata_sha256']
                and row['metadata_path']==original['metadata_path'] and row['partition']==PARTITIONS[original['partition']]
                and row['artist_key']==original['artist_key'],'Index original identity differs')
            require(row['class_known']==known_mask(row['tracks']).tolist(),'Known mask differs')
            reasons=admission_reasons(original['tracks'],original.get('cross_partition_duplicate_quarantine'))
            require(row['exclusion_reasons']==reasons and row['complete_reconstruction_candidate'] is (not reasons)
                and row['reconstruction_known']==[not reasons]*len(CLASSES),'Reconstruction admission differs')
            require(row['frames']==max(t['header']['frames'] for t in original['tracks'])
                and row['common_frames']==min(t['header']['frames'] for t in original['tracks']),'Original timeline differs')

    def _verify(self,path,track):
        sig=signature(path)
        if path not in self.verified:
            require(digest(path)==track['sha256'] and signature(path)==sig,'Original source body changed')
            with sf.SoundFile(path) as f:
                require((f.samplerate,f.channels,len(f))==(44100,2,track['header']['frames']),'Original header differs')
                require(f.subtype in ('PCM_16','PCM_24','PCM_32','FLOAT','DOUBLE'),'Unsupported original encoding')
            self.verified[path]=sig
        require(self.verified[path]==sig,'Previously verified source changed')

    def read_crop(self,song_id,start_frame,frames,*,target_rate=48000,context_frames=0,partition=None):
        row=self.rows[song_id]
        require(row['partition']!='TEST','Sealed TEST PCM access refused')
        if partition is not None:require(row['partition']==partition,'Requested partition differs')
        require(row['complete_reconstruction_candidate'],'Source song not admitted for complete targets')
        require(type(start_frame)is int and start_frame>=0 and type(frames)is int and frames>0,'Invalid crop geometry')
        require(type(context_frames)is int and context_frames>=0,'Invalid real-past context')
        require(target_rate in (44100,48000),'Unsupported rate')
        scored_start,scored_frames=start_frame,frames
        past=min(context_frames,start_frame);start_frame-=past;frames+=past
        require(frames<=self.max_seconds*target_rate,'Rate/duration cap')
        total=(row['frames']*target_rate+44099)//44100
        require(start_frame+frames<=total,'Crop outside declared full timeline')
        require(signature(self.path)==self.signature and digest(self.path)==self.sha256,'Index changed')
        meta=contained(self.root,row['metadata_path']);meta_sig=signature(meta)
        require(digest(meta)==row['metadata_sha256'] and signature(meta)==meta_sig,'Metadata changed')
        if target_rate==44100:native_start,native_end=start_frame,start_frame+frames
        else:
            # Align input origin to147 samples => output origin integer multiple160.
            # scipy resample_poly default FIR half support is10 native samples;
            #32 native guard samples cover it conservatively at each edge.
            native_start=max(0,((start_frame*147//160-32)//147)*147)
            native_end=min(row['frames'],((start_frame+frames)*147+159)//160+32)
        n=native_end-native_start
        require(n*2*(len(CLASSES)+3)*8<=self.max_bytes,'Crop allocation cap')
        targets=np.zeros((n,len(CLASSES),2),dtype='f8');mix=np.zeros((n,2),dtype='f8');residual=np.zeros((n,2),dtype='f8')
        proofs=[]
        for track in row['tracks']:
            path=contained(self.root,track['path']);self._verify(path,track)
            available=max(0,min(native_end,track['header']['frames'])-native_start)
            if available:
                with sf.SoundFile(path) as f:f.seek(native_start);audio=f.read(available,dtype='float64',always_2d=True)
                require(audio.shape==(available,2) and np.isfinite(audio).all(),'Invalid native crop')
                require(np.array_equal(audio,audio.astype('f4').astype('f8')),'Original values not exactly float32 representable')
                mix[:available]+=audio;k=source_class(track)
                if k is None:residual[:available]+=audio
                else:targets[:available,k]+=audio
            proofs.append(dict(source_id=track['id'],sha256=track['sha256'],native_read_start=native_start,
                native_read_frames=available,zero_extension_frames=n-available,original_frames=track['header']['frames']))
        # One concatenated polyphase operation: identical phase/filter/gain for
        # mixture, every disjoint source family and residual.
        packed=np.concatenate((mix[:,None,:],targets,residual[:,None,:]),axis=1)
        if target_rate==48000:
            from scipy.signal import resample_poly
            packed=resample_poly(packed,160,147,axis=0,padtype='constant')
            offset=start_frame-native_start*160//147;packed=packed[offset:offset+frames]
        require(packed.shape==(frames,len(CLASSES)+2,2) and np.isfinite(packed).all(),'Prepared geometry/nonfinite')
        packed=packed.astype('f4');x,t,r=packed[:,0],packed[:,1:-1],packed[:,-1]
        closure=float(np.max(np.abs(x.astype('f8')-t.astype('f8').sum(1)-r.astype('f8'))))
        require(closure<=max(1e-7,float(np.max(np.abs(packed)))*2e-6),'Float closure outside declared rounding')
        for track in row['tracks']:require(signature(contained(self.root,track['path']))==self.verified[contained(self.root,track['path'])],'Source changed during crop')
        require(signature(meta)==meta_sig and signature(self.path)==self.signature,'Metadata/index changed during crop')
        exposure=self.exposure.setdefault(song_id,dict(partition=row['partition'],artist_key=row['artist_key'],presentations=0,intervals=[]))
        exposure['presentations']+=1;exposure['intervals'].append([scored_start,scored_start+scored_frames,target_rate])
        return dict(id=song_id,song_id=song_id,artist=row['artist'],group_id=row['group_id'],partition=row['partition'],sample_rate=target_rate,
            mixture=x,targets=t,residual=r,class_known=np.asarray(row['class_known'],dtype=bool),
            detection_known=np.asarray(row['class_known'],dtype=bool),reconstruction_known=np.asarray(row['reconstruction_known'],dtype=bool),
            core_start=past,core_stop=frames,global_start_frame=start_frame,classes=CLASSES,role_status='UNKNOWN',
            provenance=dict(index_sha256=self.sha256,start_frame=start_frame,frames=frames,original_sample_rate=44100,
                core_start=past,core_stop=frames,scored_global_start_frame=scored_start,requested_context_frames=context_frames,actual_context_frames=past,
                original_song_frames=row['frames'],original_common_frames=row['common_frames'],source_proofs=proofs,
                target_rate=target_rate,resample='none' if target_rate==44100 else 'shared scipy.signal.resample_poly160/147 constantpad globalphase',
                independent_gain=False,common_gain=1.,source_tails_truncated=False,closure_max_abs=closure,test_pcm_accessed=False),car_approved=False)

    def verify_sources(self):
        """End-of-run checks: body SHA already verified once; no rehash per crop."""
        require(signature(self.path)==self.signature and digest(self.path)==self.sha256,'Index changed at finish')
        p=Path(self.index['catalog_path'])
        require(signature(p)==self.catalog_signature and digest(p)==self.index['catalog_sha256'],'Catalog changed at finish')
        for p,sig in self.verified.items():require(signature(p)==sig,'Original changed after verified read')
        for song in self.exposure:
            row=self.rows[song];require(digest(contained(self.root,row['metadata_path']))==row['metadata_sha256'],'Source metadata changed at finish')
        return dict(verified_source_bodies=len(self.verified),signatures_rechecked=True,index_sha256=self.sha256,test_pcm_accessed=False)

    def exposure_summary(self):
        result={}
        for song,row in self.exposure.items():
            intervals=sorted((a/r,b/r) for a,b,r in row['intervals']);merged=[]
            for a,b in intervals:
                if merged and a<=merged[-1][1]:merged[-1][1]=max(b,merged[-1][1])
                else:merged.append([a,b])
            result[song]=dict(row,unique_seconds=sum(b-a for a,b in merged),total_seconds=sum(b-a for a,b in intervals))
        return result

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog',type=Path,required=True);parser.add_argument('--source-root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    p=build_index(args.catalog,args.source_root,args.output);print(json.dumps(dict(index=str(p),sha256=digest(p),pcm_decoded=False)))
