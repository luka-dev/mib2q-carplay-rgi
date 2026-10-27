"""Explicit successful mode5/512-frame12FAF2 consumer call schedule.

Coefficient state only: not a dynamic audio/delay-transition renderer, callback
scheduler, or proof of the car's lifecycle. Filters must be active/nonreset and
all callees successful; caller supplies effective coefficients and18 pointers.
"""
from collections import Counter
import hashlib
from pathlib import Path
import struct
import sys

import numpy as np

from amp_b_publication import (ROOT,LOADER,BConfigPublicationReference,
                               publication_evidence)
from amp_b_tail_binding import bind_effective_tails,_unsigned_control

PROOFS=(
 (0x131208,0x13163D,'1d05ce72048326c025150ab4be28a3965a0c33f64ed55bab82a997617b20ff75'),
 (0x12EB9B,0x12EC7A,'2ffa1e19a867f854554b61625c849bbc625c0d98e89e2e08e852c174afe0e553'),
 (0x12DBDE,0x12DD16,'23598cc4741faf0dd05e860963ec6ba2c5e815437f5df30d24f13341683ec14c'),
 (0x12F411,0x12F639,'a07848d6c80150f5469aa5a2038e1bee9d1d8021d302a8dfaacf1e455485c2b4'),
 (0x12B87B,0x12B90B,'0749f3a8e984b04a7065ad214b7e08a8d7757ab762a7a0d6eff1c09b5d2c3010'),
 (0x132D0B,0x132D56,'a9a04e980cda74b66cbfb4e48e8c1821fd244018ababc1382361c4e0f41024b2'),
 (0x12A7F9,0x12AB01,'fed3b6b0fa812567c8a37a622266c9c620e22f60341b2e3ae90a13e94505dc8a'),
 (0x12EDE6,0x12EF05,'8537512b7c5577031fbde6886acf6c3e65c1fb239ffb740f3f5fea311693472d'),
 (0x132F86,0x1330E6,'ed08701324904e0e398247071ec3fa316d3cb115f77dc01aae3fff980dfae018'),
)


def mode5_schedule_evidence(loader=LOADER):
    sys.path.insert(0,str(ROOT/'tools'))
    from inspect_amp_visa import load_shortwords,decode
    mem=load_shortwords(Path(loader).read_bytes())
    for a,b,h in PROOFS:
        actual=hashlib.sha256(struct.pack(f'<{b-a}H',*[mem[p] for p in range(a,b)])).hexdigest()
        if actual!=h:raise ValueError(f'mode5 schedule code fingerprint mismatch:{a:x}')
    pins={0x12DC87:'R3=R5+R2 || R4=R5',0x12DC91:'R4=R5-1',
          0x12DC9B:'R4=R5+R1',0x12DCA6:'R4=R5+R0',
          0x12DCAF:'R5=R5+R7 || R4=R3',
          0x12F454:'CJUMP 0x12faf2 (DB); R2=I6, I6=I7',
          0x132D23:'CJUMP 0x12faf2 (DB); R2=I6, I6=I7',
          0x131574:'JUMP 0x13130b (DB)',0x13152E:'JUMP 0x13136c (DB)'}
    for pc,text in pins.items():
        if decode(mem,pc)[1]!=text:raise ValueError(f'mode5 schedule instruction mismatch:{pc:x}')
    return {'ranges':[{'start':hex(a),'end_exclusive':hex(b),'sha256':h} for a,b,h in PROOFS],
            'instruction_pins':{hex(k):v for k,v in pins.items()}}


def mode5_frame_schedule(*,effective_config,pointers,assume_successful_active_path,loader=LOADER):
    """One131208 invocation, mode5,512 frames; no early returns/errors/reset.

    The fixed retained-tail reference validates counts/records/domain. In
    particular filters have positive counts, shared reset-state!=2 and enough
    allocated capacity; these lifecycle premises are explicit, not measured.
    The list includes object97's lower-bound no-op as offset-202.
    """
    if assume_successful_active_path is not True:
        raise ValueError('explicit successful active nonreset mode5 path required')
    bind_effective_tails(effective_config=effective_config,pointers=pointers,library_mode=5,
                        assume_successful_settled_allocations=True,zero_histories=True)
    cfg=np.asarray(effective_config,dtype=np.float32)
    evidence=mode5_schedule_evidence(loader)
    events=[]
    def scalar(label,offset,caller):
        events.append({'consumer':label,'offset':offset,'caller':hex(caller),'smoothing_call':'0x132d23'})
    def delay(label,offset,caller):
        events.append({'consumer':label,'offset':offset,'caller':hex(caller),'smoothing_call':'0x12f454'})
    def eq(label,count_offset,caller):
        count=_unsigned_control(float(cfg[count_offset]),rounded=True)
        for section in range(count):
            for delta,pc in zip((5,4,3,2,1),(0x12DC8A,0x12DC92,0x12DC9D,0x12DCA8,0x12DCB2)):
                events.append({'consumer':label,'offset':count_offset+section*5+delta,
                    'caller':hex(caller),'smoothing_call':hex(pc),'section':section})
    def pair(label,base,caller):
        delay(label+'.left_delay',base,0x12EBC9)
        delay(label+'.right_delay',base+1,0x12EC04)
        scalar(label+'.pre_eq_gain',base+2,0x12EC3A)
        start=len(events);eq(label+'.eq',base+3,0x12EC4D)
        for item in events[start-3:]:item['pair_caller']=hex(caller)
    scalar('mode5.third_input_gain_lower_bound_bypass',-202,0x1315F3)
    delay('two.common_delay',pointers[1160],0x132FD5)
    pair('two',316,0x133015)
    scalar('two.tail_gain',pointers[1158],0x1330D7)
    scalar('two.inline_gain',377,0x13156D)
    scalar('six.pre_gain',123,0x12A864)
    delay('six.common_delay',pointers[1159],0x12A87C)
    for base,pointer,pc,gpc in ((145,1171,0x12A98A,0x12AA02),
                               (196,1172,0x12AA1A,0x12AA97),
                               (247,1173,0x12AAAF,0x12AADF)):
        pair(f'six.pair{base}',base,pc)
        scalar(f'six.pair{base}.gain',pointers[pointer],gpc)
    scalar('six.tail_gain',pointers[1157],0x12AAF2)
    scalar('six.inline_gain',293,0x131490)
    delay('four.common_delay',32,0x12EE46)
    pair('four.pair33',33,0x12EEA2);pair('four.pair77',77,0x12EECB)
    scalar('four.tail_gain',pointers[1156],0x12EEF6)
    scalar('four.inline_gain',122,0x1314DA)
    eq('finish.six_eq',294,0x13138C)
    eq('finish.two_eq',378,0x1313E6)
    eq('finish.all_eq',399,0x12B8BA)
    scalar('finish.final_gain',420,0x12B8F0)
    counts=Counter(e['offset'] for e in events)
    marked=publication_evidence(loader)['initial_interpolated_config_offsets']
    return {'events':events,'offsets':[e['offset'] for e in events],
            'metadata':{'kind':'successful_mode5_512_sample_consumer_schedule',
                'algorithm':'0x131208','library_mode':5,'frames':512,
                'initial31_interpolated_visits':{str(o):counts[o] for o in marked},
                'actual_vehicle_state_proven':False,'evidence':evidence,
                'assumptions':['one complete successful131208 invocation;512 frames',
                    'positive EQ counts, active/nonreset filter state, stable coefficients',
                    'enough delay/filter allocations; all callees succeed',
                    'no mode/pointer/count mutation or reentrant publication during invocation'],
                'not_emulated':['audio with changing filters/delays','callback-to-invocation scheduling',
                    'error/early-return prefixes','zero-count/state2 filter lifecycle']}}


def advance_mode5_frame(state,*,pointers,assume_successful_active_path,loader=LOADER):
    """Advance coefficient state only; return schedule and per-access values.

    Uses a private trial state and validates the post-update active filter
    domain before commit. End-of-frame coefficients must NOT be applied to
    an entire dynamic audio frame: consumers read at their own event boundary.
    """
    if not isinstance(state,BConfigPublicationReference):raise ValueError('explicit publication state required')
    schedule=mode5_frame_schedule(effective_config=state.effective,pointers=pointers,
        assume_successful_active_path=assume_successful_active_path,loader=loader)
    trial=BConfigPublicationReference(effective_config=state.effective,target_config=state.target,steps=state.steps)
    accesses=[]
    for event in schedule['events']:
        offset=event['offset'];before=None if offset<0 else float(trial.effective[offset])
        trial.advance_offsets([offset])
        accesses.append({**event,'before':before,'after':None if offset<0 else float(trial.effective[offset])})
    bind_effective_tails(effective_config=trial.effective,pointers=pointers,library_mode=5,
                        assume_successful_settled_allocations=True,zero_histories=True)
    state.effective,state.steps=trial.effective,trial.steps
    return {**state.snapshot(),'schedule':schedule,'accesses':accesses,
            'boundary':'coefficient state only; not interleaved dynamic audio or delay transitions'}
