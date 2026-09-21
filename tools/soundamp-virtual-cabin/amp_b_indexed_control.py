"""Offline effect plans for canonical extended B writes; no firmware I/O.

Plans describe one accepted FIFO record after the normal-pending pass succeeds.
They do NOT emulate callee side effects, interpolation, errors, scheduling or
full memory. Raw writes outside829-word config are deliberately visible.
"""


def _word(v):
    if type(v) is not int or not 0<=v<=0xffffffff:
        raise ValueError('raw uint32 word required')
    return v


def indexed_command_plan(command,value_word,*,ready,queue_count,effective_words):
    """Explicit prior effective controls16..29 as RAW words, not float guesses.

    Unknown/noncanonical headers fail closed. The returned control words reflect
    the direct store only, before callees; call descriptors preserve actual order.
    No assumption about whether this command was ever sent in the car.
    """
    _word(command);_word(value_word)
    if type(ready) is not int or ready!=1 or type(queue_count) is not int or not 0<=queue_count<64:
        raise ValueError('ready1 and pre-append FIFO count0..63 required')
    if set(effective_words)!=set(range(16,30)):
        raise ValueError('all14 prior effective control words required')
    effective={k:_word(v) for k,v in effective_words.items()}
    family=command&0xffff0000;index=command&65535
    writes=[];calls=[]
    if family==0x14000000:
        if not 1<=index<=842:raise ValueError('config/control index1..842 required')
        writes=[{'space':'object','offset':299+index,'word':value_word},
                {'space':'target','offset':index,'word':value_word}]
        if index>=829:effective[16+index-829]=value_word
        if index<=829:calls=[{'helper':'0x1320ce'}]
        else:
            calls=[{'helper':'0x132168'},
                {'helper':'0x12b348','api':20,'value_word':effective[20]},
                {'helper':'0x12b348','api':16,'value_word':effective[20]},
                {'helper':'0x132346','value_word':effective[17]}]
    elif family==0x12000000:
        if not 4<=index<=206:raise ValueError('core index4..206 required')
        writes=[{'space':'object','offset':92+index,'word':value_word}]
    elif command==0x10000000:
        if value_word not in (0,1,2):raise ValueError('raw defer0/1/2 required')
        writes=[{'space':'object','offset':1178,'word':value_word}]
    elif command==0x20000003:
        if value_word not in (1,2,3,4):raise ValueError('raw source bank1..4 required')
        calls=[{'helper':'0x132aef','source_offset':829*(value_word-1),
                'effective_config_object_offset':299,'words':829,'refresh_crc':True}]
    else:raise ValueError('canonical indexed/defer/source-store command required')
    return {'direct_writes':writes,'calls':calls,'effective_words_after_direct_write':effective,
        'target_offsets_outside_config':[w['offset'] for w in writes if w['space']=='target' and w['offset']>=829],
        'shadow_controls_written':False,'actual_vehicle_state_proven':False,
        'scope':'one accepted record; direct stores and call plan only, not callee execution or complete queue state'}
