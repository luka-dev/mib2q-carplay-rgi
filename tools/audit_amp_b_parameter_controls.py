"""Image-bound API18/19 scalar targets and four EQ rebuild call sites.

Offsets are 32-bit words relative to config base, PCs are shortword addresses.
These bindings are not an actual-car preset or complete coefficient designer.
"""
import hashlib
import struct

RANGES={'first_b_eq_identity':(0x12AB0A,0x12AB1F),
        'first_b_eq_design':(0x12AB1F,0x12AD70)}
PINS=[
    (0x12BF21,0x12C4EF,'f92217993f23612d061af2c029459cc59701a074094562d1aa1b24285dfef77e'),
    (0x12C4EF,0x12C9B7,'68972861cc958159dc940000791e45930ab5b58cc7a8ec28a855f65989842ade'),
    (0x12BC52,0x12BF21,'eeac4d6df5bc8b95d98d57690c8d4b15356301428093610eb7b94b93818f90ac'),
    (0x12AB0A,0x12AD70,'ddb418950c281e25fbd759923641e253b58582764d56887f89c96b40b19b9ea4'),
    (0x131383,0x131393,'1a946e41eb1292f2d3b8da986ca6bc01d248c96a2a6a35b7af3443c07411ceec'),
    (0x1313D6,0x1313ED,'f76146f9637442da8ad76414adbbc588d01f17ce23f1def14d25a823df407d01'),
    (0x133009,0x13301C,'b988303e817b074ae206d5453d7d839d339a3e708e7c327e6c898fad3c74a441'),
    (0x12EB9B,0x12EC0B,'2ed96a3561325ad85c17ca59711e7a03889b9e19054a5278c96226d5056431ef'),
    (0x12F441,0x12F477,'e769191c11c72a5dc17cb41123c07cd52bf30cf392e0f81506fce86b81872f7a'),
]
TARGETS=dict(zip(range(44,59),[35,79,122,147,198,249,293,377,316,317,420,307,312,391,396]))


def parameter_controls(mem,read):
    from audit_amp_b_gain_controls import curve_arguments,SLOTS
    for a,b,wanted in PINS:
        actual=hashlib.sha256(struct.pack(f'<{b-a}H',*[mem[p] for p in range(a,b)])).hexdigest()
        if actual!=wanted:raise ValueError(f'API18/19 binding code changed at {a:x}')
    rows=[]
    for slot,api,start,end,*_ in SLOTS[:2]:
        calls,descriptors=curve_arguments(mem,start,end,selected=TARGETS)
        expected=list(range(44,59)) if api==19 else [*range(44,51),*range(54,59)]
        if descriptors!=expected:raise ValueError('API parameter call order changed')
        for i,row in enumerate(calls):
            d=row['descriptor'];offset=TARGETS[d]
            primary=[509+3*(d-44),554+3*(d-44)] if api==19 else [599+3*i,635+3*i]
            if api==19 and d in (52,53):primary=[494+3*(d-52),500+3*(d-52)]
            if row['record_offsets']!=primary:raise ValueError('API parameter curve records changed')
            variants={'default':primary}
            if api==19 and d in (52,53):
                variants={'shadow16_eq5':primary,
                    'shadow20_eq3':[427+3*(d-52),433+3*(d-52)],
                    'default':[533+3*(d-52),578+3*(d-52)]}
            row.update(target_config_offset=offset,effective_object_offset=299+offset,
                base=read(offset),clamp=[-100,20] if d>=55 else [0,288 if d in (52,53) else 5],
                variants={name:{'record_offsets':pair,
                    'records':[[read(j) for j in range(o,o+3)] for o in pair]}
                    for name,pair in variants.items()})
            del row['record_offsets']
        rows.append({'api':api,'slot':slot,'parameters':calls})
    eq=[]
    for d,design,coeff,channels,calls in [
        (55,305,295,6,[0x12C2EF,0x12C7E8]),(56,310,300,6,[0x12C376,0x12C873]),
        (57,389,379,2,[0x12C3FE,0x12C8FF]),(58,394,384,2,[0x12C481,0x12C986])]:
        eq.append({'descriptor':d,'design_config_offset':design,'coefficient_config_offset':coeff,
            'design_words':[read(design+j) for j in range(5)],
            'coefficient_words':[read(coeff+j) for j in range(5)],
            'gain_index':2,'type_index':3,'sample_rate':48000,'channels':channels,
            'api19_call':hex(calls[0]),'api18_call':hex(calls[1]),
            'consumer_call':hex(0x13138C if channels==6 else 0x1313E6),
            'consumer_object_offset':593 if channels==6 else 677})
    return {'apis':rows,'eq_rebuilds':eq,
        'delay_bindings':{'descriptors':[52,53],'config_offsets':[316,317],
            'object_offsets':[615,616],'tail_call':'0x133015','pair_helper':'0x12eb9b',
            'channel_calls':['0x12ebc9','0x12ec04'],'delay_helper':'0x12f411',
            'settled_units':'samples; consumer truncates(value+0.5)',
            'boundary':'individual delays inside two-channel B tail, not total wrapper/acoustic latency; transitions read old value then smooth and read new value'},
        'selector_rule':'for API19 descriptors52/53: uint32-trunc(shadow API16)==5 first, else uint32-trunc(shadow API20)==3, else default',
        'selector_object_offsets':[1142,1146],
        'eq_zero_gain_proof':'12AB1F calls12AB0A to write [1,0,0,0,0]; rounded type0/1/2 and gain0 return without replacing identity',
        'boundary':'successful nondeferred parameter targets only; no queue, publication, smoothing or coefficient design for nonzero EQ gain. Other descriptors are config-word bindings, not physical speakers or UI labels.'}
