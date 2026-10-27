"""Bind first-B spatial coefficients to an explicit successful fresh-init state.

Retained/effective coefficients are caller supplied, never a live preset. The
fresh constructor state is instruction-derived, not an arbitrary all-zero
history: gate starts at ONE. Subsequent playback histories remain unknown.
"""
import hashlib
import math
from pathlib import Path
import struct
import sys

import numpy as np

from amp_b_spatial import SpatialFrameReference

ROOT = Path(__file__).resolve().parents[2]
LOADER = ROOT/'assets/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'
PROOFS = (
    (0x12E109,0x12E116,'42ab88975234af523b4eaaf410eb3f11c9a1d965c5204f0c38b9a39ad1127187'),
    (0x12FD24,0x12FE4F,'c5e317678a99646b2fc0740321470b16ef24c6bc5ce4a764e20daad1c853fa72'),
    (0x132D56,0x132E08,'df3f1daeadbd18dc082601844941003a38238f828ba7e4ea03890cba9ed507bd'),
    (0x130771,0x130A1C,'91ef90cbafac3c0d3ea203f4f4a45e19b54d52a1074bc1202b77a77654216d26'),
    (0x12FFBA,0x130237,'5d7f5156b04b1b04d99e7edc37848ba1deea58cabdbd59927c2f377cb36190de'),
)


def spatial_binding_evidence(loader=LOADER):
    sys.path.insert(0,str(ROOT/'tools'))
    from inspect_amp_visa import load_shortwords, decode
    from audit_amp_output_bindings import loader_dm
    from extract_amp_sharc_app import parse_blocks, BITREV_SHA256
    raw=Path(loader).read_bytes()
    mem=load_shortwords(raw)  # Entire loader hash, not just selected records.
    for a,b,expected in PROOFS:
        actual=hashlib.sha256(struct.pack(f'<{b-a}H',*[mem[p] for p in range(a,b)])).hexdigest()
        if actual!=expected:
            raise ValueError(f'spatial instruction range mismatch at {a:x}')
    pins={0x12E10D:'R8=0x4a',0x12E6A7:'R8=DM(+1166,I3)',
          0x12FDF6:'R4=0x3f800000',
          0x12FE2B:'I12=0x40a00001',0x12FE2E:'I13=0xc0600001',
          0x12FE33:'DM(+10,I5)=I12',0x12FE35:'DM(+11,I5)=I13',
          0x132DF2:'R14=PASS R2 || R8=M5',0x1257DD:'DM(I4,+1)=R8',
          0x1308A5:'R1=DM(+8,I5)',0x130001:'R0=DM(+2,I3)'}
    for pc,expected in pins.items():
        if decode(mem,pc)[1]!=expected:
            raise ValueError(f'spatial instruction mismatch at {pc:x}')
    dm=loader_dm(raw,parse_blocks(raw))
    if dm[0xB9097]!=0xB904C or dm[0xB9098]!=74:
        raise ValueError('retained spatial band-map pointer/count changed')
    edges=[dm[0xB904C+i] for i in range(75)]
    return {'loader_sha256':BITREV_SHA256,'edges':edges,
            'ranges':[{'start':hex(a),'end_exclusive':hex(b),'sha256':h} for a,b,h in PROOFS],
            'instruction_pins':{hex(k):v for k,v in pins.items()},
            'address_units':'SHARC shortword PCs; DM/config offsets are32-bit words'}


def _converted(value):
    # Retained controls are in this bounded nonnegative domain. Do not invent
    # unsigned overflow/negative TRUNC behavior for unrelated effective writes.
    if not math.isfinite(value) or not 0<=value<=480000:
        raise ValueError('selector outside bounded nonnegative conversion domain')
    return math.trunc(float(np.float32(np.float32(value)+np.float32(.5))))


def bind_effective_spatial(*, effective_config, pointers, edges,
                           assume_successful_fresh_initialization, loader=LOADER):
    """Return constructor kwargs, process enable and explicit state provenance.

    All18 pointer offsets identify one explicitly selected effective snapshot;
    caller owns bank selection, command ordering and delivery/settling premise.
    Only the retained74-band/513-bin constructor is bound. This deliberately
    does not initialize from a guessed learned/current playback state.
    """
    if assume_successful_fresh_initialization is not True:
        raise ValueError('explicit successful fresh initialization required')
    if np.iscomplexobj(effective_config):
        raise ValueError('real effective configuration required')
    cfg=np.asarray(effective_config,dtype=float)
    if cfg.shape!=(829,) or not np.isfinite(cfg).all():
        raise ValueError('finite effective829-word configuration required')
    with np.errstate(over='ignore',invalid='ignore'):
        cfg32=cfg.astype('<f4')
    if not np.isfinite(cfg32).all():
        raise ValueError('finite float32 configuration required')
    if set(pointers)!=set(range(1156,1174)) or any(type(v)is not int or not 0<=v<829 for v in pointers.values()):
        raise ValueError('all18 explicit config-relative pointer offsets required')
    offset=pointers[1166]
    if offset+28>829:
        raise ValueError('spatial parameter pointer overruns effective bank')
    evidence=spatial_binding_evidence(loader)
    if list(edges)!=evidence['edges']:
        raise ValueError('explicit edges must equal the retained74-band map')
    p=cfg32[offset:offset+28].astype(float).tolist()
    converted={i:_converted(p[i]) for i in (0,1,2,3,6,7)}
    args={'bins':513,'edges':list(edges),'params':p,
          'eigen_selector':converted[1],'balance_enabled':converted[2]!=0,
          'gate_selector':converted[3],'low_band':converted[6],'high_band':converted[7],
          'balance_slope':struct.unpack('<f',struct.pack('<I',0x40A00001))[0],
          'balance_offset':struct.unpack('<f',struct.pack('<I',0xC0600001))[0],
          'left_history':[0.]*74,'right_history':[0.]*74,'cross_history':[0.]*74,
          'diffuse_history':[0.]*74,'minimum_history':0.,'mean_history':0.,'gate_history':1.}
    SpatialFrameReference(**args)  # Fail closed on invalid converted bounds.
    return {'constructor':args,'enabled':converted[0]!=0,
            'metadata':{'kind':'explicit_effective_spatial_successful_fresh_init',
                'actual_vehicle_state_proven':False,'parameter_pointer_word':1166,
                'parameter_config_offset':offset,'converted_controls':{str(k):v for k,v in converted.items()},
                'parameter_sha256_float32_le':hashlib.sha256(cfg32[offset:offset+28].tobytes()).hexdigest(),
                'effective_config_sha256_float32_le':hashlib.sha256(cfg32.tobytes()).hexdigest(),
                'history_origin':'12FD24 successful constructor via zero-filling132D56; gate explicitly filled1 at12FDF6..12FDFC',
                'balance_raw_words':['40a00001','c0600001'],
                'ramp_step_origin':'parameter8 is supplied, NOT derived from endpoints or band width',
                'assumptions':['supplied effective configuration and pointer ordering',
                               'successful fresh constructor with sufficient allocation',
                               'retained band map unchanged; no later state/parameter writes'],
                'not_established':['live bank/mode/pointer state','current playback histories',
                                   'publication/interpolation timing','SHARC bit-exact arithmetic'],
                'evidence':evidence}}


def build_effective_spatial(**kwargs):
    """Return (fresh SpatialFrameReference, binding) without choosing a preset."""
    binding=bind_effective_spatial(**kwargs)
    return SpatialFrameReference(**binding['constructor']),binding
