#!/usr/bin/env python3
"""Matched true-stem generalization after the completed causal fitting pair.

No teacher, remix, new corpus, DEV selection, architecture edit or gain search.
Both 10k fitting checkpoints receive fresh optimizers and 40k broad TRAIN steps.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import math
from pathlib import Path
import time

import numpy as np
import torch
import mask_net as MN
import sep_net as SN
import separator_causal_reference as CR
import separator_causal_reference_probe as P
from separator_fit_probe import objective, sha, write_json

MODES = P.MODES
STEPS = 40000
SNAPSHOTS = (0, 1000, 4000, 10000, 20000, 30000, 40000)
SEED, BATCH, SECONDS = 1, 4, 8
INITIAL = {
    'original': '7d3d1fde1f8bd2c64bd539292a7cb0eac4bce19b8377b06f7c22a32ed4b4e470',
    'residual96x4': '682c09f1d253fb694bfde6fa43e1bfd1168f9161398a3abd969b0ad7fec1603a',
}


def clean(value):
    if isinstance(value, dict):
        return {k: clean(v) for k, v in value.items()}
    if isinstance(value, (tuple, list)):
        return [clean(v) for v in value]
    if isinstance(value, (float, np.floating)):
        return float(value) if np.isfinite(value) else None
    if isinstance(value, np.integer):
        return int(value)
    return value


def write(path, value):
    write_json(path, clean(value))


def median(values):
    v = [x for x in values if x is not None and np.isfinite(x)]
    return float(np.median(v)) if v else None


def configure(device):
    torch.set_num_threads(4)
    torch.manual_seed(SEED)
    torch.backends.cuda.matmul.allow_tf32 = False
    torch.backends.cudnn.allow_tf32 = False
    if device == 'cuda' and not torch.cuda.is_available():
        raise RuntimeError('CUDA required for GPU experiment')


class Reader:
    """Plain persistent reads: one CPU producer, no mmap/rapid cache mutation."""
    def __init__(self, cache, inventory):
        self.cache, self.inventory, self.handles = cache, inventory, {}

    def read(self, song, start, samples):
        row = self.inventory[song]
        n = row['samples']
        if start < 0 or start + samples > n:
            raise ValueError('Crop outside cache')
        if song not in self.handles:
            self.handles[song] = (self.cache / (song + '.npy')).open('rb')
        f = self.handles[song]
        out = np.empty((7, 2, samples), np.int16)
        for g in range(7):
            for c in range(2):
                f.seek(row['offset'] + ((g*2+c)*n+start)*2)
                data = f.read(samples*2)
                if len(data) != samples*2:
                    raise ValueError('Short cache read')
                out[g, c] = np.frombuffer(data, '<i2')
        return out

    def batch(self, step):
        # Step-local RNG makes donor/offset/gain choices identical across arms,
        # including after resume; optimization/evaluation cannot consume it.
        rng = np.random.default_rng([SEED, step])
        ids = sorted(self.inventory)
        records, roles = [], []
        for _ in range(BATCH):
            song = ids[int(rng.integers(len(ids)))]
            n = SECONDS*SN.SR
            start = int(rng.integers(self.inventory[song]['samples']-n+1))
            gains = np.power(10., rng.uniform(-6., 6., size=7)/20).astype(np.float32)
            w = self.read(song, start, n).astype(np.float32)/32768
            w *= gains[:, None, None]
            roles.append(np.stack([w[6], w[0]+w[1], w[2:5].sum(0), w[5]]))
            records.append(dict(song=song, start=start, gains=gains.tolist()))
        arrays = np.stack(roles)
        energies = np.square(arrays.astype(np.float64)).mean((2, 3))
        return arrays, records, energies

    def close(self):
        for f in self.handles.values():
            f.close()
        self.handles.clear()


def eval_row(net, raw, record, device):
    roles = torch.from_numpy(np.stack([raw[6], raw[0]+raw[1], raw[2:5].sum(0), raw[5]])).to(device)
    n = roles.shape[-1]
    cut = record['warm_samples']
    net.modules.eval()
    with torch.no_grad():
        spectra = SN.stft(torch, roles)
        estimate = net.separate_stft(spectra.sum(0)[None])
        sent = SN.istft(torch, net.last_mask * spectra, n)[:, :, cut:]
        waveform = SN.istft(torch, estimate, n)[0, :, cut:]
        refs = roles[:, :, cut:]
        closure = float((sent.sum(0)-waveform).square().sum()/waveform.square().sum().clamp_min(1e-9))
        if closure > 1e-9:
            raise RuntimeError('Attributed source closure failed')
        ref_power = refs.square().sum((1, 2))
        tx = [float(num/den) if float(den)>1e-9 else None
              for num, den in zip(sent.square().sum((1, 2)), ref_power)]
        y, e = SN.stft(torch, waveform[None]), SN.stft(torch, refs[0][None])
        sdr = float(SN.si_sdr(torch, waveform, refs[0]).mean()) if float(ref_power[0])>1e-9 else None
        bands = {}
        attributed_e = SN.stft(torch, sent[0][None])
        for name, lo, hi in [('0-300',0,13), ('300-8k',13,342), ('8-14k',342,598), ('14-24k',598,1025)]:
            den = e[:, :, lo:hi].abs().square().sum()
            bands[name] = dict(error_nse=float((y[:, :, lo:hi]-e[:, :, lo:hi]).abs().square().sum()/den) if float(den)>1e-9 else None,
                               eligible_tx=float(attributed_e[:, :, lo:hi].abs().square().sum()/den) if float(den)>1e-9 else None)
        row = dict(song=record['song'],loss=float(objective(torch,y,e,refs[0][None],refs.shape[-1])),
                   sdr_db=sdr,nse=float((waveform-refs[0]).square().sum()/ref_power[0]) if float(ref_power[0])>1e-9 else None,
                   source_tx=dict(zip(('eligible','vocal','drums','bass'),tx)),bands=bands,
                   closure_nse=closure)
    net.modules.train()
    if not np.isfinite(row['loss']):
        raise RuntimeError('Nonfinite evaluation')
    return row


def evaluate(net, cache, records, device):
    rows = []
    for r in records:
        raw = MN.read_window(str(cache/(r['song']+'.npy')),r['start_sample'],r['samples'])
        if hashlib.sha256(raw.tobytes()).hexdigest()!=r['crop_sha256']:
            raise RuntimeError('Evaluation crop changed')
        rows.append(eval_row(net, raw.astype(np.float32)/32768,r,device))
    return dict(rows=rows,summary=dict(loss=median([r['loss'] for r in rows]),
                sdr_db=median([r['sdr_db'] for r in rows]),nse=median([r['nse'] for r in rows]),
                source_tx={s:median([r['source_tx'][s] for r in rows]) for s in rows[0]['source_tx']},
                bands={b:{k:median([r['bands'][b][k] for r in rows]) for k in rows[0]['bands'][b]} for b in rows[0]['bands']}))


def source_hashes():
    modules = [SN, MN, CR, P]
    return {Path(m.__file__).name:sha(Path(m.__file__)) for m in modules} | {
        Path(__file__).name:sha(Path(__file__)),
        'separator_fit_probe.py':sha(Path(__file__).with_name('separator_fit_probe.py'))}


def prepare(a):
    if a.out.exists():
        raise FileExistsError('New experiment root required')
    split = MN.load_split(str(a.cache))
    if tuple(len(split[k]) for k in ('train','dev','test'))!=(176,40,24):
        raise ValueError('Frozen existing split coverage differs')
    if any(set(split[x])&set(split[y]) for x,y in [('train','dev'),('train','test'),('dev','test')]):
        raise ValueError('Song-ID split overlap')
    artists, song_keys = {}, {}
    inventory = {}
    for part in ('train','dev'):
        artists[part], song_keys[part] = set(),set()
        for song in split[part]:
            p = a.cache/(song+'.npy')
            shape, dtype, offset = MN.npy_header(str(p))
            if shape[:2]!=(7,2) or dtype!=np.dtype('int16') or shape[2] < SECONDS*SN.SR:
                raise ValueError('Invalid real-stem cache')
            meta = json.loads(p.with_suffix('.json').read_text(encoding='utf-8'))
            artist = str(meta.get('artist','')).strip().casefold()
            title = str(meta.get('song','')).strip().casefold()
            if not artist or not title:
                raise ValueError('Artist/title metadata required')
            artists[part].add(artist); song_keys[part].add((artist,title))
            if part=='train':
                inventory[song]=dict(samples=shape[2],offset=offset,bytes=p.stat().st_size,
                                     metadata_sha256=sha(p.with_suffix('.json')))
    if artists['train']&artists['dev'] or song_keys['train']&song_keys['dev']:
        raise ValueError('Artist/song split overlap')
    fit_manifest=json.loads((a.initial/'manifest.json').read_text())
    excluded={r['song'] for r in fit_manifest['windows']}
    records={}
    for part in ('train','dev'):
        ids = sorted(set(split[part])-excluded,key=lambda s:hashlib.sha256(('broad-eval-v1:'+s).encode()).hexdigest()) if part=='train' else split['dev']
        if part=='train': ids=ids[:12]
        records[part]=[]
        for song in ids:
            p=a.cache/(song+'.npy'); length=MN.npy_header(str(p))[0][2]
            n=min(20*SN.SR,length)
            start=int(MN.dev_excerpt(str(p),n)) if length>n else 0
            warm=min(start,10*SN.SR)
            raw=MN.read_window(str(p),start-warm,n+warm)
            records[part].append(dict(song=song,start_sample=start-warm,samples=n+warm,
                 warm_samples=warm,score_samples=n,crop_sha256=hashlib.sha256(raw.tobytes()).hexdigest()))
    initial={}
    configure('cpu')
    heads=[]; stats=[]
    for mode in MODES:
        path=a.initial/mode/'fit.pt'
        if sha(path)!=INITIAL[mode]: raise ValueError('Initial fitted checkpoint changed')
        net,ck=CR.load(torch,path)
        if ck['mode']!=mode or ck['step']!=10000: raise ValueError('Wrong fitting initialization')
        heads.append(P.tensors_sha(net.dec)); stats.append(ck['stats'])
        initial[mode]=dict(sha256=sha(path),parameters=sum(p.numel() for p in net.parameters()),
                          estimated_neural_macs_s=net.macs*SN.FPS,decoder_sha256=heads[-1])
    if len(set(heads))!=1 or stats[0]!=stats[1]: raise ValueError('Initial decoder/stats parity differs')
    manifest=dict(scope='causal package generalization, true Moises TRAIN only',sources=source_hashes(),
             split_sha256=sha(a.cache/'split.json'),train_inventory=inventory,evaluation=records,
             initial=initial,stats=stats[0],steps=STEPS,batch=BATCH,seconds=SECONDS,seed=SEED,
             snapshots=list(SNAPSHOTS),optimizer='fresh AdamW: weight_decay1e-4, clip5, float32 TF32 off',
             schedule='2k linear warmup to3e-4, cosine decay to3e-5 at40k; no loss switch',
             objective='unchanged separator_fit_probe.objective; all parameters trainable',
             data='uniform all176 TRAIN songs, uniform valid8s offset, independent seven-slot ±6dB gains; no remix/pseudo/new corpus',
             split_audit=dict(train=176,dev=40,test_audio_unread=24,artist_overlap=0,artist_title_overlap=0),
             fitting_songs_excluded_from_train_monitor=sorted(excluded),
             selection='fixed terminal40k; no DEV best-checkpoint selection, no 4k acoustic stopping',
             scope_limits='Four-crop fitted warm starts in both arms; PC reference above device budget; no independent Test, port or car claim')
    a.out.mkdir(parents=True)
    write(a.out/'manifest.json',manifest)
    print('PREPARED train176/dev40/train_monitor12; Test audio untouched',flush=True)


def check_manifest(a):
    m=json.loads((a.out/'manifest.json').read_text())
    if m['sources']!=source_hashes() or m['split_sha256']!=sha(a.cache/'split.json'):
        raise ValueError('Frozen sources/split changed')
    return m


def save(path, value):
    temporary=path.with_suffix('.pt.new')
    torch.save(value,temporary);temporary.replace(path)


def lr_at(step):
    if step<=2000: return 3e-4*step/2000
    return 3e-5+(3e-4-3e-5)*.5*(1+math.cos(math.pi*(step-2000)/(STEPS-2000)))


def train(a):
    configure('cuda');m=check_manifest(a)
    destination=a.out/(a.mode+('_smoke' if a.smoke else ''))
    destination.mkdir(exist_ok=a.resume)
    torch.manual_seed(SEED)
    net,ck=CR.load(torch,a.initial/a.mode/'fit.pt','cuda')
    if sha(a.initial/a.mode/'fit.pt')!=m['initial'][a.mode]['sha256']:
        raise ValueError('Fitted warm start changed')
    for parameter in net.parameters(): parameter.requires_grad_(True)
    net.modules.train()
    optimizer=torch.optim.AdamW(net.parameters(),lr=lr_at(1),weight_decay=1e-4)
    start=0;history=[];counts=Counter();exposure=np.zeros(4,np.int64);digest=hashlib.sha256()
    descriptor_hashes=[];clipped=0
    if a.resume:
        last=torch.load(destination/'last.pt',map_location='cuda',weights_only=True)
        if last['protocol_sha256']!=sha(a.out/'manifest.json') or last['mode']!=a.mode:
            raise ValueError('Resume semantics changed')
        net.modules.load_state_dict(last['state'],strict=True);optimizer.load_state_dict(last['optimizer'])
        start=last['step'];history=last['history'];counts.update(last['song_counts'])
        exposure=np.array(last['audible_source_windows']);descriptor_hashes=last['descriptor_hashes'];clipped=last['clipped_steps']
        for v in descriptor_hashes: digest.update(bytes.fromhex(v))
    cap=20 if a.smoke else STEPS
    reader=Reader(a.cache,m['train_inventory']);begin=time.monotonic();prior_seconds=history[-1]['seconds'] if history else 0.
    if not start and not a.smoke:
        state=dict(architecture='causal-reference-v1',mode=a.mode,state=net.modules.state_dict(),
                   stats=dict(SN.STATS),step=0,protocol_sha256=sha(a.out/'manifest.json'))
        save(destination/'step0.pt',state)
        metrics={part:evaluate(net,a.cache,records,'cuda') for part,records in m['evaluation'].items()}
        write(destination/'eval_step0.json',dict(step=0,mode=a.mode,smoke=False,metrics=metrics,
             checkpoint_sha256=sha(destination/'step0.pt'),decoder_sha256=P.tensors_sha(net.dec),
             protocol_sha256=sha(a.out/'manifest.json')))
        print('INITIAL_EVAL',a.mode,json.dumps({k:v['summary'] for k,v in metrics.items()}),flush=True)
    try:
        with ThreadPoolExecutor(max_workers=1) as producer:
            pending=producer.submit(reader.batch,start+1)
            for step in range(start+1,cap+1):
                arrays,descriptors,energies=pending.result()
                if step<cap:pending=producer.submit(reader.batch,step+1)
                desc_sha=hashlib.sha256(json.dumps(descriptors,sort_keys=True).encode()).hexdigest()
                descriptor_hashes.append(desc_sha);digest.update(bytes.fromhex(desc_sha))
                counts.update(r['song'] for r in descriptors)
                exposure+=(energies>10**(-55/10)).sum(0)
                w=torch.from_numpy(arrays).to('cuda')
                x=SN.stft(torch,w.sum(1));target=SN.stft(torch,w[:,0])
                optimizer.zero_grad(set_to_none=True)
                for group in optimizer.param_groups:group['lr']=lr_at(step)
                value=objective(torch,net.separate_stft(x),target,w[:,0],SECONDS*SN.SR)
                if not bool(torch.isfinite(value)):raise ValueError('Nonfinite training objective')
                value.backward()
                grad=float(torch.nn.utils.clip_grad_norm_(net.parameters(),5.))
                if not np.isfinite(grad):raise ValueError('Nonfinite gradient')
                clipped+=int(grad>5)
                decoder_grad=sum(float(p.grad.square().sum()) for p in net.dec.parameters() if p.grad is not None)**.5
                optimizer.step()
                if step%250==0 or step==cap:
                    row=dict(step=step,loss=float(value),lr=lr_at(step),grad_norm=grad,
                             decoder_grad_norm=decoder_grad,clipped_steps=clipped,seconds=prior_seconds+time.monotonic()-begin,
                             distinct_train_songs=len(counts),windows=BATCH*step,audible_source_windows=exposure.tolist(),
                             sample_sequence_sha256=digest.hexdigest())
                    history.append(row)
                    write(destination/'progress.json',dict(rows=history,song_counts=dict(counts)))
                    state=dict(architecture='causal-reference-v1',mode=a.mode,state=net.modules.state_dict(),stats=dict(SN.STATS),
                        step=step,optimizer=optimizer.state_dict(),history=history,song_counts=dict(counts),
                        audible_source_windows=exposure.tolist(),descriptor_hashes=descriptor_hashes,clipped_steps=clipped,
                        protocol_sha256=sha(a.out/'manifest.json'))
                    save(destination/'last.pt',state)
                    if step in SNAPSHOTS or step==cap:save(destination/f'step{step}.pt',state)
                    print(f'{a.mode} step{step} train_loss{float(value):.6f} grad{grad:.3f} decoder_grad{decoder_grad:.3f} songs{len(counts)} elapsed{row["seconds"]:.0f}s',flush=True)
                if step in SNAPSHOTS or (a.smoke and step==cap):
                    subsets={k:(v[:2] if a.smoke else v) for k,v in m['evaluation'].items() if not a.smoke or k=='train'}
                    metrics={part:evaluate(net,a.cache,records,'cuda') for part,records in subsets.items()}
                    # Add a dedicated TRAIN-only deterministic replay, never a DEV-tuned checkpoint.
                    replay=m['evaluation']['train'][:2]
                    replay_result=metrics['train'] if a.smoke else evaluate(net,a.cache,replay,'cuda')
                    cause=P.causality(torch,net,x)
                    result=dict(step=step,mode=a.mode,smoke=a.smoke,metrics=metrics,replay=replay_result,
                                causality=cause,checkpoint_sha256=sha(destination/f'step{step}.pt'),
                                decoder_sha256=P.tensors_sha(net.dec),protocol_sha256=sha(a.out/'manifest.json'))
                    write(destination/f'eval_step{step}.json',result)
                    print('EVAL',a.mode,step,json.dumps({k:v['summary'] for k,v in metrics.items()}),flush=True)
                    # Discard graphs of the last batch before the next allocation.
                del value,w,x,target
    finally:
        reader.close()
    final=dict(complete=True,mode=a.mode,step=cap,smoke=a.smoke,last=history[-1],
               checkpoint_sha256=sha(destination/f'step{cap}.pt'),protocol_sha256=sha(a.out/'manifest.json'))
    write(destination/'train_complete.json',final)


def audit(a):
    configure('cpu');m=check_manifest(a);results={}
    for mode in MODES:
        folder=a.out/(mode+('_smoke' if a.smoke else ''));step=20 if a.smoke else STEPS
        saved=json.loads((folder/f'eval_step{step}.json').read_text())
        p=folder/f'step{step}.pt'
        if sha(p)!=saved['checkpoint_sha256']:raise ValueError('Saved checkpoint changed')
        net,ck=CR.load(torch,p)
        if ck['step']!=step or ck['mode']!=mode or ck['protocol_sha256']!=sha(a.out/'manifest.json'):
            raise ValueError('Checkpoint horizon/mode/protocol differs')
        if not all(bool(torch.isfinite(v).all()) for v in net.modules.state_dict().values()):raise ValueError('Nonfinite checkpoint')
        measured=evaluate(net,a.cache,m['evaluation']['train'][:2],'cpu')
        errors=[]
        for x,y in zip(measured['rows'],saved['replay']['rows']):
            if x['song']!=y['song'] or (x['sdr_db'] is None)!=(y['sdr_db'] is None) or any(
                    (x['source_tx'][s] is None)!=(y['source_tx'][s] is None) for s in x['source_tx']):
                raise ValueError('CPU replay source coverage differs')
            e=dict(loss=abs(x['loss']-y['loss']),sdr=abs(x['sdr_db']-y['sdr_db']) if x['sdr_db'] is not None else 0.,
                   tx=max((abs(x['source_tx'][s]-y['source_tx'][s]) for s in x['source_tx'] if x['source_tx'][s] is not None),default=0.))
            if e['loss']>1e-4 or e['sdr']>.05 or e['tx']>.005:raise ValueError('CPU/GPU replay outside frozen tolerance')
            errors.append(e)
        if P.tensors_sha(net.dec)!=saved['decoder_sha256'] or P.tensors_sha(net.dec)==m['initial'][mode]['decoder_sha256']:
            raise ValueError('Decoder audit differs or decoder never learned')
        r=m['evaluation']['train'][0]
        raw=MN.read_window(str(a.cache/(r['song']+'.npy')),r['start_sample'],r['samples']).astype(np.float32)/32768
        cause=P.causality(torch,net,SN.stft(torch,torch.from_numpy(raw.sum(0)[None])))
        results[mode]=dict(errors=errors,causality=cause,checkpoint_sha256=sha(p))
    finals={mode:json.loads((a.out/(mode+('_smoke' if a.smoke else ''))/'train_complete.json').read_text()) for mode in MODES}
    if len({f['last']['sample_sequence_sha256'] for f in finals.values()})!=1:
        raise ValueError('Matched source/offset/gain sequence differs')
    write(a.out/('smoke_audit.json' if a.smoke else 'audit.json'),dict(verified=True,smoke=a.smoke,arms=results,
          matched_sample_sequence_sha256=next(iter(finals.values()))['last']['sample_sequence_sha256'],
          protocol_sha256=sha(a.out/'manifest.json'),audit_script_sha256=sha(Path(__file__))))
    print('CPU_SAVED_CHECKPOINT_AUDIT_PASSED',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=['prepare','train','audit'])
    for name in ('cache','initial','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--mode',choices=MODES);p.add_argument('--smoke',action='store_true');p.add_argument('--resume',action='store_true')
    a=p.parse_args()
    if a.action=='train' and not a.mode:p.error('Training requires mode')
    {'prepare':prepare,'train':train,'audit':audit}[a.action](a)
