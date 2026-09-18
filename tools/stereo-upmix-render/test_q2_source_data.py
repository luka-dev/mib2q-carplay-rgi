"""Synthetic original-stem fixtures; no music quality claims or TEST PCM."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
from scipy.signal import resample_poly
import soundfile as sf

import q2_source_data as q

def write_json(path,value):path.write_text(json.dumps(value))

def fixture(root,*,kit=False,other=False,bleed=False):
    corpus=root/'originals';corpus.mkdir();songs=[];native={}
    for i,partition in enumerate(('train','validation','heldout')):
        sid='song-'+str(i);folder=corpus/sid;folder.mkdir();tracks=[];stems=[];arrays=[]
        for j,(stem,kind) in enumerate([('vocals','lead_male_singer'),('drums','full_acoustic_drumkit' if kit else 'hi_hat'),
                                      ('other','fx/processed_sound,_scratches,_gun_shots,_explosions_etc') if other else ('guitar','clean_electric_guitar')]):
            n=20000-j*731;data=(np.random.default_rng(i*7+j).integers(-10000,10000,size=(n,2))/2**23).astype('f8')
            tid=sid+'-source-'+str(j);path=folder/(tid+'.wav');sf.write(path,data,44100,subtype='PCM_24');arrays.append(data)
            is_bleed=bleed and i==0 and j==0
            tracks.append(dict(id=tid,path=path.relative_to(corpus).as_posix(),stem=stem,track_type=kind,has_bleed=is_bleed,
                isolated_source_candidate=not is_bleed,header=dict(sample_rate=44100,channels=2,frames=n,sample_width=3),
                file_bytes=path.stat().st_size,sha256=q.digest(path)))
            stems.append(dict(stemName=stem,tracks=[dict(id=tid,trackType=kind,has_bleed=is_bleed)]))
        meta=folder/'data.json';write_json(meta,dict(stems=stems));native[sid]=arrays
        songs.append(dict(id=sid,artist='Artist'+str(i),artist_key='artist'+str(i),title='Synthetic',genre='synthetic',
            partition=partition,metadata_path=meta.relative_to(corpus).as_posix(),metadata_sha256=q.digest(meta),tracks=tracks,
            cross_partition_duplicate_quarantine=False))
    catalog=root/'catalog.json';write_json(catalog,dict(songs=songs,duplicate_groups=[],split_seed='test'))
    return corpus,catalog,native

def index(root,**kwargs):
    corpus,catalog,native=fixture(root,**kwargs)
    output=q.build_index(catalog,corpus,root/'index.json',expected_catalog_sha256=q.digest(catalog),min_free_bytes=0)
    return output,corpus,catalog,native

def reference(native):
    targets=np.zeros((20000,12,2),dtype='f8')
    for data,k in zip(native,(0,5,7)):targets[:len(data),k]+=data
    return targets,targets.sum(1)

class Q2DataTests(unittest.TestCase):
    def test_portable_bundle_hashes_shapes_and_bounded_expansion(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);output,_,_,_=index(root);row=q.SourceReader(output).read_crop('song-0',1000,512,target_rate=48000,context_frames=127)
            owner=root/'bundle';owner.mkdir();cases=[]
            for i in range(4):
                arrays={k:row[k] for k in ('mixture','targets','residual','reconstruction_known','detection_known')}
                path=owner/(str(i)+'.npz');np.savez_compressed(path,**arrays)
                cases.append(dict(id=str(i),song_id='synthetic-'+str(i),partition='TRAIN',frames=len(row['mixture']),
                    sample_rate=48000,core_start=127,core_stop=639,artifact=dict(path=path.name,bytes=path.stat().st_size,
                    sha256=q.digest(path),decoded_bytes=sum(v.nbytes for v in arrays.values()),
                    arrays={k:dict(shape=list(v.shape),dtype=v.dtype.str,sha256=q.array_digest(v)) for k,v in arrays.items()})))
            manifest=owner/'manifest.json';write_json(manifest,dict(schema='q2-source-fit-input-v1',complete=True,classes=list(q.CLASSES),cases=cases,car_approved=False))
            _,loaded=q.load_bundle(manifest,expected_sha256=q.digest(manifest));self.assertEqual(len(loaded),4)
            np.testing.assert_array_equal(loaded[0]['mixture'],row['mixture'])
            with self.assertRaisesRegex(ValueError,'cap'):q.load_bundle(manifest,max_bytes=1)
            with (owner/'0.npz').open('ab') as f:f.write(b'changed')
            with self.assertRaisesRegex(ValueError,'body'):q.load_bundle(manifest)

    def test_metadata_index_no_pcm_split_and_labels(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);corpus,catalog,_=fixture(root)
            with patch.object(sf,'SoundFile',side_effect=AssertionError('PCM opened in metadata index')):
                output=q.build_index(catalog,corpus,root/'index.json',expected_catalog_sha256=q.digest(catalog),min_free_bytes=0)
            loaded=q.load_index(output,expected_sha256=q.digest(output))
            self.assertEqual([r['partition'] for r in loaded['rows']],['TRAIN','DEV','TEST'])
            self.assertEqual(loaded['rows'][0]['frames'],20000);self.assertEqual(loaded['rows'][0]['common_frames'],18538)
            self.assertFalse(loaded['source_pcm_decoded']);self.assertEqual(tuple(loaded['classes']),q.CLASSES)
            reader=q.SourceReader(output)
            with patch.object(sf,'SoundFile',side_effect=AssertionError('TEST opened')),self.assertRaisesRegex(ValueError,'TEST'):
                reader.read_crop('song-2',0,10)

    def test_native_unit_gain_zero_extended_tails_context_and_closure(self):
        with tempfile.TemporaryDirectory() as t:
            output,_,_,native=index(Path(t));reader=q.SourceReader(output)
            row=reader.read_crop('song-0',19000,1000,target_rate=44100,context_frames=1024,partition='TRAIN')
            targets,mix=reference(native['song-0'])
            np.testing.assert_array_equal(row['mixture'],mix[17976:20000].astype('f4'))
            np.testing.assert_array_equal(row['targets'],targets[17976:20000].astype('f4'))
            self.assertEqual((row['core_start'],row['core_stop'],row['global_start_frame']),(1024,2024,17976))
            self.assertGreater(row['provenance']['source_proofs'][-1]['zero_extension_frames'],0)
            self.assertTrue(row['reconstruction_known'].all());self.assertFalse(row['residual'].any())
            self.assertFalse(row['car_approved']);self.assertEqual(reader.verify_sources()['verified_source_bodies'],3)

    def test_polyphase_whole_timeline_exact_at_arbitrary_global_origins(self):
        with tempfile.TemporaryDirectory() as t:
            output,_,_,native=index(Path(t));reader=q.SourceReader(output)
            targets,mix=reference(native['song-0']);joined=np.concatenate([mix[:,None],targets,np.zeros((20000,1,2))],1)
            expected=resample_poly(joined,160,147,axis=0,padtype='constant').astype('f4')
            for start,frames in [(0,500),(1,301),(159,512),(160,512),(997,2049),(len(expected)-700,700)]:
                row=reader.read_crop('song-0',start,frames,target_rate=48000)
                np.testing.assert_array_equal(row['mixture'],expected[start:start+frames,0])
                np.testing.assert_array_equal(row['targets'],expected[start:start+frames,1:-1])
            row=reader.read_crop('song-0',2000,800,target_rate=48000,context_frames=997)
            np.testing.assert_array_equal(row['mixture'],expected[1003:2800,0]);self.assertEqual(row['core_start'],997)

    def test_ambiguous_detection_not_reconstruction_and_no_noise_label(self):
        for options in (dict(kit=True),dict(other=True)):
            with self.subTest(options=options),tempfile.TemporaryDirectory() as t:
                output,_,_,_=index(Path(t),**options);reader=q.SourceReader(output);row=reader.read_crop('song-0',0,200,target_rate=44100)
                self.assertTrue(row['reconstruction_known'].all());self.assertFalse(row['detection_known'][5])
                self.assertEqual(row['role_status'],'UNKNOWN');self.assertNotIn('noise',q.CLASSES)
                if options.get('other'):
                    self.assertTrue(np.any(row['targets'][:,11]));self.assertFalse(row['detection_known'][:11].any())

    def test_body_hash_once_late_changes_rejected_and_exposure_unique(self):
        with tempfile.TemporaryDirectory() as t:
            output,corpus,_,_=index(Path(t));reader=q.SourceReader(output)
            original=q.digest;calls=[]
            def count(path):
                if str(path).endswith('.wav'):calls.append(str(path))
                return original(path)
            with patch.object(q,'digest',side_effect=count):
                reader.read_crop('song-0',0,1000,target_rate=44100);reader.read_crop('song-0',500,1000,target_rate=44100)
                reader.verify_sources()
            self.assertEqual(len(calls),3);exposure=reader.exposure_summary()['song-0']
            self.assertAlmostEqual(exposure['unique_seconds'],1500/44100);self.assertAlmostEqual(exposure['total_seconds'],2000/44100)
            p=next((corpus/'song-0').glob('*.wav'))
            with p.open('ab') as f:f.write(b'x')
            with self.assertRaisesRegex(ValueError,'changed'):reader.verify_sources()

    def test_dirty_song_and_split_metadata_path_hash_admission(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);output,_,_,_=index(root,bleed=True);reader=q.SourceReader(output)
            with self.assertRaisesRegex(ValueError,'not admitted'):reader.read_crop('song-0',0,100)
        for mutation in ('artist','group','labels','path','sha'):
            with self.subTest(mutation=mutation),tempfile.TemporaryDirectory() as t:
                root=Path(t);corpus,catalog,_=fixture(root);j=json.loads(catalog.read_text())
                if mutation=='artist':j['songs'][1]['artist_key']=j['songs'][0]['artist_key']
                if mutation=='group':j['duplicate_groups']=[[{'song':'song-0'},{'song':'song-1'}]]
                if mutation=='labels':j['songs'][0]['tracks'][0]['track_type']='background_vocals'
                if mutation=='path':j['songs'][0]['tracks'][0]['path']='../escape.wav'
                if mutation=='sha':j['songs'][0]['metadata_sha256']='0'*64
                write_json(catalog,j)
                with self.assertRaises(ValueError):q.build_index(catalog,corpus,root/'bad.json',expected_catalog_sha256=q.digest(catalog),min_free_bytes=0)
                self.assertFalse((root/'bad.json').exists())

    def test_index_tamper_bounds_and_source_pcm32_loss_refused(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);output,corpus,catalog,_=index(root);reader=q.SourceReader(output)
            for args in [(-1,20),(0,500000),(19999,2)]:
                with self.assertRaises(ValueError):reader.read_crop('song-0',*args,target_rate=44100)
            j=json.loads(output.read_text());j['rows'][0]['tracks'][0]['stem']='drums';write_json(output,j)
            with self.assertRaisesRegex(ValueError,'identity'):q.SourceReader(output)
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);corpus,catalog,_=fixture(root);j=json.loads(catalog.read_text());track=j['songs'][0]['tracks'][0];p=corpus/track['path']
            sf.write(p,np.full((20000,2),.25+2**-31),44100,subtype='PCM_32');track['sha256']=q.digest(p);track['file_bytes']=p.stat().st_size;track['header']['sample_width']=4
            write_json(catalog,j);out=q.build_index(catalog,corpus,root/'index.json',expected_catalog_sha256=q.digest(catalog),min_free_bytes=0)
            with self.assertRaisesRegex(ValueError,'float32 representable'):q.SourceReader(out).read_crop('song-0',0,100,target_rate=44100)

if __name__=='__main__':unittest.main()
