"""Static API18-coordinate coefficient side paths and matrix orientation."""
import hashlib
import struct
from audit_amp_output_bindings import SOURCE_SHA256, SOURCE_SIZE, source_offset, gain_from_tenths_db
from inspect_amp_visa import decode

PROOFS=[
    ('coefficient_pair_consumer',0x12694C,0x1269BC,'0419ced0ac43bc29c642e97f2c219a59e91fe5778a41b54e46731d945171e66c'),
    ('coefficient_matrix_consumer',0x1269BC,0x126A95,'849b0de8635d87d794e6003c707c76491f78922cadcb17f19aae1172b904c305'),
]
RANGES={n:(a,b) for n,a,b,_ in PROOFS}


def matrix_control(data,dm,mem):
    if len(data)!=SOURCE_SIZE or hashlib.sha256(data).hexdigest()!=SOURCE_SHA256:
        raise ValueError('wrong CodeFlash')
    for name,a,b,digest in PROOFS:
        if hashlib.sha256(struct.pack(f'<{b-a}H',*[mem[p] for p in range(a,b)])).hexdigest()!=digest:
            raise ValueError(f'changed consumer {name}')
    pins={0x1269E2:'R1=DM(+5,I4)',0x1269E4:'R2=DM(+7,I4)',
          0x1269E6:'I3=DM(+9,I4)',0x1269F3:'R4=R1+R3',
          0x126A02:'R4=R7*R2 (SSI)',0x126A04:'R4+=R3',
          0x126A07:'R4=DM(M4,I3)',0x126A10:'M4=R7',
          0x126A3E:'R7=R7+1',0x126A5A:'R3=R3+1'}
    for pc,text in pins.items():
        if decode(mem,pc)[1]!=text:raise ValueError(f'changed matrix pin {pc:x}')
    for desc,shape,target in [(0xC4AED,(8,8),0xC022A),(0xC4B4F,(20,10),0xE0584)]:
        if (dm[desc+5],dm[desc+7],dm[desc+9])!=(*shape,target):
            raise ValueError('changed mixer descriptor')
    tables={}
    for name,rel in [('pair_odd',-0x12ACC),('matrix_input',-0x12AA6),('matrix_output',-0x12A80)]:
        a=source_offset(rel)
        tables[name]={'ram_relative':rel,'file_offset':hex(a),
                      'tenths_db':list(struct.unpack_from('<19h',data,a))}
    return {'source_sha256':SOURCE_SHA256,
        'scope':'calibrated source-copy scenario, explicit flags/state; not current vehicle settings',
        'tables':tables,
        'gain_lookup':{'min_tenths':-1000,'max_tenths':200,
            'address':'0x69398 - 4*tenths',
            'linear':[gain_from_tenths_db(data,i) for i in range(-1000,201)]},
        'pair_rows':{str(p):[list(struct.unpack_from('<20b',data,source_offset(-0x135F8,p)+20*i))
                            for i in range(5)] for p in (0,1)},
        'pair':{'writer':'AA644','target':'0xe0584','consumer':'0x12694c',
            'row':'level+1 iff gp-17771==A5 AND gp-17756==A5, else0',
            'state':'gp-16784:5A copies selected row;AA moves each signed current byte by1 toward row;FF copies even and mutes odd at-100; other values retain current',
            'ramp_completion':'AA changes to5A only on a call with no byte moved, not on the last moving call',
            'correction':'row>0 AND ASAD(gp-15905)==C0 AND odd index: add pair_odd[9+coordinate], then clamp -990..200',
            'otherwise':'10*current code WITHOUT that clamp; table -1000 is exact0',
            'formula':'out[j]=in[j]*gain[2*j]+in[10+j]*gain[2*j+1]',
            'zero_caveat':'pair_odd is all0, but entering correction branch changes stored -100 to -99dB via clamp'},
        'matrix':{'writer':'A903C','target':'0xc022a','consumer':'0x1269bc',
            'ram':'gp-15904,64 signed bytes',
            'writer_index':'loop j0..7,i0..7: RAM[8*i+j] -> SHARC[8*j+i]',
            'consumer_index':'output i,input j: SHARC[8*j+i]',
            'effective_orientation':'RAM rows OUTPUTS, columns INPUTS; effective gain[i,j] uses RAM[8*i+j]',
            'correction_priority':['if ASAD&15!=0 and j>=4: matrix_input[9+coordinate]',
                'else if ASAD==C0 and gp-17771==5A and i>=4: matrix_output[9+coordinate]',
                'else0'],
            'clamp':'every coefficient: -990..200 tenths after addition',
            'result':'zero coordinate leaves matrix correction0, NOT zero/identity matrix',
            'prior_model_error':'v5/v6 transposed RAM as if rows were inputs; output-labelled bass claims from those renders are superseded'},
        'consumer_proofs':[{'name':n,'start':hex(a),'end':hex(b),'sha256':h} for n,a,b,h in PROOFS],
        'consumer_pins':{hex(p):v for p,v in pins.items()},
        'remaining':['actual ASAD/mode/state and request delivery','full transition scheduling and concurrent transport',
                     'fallback calibration and indirect table overwrites','physical input/output labels remain separate proof'],
    }
