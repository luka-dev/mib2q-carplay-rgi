"""Finite API16/20 pointer-write reference, excluding queue/audio/replay math."""
import math


def apply_mode_control(binding, *, api, value, deferred, effective_config487,
                       pointers, shadow, effective):
    """Caller supplies ALL18 prior config-relative pointers and selector state.

    API20 auto6 resolves against the EFFECTIVE config before even deferred shadow
    storage. Shared pointers are overwritten by the last applied command; no
    final-mode-pair shortcut can reconstruct command history in general.
    """
    if api not in (16,20) or value not in binding['apis'][str(api)]['accepted']:
        raise ValueError('validated API16/20 value required')
    if type(deferred) is not bool or not math.isfinite(effective_config487):
        raise ValueError('explicit defer flag and finite effective config487 required')
    if set(pointers)!=set(range(1156,1174)) or any(type(v) is not int or not 0<=v<829 for v in pointers.values()):
        raise ValueError('all18 prior config-relative pointer offsets required')
    if set(shadow)!={16,20} or set(effective)!={16,20} or not all(math.isfinite(v) for v in [*shadow.values(),*effective.values()]):
        raise ValueError('explicit finite prior shadow/effective16/20 required')
    resolved=(1 if effective_config487==1 else 3) if api==20 and value==6 else value
    out=dict(pointers);sh=dict(shadow);eff=dict(effective);sh[api]=resolved
    writes={}
    if not deferred:
        eff[api]=resolved
        key=f'{api}:{resolved:g}' if (api,resolved) in ((16,5),(20,3)) else f'{api}:default'
        writes={int(p):v for p,v in binding['pointer_maps'][key].items()};out.update(writes)
    return {'pointers':out,'pointer_writes':writes,'shadow':sh,'effective':eff,
            'resolved_value':resolved,'replay_required':not deferred,
            'actual_vehicle_state_proven':False}


def resolve_mode_parameters(binding, *, effective_config, pointers):
    """Dereference only proven consumer banks, no preset/state initialization.

    Input is an explicit effective829-word FLOAT snapshot/scenario, not the
    desired target. Returned post_crossfeed feeds amp_b_pipeline.crossmix;
    nested/spatial records still need their consumers. Unbound1162..1165 are
    deliberately omitted, not treated as zero or identity audio stages.
    """
    if len(effective_config)!=829 or not all(math.isfinite(v) for v in effective_config):
        raise ValueError('explicit finite829-word effective config required')
    if set(pointers)!=set(range(1156,1174)):
        raise ValueError('all18 pointers required')
    result={}
    for p,consumer in binding['consumers'].items():
        offset=pointers[int(p)];width=consumer['words']
        if type(offset) is not int or not 0<=offset<=829-width:
            raise ValueError('consumer record outside effective config')
        values=list(effective_config[offset:offset+width])
        result[consumer['role']]=values[0] if width==1 else values
    return result
