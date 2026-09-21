"""Static descriptor46/50/51 nine-slot bindings, not an actual-car preset."""
import hashlib
import re
import struct
from inspect_amp_visa import decode, listing

# slot, API, function start/end, validator, pending applier, effective/shadow
SLOTS=[
    (0,19,0x12BF21,0x12C4EF,0x131EE4,0x132501,1131,1145),
    (1,18,0x12C4EF,0x12C9B7,0x131EC8,0x1324D8,1130,1144),
    (2,22,0x12C9B7,0x12CB59,0x131F46,0x1325B2,1134,1148),
    (3,21,0x12CB59,0x12CCB9,0x131F2A,0x132589,1133,1147),
    (4,23,0x12CCB9,0x12CD95,0x131F62,0x1325DB,1135,1149),
    (5,24,0x12CD95,0x12CE71,0x131F7E,0x132604,1136,1150),
    (6,25,0x12CE71,0x12CF4D,0x131F9A,0x13262D,1137,1151),
    (7,27,0x12CF4D,0x12D029,0x131FD9,0x132667,1139,1153),
    (8,29,0x12D029,0x12D105,0x132018,0x13269E,1141,1155),
]
RANGES={f'first_b_gain_slot{slot}':(start,end) for slot,_,start,end,*_ in SLOTS if slot!=6}
RANGES['first_b_gain_validators']=(0x131EC8,0x132034)
EXPECTED=[{46:[515,560],50:[527,572],51:[530,575]},
          {46:[605,641],50:[617,653]},
          {46:[671,689],50:[680,698],51:[683,701]},
          {46:[707,722],50:[713,728],51:[716,731]},
          {46:[737,746],50:[740,749],51:[743,752]},
          {46:[755,764],50:[758,767],51:[761,770]},
          {46:[773,782],50:[776,785],51:[779,788]},
          {46:[792,801],50:[795,804],51:[798,807]},
          {46:[810,819],50:[813,822],51:[816,825]}]


def curve_arguments(mem,start,end,selected=(46,50,51)):
    """Bounded pointer-construction recovery, not full-function execution.

    Track target-relative pointers stored on the stack and the local two-record
    arrays passed to12B00F. Default emits invariant descriptor46/50/51 calls.
    Selecting52/53 gives ONLY the initial mode5 path, not alternate branches.
    """
    pointers={};stack={};cursor=None;descriptor=None;slot=None;out=[];all_descriptors=[]
    pc=start
    while pc<end:
        n,text,target=decode(mem,pc)
        for op in text.split(' || '):
            m=re.fullmatch(r'(I\d+)=DM\(\+1180,I[25]\)',op)
            if m:pointers[m[1]]=0
            m=re.fullmatch(r'(I\d+)=MODIFY\((I\d+),([+-]\d+)\)',op)
            if m:
                if m[2] in pointers:pointers[m[1]]=pointers[m[2]]+int(m[3])
                if m[1]=='I4' and m[2]=='I6':cursor=int(m[3])
            m=re.fullmatch(r'DM\((-\d+),I6\)=(I\d+)',op)
            if m and m[2] in pointers:stack[int(m[1])]=pointers[m[2]]
            m=re.fullmatch(r'R4=DM\(\+(\d+),I[25]\)',op)
            if m:descriptor=int(m[1])
            m=re.fullmatch(r'R8=(M[56]|0x[0-9a-f]+)',op)
            if m:slot={'M5':0,'M6':1}.get(m[1],int(m[1],16) if m[1].startswith('0x') else None)
        if target==0x12B00F:
            all_descriptors.append(descriptor)
            if descriptor in selected:
                out.append({'descriptor':descriptor,'slot':slot,'record_offsets':[stack[cursor],stack[cursor+1]],'call':hex(pc)})
        pc+=n
    if pc!=end:raise ValueError('curve setter end inside instruction')
    return out,all_descriptors


def gain_controls(data,dm,mem,config_bank=1):
    from audit_amp_output_bindings import SOURCE_SHA256,SOURCE_SIZE
    if len(data)!=SOURCE_SIZE or hashlib.sha256(data).hexdigest()!=SOURCE_SHA256:
        raise ValueError('wrong CodeFlash')
    if dm[0xB95E5]!=0xC3C6A:raise ValueError('source pointer changed')
    if type(config_bank) is not int or config_bank not in (1,2,3,4):
        raise ValueError('explicit B configuration bank1..4 required')
    bank_delta=829*(config_bank-1)
    def read(offset):
        address=0xC3D5F+bank_delta+offset
        lw=dm[address];a=0x1203E0+4*(bank_delta+offset);bw=struct.unpack_from('<I',data,a)[0]
        f=lambda w:struct.unpack('<f',struct.pack('<I',w))[0]
        return {'offset':offset,'loader_address':hex(address),'b01_file_offset':hex(a),
                'loader_word':hex(lw),'b01_word':hex(bw),'loader':f(lw),'b01':f(bw)}
    rows=[]
    for slot,api,start,end,validator,applier,effective,shadow in SLOTS:
        if (dm[0xB913D+api-16],dm[0xB914B+api-16])!=(validator,applier):
            raise ValueError('API jump table changed')
        calls,descriptors=curve_arguments(mem,start,end)
        if {v['descriptor']:v['record_offsets'] for v in calls}!=EXPECTED[slot] or any(v['slot']!=slot for v in calls):
            raise ValueError(f'curve arguments changed for slot{slot}')
        if decode(mem,validator+4)[1]!=f'DM(I7,M7)={api}':raise ValueError('validator ID changed')
        hi=0x43C80000 if api==27 else 0x43480000
        if decode(mem,validator+8)[1]!=f'R12={hi:#x}' or decode(mem,validator+11)[1]!='R8=0xc3480000':
            raise ValueError('validator bounds changed')
        apply_text='\n'.join(listing(mem,applier,applier+(44 if api==25 else 41)))
        if f'CJUMP {start:#x}' not in apply_text:raise ValueError('applier helper changed')
        def address(offset):
            delta=offset-1147
            return {0:'DM(I3,M5)',1:'DM(M6,I3)',-1:'DM(M7,I3)'}.get(delta,f'DM({delta:+d},I3)')
        if any(address(offset)+'=R8' not in apply_text for offset in (effective,shadow)):
            raise ValueError('applier state offset changed')
        if 'R2=DM(+31,I3)' not in apply_text:raise ValueError('defer gate changed')
        for v in calls:v['records']=[[read(i) for i in range(offset,offset+3)] for offset in v['record_offsets']]
        rows.append({'slot':slot,'api':api,'helper':hex(start),'validator':hex(validator),
                     'applier':hex(applier),'accepted_range':[-200,400 if api==27 else 200],
                     'effective_object_offset':effective,'shadow_object_offset':shadow,
                     'coordinate':'value - effective config791' if api==25 else 'value',
                     'branches':calls,'all_helper_descriptors':descriptors,
                     'range_sha256':hashlib.sha256(struct.pack(f'<{end-start}H',*[mem[p] for p in range(start,end)])).hexdigest()})
    events={event:hex(0xAA60E+2*struct.unpack_from('<h',data,0xAA60E+2*(event-45))[0]) for event in range(45,54)}
    if events!={45:'0xaa4a4',46:'0xaa468',47:'0xaa39a',48:'0xaa3c4',49:'0xaa4ce',50:'0xaa522',51:'0xaa546',52:'0xaa568',53:'0xaa59a'}:
        raise ValueError('V850 event routing changed')
    from audit_amp_b_parameter_controls import parameter_controls
    return {'source_sha256':SOURCE_SHA256,'scope':'three branch gains plus API18/19 parameter targets; no physical-speaker/UI/live-state inference',
            'config_bank':config_bank,
            'parameter_controls':parameter_controls(mem,read),
            'slots':rows,'bases':{str(d):read(o) for d,o in [(46,122),(50,293),(51,377)]},
            'untouched':{'51':[1]},'event_targets':{str(k):v for k,v in events.items()},
            'v850_sources':{'19':'event47: float table6EA68[u8(gp-16783)]',
                '21':'event49: float(s8(gp-15758+u8(gp-15740)))',
                '22':'event49: float(s8(gp-15739+u8(gp-15740)))',
                '23':'event50: float(s8(gp-15721))','24':'event51: float(s8(gp-15719))',
                '25':'event52: max(0,s8(gp-15718)+100)',
                '27':'event53: float(min(400,u32(gp-17832)//100))',
                '18':'events22/368: AA886 sends float(s8(gp-15840)); CA4B4 selector5/0 accepts -12..12 and clamps to -9..9; see b_outer_control.json',
                '29':'no dedicated writer bound; generic proxy A782E reads ID gp-15936/value gp-15932; D40C8/D4130 stage two request words and queue369'},
            'api19_table':list(struct.unpack_from('<4f',data,0x6EA68)),
            'boundary':'shadow writes precede defer1178 check; direct scalar changes only if not deferred; full helpers also change other DSP parameters. Request semantics/order, later config replacement and indirect writers remain open'}
