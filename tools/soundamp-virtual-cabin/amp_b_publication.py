"""Bounded first-B replay/publication and explicitly scheduled coefficient reads.

No car state, callback scheduler, error rollback, mode-pointer writes or audio
history reset is inferred. A coefficient advances when the caller explicitly
models a12FAF2 call, not once per sample/frame by assumption.
"""
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys

import numpy as np

from amp_b_bound_scenario import retained_config, replay_bound_scalar_targets

ROOT=Path(__file__).resolve().parents[2]
LOADER=ROOT/'assets/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'
STEP_BASE=0xB8CFD
PROOFS=((0x12FAF2,0x12FC78,'0cd10d117e3b0f9a9aa5948ebdb6c4f382c09e17b7329b4280e4da022a8e4ec2'),
        (0x132168,0x1323DE,'f4ead817847774bea62ea0a7e00218d2b56eaf4d788cd39e5848c357c051679e'))


def publication_evidence(loader=LOADER):
    sys.path.insert(0,str(ROOT/'tools'))
    from inspect_amp_visa import load_shortwords,decode
    from extract_amp_sharc_app import BITREV_SHA256
    mem=load_shortwords(Path(loader).read_bytes())
    for a,b,h in PROOFS:
        actual=hashlib.sha256(struct.pack(f'<{b-a}H',*[mem[p] for p in range(a,b)])).hexdigest()
        if actual!=h:raise ValueError(f'publication code fingerprint mismatch:{a:x}')
    pins={0x12FBCC:'R2=BTGL R8 BY 0',0x12FBCF:'I5=DM(0xb903a)',
          0x12FC05:'COMP(F2,F13) || R2=DM(I4,M6)',
          0x12FC3A:'DM(I5,M6)=R2',0x12FC34:'DM(M7,I3)=I12',
          0x12FB1B:'IF GT JUMP 0x12fb2c',0x12FB1E:'DM(M5,I4)=R1',
          0x12FB22:'DM(M4,I5)=I12',0x12FB2C:'F2+=F0',
          0x132329:'R4=PASS R10 || R8=R15',
          0x1323A3:'CJUMP 0x132168 (DB); R2=I6, I6=I7',
          0x1323AA:'DM(+1129,I2)=R14'}
    for pc,expected in pins.items():
        if decode(mem,pc)[1]!=expected:raise ValueError(f'publication instruction mismatch:{pc:x}')
    marked=[];pc=0x12FB42
    while pc<0x12FB9F:
        size,op,*_=decode(mem,pc)
        match=re.fullmatch(r'DM\((0x[0-9a-f]+)\)=I4',op)
        if match is None:raise ValueError('interpolation-mask instruction not a direct zero store')
        marked.append(int(match[1],16)-STEP_BASE);pc+=size
    if len(marked)!=31 or len(set(marked))!=31 or not all(0<=v<829 for v in marked):
        raise ValueError('unexpected interpolation-mask coverage')
    return {'loader_sha256':BITREV_SHA256,'initial_interpolated_config_offsets':marked,
            'ranges':[{'start':hex(a),'end_exclusive':hex(b),'sha256':h} for a,b,h in PROOFS],
            'instruction_pins':{hex(k):v for k,v in pins.items()},
            'address_units':'SHARC shortword code PCs; config offsets are32-bit words'}


def initial_interpolation_steps(*,assume_successful_initialization,loader=LOADER):
    if assume_successful_initialization is not True:
        raise ValueError('explicit successful12FC3C initialization required')
    steps=np.ones(829)
    steps[publication_evidence(loader)['initial_interpolated_config_offsets']]=0
    return steps


def _vector(values):
    if np.iscomplexobj(values):raise ValueError('real829-word vector required')
    with np.errstate(over='ignore',invalid='ignore'):
        result=np.asarray(values,dtype=np.float32)
    if result.shape!=(829,) or not np.isfinite(result).all():
        raise ValueError('finite float32-representable829-word vector required')
    return result.astype(float)


def _duration(value):
    if not 0<=value<=480000:raise ValueError('duration outside bounded nonnegative domain0..480000')
    return max(1,math.trunc(value))  #12FBB7..12FBC9: no +.5.


class BConfigPublicationReference:
    """12FBA5/12FAF2 float32 mathematical state, no implicit scheduler.

    Prior steps are explicit: exact float1 is an immediate-copy SENTINEL.
    A calculated step may itself equal1; the next publication then sees it as
    the sentinel, exactly as the code does. Do not replace this with bool flags.
    Float32 arithmetic is not a SHARC40-bit/RECIPS bit-exact execution claim.
    """
    def __init__(self,*,effective_config,target_config,steps):
        self.effective=_vector(effective_config);self.target=_vector(target_config)
        self.steps=_vector(steps);self.duration=_duration(self.target[2])

    def publish(self,target_config,*,publication_flag):
        if type(publication_flag)is not int or not 0<=publication_flag<=0xffffffff:
            raise ValueError('explicit raw uint32 publication flag required')
        target=_vector(target_config);duration=_duration(target[2])
        effective=self.effective.copy();steps=self.steps.copy()
        if publication_flag==1:
            effective=target.copy()  #12FBD5 copies all829, leaves steps intact.
        else:
            for i in range(829):
                if steps[i]==1:
                    effective[i]=target[i]
                else:
                    delta=np.float32(np.float32(target[i])-np.float32(effective[i]))
                    steps[i]=float(np.float32(delta/np.float32(duration))) if delta!=0 else 0.
        if not np.isfinite(steps).all():raise ValueError('publication step outside finite reference domain')
        self.effective,self.target,self.steps,self.duration=effective,target,steps,duration
        return self.snapshot()

    def advance_offsets(self,offsets):
        """One12FAF2 call per supplied offset, preserving order/repetitions.

        Negative offsets model the proven lower-bound bypass (object97 versus
        config299, for example). Beyond829 is rejected, not silently clamped:
        firmware has no upper check but host lacks those memory words.
        """
        offsets=list(offsets)
        if any(type(i)is not int or not -299<=i<829 for i in offsets):
            raise ValueError('explicit integer call offsets within object0..1127 required')
        effective=self.effective.copy();steps=self.steps.copy()
        for i in offsets:
            if i<0 or self.target[i]==effective[i]:continue
            remaining=abs(float(np.float32(np.float32(self.target[i])-np.float32(effective[i]))))
            if remaining<=abs(steps[i]):
                effective[i]=self.target[i];steps[i]=0.
            else:
                effective[i]=float(np.float32(np.float32(effective[i])+np.float32(steps[i])))
        if not np.isfinite(effective).all():raise ValueError('coefficient advance outside finite reference domain')
        self.effective,self.steps=effective,steps
        return self.snapshot()

    def snapshot(self):
        return {'effective_config':self.effective.tolist(),'target_config':self.target.tolist(),
                'steps':self.steps.tolist(),'duration':self.duration,
                'all_current_words_equal_target':bool(np.array_equal(self.effective,self.target))}


def replay_retained_bank_publication(report,*,source,bank,effective_values,contributions,
        shadow_selectors,current_effective,prior_steps,publication_flag,
        assume_successful_validated_bank,allow_nonzero_eq=False,loader=LOADER):
    """Successful132346 bank-copy/rebase/all9-control-replay/publication slice.

    Caller supplies prior135 contribution slots, current829 words, prior829
    step values, all9 effective replay controls and truncated shadow16/20.
    Rebase uses the chosen retained bank; slots survive. Common header, mode
    pointer writes, queues, callbacks and audio-history transitions are outside.
    This returns a WHOLE829-word target and the exact bounded publication
    result, never calls a still-ramping vector settled by assumption.
    """
    if assume_successful_validated_bank is not True:
        raise ValueError('explicit successful validated retained-bank path required')
    evidence=publication_evidence(loader)
    target=retained_config(report,source=source,bank=bank)
    replay=replay_bound_scalar_targets(report,source=source,bank=bank,
        effective_values=effective_values,contributions=contributions,
        shadow_selectors=shadow_selectors,allow_nonzero_eq=allow_nonzero_eq)
    for offset,value in replay['config_updates'].items():target[offset]=value
    state=BConfigPublicationReference(effective_config=current_effective,
                                     target_config=target,steps=prior_steps)
    published=state.publish(target,publication_flag=publication_flag)
    return {**published,'contributions':copy.deepcopy(replay['contributions']),
        'scalar_targets':copy.deepcopy(replay['targets']),
        'metadata':{'kind':'successful_retained_bank_all9_replay_publication',
            'source':source,'bank':bank,'publication_flag_raw':publication_flag,
            'retained_bank_sha256':report['sources'][source]['banks'][bank-1]['sha256'],
            'control_binding_sha256':hashlib.sha256(json.dumps(
                report['control_bindings_by_bank'][bank-1],sort_keys=True).encode()).hexdigest(),
            'publication_copy_all829':publication_flag==1,
            'effective_api17_after_successful_path':bank,
            'replay_order':[19,18,22,21,23,24,25,27,29],
            'updated_config_offsets':sorted(replay['config_updates']),
            'actual_vehicle_state_proven':False,'eq_design':replay['eq_design'],
            'evidence':evidence,'numerical_boundary':'float32 host math, not SHARC40-bit/RECIPS exact',
            'not_emulated':['failed/partial replay or rollback','FIFO/defer and command delivery',
                'mode pointer writes and ordering','implicit audio call schedule',
                'common245-word header updates','audio-history reset/lifecycle'],
            'scope':'all829 post-replay target words and explicit current publication state; no current-car or settled-time claim'}}
