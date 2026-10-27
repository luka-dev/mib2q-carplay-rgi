"""Static V850 input-level control helpers; explicit supplied state only.

No defaults for vehicle controls, no protocol transport, no audio gain stage.
The continuous ramp, scheduler, queue delivery and other writers are outside
this reference. Returned bank is a writer target, NOT actual SHARC RAM.
"""
import math
from amp_b_upstream import select_level_bank

SOURCE_IDS=(0x21,0x22,0x23,0x24,0x26,0x2A,0x32)


def _integer(value, lo, hi):
    if type(value) is not int or not lo <= value <= hi:
        raise ValueError(f'integer in {lo}..{hi} required')
    return value


def normal_level_target(*, raw_level, source_trim, active_flag, inhibit):
    """C6928..C69C2 for the validated raw request domain0..126."""
    _integer(raw_level,0,126); _integer(source_trim,-128,127)
    _integer(active_flag,0,255); _integer(inhibit,0,255)
    if active_flag != 0xA5 or inhibit != 0 or raw_level == 0 or source_trim <= -13:
        return -100
    return min(26,raw_level-100+source_trim)


def select_level_targets(normal_targets, *, override_mode, override_level):
    """C35F0/C3790: select normal path or explicit lane0 override."""
    if len(normal_targets) != 4:
        raise ValueError('four normal targets required')
    for v in normal_targets: _integer(v,-128,127)
    _integer(override_mode,0,255); _integer(override_level,-100,26)
    return [override_level,-100,-100,-100] if override_mode == 0xA5 else list(normal_targets)


def gated_level_targets(targets, *, gate_a, gate_b, flags, mapped_active_flags):
    """AEFC0..AF078; caller has resolved source indices before this function."""
    if len(targets) != 4 or len(mapped_active_flags) != 4:
        raise ValueError('four target levels and mapped active flags required')
    for v in targets: _integer(v,-128,127)
    for v in (gate_a,gate_b,flags,*mapped_active_flags): _integer(v,0,255)
    out=list(targets) if gate_a == gate_b == 0x5A and not flags & 3 else [-100]*4
    if flags & 0x3C: out[0]=-100
    return [v if v >= -100 and active == 0xA5 else -100
            for v,active in zip(out,mapped_active_flags)]


def eq_selector_step(*, level_word, thresholds, override_mode,
                     normal_enable, override_enable, commit_gate,
                     previous_enable, previous_index):
    """B0066 + A8B26 writer target if queued event is later delivered.

    Pass current ramp word, not final requested target, for transient checks.
    enabled/disabled and index are cached even though a disabled upload uses0.
    """
    for v in (override_mode,normal_enable,override_enable,commit_gate,previous_enable):
        _integer(v,0,255)
    _integer(previous_index,0,15)
    candidate=select_level_bank(level_word,thresholds)
    enable=override_enable if override_mode == 0xA5 else normal_enable
    queued=commit_gate == 0xA5 and (enable != previous_enable or candidate != previous_index)
    cached_enable=enable if queued else previous_enable
    cached_index=candidate if queued else previous_index
    return {'candidate_index':candidate,'event17_queued':queued,
            'cached_enable':cached_enable,'cached_index':cached_index,
            'writer_target_if_delivered':cached_index if cached_enable == 0xA5 else 0,
            'actual_sharc_bank_proven':False}


def source_adjustment_request(payload, *, values, composite):
    """C7870/C53A0 payload only; no request transport or event emulator.

    values is18 rows of1 trim or3 signed fields (trim plus two controls whose
    UI names are not established here). Byte1 is
    opaque here. All fields are validated before any returned state changes.
    """
    if type(composite) is not bool:raise ValueError('explicit composite choice required')
    width=3 if composite else 1
    if len(values)!=18 or any(len(row)!=width for row in values):
        raise ValueError('18 source rows with explicit field count required')
    for row in values:
        for v in row:_integer(v,-128,127)
    if not isinstance(payload,(bytes,bytearray)) or not payload:
        raise ValueError('nonempty byte payload required')
    selector=payload[0]
    if len(payload)!=(2+7*width if selector==0 else 2+width):
        raise ValueError('wrong request length')
    out=[list(row) for row in values]
    if selector not in (0,255,*SOURCE_IDS):
        return {'values':out,'changed_sources':[],'selector_recognized':False,'recompute_requested':False}
    ids=SOURCE_IDS if selector in (0,255) else (selector,)
    limits=[32,9,9] if composite else [13]
    updates=[]
    for i,src in enumerate(ids):
        pos=2+(i*width if selector==0 else 0)
        row=[]
        for raw,limit in zip(payload[pos:pos+width],limits):
            value=raw-256 if raw>=128 else raw
            if raw==128:row.append(None)
            elif -limit<=value<=limit:row.append(value)
            else:raise ValueError('invalid signed field; no update applied')
        updates.append((src-33,row))
    changed=[]
    for src,row in updates:
        old=list(out[src])
        for i,v in enumerate(row):
            if v is not None:out[src][i]=v
        if out[src]!=old:changed.append(src)
    return {'values':out,'changed_sources':changed,'selector_recognized':True,'recompute_requested':True}


def override_mode_request(*, operation, length, mode, session_state, guard_flags, tick):
    """Inner function375 only, A1FB4; no assumption about external transport."""
    for v in (operation,mode,session_state,guard_flags):_integer(v,0,255)
    _integer(length,0,65535);_integer(tick,0,0xffffffff)
    result=dict(mode=mode,session_state=session_state,guard_flags=guard_flags,
                deadline=None,event_9_28=False,error=None)
    if guard_flags & 0xEF:result['error']=36
    elif operation==3:
        if length!=1:result['error']=19
        else:result.update(mode=165,session_state=192,guard_flags=guard_flags|16,event_9_28=mode!=165)
    elif operation==0:
        if session_state==192:
            result.update(mode=90,session_state=64,guard_flags=guard_flags & ~16,
                          deadline=(tick+5000)&0xffffffff,event_9_28=mode!=90)
    else:result['error']=49
    return result


def b_level_parameter(selected_target):
    """AA568 event52: unsmoothed selected signed byte, not common-EQ Q24."""
    return float(max(0,_integer(selected_target,-128,127)+100))


def b_level_apply(value, *, previous_effective, baseline, defer_word):
    """Finite API25 accepted-value projection; no library readiness/queue model.

    Shadow always updates; nonzero object1178 defers effective write/helper.
    helper_delta is a curve coordinate, NOT an audio gain in dB.
    Python float arithmetic here is not a bit-exact SHARC arithmetic emulator.
    """
    if not all(math.isfinite(v) for v in (value,previous_effective,baseline)) or not -200<=value<=200:
        raise ValueError('finite API25 value in -200..200 and finite supplied state required')
    _integer(defer_word,0,0xffffffff)
    return {'shadow_1151':float(value),
            'effective_1137':float(previous_effective if defer_word else value),
            'helper_delta':None if defer_word else float(value-baseline),
            'curve_helper_called':defer_word==0,'actual_audio_effect_proven':False}
