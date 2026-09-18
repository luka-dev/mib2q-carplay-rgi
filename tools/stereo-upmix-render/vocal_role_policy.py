"""Role-aware rear permission. A stem name or clean-separation vote is not a role."""
import math

POLICY='lead-front-verified-backing-optional-v1'
ROLES={'lead','co_lead','lead_double','vocal_hook','backing','vocal_reverb','mixed_vocal','unknown','non_vocal'}
ROLE_FIELDS={'role','role_confirmed','lead_contamination','interval_start','interval_end'}


def validate_role_fields(value, entry):
    if not ROLE_FIELDS.intersection(value):return {}
    if not ROLE_FIELDS.issubset(value):raise ValueError('complete role interval required')
    role=value['role'];confirmed=value['role_confirmed'];contamination=value['lead_contamination']
    if role not in ROLES or type(confirmed) is not bool:raise ValueError('invalid musical role')
    if contamination not in {'absent','present','unknown'}:raise ValueError('invalid lead contamination')
    start=value['interval_start'];end=value['interval_end']
    if (type(start) not in (int,float) or type(end) not in (int,float)
        or not math.isfinite(start) or not math.isfinite(end)
        or not entry.get('core_start',0)<=start<end<=entry.get('core_end',entry['seconds'])):
        raise ValueError('role interval must be inside the audible core')
    return {k:value[k] for k in ROLE_FIELDS}


def rear_permission(note, source_sha256, stem_sha256):
    """Evidence admission only; never sets runtime gain or training approval."""
    if not note:return dict(allow=False,reason='unreviewed',policy=POLICY)
    reason='protected-or-unknown-role'
    if note.get('source_sha256')!=source_sha256 or note.get('stem_sha256')!=stem_sha256:reason='stale-evidence'
    elif note.get('reviewer')!='human-local':reason='role-not-human-reviewed'
    elif note.get('decision')!='good':reason='separation-not-approved'
    elif note.get('role')=='backing':
        if note.get('role_confirmed') is not True:reason='backing-role-unconfirmed'
        elif note.get('lead_contamination')!='absent':reason='lead-contamination-unresolved'
        elif not all(k in note for k in ('interval_start','interval_end')):reason='missing-interval'
        elif (type(note['interval_start']) not in (int,float) or type(note['interval_end']) not in (int,float)
              or not math.isfinite(note['interval_start']) or not math.isfinite(note['interval_end'])
              or not 0<=note['interval_start']<note['interval_end']):reason='invalid-interval'
        else:return dict(allow=True,reason='reviewed-backing-candidate',policy=POLICY,training_approved=False)
    return dict(allow=False,reason=reason,policy=POLICY)
