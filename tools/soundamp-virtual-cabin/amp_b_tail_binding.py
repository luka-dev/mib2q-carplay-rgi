"""Bind a supplied effective B configuration to its three fixed audio tails.

Successful settled processing, sufficient allocated capacity and zero histories
are explicit caller assumptions. Not a live preset, publication/interpolation
model, full spectral configuration, transition or SHARC bit-exact emulator.
"""
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

import numpy as np

from amp_b_bound_scenario import bind_effective_output
from amp_b_decorrelator import BTwoChannelTailReference
from amp_b_output import BSixChannelTailReference, BFourChannelTailReference, BOutputFinishReference
from amp_b_pipeline import BMode5FixedReference, BSpectralFrameReference

ROOT = Path(__file__).resolve().parents[2]
LOADER = ROOT/'assets/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'
PROOFS = [
    (0x12A7F9, 0x12AB01, 'fed3b6b0fa812567c8a37a622266c9c620e22f60341b2e3ae90a13e94505dc8a'),
    (0x12EDE6, 0x12EF05, '8537512b7c5577031fbde6886acf6c3e65c1fb239ffb740f3f5fea311693472d'),
    (0x132F86, 0x1330E6, 'ed08701324904e0e398247071ec3fa316d3cb115f77dc01aae3fff980dfae018'),
    (0x12EB9B, 0x12EC7A, '2ffa1e19a867f854554b61625c849bbc625c0d98e89e2e08e852c174afe0e553'),
    (0x12D10E, 0x12D21B, '7f52afbe98c74a2d71ae2b390c6f8f37b4c113399aa7a76d6892d0e833cb29b9'),
    (0x12B90B, 0x12B964, '26024e894c7179b1b0320dfb47288c5b85b219b7b1080ee35bcc2aa440abcdf8'),
    (0x12DBDE, 0x12DC24, '6adbd8e311da97e64bd9d20634b74493ea2e4d7949554f5711f1c925540bc1b5'),
    (0x12F411, 0x12F477, '25a2806e08e145a5eed36bf078b3b1c769e98bd25043b7cf0ffbe7158fc1b52e'),
]


def tail_binding_evidence(loader=LOADER):
    sys.path.insert(0, str(ROOT/'tools'))
    from extract_amp_sharc_app import SOURCE_SHA256, BITREV_SHA256
    from inspect_amp_visa import load_shortwords, decode
    mem = load_shortwords(Path(loader).read_bytes())
    for a, b, expected in PROOFS:
        actual = hashlib.sha256(struct.pack(f'<{b-a}H', *[mem[p] for p in range(a,b)])).hexdigest()
        if actual != expected:
            raise ValueError(f'tail code fingerprint mismatch at {a:x}')
    pins = {
        0x12EE3F:'R15=0x14b', 0x12EE41:'R8=R10+R15 || DM(I7,M7)=R11',
        0x12EE80:'R14=0x14c', 0x12EEC4:'R2=0x178',
        0x12A85D:'R3=0x1a6', 0x12A983:'R2=0x1bc',
        0x12AA13:'R2=0x1ef', 0x12AAA8:'R2=0x222',
        0x12A9AC:'R2=0x1e8', 0x12A9D5:'R2=0x1eb',
        0x12AA3C:'R2=0x21b', 0x12AA65:'R2=0x21e',
        0x13300E:'R2=0x267', 0x133035:'R2=0x293',
        0x13305C:'R2=0x298', 0x133083:'R2=0x29b', 0x1330AA:'R2=0x2a0',
        0x12D12E:'R2=DM(M5,I4)', 0x12D133:'F2=F2+F15 || I5=R4',
        0x12B930:'R1=DM(M5,I2)', 0x12B938:'R1=TRUNC F1 || I4=I15',
        0x12EC36:'R4=R13+R12 || R15=I4', 0x12EC47:'R8=R13+R2 || DM(M6,I7)=I12',
        0x12D157:'COMPU(R4,R0)', 0x12D165:'COMPU(R0,R2) || R1=DM(+5,I5)',
        0x12D168:'IF LT R2=M5', 0x12D17C:'R14=R2+1 || M4=R1',
        0x12D184:'F12=F13*F2 || R2=DM(M3,I2)',
        0x12D18B:'F8=F8*F4 || DM(M3,I2)=R8',
        0x12D191:'COMPU(R0,R14) || R11=DM(M4,I4)', 0x12D195:'IF LE R2=M5',
        0x12D19D:'COMPU(R15,R1) || DM(M4,I4)=R8', 0x12D1A0:'IF LE R1=M5',
    }
    for pc, expected in pins.items():
        if decode(mem, pc)[1] != expected:
            raise ValueError(f'tail instruction changed at {pc:x}')
    return {'source_sha256':SOURCE_SHA256, 'loader_sha256':BITREV_SHA256,
            'ranges':[{'start':hex(a),'end_exclusive':hex(b),'sha256':h} for a,b,h in PROOFS],
            'instruction_pins':{hex(a):v for a,v in pins.items()},
            'address_units':'SHARC shortword PCs; object/config offsets are 32-bit words'}


def _unsigned_control(value, *, rounded):
    # Bounded nonnegative domain avoids inventing firmware uint overflow/errors.
    # FLOAT32 addition before TRUNC matters at half-integer boundaries.
    if not math.isfinite(value) or not 0 <= value <= 480000:
        raise ValueError('control outside bounded nonnegative host domain0..480000')
    return math.trunc(float(np.float32(np.float32(value) + np.float32(.5)))) if rounded else math.trunc(float(np.float32(value)))


def bind_effective_tails(*, effective_config, pointers, library_mode,
                         assume_successful_settled_allocations, zero_histories):
    """Return constructor arguments, never infer current state from image banks.

    Config and pointers must describe the same explicit effective snapshot.
    Pointer ordering and delivery remain caller responsibilities. Missing or
    unstable state fails. Zero-section EQ lifecycle is deliberately unsupported.
    """
    if assume_successful_settled_allocations is not True or zero_histories is not True:
        raise ValueError('explicit successful settled allocations and zero histories required')
    if np.iscomplexobj(effective_config):
        raise ValueError('real effective configuration required')
    cfg = np.asarray(effective_config, dtype=float)
    if cfg.shape != (829,) or not np.isfinite(cfg).all():
        raise ValueError('finite real effective829-word config required')
    with np.errstate(over='ignore',invalid='ignore'):
        cfg32=cfg.astype('<f4')
    if not np.isfinite(cfg32).all():
        raise ValueError('effective configuration must be representable as finite float32')
    if set(pointers) != set(range(1156,1174)) or any(type(v) is not int or not 0 <= v < 829 for v in pointers.values()):
        raise ValueError('all18 explicit config-relative pointer offsets required')
    def pointer(word, width=1):
        start=pointers[word]
        if start+width>829:
            raise ValueError('pointer record exceeds effective config')
        return cfg[start:start+width].tolist()
    def delay(v): return _unsigned_control(v, rounded=True)
    def nested(record):
        if _unsigned_control(record[0], rounded=True)==0: return None
        result={'delay1':delay(record[1]),'gain1':record[2],
                'delay2':delay(record[3]),'gain2':record[4]}
        if result['delay1']==0 or result['delay2']==0:
            result['allow_zero_delay_cells']=True
        return result
    def single(offset):
        record=cfg[offset:offset+3]
        # Unlike nested enable, single enable does NOT add0.5 before truncation.
        if _unsigned_control(record[0], rounded=False)==0: return None
        return {'delay':delay(record[1]),'gain':float(record[2])}
    def pair(offset, singles, final_gain):
        count=delay(cfg[offset+3])
        if not 1 <= count <= 8:
            raise ValueError('active EQ count1..8 required; zero-section lifecycle not modelled')
        return {'channel_delays':[delay(v) for v in cfg[offset:offset+2]],
                'pre_eq_gain':float(cfg[offset+2]),
                'eq_records':cfg[offset+4:offset+4+5*count].reshape(count,5).tolist(),
                'single':singles, 'final_gain':final_gain}
    four={'common_delay':delay(cfg[32]),
          'pair_configs':[pair(o,[None,None],1) for o in (33,77)],
          'final_gain':pointer(1156)[0]}
    six={'pre_gain':float(cfg[123]),'common_delay':delay(pointer(1159)[0]),
         'nested':[[nested(pointer(p,5)) for p in row] for row in ((1167,1168),(1169,1170))],
         'pair_configs':[pair(o, [single(s) for s in ss] if ss else [None,None], pointer(p)[0])
                         for o,ss,p in ((145,(189,192),1171),(196,(240,243),1172),(247,(),1173))],
         'final_gain':pointer(1157)[0]}
    two=dict(pair(316,[single(365),single(373)],pointer(1158)[0]),
             common_delay=delay(pointer(1160)[0]),
             nested=[nested(cfg[o:o+5].tolist()) for o in (360,368)])
    finish=bind_effective_output(effective_config=cfg.tolist(),library_mode=library_mode,
                                 successful_filter_path=True)['finish_parameters']
    # Validate all active kernels, including positive enabled allpass delays.
    BSixChannelTailReference(**six); BFourChannelTailReference(**four)
    BTwoChannelTailReference(**two); BOutputFinishReference(**finish)
    return {'six_tail':six,'four_tail':four,'two_tail':two,'finish':finish,
            'metadata':{'kind':'explicit_effective_first_B_tail_binding',
                'library_mode':library_mode, 'zero_histories':True,
                'successful_settled_allocations_assumed':True,'actual_vehicle_state_proven':False,
                'nested_zero_request':'one addressed history cell, requiring valid backing memory; no allocation or arbitrary-cursor proof',
                'effective_config_sha256_float64_le':hashlib.sha256(cfg.astype('<f8').tobytes()).hexdigest(),
                'effective_config_sha256_float32_le':hashlib.sha256(cfg32.tobytes()).hexdigest(),
                'pointers_sha256':hashlib.sha256(json.dumps(pointers,sort_keys=True).encode()).hexdigest(),
                'nested_requested_records':{**{str(p):pointer(p,5) for p in range(1167,1171)},
                                            'two_left':cfg[360:365].tolist(),'two_right':cfg[368:373].tolist()},
                'unbound_indirect_pointer_words':[1162,1163,1164,1165],
                'evidence':tail_binding_evidence(),
                'boundary':'spectral branch PCM to8 B outputs; no spectral parameter binding, interpolation, allocation checks, live state or lifecycle'}}


def build_effective_mode5(*, spectral, input_mix_gain, **binding_args):
    """Join supplied spectral reference to bound tails/finish; zero tail histories.

    Spectral parameters/histories and object97 gain remain separately explicit;
    object97 is NOT in the829-word effective block starting at object299.
    Returned metadata must accompany any scenario passed to AmpChain.
    """
    if not isinstance(spectral, BSpectralFrameReference) or binding_args.get('library_mode') != 5:
        raise ValueError('explicit spectral reference and mode5 required')
    bound=bind_effective_tails(**binding_args)
    ref=BMode5FixedReference(spectral=copy.deepcopy(spectral),input_mix_gain=input_mix_gain,
        six_tail=BSixChannelTailReference(**bound['six_tail']),
        four_tail=BFourChannelTailReference(**bound['four_tail']),
        two_tail=BTwoChannelTailReference(**bound['two_tail']),
        finish=BOutputFinishReference(**bound['finish']))
    # Retain binding/proof boundary inside the reference so AmpChain's state
    # fingerprint includes it. No change to AmpChain's default metadata.
    ref.tail_binding_metadata=copy.deepcopy(bound['metadata'])
    return ref
