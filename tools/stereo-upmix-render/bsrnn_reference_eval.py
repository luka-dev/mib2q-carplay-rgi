#!/usr/bin/env python3
"""Frozen pretrained SIMO evaluation: actual-mask attribution, entries and real C.

No training, DEV selection, gain search, upstream edits or Track A edits.
"""
import argparse
import csv
import gc
import faulthandler
import tempfile
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time

import numpy as np
import torch
import torchaudio

import bsrnn_reference_infer as ref
import diag_transitions as DT
import evaluate_oracle_renderer_probe as gate
import mask_net as MN
import oracle_renderer_sweep as sweep
import sep_net as SN
import upmix_behavior_tests as T
import upmix_stem_tests as H

SR = 48000
ROLES = ('eligible', 'vocal', 'drums', 'bass')


def clean(value):
    if isinstance(value, dict):
        return {k: clean(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [clean(v) for v in value]
    if isinstance(value, (np.floating, float)):
        return float(value) if np.isfinite(value) else None
    if isinstance(value, np.integer):
        return int(value)
    return value


def write(path, value):
    ref.write_json(path, clean(value))


def fraction(a, b):
    den = float(b.abs().double().square().sum())
    return float(a.abs().double().square().sum()) / den if den > 1e-9 else None


def med(values):
    vals = [v for v in values if v is not None and np.isfinite(v)]
    return float(np.median(vals)) if vals else None


class FrozenMask:
    def __init__(self, model):
        self.model = model
        self.parts = [None] * len(model.band_width)
        self.hooks = []
        head = model.maskers[list(model.targets).index('other')]
        if head.mask_ctxt or model.subtract_last_trgt:
            raise RuntimeError('Unsupported mask context/residual output')
        for i, module in enumerate(head.mask):
            def capture(module, inputs, out, i=i):
                shaped = out.view(-1, 2, 2, model.band_width[i], out.shape[-1])
                mask = shaped[:, 0] * torch.sigmoid(shaped[:, 1])
                self.parts[i] = torch.complex(mask[:, 0], mask[:, 1]).detach()
            self.hooks.append(module.register_forward_hook(capture))

    @torch.inference_mode()
    def apply(self, mixture, sources):
        # Inputs are [2,N] and [roles,2,N], at stable original gain and 48 kHz.
        if not torch.allclose(sources.sum(0), mixture, atol=2e-7, rtol=2e-6):
            raise RuntimeError('True source closure differs from mixture')
        n48 = mixture.shape[-1]
        mix44 = torchaudio.functional.resample(mixture, SR, 44100)
        src44 = torchaudio.functional.resample(sources, SR, 44100)
        peak = float(mix44.abs().max())
        if peak <= 0:
            raise RuntimeError('Silent mixture')
        n = mix44.shape[-1]
        chunk, hop, overlap = 11 * 44100, 10 * 44100, 44100
        estimate = torch.zeros((2, n))
        attributed = torch.zeros((len(sources), 2, n))
        weights = torch.zeros(n)
        parity = []
        for start in range(0, n, hop):
            end = min(start + chunk, n)
            m = (mix44[:, start:end] / peak).unsqueeze(0).to('cuda')
            true = (src44[:, :, start:end] / peak).to('cuda')
            result = self.model(m)
            mask = torch.cat(self.parts, dim=1)
            other_index = list(self.model.targets).index('other')
            X = self.model.stft(m[0])
            expected = result['stfts'][0, other_index]
            spec_error = fraction(mask * X - expected, expected)
            applied = self.model.istft(mask[None] * self.model.stft(true), length=end-start)
            prediction = result['waveforms'][0, other_index]
            closure = fraction(applied.sum(0) - prediction, prediction)
            if spec_error is None or spec_error > 1e-10 or closure is None or closure > 1e-9:
                raise RuntimeError(f'Mask/source reconstruction failed: {spec_error}, {closure}')
            window = torch.ones(end-start)
            if start:
                window[:overlap] = torch.linspace(0, 1, overlap)
            if end < n:
                window[-overlap:] = torch.linspace(1, 0, overlap)
            estimate[:, start:end] += prediction.cpu() * peak * window
            attributed[:, :, start:end] += applied.cpu() * peak * window
            weights[start:end] += window
            parity.append(dict(start44=start, samples44=end-start,
                               mask_nse=spec_error, source_sum_nse=closure))
            del result, mask, X, expected, applied, prediction, m, true
            if end == n:
                break
        if not torch.allclose(weights, torch.ones_like(weights), atol=2e-6, rtol=0):
            raise RuntimeError('Overlap coverage is not unity')
        estimate /= weights
        attributed /= weights
        estimate = torchaudio.functional.resample(estimate, 44100, SR)
        attributed = torchaudio.functional.resample(attributed, 44100, SR)
        if estimate.shape[-1] != n48 or not torch.isfinite(attributed).all():
            raise RuntimeError('Output shape/finiteness failed')
        closure = fraction(attributed.sum(0) - estimate, estimate)
        if closure is None or closure > 1e-9:
            raise RuntimeError('Final source closure failed')
        return estimate, attributed, dict(input44_peak=peak, final_source_sum_nse=closure,
                                         chunks=parity)


def roles_from_stems(stems):
    empty = torch.zeros_like(torch.from_numpy(next(iter(stems.values()))))
    def source(names):
        return sum((torch.from_numpy(x) for k, x in stems.items() if k in names), empty.clone())
    return torch.stack([source(set(stems)-{'vocals','bass','drums','percussion'}),
                        source({'vocals'}), source({'drums','percussion'}), source({'bass'})])


def render_rows(out, track_id, meta, stems, mixture, estimate, executable, args):
    out.mkdir(exist_ok=True)
    pcm, inj, dst = (out / (track_id + suffix) for suffix in ('.wav', '.inj.wav', '.6ch.wav'))
    for label, audio in [('mixture', mixture), ('injection', estimate)]:
        if not torch.isfinite(audio).all() or float(audio.abs().max()) > 1:
            raise RuntimeError(f'{label} would clip in C PCM input')
    T.write_wav(str(pcm), mixture.numpy())
    T.write_wav(str(inj), estimate.numpy())
    if dst.exists():
        dst.unlink()  # Own partial C output; avoid WinBtrfs identical-inode guard.
    subprocess.run([str(executable), *args, '--rear-audio', str(inj), str(pcm), str(dst)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    out6 = T.read_wav6(str(dst))
    if out6.shape != (6, mixture.shape[-1]):
        raise RuntimeError('C output duration differs')
    rows = rows_from_output(meta, stems, out6)
    write(out / (track_id+'.rows.json'), rows)
    return rows, dict(mixture_sha256=ref.sha(pcm), injection_sha256=ref.sha(inj),
                      output_sha256=ref.sha(dst), input_peak=float(mixture.abs().max()),
                      injection_peak=float(estimate.abs().max()))


def rows_from_output(meta, stems, out6):
    """Use the original harness's exact eligibility expression, including gain."""
    tag = re.sub(r'[\\/:*?"<>|]', '_', f"{meta.get('artist','?')} - {meta.get('song','?')}")[:60]
    k = 0.7 / (np.abs(sum(stems.values())).max() + 1e-9)
    dry_mix = sum(stems.values())
    rear_rms = float(20*np.log10(np.sqrt(((out6[4]**2+out6[5]**2)/2).mean())+1e-9))
    rows = []
    for name, stem in stems.items():
        lead, rest = stem*k, (dry_mix-stem)*k
        if (lead**2).sum() < 1e-3 * (dry_mix**2).sum():
            continue
        share, w, dt = T.rear_share(lead, rest, out6)
        metrics = T.metrics(share, w, dt, np.ones(dry_mix.shape[-1], bool))
        slope, corr, level = H.level_dependence(lead, rest, share, dt)
        rows.append(dict(song=tag, genre=meta.get('genre','?'), stem=name,
                         median_rear=metrics['median_rear'], steady_std=metrics['steady_std'],
                         range_1s=metrics['range_1s'], worst_half_s=metrics['worst_half_s'],
                         slope_per_6dB=slope, level_corr=corr, level_db=level,
                         frames=int(np.sum(~np.isnan(share))), rear_rms_db=rear_rms,
                         **dict(zip(H.BAND_KEYS, H.band_shares(lead, rest, out6)))))
    return rows


def extraction_row(song, estimate, attributed, refs):
    row = dict(song=song, transmission={role:fraction(attributed[i], refs[i])
                                        for i, role in enumerate(ROLES)},
               **ref.acoustic_sanity(estimate, refs[0]), bands={})
    # Same 48 kHz STFT analysis convention, after synthesis/resampling.
    with torch.inference_mode():
        for label, lo, hi in [('8-14k',342,598), ('>14k',598,1025)]:
            A = SN.stft(torch, attributed[0][None].to('cuda'))[:, :, lo:hi]
            B = SN.stft(torch, refs[0][None].to('cuda'))[:, :, lo:hi]
            row['bands'][label] = fraction(A, B)
    return row


def transitions(frozen, cache, track_ids, out, resume=False):
    progress_path = out/'transitions_progress.json'
    rows = json.loads(progress_path.read_text())['rows'] if resume and progress_path.exists() else []
    event_key = lambda r: (r['song'], r['kind'], int(round(r['t']*SR)))
    cached = {event_key(r):r for r in rows}
    if len(cached) != len(rows):
        raise RuntimeError('Duplicate cached transition')
    for r in rows:
        closure = r.get('closure_nse')
        if r['song'] not in track_ids or closure is None or not 0 <= closure <= 1e-9:
            raise RuntimeError('Cached transition attribution invalid')
        if r['duck'] is not None and abs(r['duck']*r['ret_pre']-r['ret_post']) > 1e-8:
            raise RuntimeError('Cached transition retention differs')
    seen = set()
    for index, song in enumerate(track_ids, 1):
        path = cache / (song+'.npy')
        n = MN.npy_header(str(path))[0][2]
        vocal = MN.read_window(str(path), 0, n, slots=[0,1]).astype(np.float32).sum(0)/32768
        events = [(kind,s) for kind,s in DT.events(vocal)
                  if s-DT.PRE-DT.WARM >= 0 and s+DT.POST <= n]
        del vocal
        for kind, onset in events:
            key = (song, kind, int(onset))
            if key in seen:
                raise RuntimeError('Duplicate detected transition')
            seen.add(key)
            if key in cached:
                continue
            start, end = onset-DT.PRE-DT.WARM, onset+DT.POST
            w = torch.from_numpy(ref.read_crop(path, start, end-start).astype(np.float32)/32768)
            sources = torch.stack([w[6],w[0]+w[1],w[2:5].sum(0),w[5]])
            _, sent, parity = frozen.apply(sources.sum(0), sources)
            t_axis = np.arange(24)*0.5-4
            with torch.inference_mode():
                cutf = SN.stft(torch, sources[0][None].to('cuda')).shape[1] - SN.stft(torch, sources[0, :, DT.WARM:][None].to('cuda')).shape[1]
                re = lambda a: SN.stft(torch, a[None].to('cuda'))[:, cutf:]
                ret = DT.per_frame(torch, re(sent[0]), re(sources[0]), 24, DT.FLOOR_DB)
                leak = DT.per_frame(torch, re(sent[1]), re(sources[1]), 24, DT.FLOOR_DB)
            r = dict(song=song, kind=kind, t=onset/SR,
                     ret_pre=float(np.nanmean(ret[t_axis<0])),
                     ret_post=float(np.nanmean(ret[t_axis>=0])),
                     leak_pre=float(np.nanmean(leak[t_axis<0])),
                     leak_0_2=float(np.nanmean(leak[(t_axis>=0)&(t_axis<2)])),
                     peak_0_2=float(np.nanmax(leak[(t_axis>=0)&(t_axis<2)])),
                     leak_steady=float(np.nanmean(leak[t_axis>=4])),
                     closure_nse=parity['final_source_sum_nse'])
            r['duck'] = r['ret_post']/r['ret_pre'] if r['ret_pre']>0 else None
            if kind == 'entry':
                r['settle'] = float(DT.settle(leak,t_axis,r['leak_steady']))
            rows.append(clean(r))
            write(out / 'transitions_progress.json',dict(rows=rows, songs_complete=index-1))
        print('TRANSITIONS',index,len(track_ids),song,'events',len(events),flush=True)
        write(progress_path,dict(rows=rows, songs_complete=index))
    if {event_key(r) for r in rows} != seen:
        raise RuntimeError('Cached/detected transition event sets differ')
    summaries = {}
    for kind in ['entry','exit','sustained']:
        selected = [r for r in rows if r['kind']==kind]
        data = dict(count=len(selected), song_count=len({r['song'] for r in selected}))
        for key in ['ret_pre','ret_post','duck','leak_pre','leak_0_2','peak_0_2','leak_steady']:
            vals = [r[key] for r in selected if r[key] is not None]
            data[key] = dict(median=med(vals), p10=float(np.percentile(vals,10)) if vals else None,
                             p90=float(np.percentile(vals,90)) if vals else None)
        summaries[kind] = data
    entry = summaries['entry']['duck']
    result = dict(scope='40 DEV extractor transitions; noncausal frozen-mask source attribution',
                  summaries=summaries, rows=rows,
                  gates=dict(entry_median=entry['median'] is not None and entry['median']>=.85,
                             entry_p10=entry['p10'] is not None and entry['p10']>=.60))
    write(out / 'transitions.json',result)
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True)
    p.add_argument('--runs',type=Path,required=True)
    p.add_argument('--tools',type=Path,required=True)
    p.add_argument('--cache',type=Path,required=True)
    p.add_argument('--moisesdb',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--smoke',action='store_true',help='TRAIN-only closure/attribution execution gate')
    p.add_argument('--resume',action='store_true',help='Reuse verified complete C pairs only')
    a=p.parse_args()
    trace_path=Path(tempfile.gettempdir())/('upmix_bsrnn_trace_'+str(int(time.time()))+'.txt')
    trace_file=trace_path.open('w')
    faulthandler.enable(file=trace_file)
    faulthandler.dump_traceback_later(90,repeat=True,file=trace_file)
    print('TRACE_FILE',str(trace_path),flush=True)
    code=a.root/'code'
    if subprocess.check_output(['git','-C',str(code),'rev-parse','HEAD'],text=True).strip()!=ref.REV:
        raise RuntimeError('Upstream revision changed')
    if subprocess.check_output(['git','-C',str(code),'status','--porcelain'],text=True).strip():
        raise RuntimeError('Upstream checkout dirty')
    sys.path.insert(0,str(code))
    torch.set_num_threads(4)
    torch.backends.cuda.matmul.allow_tf32=False
    torch.backends.cudnn.allow_tf32=False
    manifest_path=a.runs/'direct_object_full40/manifest.json'
    manifest=json.loads(manifest_path.read_text())
    ids=list(manifest['song_track_ids'].values())
    split=MN.load_split(str(a.cache))
    if len(ids)!=40 or len(set(ids))!=40 or not set(ids)<=set(split['dev']):
        raise RuntimeError('Manifest differs from declared DEV40')
    build=json.loads((a.tools/'direct_object_host/manifest.json').read_text())
    binaries={name:a.tools/'direct_object_host'/('upmix_'+name+'.exe')
              for name in ['control','direct_no_bed']}
    for name,path in binaries.items():
        if ref.sha(path)!=build['builds'][name]['sha256']:
            raise RuntimeError('Frozen C binary changed')
    intake=json.loads((a.root/'INTAKE_COMPLETE.json').read_text())
    bundle=next(b for b in intake['bundles'] if b['name']=='simo-bsrnn-opt')
    ck=bundle['extracted']['checkpoints'][0]
    model,audit=ref.load_strict(a.root/'weights'/bundle['name']/ck['path'],ck['hashes'],ref.TARGETS,code)
    frozen=FrozenMask(model)
    a.out.mkdir(parents=True,exist_ok=a.resume)
    if a.smoke:
        if fraction(torch.tensor([2j]),torch.tensor([1j])) != 4.0:
            raise RuntimeError('Complex energy discarded imaginary part')
        train_manifest=json.loads((a.runs/'separator_fit_probe_v1/manifest.json').read_text())
        w=train_manifest['windows'][0]
        raw=ref.read_crop(a.cache/(w['song']+'.npy'),w['start_sample'],w['samples'])
        if hashlib.sha256(raw.tobytes()).hexdigest()!=w['selected_int16_sha256']:
            raise RuntimeError('TRAIN smoke crop changed')
        x=torch.from_numpy(raw.astype(np.float32)/32768)
        sources=torch.stack([x[6],x[0]+x[1],x[2:5].sum(0),x[5]])
        estimate,sent,parity=frozen.apply(sources.sum(0),sources)
        with torch.inference_mode():
            num=SN.stft(torch,sent[0][None].to('cuda'))
            den=SN.stft(torch,sources[0][None].to('cuda'))
            frame=DT.per_frame(torch,num,den,16,DT.FLOOR_DB)
        write(a.out/'complete.json',dict(scope='TRAIN smoke only',complete=True,
              parity=parity,finite_retention_bins=int(np.isfinite(frame).sum()),
              reconstruction=ref.acoustic_sanity(estimate,sources[0]),checkpoint=audit))
        print('TRAIN_SMOKE_COMPLETE',flush=True)
        return
    if a.resume:
        if json.loads((a.out/'manifest.json').read_text())!=manifest:
            raise RuntimeError('Resume song manifest changed')
    else:
        write(a.out/'manifest.json',manifest)
    protocol=dict(source_revision=ref.REV,script_sha256=ref.sha(__file__),
                  intake_script_sha256=ref.sha(ref.__file__),checkpoint=audit,
                  manifest_sha256=ref.sha(manifest_path),binary_hashes={k:ref.sha(v) for k,v in binaries.items()},
                  harness_hashes={m.__name__:ref.sha(m.__file__) for m in [H,T,DT,SN,MN]},
                  injection_gain=1.0,chunk_seconds=11,hop_seconds=10,
                  scope='previously inspected DEV40; offline noncausal SIMO, no training')
    protocol['trace_file']=str(trace_path)
    if a.resume:
        old=json.loads((a.out/'protocol.json').read_text())
        if old['script_sha256'] != '468bc58cbce8c16f80bfcc9fe51c1a8a2b8cf593f16bf1f6c90e5d7fc440ec68':
            raise RuntimeError('Resume requires the corrected frozen audit')
        for key in ['manifest_sha256','binary_hashes','harness_hashes','injection_gain','chunk_seconds','hop_seconds']:
            if old[key] != protocol[key]:
                raise RuntimeError('Resume changed protocol: '+key)
        if old['checkpoint']['sha256'] != audit['sha256']:
            raise RuntimeError('Resume checkpoint changed')
        write(a.out/'resume_protocol.json',protocol)
    else:
        write(a.out/'protocol.json',protocol)
    common=T.VARIANTS['shipped']+['--model',str(a.tools/'hssr_model.bin'),
                                 '--guard-model',str(a.tools/'hssr_guard.bin'),'--mask-drive','1.0']
    rowsets={'baseline':[], 'direct_no_bed_body0_cal':[]}
    extractions=[]
    input_hashes=[]
    saved_extraction={r['track_id']:r for r in json.loads((a.out/'extraction_progress.json').read_text())['rows']} if a.resume else {}
    saved_inputs={r['track_id']:r for r in json.loads((a.out/'inputs_progress.json').read_text())} if a.resume else {}
    for index,(song,track_id) in enumerate(manifest['song_track_ids'].items(),1):
        started=time.perf_counter()
        if a.resume and track_id in saved_extraction and track_id in saved_inputs:
            folders=[a.out/point for point in rowsets]
            paths=[folder/(track_id+'.rows.json') for folder in folders]
            if all(path.exists() for path in paths):
                cached=saved_inputs[track_id]
                for point,path in zip(rowsets,paths):
                    for suffix,key in [('.wav','mixture_sha256'),('.inj.wav','injection_sha256'),('.6ch.wav','output_sha256')]:
                        if ref.sha(path.parent/(track_id+suffix)) != cached[point][key]:
                            raise RuntimeError('Resume C file hash changed')
                    rows=json.loads(path.read_text())
                    if not rows or any(r['song']!=song for r in rows):
                        raise RuntimeError('Resume rows changed')
                    for r in rows:
                        for key,value in r.items():
                            if value is None and key not in ('song','genre','stem'):
                                r[key]=float('nan')
                    rowsets[point].extend(rows)
                if saved_extraction[track_id]['parity']['final_source_sum_nse']>1e-9:
                    raise RuntimeError('Resume source closure invalid')
                extractions.append(saved_extraction[track_id])
                input_hashes.append(cached)
                print('REUSED_C_PAIR',index,40,song,flush=True)
                continue
        print('LOAD_SONG',index,40,song,flush=True)
        meta,stems=H.load_song(str(a.moisesdb/track_id),60)
        print('LOADED_SONG',index,flush=True)
        n=min(v.shape[-1] for v in stems.values())
        stems={k:v[:,:n] for k,v in stems.items()}
        mixture=torch.from_numpy(sum(stems.values()))
        k=0.7/(float(mixture.abs().max())+1e-9)
        refs=roles_from_stems(stems)*k
        mixture=mixture*k
        estimate,sent,parity=frozen.apply(mixture,refs)
        print('INFERRED_SONG',index,flush=True)
        row=extraction_row(song,estimate,sent,refs)
        row.update(track_id=track_id, samples=n, parity=parity)
        extractions.append(row)
        write(a.out/'extraction_progress.json',dict(rows=extractions))
        hashes={}
        for point,binary,extra in [('baseline','control',[]),
                                   ('direct_no_bed_body0_cal','direct_no_bed',['--rear-body','0','--surround','0.3235'])]:
            print('C_RENDER',index,point,flush=True)
            rows,h=render_rows(a.out/point,track_id,meta,stems,mixture,estimate,binaries[binary],common+extra)
            if not rows or any(r['song']!=song for r in rows):
                raise RuntimeError('C song label/rows differ from manifest')
            rowsets[point].extend(rows)
            hashes[point]=h
        if len({v['injection_sha256'] for v in hashes.values()})!=1 or len({v['mixture_sha256'] for v in hashes.values()})!=1:
            raise RuntimeError('Paired C input differs')
        input_hashes.append(dict(track_id=track_id, **hashes))
        write(a.out/'inputs_progress.json',input_hashes)
        print('C_DONE',index,40,song,round(time.perf_counter()-started,2),'seconds',flush=True)
        del stems, refs, sent, mixture, estimate
        gc.collect()
    summaries={}
    for point,rows in rowsets.items():
        if len(rows)!=231:
            raise RuntimeError(f'Expected original231 C stem rows; {point} got {len(rows)}')
        with (a.out/point/'stems.csv').open('w',newline='') as f:
            writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
        summaries[point]=gate.summary(rows)
    extraction=dict(rows=extractions,summary=dict(transmission={role:med([r['transmission'][role] for r in extractions]) for role in ROLES},
                   other_si_sdr_db=med([r['other_si_sdr_channel_mean_db'] for r in extractions]),
                   other_nsdr_db=med([r['other_nsdr_db'] for r in extractions]),
                   bands={band:med([r['bands'][band] for r in extractions]) for band in ['8-14k','>14k']}))
    write(a.out/'extraction.json',extraction)
    entries=transitions(frozen,a.cache,ids,a.out,resume=a.resume)
    oracle= json.loads((a.runs/'direct_object_full40/decision.json').read_text())
    source={**summaries, 'true_audio_reference': oracle}
    decisions={}
    for point,s in summaries.items():
        gates=gate.passes(s,summaries['baseline'],'representation40')
        # The .5 dB comparison is to the same extractor on original C, not a fit.
        gates.update(entries['gates'])
        decisions[point]=dict(summary=s,coverage={stem:sum(r['stem']==stem for r in rowsets[point]) for stem in gate.SHARE_LIMITS},
                             gates=gates,observed_joint_gates=all(gates.values()),
                             keys_optional_012=s['shares']['other_keys'] is not None and s['shares']['other_keys']>=.12,
                             true_audio_rear_delta_db=s['rear_rms_db']-(oracle['baseline'] if point=='baseline' else oracle['variants'][point]['summary'])['rear_rms_db'])
    final=dict(scope=protocol['scope'],complete=True,songs=40,stem_rows_each=231,
               extraction=extraction['summary'],transitions=entries['summaries'],variants=decisions,
               maximum_source_closure_nse=max(r['parity']['final_source_sum_nse'] for r in extractions),
               device_causal_and_listening_gates='unmeasured; reference cannot be deployed',
               protocol_sha256=ref.sha(a.out/'protocol.json'),
               resume_protocol_sha256=ref.sha(a.out/'resume_protocol.json') if a.resume else None,
               coverage_repair_sha256=ref.sha(a.out/'coverage_repair.json') if (a.out/'coverage_repair.json').exists() else None)
    write(a.out/'complete.json',final)
    faulthandler.cancel_dump_traceback_later()
    print('COMPLETE',json.dumps(clean(final)),flush=True)


if __name__=='__main__':
    main()
