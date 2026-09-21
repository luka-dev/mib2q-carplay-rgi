"""Static B calibration-bank replacement and effective-control replay.

B bank1..4 here is NOT the input matrix stereo/5.1/7.1 bank0..2.
All data are local image contents, not a current vehicle snapshot.
"""
import hashlib
import struct
from inspect_amp_visa import decode

PROOFS=[
 ('first_b_descriptor_rebase',0x12AF83,0x12AF9A,'5f9e39e22f2fb53913d5b766d3ffb813c6889b8fce7278d350e1ec0362793cde'),
 ('first_b_all_descriptors_rebase',0x1320CE,0x132168,'38bf4abb19c3e429bb1adac8fc4380868b9ebfa15d766e2ecd6e3cc97cae0784'),
 ('first_b_effective_control_replay',0x132168,0x132346,'37a4bd410ee4d6d6393c64dc8aab9cadc3a2419ff44ce8022e15f80e6b878f54'),
 ('first_b_config_bank_replace',0x132346,0x1323DE,'919191009a45180683e12cd61fb8e98737bc32f4ec861cd29825e1a38ed89995'),
 ('first_b_config_crc',0x1328EA,0x13299B,'8cb8bf53a891648f7952353d0d4d034a1cfa7e081f04971d668500f92cbe4daa'),
 ('first_b_config_validate',0x13299B,0x1329FE,'e759f4686c92fcbc713bb9892c41e89fd28e5079166b97225b4196e8328b2eed'),
 ('first_b_mode_bank_validators',0x131E6D,0x131EC8,'348c17aa577b9bc5a7c2cb82105fb9b879f42d3d57745b02cda72fa6eb92e79d'),
]
RANGES={name:(a,b) for name,a,b,_ in PROOFS}


def config_crc(words,table):
    """1328EA: low-to-high bytes of each word, zero initial 16-bit CRC."""
    crc=0
    for word in words:
        for byte in struct.pack('<I',word):
            crc=((crc<<8)^table[((crc>>8)^byte)&255])&0xffff
    return crc


def config_control(data,dm,mem):
    from audit_amp_output_bindings import SOURCE_SHA256,SOURCE_SIZE
    from audit_amp_b_gain_controls import gain_controls,SLOTS
    if len(data)!=SOURCE_SIZE or hashlib.sha256(data).hexdigest()!=SOURCE_SHA256:
        raise ValueError('wrong CodeFlash')
    if dm[0xB95E5]!=0xC3C6A:raise ValueError('B source pointer changed')
    for name,a,b,sha in PROOFS:
        if hashlib.sha256(struct.pack(f'<{b-a}H',*[mem[p] for p in range(a,b)])).hexdigest()!=sha:
            raise ValueError(f'config proof changed: {name}')
    if dm[0xB913D+1]!=0x131E97 or dm[0xB914B+1]!=0x1324A6:
        raise ValueError('API17 dispatch changed')
    for pc,wanted in {0x1324A6:'R2=DM(+31,I3)',
        0x1324A8:'R2=PASS R2 || DM(-4,I3)=R8',
        0x1324B6:'CJUMP 0x132346 (DB); R2=I6, I6=I7'}.items():
        if decode(mem,pc)[1]!=wanted:raise ValueError('API17 applier changed')
    table=[dm[0xB915A+i] for i in range(256)]
    sources={}
    for source in ('loader','b01'):
        full=[dm[0xC3C6A+i] for i in range(3562)] if source=='loader' else list(struct.unpack_from('<3562I',data,0x12000C))
        if config_crc(full[:-1],table)!=full[-1]:raise ValueError('whole B image CRC mismatch')
        banks=[];first=full[245:1074]
        for bank in range(1,5):
            offset=245+829*(bank-1);words=full[offset:offset+829]
            if words[0]!=0x40128F5C or config_crc(words[:-1],table)!=words[-1]:
                raise ValueError('B config version/CRC mismatch')
            banks.append({'bank':bank,'words':words,'source_word_offset':offset,
                'loader_address':hex(0xC3C6A+offset) if source=='loader' else None,
                'b01_byte_offset':hex(0x12000C+4*offset) if source=='b01' else None,
                'sha256':hashlib.sha256(struct.pack('<829I',*words)).hexdigest(),
                'crc':hex(words[-1]),'different_offsets_from_bank1':[i for i,(x,y) in enumerate(zip(first,words)) if x!=y]})
        sources[source]={'whole_crc':hex(full[-1]),'banks':banks}
    return {'sources':sources,'crc_table':table,
        'control_bindings_by_bank':[gain_controls(data,dm,mem,config_bank=i) for i in range(1,5)],
        'api17':{'accepted':[1,2,3,4],'shadow_object_offset':1143,'effective_object_offset':1129,
            'defer_object_offset':1178,'source':'object1179 + (uint32-trunc(value)-1)*829',
            'sequence':['validate version and CRC before copying','copy829 words to object1180 target',
                'refresh descriptor44..58 bases from their targets; slots are NOT cleared',
                'replay effective controls19,18,22,21,23,24,25,27,29','publish via12FBA5','store effective selector1129'],
            'boundary':'sequence describes success; selector store follows replay call even if it returns an error; no rollback inferred'},
        'replay':{'helper':'0x132168','api_order':[s[1] for s in SLOTS],
            'effective_offsets':[s[6] for s in SLOTS],
            'api25':'raw effective1137, WITHOUT subtracting object1090; unlike single API25 applier13262D',
            'other_callers':['132489 after API16 mode helper','13255A after API20 mode helper'],
            'boundary':'mode pointer writes are bound separately in b_mode_control.json; full failure/queue lifecycle not modelled here'},
        'v850_selector':{'event':46,'writer':'0xaa468','api':17,'table_address':'0x100008',
            'table':[list(struct.unpack_from('<21b',data,0x100008+21*i)) for i in range(2)],
            'state_a_gp':-17770,'state_b_gp':-17810,
            'formula':'float(s8(table[state_a+21*state_b])+1)',
            'boundary':'for table-domain state_a0..20/state_b0..1: requests1/2 or invalid0; NOT matrix bank0/1/2; no delivered/live state proof'},
        'remaining':['generic API18/29 outer transport/UI semantics','extended command senders; indexed writes/source-RAM mutation bound in b_indexed_control.json',
            'full deferred queue/failure lifecycle beyond mode pointer writes','current vehicle configuration is not fixed by static source']}
