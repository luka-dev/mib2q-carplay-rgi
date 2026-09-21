"""API16/20 pointer rebinding, not an inferred live B configuration."""
import hashlib
import re
import struct
from inspect_amp_visa import decode

PROOFS=[
 ('first_b_mode_spatial_pointers',0x12B1E7,0x12B232,'b9907d83d4ae097c030bd47ccb51c9197ee5e720ec605ed1e9c2ad51fdb72667'),
 ('first_b_mode_pair_gain_pointers',0x12B232,0x12B26D,'46c80723d6c6e16b97e40fa28175df4e0d8f5d97206ac049b758a50a5e197142'),
 ('first_b_mode_pointer_switch',0x12B348,0x12B48F,'0939e249c9b09102d10e9b4366eb34c41d88ac478d6c43f0fb22725d9c2a6bd1'),
 ('first_b_mode_pointer_init',0x12B48F,0x12B4D0,'fccd7ac8c8f7a64a880f023de11e8e22d8857f7e3cc7ddc8cb81a0b08ec253f3'),
]
RANGES={name:(a,b) for name,a,b,_ in PROOFS}
CALLER_PROOFS=[
 (0x131E6D,0x131E97,'c73bb7692f4a36eee407e0c63ddd0672c922449060cafda626a0d64400e7ffa3'),
 (0x131F00,0x131F2A,'3491af0b2ba5d04b05ff851bcc62259f4cb84c84a96d8122d833518f1cbae8a8'),
 (0x132402,0x132434,'01e78814e81636349bf83646dd179d2d38d85da72a130bbc6119555e17154633'),
 (0x132434,0x1324A6,'da95535fa48982cec44c6ef89dd937246d2f12de3b25817be75a4bdc03f636af'),
 (0x13252A,0x132589,'83e1e1b3a0d9fb36d963ac14dc2ff40402ae025e5aac5c2aebbd7d5da2a9a0fa'),
]
COMMON_DEFAULT=[121,292,376,124,315,31,533,536,578,581]
COMMON16=[488,489,490,491,492,493,494,497,500,503]
COMMON20=[421,422,423,424,425,426,427,430,433,436]
SPATIAL_DEFAULT=[3,125,130,135,140]
SPATIAL20=[439,467,472,477,482]
PAIR_DEFAULT=[195,246,291]
PAIR16=[506,507,508]
CENSUS_START=0x12A7F9
CENSUS_END=0x1331AF
CENSUS_SHA256='a30e9c631734d5846aa97ac2ac4c50dacc1769b5284c282960363070880552d4'


def pointer_literal_census(mem):
    """Aligned literal-use census, NOT alias analysis or proof of dead fields.

    Include full-object and core(+59)-relative pointer table offsets. A match
    has no base provenance until a separate caller-chain proof establishes it.
    """
    pc=CENSUS_START;count=0;reads=[];constants=[];read_offsets=set()
    offsets=set(range(1156,1174))|set(range(1097,1115))
    while pc<CENSUS_END:
        n,insn,_=decode(mem,pc)
        if n<=0 or 'UNKNOWN' in insn:raise ValueError(f'incomplete B literal census at {pc:x}')
        matches=re.findall(r'=DM\(\+(\d+),I\d+\)|=DM\(I\d+,\+(\d+)\)',insn)
        found={int(value) for pair in matches for value in pair if value}&offsets
        read_offsets.update(found)
        if found:
            reads.append({'pc':hex(pc),'instruction':insn})
        if any(int(v,0) in offsets for v in re.findall(r'=(0x[0-9a-f]+)\b',insn)):
            constants.append({'pc':hex(pc),'instruction':insn})
        count+=1;pc+=n
    if pc!=CENSUS_END:raise ValueError('unaligned B literal census end')
    return {'start':hex(CENSUS_START),'end':hex(CENSUS_END),'instructions':count,
        'unknown':0,'immediate_offset_reads':reads,'address_literal_candidates':constants,
        'not_found_offsets':sorted(offsets-read_offsets),
        'boundary':'literal census only: dynamic addressing, other base aliases, external writers and code outside this span are not excluded. No dead-field/no-audio-effect claim.'}


def pointer_maps():
    def mapping(common,suffix,start):
        return {str(1156+i):v for i,v in enumerate(common)} | {str(start+i):v for i,v in enumerate(suffix)}
    return {'16:5':mapping(COMMON16,PAIR16,1171),
        '16:default':mapping(COMMON_DEFAULT,PAIR_DEFAULT,1171),
        '20:3':mapping(COMMON20,SPATIAL20,1166),
        '20:default':mapping(COMMON_DEFAULT,SPATIAL_DEFAULT,1166)}


def mode_control(data,dm,mem):
    from audit_amp_output_bindings import SOURCE_SHA256,SOURCE_SIZE
    if len(data)!=SOURCE_SIZE or hashlib.sha256(data).hexdigest()!=SOURCE_SHA256:
        raise ValueError('wrong CodeFlash')
    for a,b,wanted in [(a,b,h) for _,a,b,h in PROOFS]+CALLER_PROOFS+[(CENSUS_START,CENSUS_END,CENSUS_SHA256)]:
        actual=hashlib.sha256(struct.pack(f'<{b-a}H',*[mem[p] for p in range(a,b)])).hexdigest()
        if actual!=wanted:raise ValueError(f'mode binding changed at {a:x}')
    for api,validator,applier in [(16,0x131E6D,0x132434),(20,0x131F00,0x13252A)]:
        if (dm[0xB913D+api-16],dm[0xB914B+api-16])!=(validator,applier):
            raise ValueError('mode dispatch table changed')
    consumers={1156:(0x12EEEB,'R4=DM(+1156,I5)','four_tail_final_gain',1),
        1157:(0x12AAED,'R4=DM(+1157,I5)','six_tail_final_gain',1),
        1158:(0x1330D2,'R4=DM(+1158,I5)','two_tail_final_gain',1),
        1159:(0x12A879,'R8=DM(+1159,I5)','six_common_delay',1),
        1160:(0x132FD2,'R8=DM(+1160,I5)','two_common_delay',1),
        1161:(0x12E683,'R4=DM(+1102,I5)','post_crossfeed',1),
        1166:(0x12E6A7,'R8=DM(+1166,I3)','spatial_parameters',28),
        1167:(0x12A8BA,'R8=DM(+1167,I5)','six_nested_L1',5),
        1168:(0x12A8E0,'R8=DM(+1168,I5)','six_nested_L2',5),
        1169:(0x12A906,'R8=DM(+1169,I5)','six_nested_R1',5),
        1170:(0x12A92C,'R8=DM(+1170,I5)','six_nested_R2',5),
        1171:(0x12A9FD,'R4=DM(+1171,I5)','six_pair1_gain',1),
        1172:(0x12AA92,'R4=DM(+1172,I5)','six_pair2_gain',1),
        1173:(0x12AADA,'R4=DM(+1173,I5)','six_pair3_gain',1)}
    for pc,wanted,_,_ in consumers.values():
        if decode(mem,pc)[1]!=wanted:raise ValueError('mode pointer consumer changed')
    widths={p:v[3] for p,v in consumers.items()} | {1162:3,1163:3,1164:3,1165:3}
    choices=pointer_maps();sources={}
    for source in ('loader','b01'):
        banks=[]
        for bank in range(1,5):
            delta=829*(bank-1)
            def read(offset):
                word=dm[0xC3D5F+delta+offset] if source=='loader' else struct.unpack_from('<I',data,0x1203E0+4*(delta+offset))[0]
                return struct.unpack('<f',struct.pack('<I',word))[0]
            banks.append({'bank':bank,'config487':read(487),
                'choices':{name:{p:[read(o+i) for i in range(widths[int(p)])] for p,o in offsets.items()}
                           for name,offsets in choices.items()}})
        sources[source]=banks
    return {'pointer_maps':choices,'pointer_object_range':[1156,1173],
        'config_base_object_offset':299,'sources':sources,
        'apis':{'16':{'accepted':[2,5,6],'shadow':1142,'effective':1128,'writes':[1156,1165,1171,1173]},
                '20':{'accepted':[1,3,6],'shadow':1146,'effective':1132,'writes':[1156,1170]}},
        'api20_auto':{'request':6,'reads_effective_object_offset':786,'config_offset':487,
            'formula':'effective config487 == 1.0 ? 1.0 : 3.0',
            'ordering':'resolve before shadow write and defer1178 check; shadow/effective store resolved1/3, not6'},
        'consumers':{str(p):{'pc':hex(v[0]),'role':v[2],'words':v[3]} for p,v in consumers.items()},
        'unbound_pointer_consumers':[1162,1163,1164,1165],
        'unbound_record_width_boundary':'three-word previews for1162..1165 follow pointer spacing only; not a proven record type or audio consumer',
        'pointer_literal_census':pointer_literal_census(mem),
        'post_crossfeed':{'pointer':1161,'config_offsets':{'default':31,'16:5':493,'20:3':426},
            'base_proof':['1315AE/B0 passes object+59 inR8 to12E80E',
                '12E841 savesR8 atstack-2;12EA39/3B forwards it asR8 to12E521',
                '12E556 savesR8 inR3;12E672 setsI5=R3;12E683 reads1102+59=1161',
                '12E686 calls12E487;12E48E reads scalar throughR4'],
            'formula':['Lout=(1-c)*L+c*R','Rout=(1-c)*R+c*L'],
            'invariants':['sum unchanged','difference multiplied by1-2*c','no amount clamp'],
            'scope':'post complex spectra before130771 spatial stage; direct effective coefficient, not complete B response'},
        'initialization':'12B48F calls20(value1) then16(value2); all18 pointers get default mappings',
        'ordering':'each nondeferred call overwrites common1156..1165; retains the other API suffix, then replays effective controls; shared pointers are last-applied-wins',
        'pending_loop_order':'132402 processes pending APIs16..29 ascending; if16 and20 are pending in the same pass,16 precedes20. Sender order does not establish queue/pass timing.',
        'v850_event48':'AA3C4: gp-15760==0 sends16 then20; otherwise20 then wait-helper DCE98(15) then16; transport timing not proved',
        'boundary':'pointer writes into EFFECTIVE config, not coefficient rewrites; image values require explicit source/bank and are not a live/smoothed snapshot. No full queue/publication/audio emulator.'}
