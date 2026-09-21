"""Retained first-B spectral coefficients, never an inferred live preset.

The common245-word source prefix is NOT a selected829-word calibration bank.
Crossfeed alone in this slice follows an explicit effective-config pointer.
Histories, allocation success, live writes and spatial controls stay explicit.
"""
import copy
import hashlib
from pathlib import Path
import struct
import sys

import numpy as np

from amp_b_adaptive import BandLevelMaskReference, FrameVectorSmoother
from amp_b_pipeline import BSpectralFrameReference, StereoTransientReference
from amp_b_tail_binding import _unsigned_control
from amp_b_window import load_spectral_tables

ROOT=Path(__file__).resolve().parents[2]
LOADER=ROOT/'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'
IMAGE=ROOT.parents[1]/'Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin'
sys.path.insert(0,str(ROOT/'tools'))
from inspect_amp_visa import load_shortwords, decode
from extract_amp_sharc_app import parse_blocks, BITREV_SHA256
from audit_amp_output_bindings import loader_dm, SOURCE_SHA256, SOURCE_SIZE
from audit_amp_b_config_control import config_crc

PINS={
    0x130F8E:'R2=0x3b',
    0x130F90:'R4=R13+R2 || R8=R9',
    0x130F93:'CJUMP 0x12b4d0 (DB); R2=I6, I6=I7',
    0x12B4DD:'R0=0x26',
    0x12B4E6:'R8=R1+R0',
    0x12B4E8:'CJUMP 0x12b2bf (DB); R2=I6, I6=I7',
    0x12B2CA:'I4=MODIFY(I4,+33)',
    0x12B2D1:'LCNTR=103, DO 0x12b2d7 UNTIL LCE',
    0x12B2D4:'R2=DM(I5,+2)', 0x12B2D7:'DM(I4,+2)=R2',
    0x12B2DF:'R2=DM(I5,M6)', 0x12B2E0:'DM(I4,M6)=R2',
    0x1315AE:'R0=0x3b', 0x1315B0:'R8=R11+R0 || DM(I7,M7)=R6',
    0x12E1FB:'R10=0x27', 0x12E1FD:'R4=R5+R10 || R12=DM(M6,I6)',
    0x12E334:'R10=0xd3', 0x12E336:'R2=R2+R10 || DM(I7,M7)=R14',
    0x12E588:'R14=0x2d', 0x12E58A:'R14=R14+R3 || DM(I7,M7)=R5',
    0x12E683:'R4=DM(+1102,I5)', 0x12E48E:'R13=DM(M5,I4)',
    0x12E6EC:'R2=0x78', 0x12E6EE:'R13=R7+R2 || I7=MODIFY(I7,M7)',
    0x12E6F4:'R12=0x100', 0x12E706:'R12=0x100',
    0x12E780:'R2=0x7b', 0x12E782:'R8=R7+R2 || DM(I7,M7)=R5',
    0x131763:'R0=DM(0xb9098)', 0x13176C:'R0=DM(0xb9097)',
    0x12F0CC:'R8=0x128', 0x12F0CE:'R14=PASS R0 || R4=M6',
    0x12F7C6:'R4=0x4a', 0x12F7D9:'R4=0x4a', 0x12F7EC:'R4=0x4a',
    0x132DF2:'R14=PASS R2 || R8=M5',
    0x132DF5:'CJUMP 0x1257d0 (DB); R2=I6, I6=I7',
    0x1257DD:'DM(I4,+1)=R8', 0x12D3E4:'DM(I4,M6)=1',
}


def _sha(data):return hashlib.sha256(data).hexdigest()
def _words_bytes(words):return struct.pack(f'<{len(words)}I',*words)


def load_spectral_calibration(*,source,loader_path=LOADER,image_path=IMAGE):
    """Hash-validated retained header and band map from the two local images.

    Both images are checked even when selecting loader controls. This also
    checks whole3562-word CRC, not just the selected prefix. No car state claim.
    """
    if source not in ('loader','b01'):raise ValueError('explicit loader or b01 source required')
    loader=Path(loader_path).read_bytes();image=Path(image_path).read_bytes()
    if len(image)!=SOURCE_SIZE or _sha(image)!=SOURCE_SHA256:raise ValueError('wrong CodeFlash image')
    mem=load_shortwords(loader)
    for pc,wanted in PINS.items():
        if decode(mem,pc)[1]!=wanted:raise ValueError(f'spectral binding instruction changed:{pc:x}')
    dm=loader_dm(loader,parse_blocks(loader))
    if dm[0xB95E5]!=0xC3C6A or dm[0xB9097]!=0xB904C or dm[0xB9098]!=74:
        raise ValueError('source or retained band-map binding changed')
    full=([dm[0xC3C6A+i] for i in range(3562)] if source=='loader'
          else list(struct.unpack_from('<3562I',image,0x12000C)))
    table=[dm[0xB915A+i] for i in range(256)]
    if config_crc(full[:-1],table)!=full[-1]:raise ValueError('whole B source CRC mismatch')
    header=full[:245];edges=[dm[0xB904C+i] for i in range(75)]
    if edges[0]!=0 or edges[-1]!=513 or any(a>b for a,b in zip(edges,edges[1:])):
        raise ValueError('retained map does not cover513 bins')
    return {'source':source,'header_words':header,'edges':edges,
        'metadata':{'kind':'retained_common_spectral_header','source':source,
            'source_sha256':SOURCE_SHA256,'loader_sha256':BITREV_SHA256,
            'loader_path':str(Path(loader_path).resolve()),'image_path':str(Path(image_path).resolve()),
            'header_words':245,'header_sha256':_sha(_words_bytes(header)),
            'whole_source_crc':hex(full[-1]),'header_start':hex(0xC3C6A if source=='loader' else 0x12000C),
            'header_address_unit':'DM32 word' if source=='loader' else 'byte/file offset',
            'band_edges_address':'0xb904c','band_count':74,
            'band_edges_sha256':_sha(_words_bytes(edges)),
            'instruction_pins':{hex(k):v for k,v in PINS.items()},
            'actual_vehicle_state_proven':False,
            'header_scope':'common source prefix, independent of selected calibration bank1..4'}}


def bind_spectral_parameters(calibration,*,effective_config,pointers):
    """Bind controls; effective_config is explicit, not a target replay result.

    Serialized vectors retain firmware order, including level p14=MAX,p15=MIN.
    No parameter fitting, unit masks or uniform frequency bands are inserted.
    """
    h=calibration['header_words'];edges=calibration['edges'];meta=calibration['metadata']
    if len(h)!=245 or _sha(_words_bytes(h))!=meta['header_sha256']:
        raise ValueError('common header fingerprint mismatch')
    if len(edges)!=75 or _sha(_words_bytes(edges))!=meta['band_edges_sha256']:
        raise ValueError('band map fingerprint mismatch')
    cfg=np.asarray(effective_config,dtype=float)
    if cfg.shape!=(829,) or not np.isfinite(cfg).all():raise ValueError('finite explicit effective829-word config required')
    offset=pointers.get(1161)
    if type(offset) is not int or not 0<=offset<len(cfg):raise ValueError('explicit config-relative pointer1161 required')
    floats=np.frombuffer(_words_bytes(h),dtype='<f4').astype(float)
    if not np.isfinite(floats).all():raise ValueError('finite retained header required')
    pre=floats[44:50];level=floats[50:66];smooth=floats[66:69]
    main=floats[157:245];post=floats[69:157]
    flag=lambda value,rounded:bool(_unsigned_control(value,rounded=rounded))
    result={'edges':list(edges),'bins':513,'input_mix_gain':float(floats[43]),
        'main_params':main.tolist(),'post_params':post.tolist(),'level_params':level.tolist(),
        'pre_subtraction':dict(enabled=flag(pre[0],False),start=float(pre[1]),end=float(pre[2]),
            low_bin=_unsigned_control(pre[3],rounded=False),high_bin=_unsigned_control(pre[4],rounded=False),
            negative_gain=float(pre[5])),
        'crossfeed':float(cfg[offset]),
        'main_enabled':flag(main[0],True),'main_selector':_unsigned_control(main[1],rounded=True),
        'post_enabled':flag(post[0],True),'post_selector':_unsigned_control(post[1],rounded=True),
        'level_enabled':flag(level[0],True),'smoothing_enabled':flag(smooth[0],False),
        'smoother':dict(size=256,history_gain=float(smooth[1]),input_gain=float(smooth[2]))}
    result['metadata']={**copy.deepcopy(meta),
        'kind':'explicit_first_B_spectral_binding','effective_config_sha256_float64_le':_sha(cfg.astype('<f8').tobytes()),
        'crossfeed_pointer_cell':1161,'crossfeed_config_offset':offset,
        'crossfeed_object_offset':299+offset,
        'common_bindings':{'input_mix_gain':{'header':43,'object':97},
            'pre_subtraction':{'header':[44,50],'object':[98,104]},
            'level':{'header':[50,66],'object':[104,120]},
            'smoother':{'header':[66,69],'object':[120,123]},
            'post_transient':{'header':[69,157],'object':[123,211]},
            'main_transient':{'header':[157,245],'object':[211,299]}},
            'histories':'not contained in this parameter binding; builder requires explicit fresh zero-state choice',
        'constructor_histories':{'main_and_post_transient':'296 zero words each,12F0B8→132D56→1257D0',
            'level_sum_and_difference':'three74-word zero banks each,12F793→132D56→1257D0',
            'smoothers':'256 zero words and first-call flag1,12D3F6→12D3E0',
            'boundary':'successful fresh initialization only; retained spatial constructor handled separately'},
        'unknowns':['live common-header/indexed writes','active/effective config and pointer map',
            'live histories, ramps, allocation lifecycle and callback timing',
            'spatial binding is supplied separately; mathematical reference is not SHARC bit-exact']}
    return result


def build_spectral_reference(binding,*,spatial,spatial_enabled,zero_histories,
                             numeric_mode='host_math',fix_rounding=None):
    """Construct the retained nonspatial branches plus separately bound spatial.

    zero_histories=True is an explicit fresh-state scenario, not RAM readback.
    Smoothers keep their proved first-enabled-call priming behavior.
    """
    if zero_histories is not True:raise ValueError('explicit fresh zero-history scenario required')
    if type(spatial_enabled) is not bool:raise ValueError('explicit spatial enable required')
    if numeric_mode not in ('host_math','firmware_library'):
        raise ValueError('known explicit numerical reference required')
    if ((numeric_mode=='host_math' and fix_rounding is not None) or
            (numeric_mode=='firmware_library' and fix_rounding not in ('nearest','floor'))):
        raise ValueError('firmware library requires explicit nearest/floor FIX hypothesis only')
    common=dict(bins=513,edges=binding['edges'])
    def transient(key):
        return StereoTransientReference(**common,params=binding[key],history=np.zeros(296))
    def level():
        options={} if numeric_mode=='host_math' else dict(numeric_mode=numeric_mode,fix_rounding=fix_rounding)
        return BandLevelMaskReference(**common,params=binding['level_params'],
            power_history=np.zeros(74),db_history=np.zeros(74),gain_history=np.zeros(74),**options)
    switches={k:binding[k] for k in ('pre_subtraction','crossfeed','main_selector','post_selector',
        'main_enabled','post_enabled','level_enabled','smoothing_enabled')}
    tables=load_spectral_tables(loader=Path(binding['metadata']['loader_path']))
    fft_table=None
    if numeric_mode=='firmware_library':
        from amp_b_fft import load_complex_fft_table
        fft_table=load_complex_fft_table(loader=Path(binding['metadata']['loader_path']))
    result=BSpectralFrameReference(main_transient=transient('main_params'),post_transient=transient('post_params'),
        level_sum=level(),level_difference=level(),spatial=spatial,spatial_enabled=spatial_enabled,
        smooth_left=FrameVectorSmoother(**binding['smoother']),
        smooth_right=FrameVectorSmoother(**binding['smoother']),
        window=tables['window'],twiddles=tables['twiddles'],
        complex_twiddles=None if fft_table is None else fft_table['twiddles'],**switches)
    result.binding_metadata=copy.deepcopy(binding['metadata'])
    result.binding_metadata['fresh_zero_histories_assumed']=True
    result.binding_metadata['transform_tables']=copy.deepcopy(tables['metadata'])
    if fft_table is not None:
        result.binding_metadata['complex_fft']=copy.deepcopy(fft_table['metadata'])
        result.binding_metadata['level_math']=result.level_sum.math_reference.metadata()
        result.binding_metadata['transform_tables']['complex_fft_boundary']=fft_table['metadata']['boundary']
    return result
