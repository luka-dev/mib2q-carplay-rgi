#!/usr/bin/env python3
"""Bounded heap-provenance closure for B's filter traversals, not dead fields.

Checks the retained loader, aligned full-B call census and constructor/consumer
instructions. Allocation domains are conditional on successful fresh setup,
preserved allocator/descriptor state and the enumerated direct caller ABI.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from audit_amp_b_mode_control import CENSUS_START, CENSUS_END, CENSUS_SHA256
from inspect_amp_visa import load_shortwords, decode
from extract_amp_sharc_app import parse_blocks
from audit_amp_output_bindings import loader_dm, SOURCE_SHA256, SOURCE_SIZE
from audit_amp_b_config_control import config_crc

LOADER = Path(__file__).resolve().parents[1]/'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'
IMAGE = Path(__file__).resolve().parents[3]/'Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin'
RANGES = [
    (0x12D844, 0x12D94C, 'c3a7203aac9df26c895d227ab67675a81544ce2da925a2e23ffd7e7ec7f13ab2'),
    (0x12D94C, 0x12D9A9, '0e8fc7abe809f4cb29e70b41ac2a938bc18618472e554e35a20d046ccbcd9b19'),
    (0x12FC78, 0x12FCE1, '2c54dd8a402c9bb4aca99e821b2b15e63160f61dbf5bc37ca62195d216287200'),
    (0x12DBDE, 0x12DCF7, '38ab6d8f27aa53269a5349e1d0de9368e1772345542e5b7455e31a53412b3ce6'),
    (0x132D56, 0x132E08, 'df3f1daeadbd18dc082601844941003a38238f828ba7e4ea03890cba9ed507bd'),
    (0x12DAA5, 0x12DBDE, 'ee2259f286b0315d04fe7d649da1ed0f16faa02a073ea8c4a992847aea87f627'),
]
PINS = {
    0x130F19: 'R8=0x49d', 0x130F1B: 'R12=0x2000', 0x130F1D: 'R15=PASS R0 || R4=M6',
    0x132D56: 'R2=R4*R8 (SSI) || R0=M5',
    0x132D5C: 'R2=R2+1 || DM(-4,I6)=R13',
    0x132D5F: 'R15=BCLR R2 BY R0 || DM(-2,I6)=R15',
    0x132D6C: 'R2=DM(0xb925a)', 0x132D6F: 'R1=0xb89b8',
    0x132D72: 'R8=R2+R15', 0x132D74: 'COMPU(R8,R1)',
    0x132D76: 'IF LT JUMP 0x132dec', 0x132DEC: 'DM(0xb925a)=R8',
    0x132DEF: 'R4=PASS R2 || R12=R15', 0x132DF2: 'R14=PASS R2 || R8=M5',
    0x132DFF: 'R1=R1+1 || R0=R14',
    0x12D887: 'DM(+2,I5)=R11', 0x12D889: 'DM(M6,I5)=R13',
    0x12D899: 'R2=PASS R0 || DM(+3,I5)=R0',
    0x12D8C0: 'DM(M3,I4)=R0', 0x12D8D8: 'R4=R1+R7 || R7=R2',
    0x12D928: 'DM(+4,I5)=R9', 0x12D92A: 'DM(M5,I4)=R10',
    0x12FCB8: 'DM(M5,I5)=R0', 0x12FC7C: 'DM(I4,M5)=0',
    0x12FC7E: 'DM(M6,I4)=I12',
    0x12D976: 'I4=DM(M6,I3)', 0x12D97A: 'I4=DM(M3,I4)',
    0x12D97C: 'R4=DM(M4,I4)',
    0x12DCDB: 'R12=PASS R13 || I4=DM(M3,I2)',
    0x12DCE5: 'R4=DM(M4,I4)',
    0x12B843: 'R12=0x4', 0x12B845: 'R8=0x8',
    0x12EB36: 'R12=0x8', 0x12EB38: 'R8=0x2',
    0x131011: 'R12=0x2', 0x131013: 'R8=0x6',
    0x131055: 'R12=0x2', 0x131057: 'R8=0x2',
    0x12B89E: 'DM(I7,M7)=8', 0x12B8B8: 'R4=DM(+43,I5)',
    0x12EC45: 'I12=0x2', 0x12EC47: 'R8=R13+R2 || DM(M6,I7)=I12',
    0x12EC4A: 'R12=PASS R11 || R4=DM(+3,I5)',
    0x131385: 'I12=0x6', 0x131387: 'R8=R11+R1 || DM(I7,M7)=I12',
    0x13138A: 'R4=DM(+34,I3)', 0x1313D6: 'R4=DM(+42,I3)',
    0x1313E1: 'I12=0x2', 0x1313E3: 'R8=R11+R2 || DM(I7,M7)=I12',
}
CALLS = {0x12D844: [0x12B849, 0x12EB3A, 0x131015, 0x131059],
         0x12D94C: [0x12DC4E],
         0x12DBDE: [0x12B8BA, 0x12EC4D, 0x13138C, 0x1313E6],
         0x12FC86: [0x12D8DB],
         0x12D10E: [0x12A8BD,0x12A8E3,0x12A909,0x12A92F,0x13303C,0x13308A],
         0x12AD70: [0x12AE54]}
DOMAINS = [
    {'constructor': '0x12b849', 'consumer': '0x12b8ba', 'slot': 'B+43', 'channels': 8, 'capacity': 4},
    {'constructor': '0x12eb3a', 'consumer': '0x12ec4d', 'slot': 'pair heap object+3', 'channels': 2, 'capacity': 8},
    {'constructor': '0x131015', 'consumer': '0x13138c', 'slot': 'B+34', 'channels': 6, 'capacity': 2},
    {'constructor': '0x131059', 'consumer': '0x1313e6', 'slot': 'B+42', 'channels': 2, 'capacity': 2},
]
CASCADE_FIELDS = [(335,8,'pair1'),(379,8,'pair2'),(447,8,'pair3'),
                  (498,8,'pair4'),(549,8,'pair5'),(618,8,'pair6'),
                  (593,2,'six_channel'),(677,2,'two_channel'),(698,4,'eight_channel')]


def cascade_domain(count, capacity, count_field):
    """Retained integral counts only; no arbitrary-float rounding claim."""
    if not isinstance(count, (int,float)) or not count == int(count) or not 0 <= count <= capacity <= 8:
        raise ValueError('unsupported retained count/capacity')
    count=int(count)
    return {'count':count,'capacity':capacity,'count_object_offset':count_field,
        'coefficient_object_offsets':list(range(count_field+1,count_field+1+5*count)),
        'state_pointer_indices':list(range(count)),
        'paired_iterations':count//2,'odd_single_section':bool(count%2),
        'coefficient_scratch_words_used':5*count,'coefficient_scratch_capacity':40,
        'state_words_read_and_written':2*count}


def retained_cascade_domains(loader, image):
    """Extract all eight real banks afresh, not trusting a JSON bank hash."""
    load_shortwords(loader)
    if len(image)!=SOURCE_SIZE or hashlib.sha256(image).hexdigest()!=SOURCE_SHA256:
        raise ValueError('wrong CodeFlash image')
    dm=loader_dm(loader,parse_blocks(loader))
    if dm[0xB95E5]!=0xC3C6A:raise ValueError('source pointer changed')
    crc_table=[dm[0xB915A+i] for i in range(256)]
    scenarios=[]
    for source in ('loader','b01'):
        words=([dm[0xC3C6A+i] for i in range(3562)] if source=='loader'
               else list(struct.unpack_from('<3562I',image,0x12000C)))
        if config_crc(words[:-1],crc_table)!=words[-1]:raise ValueError('whole source CRC changed')
        for bank in range(1,5):
            start=245+(bank-1)*829;raw_words=words[start:start+829]
            if config_crc(raw_words[:-1],crc_table)!=raw_words[-1]:raise ValueError('bank CRC changed')
            raw=struct.pack('<829I',*raw_words);values=struct.unpack('<829f',raw)
            rows=[cascade_domain(values[field-299],cap,field)|{'owner':name}
                  for field,cap,name in CASCADE_FIELDS]
            all_offsets={offset for row in rows for offset in row['coefficient_object_offsets']}
            if all_offsets & set(range(1162,1166)):raise ValueError('coefficient domain reaches pointer cells')
            scenarios.append({'source':source,'bank':bank,'bank_sha256':hashlib.sha256(raw).hexdigest(),
                'rows':rows,'coefficient_object_offset_min_max':[min(all_offsets),max(all_offsets)],
                'coefficient_pointer_cell_collisions':[]})
    return {'source_sha256':SOURCE_SHA256,'scenarios':scenarios,
        'scope':'all retained bank1..4 for loader/B01; includes all fixed tail descriptors as a conservative superset of mode5',
        'conditional_read_sites':['0x12db1a','0x12db1d','0x12db20','0x12db23','0x12db29',
            '0x12db3b','0x12db44','0x12db4b','0x12db4f','0x12db51','0x12db57',
            '0x12dbb5','0x12dbbb','0x12dbbf','0x12dbc1'],
        'conditional_write_sites':['0x12db6a','0x12db73','0x12db79','0x12db82','0x12dbd3','0x12dbdc'],
        'assumptions':['previous constructor/heap proof assumptions',
            'counts are retained unmutated integral values; no API/indexed count overwrite',
            'fixed descriptor/caller bank bases from existing tail and smoothing bindings',
            'audio-kernel internals and arbitrary external callers are outside this address-domain proof'],
        'dead_field_proven':False,'fixed_mode5_complete_alias_proof':False}


def layout(channels, capacity, *, object_base=100000):
    """Allocation-domain witness, not an assertion of actual firmware addresses.

Fresh successful type0x2000 bump allocation; ignored intervening allocations
could only move these blocks further forward. No state reconstruction here.
"""
    if type(channels) is not int or type(capacity) is not int or not 1 <= channels <= 8 or not 1 <= capacity <= 8:
        raise ValueError('outside retained constructor domain')
    cursor = object_base+1182
    memory = {}; allocations = []
    def allocate(words, kind):
        nonlocal cursor
        start = cursor; cursor += (words+1)&~1
        allocations.append({'start': start, 'end': cursor, 'kind': kind})
        memory.update({p: 0 for p in range(start, cursor)})
        return start
    descriptor = allocate(5, 'descriptor')
    array = allocate(channels, 'channel pointers')
    memory.update({descriptor+1: channels, descriptor+2: capacity, descriptor+3: array})
    for channel in range(channels):
        section_array = allocate(capacity, 'section pointers')
        memory[array+channel] = section_array
        for section in range(capacity):
            memory[section_array+section] = allocate(2, 'biquad state')
    return {'descriptor': descriptor, 'memory': memory, 'allocations': allocations,
            'parent_interval': [object_base, object_base+1182]}


def audit(data, image=None):
    mem = load_shortwords(data)  # pins whole source, not merely selected instructions
    for start, end, wanted in RANGES+[(CENSUS_START, CENSUS_END, CENSUS_SHA256)]:
        raw = struct.pack(f'<{end-start}H', *(mem[p] for p in range(start, end)))
        if hashlib.sha256(raw).hexdigest() != wanted: raise ValueError(f'code range changed: {start:x}')
    for pc, wanted in PINS.items():
        if decode(mem, pc)[1] != wanted: raise ValueError(f'instruction changed: {pc:x}')
    calls = {target: [] for target in CALLS}; pc = CENSUS_START; count = 0
    while pc < CENSUS_END:
        size, text, target = decode(mem, pc)
        if 'UNKNOWN' in text: raise ValueError(f'unknown at {pc:x}')
        if text.startswith(('CJUMP ', 'CALL ')) and target in calls: calls[target].append(pc)
        pc += size; count += 1
    if pc != CENSUS_END or calls != CALLS: raise ValueError('aligned direct-caller domain changed')
    domains = []
    for domain in DOMAINS:
        witness = layout(domain['channels'], domain['capacity'])
        parent = set(range(*witness['parent_interval']))
        assert not parent.intersection(witness['memory'])
        domains.append(domain | {'caller_channels_equal_capacity': True,
            'heap_words_excluding_parent': sum(a['end']-a['start'] for a in witness['allocations']),
            'reset_state_count': domain['channels']*domain['capacity']})
    result = {'schema': 2, 'loader_sha256': hashlib.sha256(data).hexdigest(),
        'architecture': 'SHARC VISA, program addresses in 16-bit shortwords; DM in 32-bit words',
        'code_span': [hex(CENSUS_START), hex(CENSUS_END)], 'code_span_sha256': CENSUS_SHA256,
        'decoded_instructions': count,
        'pinned_ranges': [{'start': hex(a), 'end': hex(b), 'sha256': h} for a,b,h in RANGES],
        'pins': {hex(p): t for p,t in PINS.items()},
        'direct_callers': {hex(t): [hex(p) for p in v] for t,v in calls.items()},
        'descriptor_layout': {'0': 'diagnostic context', '1': 'channel capacity',
            '2': 'section capacity', '3': 'allocated channel pointer array', '4': 'external lifecycle flag pointer'},
        'allocation': {'parent_requested_words': 1181, 'parent_reserved_words': 1182,
            'kind': '0x2000', 'cursor_address': '0xb925a', 'exclusive_limit': '0xb89b8',
            'rounding': '(requested_words+1)&~1', 'failure_or_wraparound_excluded': True},
        'caller_domains': domains,
        'conditionally_closed_reads': ['0x12d97a', '0x12d97c', '0x12dcdb', '0x12dce5'],
        'conclusion': 'These bounded filter pointer traversals read owned heap arrays, not B object pointer cells1162..1165.',
        'assumptions': ['successful fresh construction with nonwrapping type0x2000 bump allocator',
            'parent B allocation predates filter allocations; cursor not reset/reused',
            'unchanged descriptors, owned arrays and fixed direct caller ABI',
            'normal DAG linear-address mode; no corrupt/externally supplied descriptor'],
        'unproved': ['whole-image indirect/external caller graph or aliases across all B control-flow joins',
            'arbitrary generic writes, descriptor mutation, heap corruption or allocator reinitialization',
            'other consumers of1162..1165; target/shadow copies do not establish an audio consumer',
            '12DAA5 cascade safety for arbitrary mutated section counts', 'actual live mode5 or live RAM'],
        'unresolved_pointer_consumers': [1162,1163,1164,1165], 'dead_field_proven': False,
        'remaining_typed_candidates': [
            {'read_sites':['0x12d182','0x12d184','0x12d191'],
             'function':'0x12d10e','type':'nested-allpass ring histories, not immediate parameter offsets',
             'base_origins':['0x12d16a: I2=DM(+1,descriptor)','0x12d177: I4=DM(+4,descriptor)'],
             'indices':'R2/R1 from descriptor+2/+5, reset when beyond requested lengths; each sample wraps at requested length',
             'capacity_checks':['0x12d157','0x12d15e'],
             'closure':False,'missing':'constructor allocation provenance and retained requested-length/capacity domains for all six direct callers'},
            {'read_sites':['0x12adc6'],'function':'0x12ad70',
             'type':'spectral ramp output-vector previous sample',
             'base_origins':['0x12ad8b: R11=DM(+3,I6)','0x12adbd..12adbf: I4=R11+adjusted_start-1'],
             'bounds':'guarded start<=end<=length-1; start0 becomes1; read only if adjusted_start<end',
             'closure':False,'missing':'caller12AE54 output-vector allocation/capacity provenance; generic invalid-input branch does not establish termination'}],
        'fixed_mode5_complete_alias_proof': False}
    if image is not None:result['retained_cascade_domains']=retained_cascade_domains(data,image)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--loader', type=Path, default=LOADER)
    parser.add_argument('--image', type=Path, default=IMAGE)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.loader.read_bytes(),args.image.read_bytes())
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({'conditionally_closed_reads': result['conditionally_closed_reads'], 'dead_field_proven': False}))
