"""12B00F finite-domain control reference, not a firmware/queue emulator.

All curves/base/other slots are explicit. Float64 interpolation stands in for
SHARC float/reciprocal/sqrt rounding. No actual-car or whole-B preset defaults.
"""
import math
import struct

CURVE_EPSILON=struct.unpack('<f',struct.pack('<I',0x3727C5AC))[0]
DIV_EPSILON=struct.unpack('<f',struct.pack('<I',0x0DA24260))[0]


def api18_request_coordinate(value):
    """CA4B4 field5 signed coordinate -> AA886 candidate, not a full request.

    Only the accepted scalar field is projected. Other selector fields, gates,
    coefficient-block side effects, enqueue, delivery and defer are NOT run.
    This coordinate has no established Bass/LFE/MMI-percent UI identity.
    """
    if type(value) is not int or not -12<=value<=12:
        raise ValueError('signed integer request coordinate -12..12 required')
    return float(min(9,max(-9,value)))


def curve_contribution(coordinate, records):
    """Signed-side records (x,y,shape); zero input forces zero contribution.

    Matches 12B04B..12B1E7 in finite [-200,400], shape0 linear,1 squared,
    2 square-root. Shapes are rounded by trunc(shape+0.5). Ordering matters
    for repeated knots. The shape belongs to the endpoint away from zero.
    """
    if not math.isfinite(coordinate) or not -200<=coordinate<=400:
        raise ValueError('finite curve coordinate in -200..400 required')
    points=[tuple(row) for row in records]
    if any(len(row)!=3 or not all(math.isfinite(v) for v in row) for row in points):
        raise ValueError('finite x,y,shape triples required')
    if coordinate==0:return 0.0
    lower=(-200.0,0.0,0.0) if coordinate<0 else (0.0,0.0,0.0)
    upper=(0.0,0.0,0.0) if coordinate<0 else (200.0,0.0,0.0)
    found_lower=found_upper=False
    for point in points:
        if lower[0]<=point[0]<=upper[0]:
            if coordinate>=point[0]:lower=point;found_lower=True
            else:upper=point;found_upper=True
    if coordinate>0:
        near,far=lower,upper
        if not found_upper:far=(far[0],near[1],far[2])
    else:
        near,far=upper,lower
        if not found_lower:far=(far[0],near[1],far[2])
    distance=far[0]-near[0]
    if abs(distance)<CURVE_EPSILON:return float(near[1])
    shape=math.trunc(far[2]+0.5)
    numerator=coordinate-near[0];denominator=distance
    if shape==1:numerator*=numerator;denominator*=denominator
    elif shape==2:numerator=math.sqrt(abs(numerator));denominator=math.sqrt(abs(denominator))
    elif shape!=0:raise ValueError('unsupported curve shape (firmware error path)')
    return float(near[1]+(far[1]-near[1])*numerator/(denominator+DIV_EPSILON))


def update_parameter_slot(*, base, contributions, slot, coordinate, records, lower, upper):
    """12B104: replace one of nine slots, sum with base, then clamp target.

    Returned target is not the currently smoothed audio gain. No mutation of
    caller state; no initialization, publication or processing is implied.
    """
    if len(contributions)!=9 or type(slot) is not int or not 0<=slot<9:
        raise ValueError('nine contributions and slot0..8 required')
    if not all(math.isfinite(v) for v in (base,lower,upper,*contributions)) or lower>upper:
        raise ValueError('finite state and ordered clamps required')
    out=list(contributions);out[slot]=curve_contribution(coordinate,records)
    total=sum(out)+base
    if not math.isfinite(total):raise ValueError('finite reference sum required')
    return {'contributions':out,'unclamped':total,'target':min(upper,max(lower,total))}


def branch_gain_targets(binding, *, source, coordinates, contributions):
    """All nine supplied helper coordinates, extracted tables and explicit state.

    Successful nondeferred curve calls only. Does not execute other parameter
    changes in the same helpers. API25 coordinate is already baseline-adjusted;
    no UI values, delivered request sequence, audio processing or smoothing is
    inferred. Descriptor51 slot1 is untouched, not silently cleared.
    """
    if source not in ('loader','b01'):raise ValueError('explicit source required')
    if set(coordinates)!={row['api'] for row in binding['slots']}:
        raise ValueError('all nine API coordinates must be explicit')
    if set(contributions)!={46,50,51}:raise ValueError('all three prior slot banks required')
    state={d:list(v) for d,v in contributions.items()}
    if any(len(v)!=9 or not all(math.isfinite(x) for x in v) for v in state.values()):
        raise ValueError('finite nine-slot banks required')
    targets={}
    for row in binding['slots']:
        x=coordinates[row['api']];lo,hi=row['accepted_range']
        if not math.isfinite(x) or not lo<=x<=hi:
            raise ValueError('coordinate outside reference domain for this API')
        for branch in row['branches']:
            d=branch['descriptor'];records=[[v[source] for v in point] for point in branch['records']]
            r=update_parameter_slot(base=binding['bases'][str(d)][source],contributions=state[d],
                slot=row['slot'],coordinate=x,records=records,lower=0,upper=5)
            state[d]=r['contributions'];targets[d]=r['target']
    return {'targets':targets,'contributions':state,'actual_vehicle_state_proven':False}


def api18_19_parameter_targets(binding, *, source, api, coordinate,
                              contributions, shadow_selectors):
    """One successful API18/19 helper's scalar targets, not a whole-B render.

    All fifteen prior descriptor slot banks are explicit; untouched banks are
    retained. Selectors are already unsigned-truncated shadow API16/20 values,
    NOT effective/current audio mode or a physical/UI bank number. API16==5
    takes priority over API20==3. Nonzero EQ coefficient design is not emulated.
    """
    if source not in ('loader','b01') or api not in (18,19):
        raise ValueError('explicit loader/b01 source and API18/19 required')
    if not math.isfinite(coordinate) or not -200<=coordinate<=200:
        raise ValueError('finite API18/19 coordinate in -200..200 required')
    if set(contributions)!=set(range(44,59)):
        raise ValueError('all fifteen prior descriptor banks required')
    if set(shadow_selectors)!={16,20} or any(type(v) is not int or not 0<=v<=0xffffffff
                                            for v in shadow_selectors.values()):
        raise ValueError('explicit unsigned-truncated shadow selectors16/20 required')
    state={d:list(v) for d,v in contributions.items()}
    if any(len(v)!=9 or not all(math.isfinite(x) for x in v) for v in state.values()):
        raise ValueError('finite nine-slot banks required')
    row=next(r for r in binding['apis'] if r['api']==api)
    targets={};config={};chosen={}
    for p in row['parameters']:
        d=p['descriptor'];variant='default'
        if api==19 and d in (52,53):
            if shadow_selectors[16]==5:variant='shadow16_eq5'
            elif shadow_selectors[20]==3:variant='shadow20_eq3'
        records=[[v[source] for v in point] for point in p['variants'][variant]['records']]
        result=update_parameter_slot(base=p['base'][source],contributions=state[d],
            slot=row['slot'],coordinate=coordinate,records=records,
            lower=p['clamp'][0],upper=p['clamp'][1])
        state[d]=result['contributions'];targets[d]=result['target']
        config[p['target_config_offset']]=result['target'];chosen[d]=variant
    designs={}
    for eq in binding['eq_rebuilds']:
        record=[v[source] for v in eq['design_words']]
        record[eq['gain_index']]=targets[eq['descriptor']]
        designs[eq['coefficient_config_offset']]=record
    return {'targets':targets,'config_updates':config,'contributions':state,
            'curve_variants':chosen,'eq_design_records':designs,
            'actual_vehicle_state_proven':False}


def zero_gain_eq_coefficients(record):
    """Verified identity exit of12AB1F only; reject every nonzero gain.

    Five design words, field2 gain and rounded field3 type0/1/2. Supported
    finite records yield identity; this is deliberately not a general designer.
    """
    if len(record)!=5 or not all(math.isfinite(v) for v in record):
        raise ValueError('five finite design words required')
    if record[2]!=0 or not 0<=record[3]<=2 or math.trunc(record[3]+.5) not in (0,1,2):
        raise ValueError('only the verified zero-gain type0/1/2 path is implemented')
    return [1.0,0.0,0.0,0.0,0.0]


def b_config_request(selector_table, *, state_a, state_b):
    """AA468 event46, table-domain states only; not a matrix bank selector.

    A candidate0 is still returned, explicitly rejected by the API17 validator.
    This helper does not imply enqueue/delivery, defer release or successful copy.
    """
    if len(selector_table)!=2 or any(len(row)!=21 for row in selector_table):
        raise ValueError('two rows of21 signed selector bytes required')
    if any(type(v) is not int or not -128<=v<=127 for row in selector_table for v in row):
        raise ValueError('signed selector bytes required')
    if type(state_a) is not int or not 0<=state_a<21 or type(state_b) is not int or not 0<=state_b<2:
        raise ValueError('explicit table-domain states required')
    profile=selector_table[state_b][state_a];candidate=profile+1
    return {'a07_profile':profile,'api17_candidate':candidate,
            'api17_accepted':candidate in (1,2,3,4),'actual_vehicle_state_proven':False}


def replay_branch_gain_targets(binding, *, source, effective_values, contributions):
    """132168 nine-effective-value replay, branch-gain slice only.

    Uses RAW effective API25; no baseline subtraction in this caller. A selected
    bank's binding supplies new bases/curves; prior descriptor slots are retained.
    Does not emulate the preceding bank copy, other scalar/coeff writes or errors.
    """
    return branch_gain_targets(binding,source=source,coordinates=effective_values,
                               contributions=contributions)
