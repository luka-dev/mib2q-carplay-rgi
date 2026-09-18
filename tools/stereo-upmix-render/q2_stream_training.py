"""Full-song causal Q2 training plumbing; no optimizer, role or release gate.

PCM overlap is only analysis/target context. Model frames are delivered exactly
once, in global order, with continuing owned recurrent state. Loss ownership is
by the STFT frame's exclusive end, not containment in a short PCM crop.
"""
from dataclasses import asdict, dataclass
import hashlib
import json
import math
from pathlib import Path

import numpy as np
import torch

from q2_local_guidance_model import Q2Config, ORACLE_TOKEN
from q2_training_core import prepare_case

SCHEMA = 'q2-full-song-stream-plan-v1'
CONTRACT = dict(
    frame_support='global frame k: [256*k-768,256*(k+1))',
    ownership='exclusive frame end in (core_start,core_stop]; final synthesis padding separate',
    context='real PCM past for analysis/targets only; never replayed into continuing model',
    cadence='global frame indices; read origin multiple1024; four-frame target means',
    supervision='complete physical song windows; quartet may cross a core boundary',
    normalization='owned complete-window powers; optional externally fixed song denominator',
    recurrent_history='caller may detach after TBPTT update; not recomputed with current weights',
    role_status='UNKNOWN', car_approved=False, test_pcm_accessed=False)


def _require(value, message):
    if not value:
        raise ValueError(message)


def _digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, allow_nan=False).encode()).hexdigest()


@dataclass(frozen=True)
class StreamChunk:
    schema: str
    sequence_sha256: str
    song_id: str
    partition: str
    sample_rate: int
    song_frames: int
    index: int
    count: int
    core_start: int
    core_stop: int
    read_start: int
    read_stop: int
    frame_start: int
    frame_stop: int
    core_hops: int
    past_hops: int
    max_seconds: float


def _geometry(song_frames, sample_rate, core_hops, past_hops, max_seconds):
    _require(type(song_frames) is int and song_frames >= 2048,
             'A declared song timeline of at least2048 samples is required')
    _require(type(sample_rate) is int and sample_rate in (44100, 48000), 'Explicit integer native or production rate required')
    _require(type(core_hops) is int and core_hops >= 4 and type(past_hops) is int and past_hops >= 8,
             'Core at least4 hops and analysis/descriptor past at least8 hops required')
    _require(type(max_seconds) in (int,float) and math.isfinite(max_seconds) and 0 < max_seconds <= 10,
             'Finite read bound up to10 seconds required')
    step = core_hops*256
    boundaries = list(range(0,song_frames,step))+[song_frames]
    # A final sub-FFT core has no independent useful loss. Merge, never drop it.
    if len(boundaries)>2 and boundaries[-1]-boundaries[-2] < 1024:
        boundaries.pop(-2)
    chunks=[]
    for index,(start,stop) in enumerate(zip(boundaries,boundaries[1:])):
        origin=max(0,start-past_hops*256)//1024*1024
        final=stop==song_frames
        frame_stop=(song_frames+255)//256+3 if final else stop//256
        _require(stop-origin <= max_seconds*sample_rate, 'Read would exceed declared bounded crop')
        chunks.append(dict(index=index,count=len(boundaries)-1,core_start=start,core_stop=stop,
                           read_start=origin,read_stop=stop,frame_start=start//256,frame_stop=frame_stop))
    return chunks


def plan_song(row, config=None, *, core_hops=None, past_hops=None, max_seconds=10.):
    """Metadata only; returns immutable ordered StreamChunk records.

    `row.frames` is the full maximum native-source timeline, not common_frames.
    Native index44100 is rescaled by the same ceil rule as SourceReader.
    """
    c=Q2Config() if config is None else config
    _require(row.get('partition') in ('TRAIN','DEV'), 'TEST or undeclared partition is forbidden')
    _require(row.get('complete_reconstruction_candidate') is True,
             'Explicit complete-source admission required; no silent row filtering')
    rate=row.get('sample_rate');frames=row.get('frames')
    _require(type(rate) is int and rate in (44100,48000) and type(frames) is int and frames>0, 'Native timeline required')
    total=(frames*c.sample_rate+rate-1)//rate
    core_hops=max(4,int(6*c.sample_rate)//256) if core_hops is None else core_hops
    past_hops=max(8,math.ceil(2*c.sample_rate/256)) if past_hops is None else past_hops
    geometry=_geometry(total,c.sample_rate,core_hops,past_hops,max_seconds)
    identity=dict(schema=SCHEMA,song_id=row['song_id'],partition=row['partition'],
                  sample_rate=c.sample_rate,song_frames=total,core_hops=core_hops,
                  past_hops=past_hops,max_seconds=max_seconds)
    sequence=_digest(identity)
    return tuple(StreamChunk(sequence_sha256=sequence,**identity,**part) for part in geometry)


def validate_chunk(chunk):
    _require(type(chunk) is StreamChunk and chunk.schema==SCHEMA and chunk.partition in ('TRAIN','DEV'),
             'Owned immutable TRAIN/DEV stream record required')
    identity={key:getattr(chunk,key) for key in ('schema','song_id','partition','sample_rate',
              'song_frames','core_hops','past_hops','max_seconds')}
    _require(chunk.sequence_sha256==_digest(identity), 'Sequence identity differs')
    geometry=_geometry(chunk.song_frames,chunk.sample_rate,chunk.core_hops,chunk.past_hops,chunk.max_seconds)
    _require(type(chunk.index) is int and 0<=chunk.index<len(geometry), 'Invalid chunk index')
    _require(all(getattr(chunk,key)==value for key,value in geometry[chunk.index].items()),
             'Global frame ownership, read phase or bounded geometry changed')


def _union(intervals):
    merged=[]
    for start,stop in sorted(intervals):
        if merged and start<=merged[-1][1]:merged[-1][1]=max(merged[-1][1],stop)
        else:merged.append([start,stop])
    return merged


def plan_index(index, *, partition, config=None, core_hops=None, past_hops=None, max_seconds=10.):
    """Metadata-only complete admitted partition; excluded rows listed explicitly."""
    c=Q2Config() if config is None else config
    _require(partition in ('TRAIN','DEV'), 'No TEST planning or PCM access')
    _require(tuple(index['classes'])==c.classes, 'Index/model source vocabulary differs')
    selected=[r for r in index['rows'] if r['partition']==partition]
    _require(len({r['song_id'] for r in selected})==len(selected), 'Duplicate partition song')
    plans=[];excluded=[]
    for row in selected:
        if row.get('complete_reconstruction_candidate') is not True:
            excluded.append(dict(song_id=row['song_id'],reasons=row.get('exclusion_reasons',[])))
            continue
        chunks=plan_song(row,c,core_hops=core_hops,past_hops=past_hops,max_seconds=max_seconds)
        total=chunks[0].song_frames;full=max(0,total//256-3)
        plans.append(dict(song_id=row['song_id'],partition=partition,artist=row.get('artist'),
            native_frames=row['frames'],native_common_frames=row.get('common_frames'),
            song_frames=total,sequence_sha256=chunks[0].sequence_sha256,
            chunks=[asdict(v) for v in chunks],
            requested_core_union=_union([(v.core_start,v.core_stop) for v in chunks]),
            read_pcm_union=_union([(v.read_start,v.read_stop) for v in chunks]),
            past_pcm_union=_union([(v.read_start,v.core_start) for v in chunks if v.read_start<v.core_start]),
            model_visible_real_pcm_union=[[0,total]],model_frame_interval=[0,(total+255)//256+3],
            loss_frame_interval=[3,total//256],loss_frame_count=full,
            loss_window_pcm_union=[] if not full else [[0,(total//256)*256]],
            left_padding_frame_count=3,right_padding_frame_count=(total+255)//256+3-total//256,
            recurrent_resets_per_song=1,source_tails_truncated=False))
    return dict(schema=SCHEMA,config=asdict(c),contract=CONTRACT,partition=partition,
                index_metadata_sha256=_digest(index),songs=plans,excluded=excluded,
                song_count=len(plans),chunk_count=sum(len(p['chunks']) for p in plans),
                source_pcm_decoded=False,test_pcm_accessed=False,car_approved=False)


def prepare_chunk(reader, chunk, config=None, *, device='cpu', denominator=None):
    """Read one bounded real context; return ONLY new frames compatible with objective.

    The optional K-vector denominator must be fixed externally (e.g. a complete
    TRAIN-song statistics pass). Otherwise use this chunk's owned valid frames.
    Neither denominator nor targets is ever forwarded by `advance` normally.
    """
    validate_chunk(chunk);c=Q2Config() if config is None else config
    _require(c.sample_rate==chunk.sample_rate, 'Model/plan sample rate differs')
    row=reader.read_crop(chunk.song_id,chunk.read_start,chunk.read_stop-chunk.read_start,
                         target_rate=c.sample_rate,context_frames=0,partition=chunk.partition)
    _require(row['song_id']==chunk.song_id and row['partition']==chunk.partition
             and row['global_start_frame']==chunk.read_start and row['sample_rate']==c.sample_rate
             and len(row['mixture'])==chunk.read_stop-chunk.read_start,
             'Reader returned a different timeline/partition')
    _require(tuple(row['classes'])==c.classes, 'Reader vocabulary differs')
    # Existing frontend/helper is used unchanged. Artificial crop-edge frames
    # remain solely in the overlap; their targets are never selected at a seam.
    local=dict(row,core_start=0,core_stop=len(row['mixture']))
    prepared=prepare_case(local,c,device='cpu')
    offset=chunk.read_start//256
    start,stop=chunk.frame_start-offset,chunk.frame_stop-offset
    _require(0<=start<stop<=prepared['original'].shape[1], 'Insufficient aligned analysis context')
    original=prepared['original'][:,start:stop].contiguous()
    targets=prepared['targets'][:,start:stop].contiguous()
    indices=torch.arange(chunk.frame_start,chunk.frame_stop,dtype=torch.int64)
    ends=(indices+1)*256;starts=ends-1024
    valid=(starts>=0)&(ends<=chunk.song_frames)
    block_valid=(starts-3*256>=0)&(ends<=chunk.song_frames)
    _require(bool(valid.any()), 'No owned complete physical windows; no false quality pass')
    truth={name:value[:,start:stop].contiguous() for name,value in prepared['truth'].items()}
    _require(torch.equal(truth['updated'][0],(indices+1)%4==0), 'Target global four-hop phase differs')
    weights=prepared['weights']
    source_power=(targets.square().sum(-1)*weights).sum(-1).mean(-1)/1024**2
    mix_power=(original.square().sum(-1)*weights).sum(-1).mean(-1)/1024**2
    source_sum=source_power[:,valid].sum((0,1));mix_sum=mix_power[:,valid].sum()
    count=int(valid.sum())
    owned_denominator=torch.maximum(source_sum/count,(mix_sum/count).expand_as(source_sum)*.01).clamp_min(1e-9)
    if denominator is not None:
        _require(isinstance(denominator,torch.Tensor) and denominator.dtype==torch.float32
                 and denominator.shape==(len(c.classes),) and bool(torch.isfinite(denominator).all())
                 and bool((denominator>0).all()), 'Explicit finite positive fixed K-vector denominator required')
        selected_denominator=denominator.detach().cpu().clone()
    else:selected_denominator=owned_denominator
    transfer=lambda value:value.to(device)
    return dict(original=transfer(original),targets=transfer(targets),valid=transfer(valid),
        block_valid=transfer(block_valid),truth={name:transfer(value) for name,value in truth.items()},
        known=transfer(prepared['known']),detection_known=transfer(prepared['detection_known']),
        denominator=transfer(selected_denominator),weights=transfer(weights),config=c,
        id=f'{chunk.song_id}:chunk{chunk.index}',song_id=chunk.song_id,artist=row.get('artist'),
        partition=chunk.partition,length=chunk.song_frames,core=[chunk.core_start,chunk.core_stop],
        stream_chunk=chunk,global_frame_indices=indices,frame_starts=starts,frame_ends=ends,
        left_padding=starts<0,right_padding=ends>chunk.song_frames,
        normalization='fixed_external_song' if denominator is not None else 'owned_complete_window_chunk',
        power_statistics=dict(source_sum=source_sum,mix_sum=mix_sum,frame_count=count),
        source_provenance=row.get('provenance',{}),
        reconstruction_requires_complete_global_frame_lattice=True)


def denominator_from_statistics(statistics):
    """Combine fixed-scope sufficient statistics; caller binds complete coverage."""
    _require(bool(statistics), 'At least one statistics block required')
    count=0;source=None;mix=None
    for entry in statistics:
        n=entry['frame_count'];s=entry['source_sum'];m=entry['mix_sum']
        _require(type(n) is int and n>0 and isinstance(s,torch.Tensor) and s.ndim==1
                 and isinstance(m,torch.Tensor) and m.ndim==0 and s.dtype==m.dtype==torch.float32
                 and bool(torch.isfinite(s).all()) and bool(torch.isfinite(m))
                 and bool((s>=0).all()) and bool(m>=0), 'Finite positive-count power statistics required')
        _require(source is None or s.shape==source.shape,'Statistics vocabulary shape differs')
        source=s.detach().cpu().double().clone() if source is None else source+s.detach().cpu().double()
        mix=m.detach().cpu().double().clone() if mix is None else mix+m.detach().cpu().double()
        count+=n
    return torch.maximum(source/count,(mix/count).expand_as(source)*.01).clamp_min(1e-9).float()


@dataclass(frozen=True)
class StreamCursor:
    owner: object
    sequence_sha256: str
    next_chunk: int
    next_frame: int
    arm: str
    # Duck-typed owned model state: frames/mode/detach, bound through the model's
    # serialization API. A separately qualified wrapper may own another type.
    model_state: object

    def detach(self):
        return StreamCursor(self.owner,self.sequence_sha256,self.next_chunk,self.next_frame,
                            self.arm,self.model_state.detach())


def advance(model, case, cursor=None, *, arm='predicted', start=0, stop=None):
    """Advance the model once over new original frames, never replay PCM context.

    start/stop are local owned-frame indices, enabling128-frame TBPTT inside a
    bounded PCM read. No optimizer or implicit detach. Missing/reordered frames
    and reset at an interior seam fail closed. A padding-only slice is forward
    only: do not call the old objective when its valid mask is entirely false.
    """
    chunk=case['stream_chunk'];validate_chunk(chunk)
    _require(arm in ('predicted','disabled','oracle_train'), 'Explicit execution arm required')
    _require(case['config']==model.config and chunk.sample_rate==model.config.sample_rate,
             'Case/model configuration differs')
    _require(case['partition']==chunk.partition and case['song_id']==chunk.song_id,
             'Prepared song/partition differs from its plan')
    size=chunk.frame_stop-chunk.frame_start
    stop=size if stop is None else stop
    _require(type(start) is int and type(stop) is int and 0<=start<stop<=size,
             'Explicit consecutive owned-frame slice required')
    global_start,global_stop=chunk.frame_start+start,chunk.frame_start+stop
    if arm=='oracle_train':
        _require(chunk.partition=='TRAIN' and model.training, 'Oracle only on explicitly TRAIN training-mode data')
    if cursor is None:
        _require(chunk.index==0 and global_start==0, 'A song must begin at global frame zero')
        state=None
    else:
        _require(type(cursor) is StreamCursor and cursor.owner is model._state_owner
                 and cursor.sequence_sha256==chunk.sequence_sha256 and cursor.arm==arm
                 and cursor.next_chunk==chunk.index and cursor.next_frame==global_start
                 and cursor.model_state.frames==global_start,
                 'Missing/reordered chunk, wrong global phase or foreign/reset state')
        state=cursor.model_state
    _require(case['original'].shape[1]==size, 'Prepared frame count differs')
    x=case['original'][:,start:stop]
    expected=(torch.arange(global_start,global_stop,device=x.device)+1)%4==0
    _require(torch.equal(case['truth']['updated'][0,start:stop],expected), 'Prepared guidance cadence differs')
    if arm=='oracle_train':
        output,new_state=model.forward_oracle_train(x,case['truth']['raw_guidance'][:,start:stop],state,
            partition='train',diagnostic_token=ORACLE_TOKEN)
    else:output,new_state=model(x,state,guidance_mode=arm)
    _require(new_state.frames==global_stop and torch.equal(output['updated'][0],expected),
             'Model frame continuity/cadence differs')
    return output,StreamCursor(model._state_owner,chunk.sequence_sha256,chunk.index+(stop==size),
                               global_stop,arm,new_state)


def cursor_to_payload(model,cursor):
    _require(type(cursor) is StreamCursor and cursor.owner is model._state_owner,
             'Owned stream cursor required')
    return dict(schema='q2-full-song-stream-cursor-v1',sequence_sha256=cursor.sequence_sha256,
                next_chunk=cursor.next_chunk,next_frame=cursor.next_frame,arm=cursor.arm,
                model_state=model.state_to_payload(cursor.model_state))


def cursor_from_payload(model,payload):
    _require(isinstance(payload,dict) and set(payload)=={'schema','sequence_sha256','next_chunk','next_frame','arm','model_state'}
             and payload['schema']=='q2-full-song-stream-cursor-v1'
             and type(payload['next_chunk']) is int and payload['next_chunk']>=0
             and type(payload['next_frame']) is int and payload['next_frame']>0
             and payload['arm'] in ('predicted','disabled','oracle_train'), 'Invalid serialized stream cursor')
    state=model.state_from_payload(payload['model_state'])
    _require(state.frames==payload['next_frame'] and state.mode==payload['arm'], 'Serialized frame/mode differs')
    return StreamCursor(model._state_owner,payload['sequence_sha256'],payload['next_chunk'],
                        payload['next_frame'],payload['arm'],state)


if __name__=='__main__':
    import argparse
    from q2_source_data import load_index
    parser=argparse.ArgumentParser(description='Metadata-only full-song TRAIN/DEV frame plan')
    parser.add_argument('--index',type=Path,required=True)
    parser.add_argument('--index-sha256',required=True)
    parser.add_argument('--partition',choices=('TRAIN','DEV'),required=True)
    parser.add_argument('--rate',type=int,choices=(44100,48000),default=48000)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    index=load_index(args.index,expected_sha256=args.index_sha256)
    report=plan_index(index,partition=args.partition,config=Q2Config(sample_rate=args.rate))
    report['index_file_sha256']=args.index_sha256
    report['planner_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    with args.output.open('x') as stream:json.dump(report,stream,indent=2,allow_nan=False);stream.write('\n')
    print(json.dumps(dict(output=str(args.output),songs=report['song_count'],chunks=report['chunk_count'],pcm_decoded=False)))
