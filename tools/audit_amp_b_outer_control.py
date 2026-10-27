"""Image-bound V850 senders/outer dispatch; static only, no device access.

Addresses are byte offsets in raw LE V850/32 CodeFlash, base0. Main request
descriptor fields are NOT a ready-to-send network packet specification.
"""
import hashlib
import struct
from audit_amp_output_bindings import SOURCE_SHA256, SOURCE_SIZE

RANGES = {
    'coefficient_matrix_writer': (0xA903C,0xA91C4),
    'coefficient_transport': (0xA8BB4,0xA8D5C),
    'outer_main_dispatch': (0x99774, 0x99BD2),
    # 99BD2..99BE0 is a seven-entry relative switch table, not instructions.
    'outer_main_response': (0x99BE0, 0x99CD4),
    'outer_alt_request': (0xA2874, 0xA2A98),
    'outer_alt_dispatch': (0xD57C8, 0xD58EC),
    'outer_be_codec': (0x99CD4, 0x99DE8),
    'outer_generic_handlers': (0xD40C8, 0xD41DC),
    'outer_alt_read_handler': (0xD42DC, 0xD433C),
    'outer_read_senders': (0xAC6CC, 0xAC818),
    'outer_event_dispatch': (0xA76B0, 0xA7D28),
    'outer_api18_writer': (0xAA644, 0xAA898),
    'outer_api18_request': (0xCA4B4, 0xCA8BC),
    'outer_api18_init': (0xB6DD0, 0xB6DD8),
    'outer_event_enqueue': (0xB5CFC, 0xB5D88),
    'outer_event_schedule': (0xC3990, 0xC39BC),
    'outer_sync_wrappers': (0xDD814, 0xDD84C),
    'outer_sync_kernel': (0xEAADC, 0xEABA4),
}


def function_operations(data, directory, function_id):
    """Bounded on-image directory8/operation12 parser; no network access."""
    for p in range(directory, min(directory+4096, len(data)-7), 8):
        fid, table = struct.unpack_from('<II', data, p)
        if fid == 0xFFFF:
            return None
        if fid != function_id:
            continue
        rows=[]
        for q in range(table, min(table+4096, len(data)-11), 12):
            op, flags, reserved, first, second=struct.unpack_from('<BBHII',data,q)
            if op == 0xFF:
                return {'directory_record':hex(p),'table':hex(table),'operations':rows}
            rows.append({'record':hex(q),'operation':op,'flags':flags,
                         'reserved':reserved,'first':hex(first),'second':hex(second)})
        raise ValueError('unterminated operation table')
    raise ValueError('unterminated function directory')


def direct_lp_candidates(data, target, start=0x80000, end=0xE0000):
    """Conservative aligned-byte JARL lp census, not a complete call graph.

    Data can look like code. Excludes indirect calls and other link registers;
    callers below are independently checked in the bounded r2 listings.
    """
    result=[]
    for pc in range(start,end,2):
        lo,hi=struct.unpack_from('<HH',data,pc)
        if lo & 0xFFC0 != 0xFF80 or hi & 1:
            continue
        offset=((lo & 63)<<16)|hi
        if offset & 0x200000:offset-=0x400000
        if pc+offset==target:result.append(pc)
    return result


def outer_control(data):
    if len(data)!=SOURCE_SIZE or hashlib.sha256(data).hexdigest()!=SOURCE_SHA256:
        raise ValueError('wrong CodeFlash size or SHA-256')
    main=function_operations(data,0x965E4,0xA21)
    alternate=function_operations(data,0x96734,0xA21)
    api18=function_operations(data,0x95A24,0xD28)
    candidates=direct_lp_candidates(data,0xAC648)
    expected=[0xA7842,0xAA3BC,0xAA3EE,0xAA410,0xAA438,0xAA460,
              0xAA49C,0xAA4C6,0xAA4F4,0xAA51A,0xAA53E,0xAA562,
              0xAA594,0xAA5F6,0xAA886,0xAC6DE,0xAC756]
    if candidates!=expected:raise ValueError('direct sender census changed')
    return {
        'source_sha256':SOURCE_SHA256,
        'scope':'static request reachability, not delivery, current vehicle state or a bass diagnosis',
        'format':'raw LE V850/32 base0; byte addresses; payload words are BE32',
        'ranges':{n:{'start':hex(a),'end_exclusive':hex(b),
                     'sha256':hashlib.sha256(data[a:b]).hexdigest()} for n,(a,b) in RANGES.items()},
        'main':{'dispatch':'0x99774','block_table':'0x401f8',
            'block_record':'0x4021c','block_id':0xF5,'instance_pointer':'0x3ff29f7',
            'directory':'0x965e4','function_id':0xA21,**main,
            'descriptor_fields':{'payload_pointer':0,'length_s16':4,'function_u16':8,
                                 'block_u8':10,'instance_u8':11,'operation_u8':12},
            'binding':'997E4 selects block table;99918 reads directory+8;99954 reads operation table; D0 calls first(response,request), low2 calls second(response,request)',
            'boundary':'instance/broadcast matching exists; full ingress/authentication/vehicle caller not bound'},
        'alternate':{'dispatch':'0xd57c8','directory':'0x96734','function_id':0xA21,**alternate,
            'caller':'0xa2874','calls':['0xa29c8','0xa29e2'],
            'gate':'gp-17868==1 rejects; BE16(message+6)>=14; BE16(message+20)==A018 selects this branch',
            'operation':'message[19]&7F:1 -> inner0 write;3 -> inner1 read;2 does not call D57C8',
            'function':'BE16(message+22)','payload':'message+24',
            'boundary':'internal message offsets, not a complete wire packet; dispatcher does not initialize input descriptor length; do not infer 8-byte length validation'},
        'write':{'handlers':['0xd40c8','0xd4130'],'payload_words':2,'byte_order':'big',
            'command_gp':-15936,'value_gp':-15932,'event':369,'consumer':'0xa782e',
            'sequence':['DD814(3)','decode two BE32 via99D98','C3990(369)',
                        'consumer reads shared staging words','AC648(command,raw_value)','DD830(3) in consumer'],
            'validation':'no inner-ID whitelist or payload-float conversion in either write handler',
            'main_response':'D40C8 copies staging value via99D44, length4, return12; no dedicated DSP-result wait here',
            'alternate_response':'D4130 returns0',
            'synchronization':'DD814/DD830 wrap EAADC/EAB44: count decrement/block and increment/wake; full scheduler/init not emulated',
            'queue':'C3990 calls B5CFC to enqueue/deduplicate a u16 event in a 376-slot ring, then signals(1,10); staging is NOT copied into each event',
            'boundary':'no claim about concurrency, request completion or resulting audio frame'},
        'read':{'handlers':['0xd4180','0xd42dc'],'payload_words':1,'byte_order':'big',
            'event':370,'consumer':'0xac6cc','command_gp':-15936,'sent_payload_raw':0,
            'result_dsp_address':'0xb95e8','result_gp':-15708,
            'main_wait':'DCFB8(12,70)','alternate_wait':'DD0A4(16,70)',
            'completion':'AC6CC reads one DSP word, stores result, signals(2,12) or(7,16) based on gp-15711',
            'response':'BE32(result), length4, return12; handlers do not branch on wait return',
            'boundary':'unrestricted command word is forwarded with zero payload; do not treat this entry as guaranteed side-effect-free for arbitrary IDs'},
        'sender_census':{'range':['0x80000','0xe0000'],'target':'0xac648',
            'direct_lp_candidates':[hex(p) for p in candidates],
            'fixed_api18':'AA886: R6=18, R7=float(s8(gp-15840))',
            'generic_write':'A7842: command and value from shared request staging',
            'generic_zero_payload':'AC6DE: command from staging, payload raw0',
            'fixed_extended':'AC756: command0800AFFE, payload raw0; event371 bulk read, not canonical indexed-write/source-store header',
            'boundary':'no fixed 14000000/12000000/10000000/20000003 sender found among these17 candidates; indirect/other transport writers not excluded'},
        'api18':{'sender':'0xaa886','events':[22,368],'value_gp':-15840,
            'formula':'float(s8(gp-15840))','init':'B6DD0 stores0',
            'request_handler':'0xca4b4','block_id':0x22,'function_id':0xD28,**api18,
            'payload':'selector5: length3, coordinate s8(payload[2]); selector0: length8, coordinate s8(payload[6]) after prior fields succeed',
            'coordinate_domain':[-12,12],'stored_raw_gp':-14973,'api_clamp':[-9,9],
            'stores':['CA728 raw signed coordinate','CA732/CA73E/CA744 clamped coordinate'],
            'event_condition':'unchanged mode bytes gp-17771/-17770/-16783 -> event22 atCA834; changed-mode paths are conditional',
            'zero':'request coordinate0 maps to API18 float0 on this path; not a +9.5dB offset',
            'other_effects':'AA644 also sends a 20-word coefficient block before API18; event22 additionally callsA903C. Same byte is read atAA7F2/A90B6/A9106',
            'boundary':'not labelled Bass/LFE/MMI percent; partial multi-field writes before later validation errors are possible; no current state inference'},
        'remaining':['outermost caller/authentication and actual external request sequence',
            'coefficient side effects of API18 source byte, API29 sender and UI mapping',
            'indirect/other transport senders, extended error lifecycle and scheduler timing',
            'pointer consumers1162..1165, target composition, nonzero EQ designer, physical routing'],
    }
