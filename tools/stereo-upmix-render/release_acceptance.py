"""V2 fail-closed, hash-bound evidence ledger; not a metric/listener producer.

The v1 frozen owner remains historical and is never silently reinterpreted.
All physical claims still require audited source measurements: hashes are not
signatures and self-declared origins do not establish scientific truth.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re

SCHEMA = 'upmix-release-protocol-v2'
EVIDENCE = 'upmix-release-gate-evidence-v2'
FREEZE = 'upmix-release-freeze-v2'
DOMAIN = 'upmix-release-domain-manifest-v2'
EVALUATOR = 'upmix-release-acceptance-evaluator-v2'
MAX_REPORT_BYTES = 16 * 1024**2
SCOPES = {'AUDIT','TRAIN','DEV','FINAL_TEST','REFERENCE_NATIVE','TARGET','LISTENING','PACKAGE','CABIN'}
COHERENT_SCOPES = {'TARGET','LISTENING'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def valid_hash(value):
    return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{64}', value) is not None


def utc(value):
    require(isinstance(value,str) and re.fullmatch(
        r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,6})?(?:Z|\+00:00)',value),
        'Explicit ISO8601 UTC timestamp required')
    result = datetime.fromisoformat(value.replace('Z','+00:00'))
    require(result.utcoffset().total_seconds()==0, 'UTC required')
    return result


def read(path):
    path = Path(path)
    require(path.is_file() and path.stat().st_size <= MAX_REPORT_BYTES, 'Bounded report required')
    raw = path.read_bytes()
    require(len(raw) <= MAX_REPORT_BYTES, 'Report grew beyond bound')
    def pairs(items):
        result={}
        for key,value in items:
            require(key not in result, 'Duplicate JSON key')
            result[key]=value
        return result
    value=json.loads(raw,object_pairs_hook=pairs,
                     parse_constant=lambda _:require(False,'Nonfinite JSON'))
    require(isinstance(value,dict), 'JSON object required')
    return value,raw


def _ids(values, name):
    require(isinstance(values,list) and all(isinstance(v,str) and v.strip() and len(v)<=512 for v in values)
            and len(set(values))==len(values), 'Unique nonempty '+name+' required')
    return values


def validate_protocol(value):
    require(isinstance(value,dict) and value.get('schema')==SCHEMA
            and value.get('evaluator_id')==EVALUATOR
            and value.get('candidate_evaluated_before_freeze') is False,
            'Prospective v2 protocol/evaluator required')
    gates=value.get('gates')
    require(isinstance(gates,list) and gates, 'Nonempty gate set')
    seen=set()
    for gate in gates:
        require(isinstance(gate,dict), 'Gate object required')
        name=gate['id']
        require(isinstance(name,str) and re.fullmatch('[a-z][a-z0-9_]+',name)
                and name not in seen, 'Unique gate identity required')
        seen.add(name)
        require(gate['scope'] in SCOPES and gate['kind'] in ('bool','min','max'), 'Unknown gate semantics')
        require(type(gate['min_count']) is int and gate['min_count']>0, 'Positive coverage count required')
        require(isinstance(gate.get('count_unit'),str) and
                re.fullmatch('[a-z][a-z0-9_]+',gate['count_unit']), 'Explicit count unit required')
        limit=gate['threshold']
        require(type(limit) is bool if gate['kind']=='bool' else
                type(limit) in (int,float) and math.isfinite(limit), 'Invalid gate threshold')
        if gate['kind']!='bool':
            domain=gate.get('numeric_domain')
            require(isinstance(domain,dict) and set(domain)=={'type','min','max'}
                    and domain['type'] in ('real','integer'), 'Explicit numeric domain/type required')
            for bound in ('min','max'):
                require(domain[bound] is None or type(domain[bound]) in (int,float)
                        and math.isfinite(domain[bound]), 'Invalid physical range')
            require(domain['min'] is None or domain['max'] is None or domain['min']<=domain['max'],
                    'Inverted physical range')
        if 'log_ratio' in gate:
            log=gate['log_ratio']
            require(gate['kind']!='bool' and gate['numeric_domain']['type']=='real'
                    and isinstance(log,dict) and log.get('multiplier') in (10,20)
                    and type(log.get('multiplier')) is int
                    and log.get('transform') in ('signed','absolute')
                    and type(log.get('both_zero_is_equal_perfect')) is bool,
                    'Explicit dB ratio semantics required')
            require(not log['both_zero_is_equal_perfect'] or name in
                    ('recovery_gain_db_median','recovery_gain_db_ci95_lower'),
                    'Only recovery gain permits equal-perfect zero/zero')
        require(type(gate.get('requires_positive_coherent_transfer',False)) is bool,
                'Explicit coherent sign contract')
        require(type(gate.get('acoustic_only',False)) is bool, 'Boolean acoustic boundary')
        require(not gate.get('acoustic_only',False) or gate['scope']=='CABIN',
                'Only cabin gates may be acoustic-only')
    require(any(g['scope']=='CABIN' for g in gates)
            and any(not g.get('acoustic_only',False) for g in gates), 'Both release and acoustic gates required')
    return value


def write_new(path,value):
    raw=(json.dumps(value,indent=2,allow_nan=False)+'\n').encode()
    with Path(path).open('xb') as stream:
        stream.write(raw)


def seal(protocol_path,specification_path,output):
    protocol,raw=read(protocol_path);validate_protocol(protocol)
    spec=Path(specification_path).read_bytes()
    require(0<len(spec)<=MAX_REPORT_BYTES,'Bounded nonempty metric specification')
    owner=Path(output)
    require(not owner.exists() and not owner.is_symlink(),'Fresh freeze owner required')
    owner.mkdir(parents=False,mode=0o700)
    evaluator=Path(__file__).read_bytes()
    for name,data in (('protocol.json',raw),('metric-specification.md',spec),('evaluator.py',evaluator)):
        with (owner/name).open('xb') as stream:stream.write(data)
    freeze=dict(schema=FREEZE,protocol_sha256=sha(raw),metric_specification_sha256=sha(spec),
                evaluator_id=EVALUATOR,evaluator_sha256=sha(evaluator),
                utc=datetime.now(timezone.utc).isoformat(),candidate_evaluated=False)
    write_new(owner/'freeze.json',freeze)
    return freeze


def load_freeze(owner):
    owner=Path(owner);freeze,_=read(owner/'freeze.json');protocol,raw=read(owner/'protocol.json')
    require(freeze.get('schema')==FREEZE and freeze.get('candidate_evaluated') is False
            and freeze.get('evaluator_id')==EVALUATOR,'Invalid v2 freeze record')
    require(utc(freeze['utc'])<=datetime.now(timezone.utc),'Future freeze timestamp')
    require(sha(raw)==freeze['protocol_sha256']
            and sha((owner/'metric-specification.md').read_bytes())==freeze['metric_specification_sha256'],
            'Frozen contract changed')
    require(sha(Path(__file__).read_bytes())==freeze['evaluator_sha256']
            and sha((owner/'evaluator.py').read_bytes())==freeze['evaluator_sha256'],
            'Evaluator version/snapshot differs')
    return validate_protocol(protocol),freeze


def _artifact(root,item):
    require(isinstance(item,dict) and isinstance(item.get('path'),str)
            and item['path'] and '\\' not in item['path'],'Artifact record/path required')
    relative=Path(item['path'])
    require(not relative.is_absolute() and '..' not in relative.parts
            and str(relative)==item['path'],'Unsafe artifact path')
    path=(root/relative).resolve()
    require(path.is_relative_to(root.resolve()) and path.is_file(),'Artifact escapes owner or is absent')
    require(valid_hash(item.get('sha256')),'Invalid artifact hash')
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda:stream.read(1024**2),b''):h.update(chunk)
    require(h.hexdigest()==item['sha256'],'Evidence artifact body changed')
    return path


def _domain(gate,report,root,freeze,candidate_sha256):
    artifacts=report.get('artifacts')
    require(isinstance(artifacts,list) and 0<len(artifacts)<=256,'Bounded source artifacts required')
    paths=[]
    for item in artifacts:
        _artifact(root,item);paths.append(item['path'])
    require(len(set(paths))==len(paths),'Duplicate artifact path')
    binding=report.get('domain_manifest')
    require(isinstance(binding,dict) and binding in artifacts
            and valid_hash(report.get('domain_manifest_sha256'))
            and binding.get('sha256')==report['domain_manifest_sha256'],
            'Domain manifest must be an exact hash-bound listed artifact')
    domain,raw=read(_artifact(root,binding))
    require(sha(raw)==binding['sha256'] and domain.get('schema')==DOMAIN
            and domain.get('scope')==gate['scope']
            and domain.get('protocol_sha256')==freeze['protocol_sha256'],'Domain scope/protocol differs')
    require(domain.get('candidate_sha256')==candidate_sha256 or
            gate['scope']=='AUDIT' and domain.get('candidate_sha256') is None,
            'Domain candidate differs')
    groups=domain.get('gate_units')
    require(isinstance(groups,dict) and isinstance(groups.get(gate['id']),dict),'Gate unit set required')
    group=groups[gate['id']]
    require(group.get('unit')==gate['count_unit']
            and report.get('count_unit')==gate['count_unit'],'Coverage unit differs')
    eligible=_ids(group.get('eligible_ids'),'eligible unit ids')
    ineligible=_ids(group.get('ineligible_ids'),'ineligible unit ids')
    require(not set(eligible)&set(ineligible) and isinstance(group.get('eligibility_method'),str)
            and group['eligibility_method'].strip(),'Explicit disjoint eligibility accounting required')
    ids=_ids(report.get('eligible_unit_ids'),'reported eligible unit ids')
    require(set(ids)==set(eligible) and type(report.get('sample_count')) is int
            and report['sample_count']==len(eligible),'Eligible units/count differ from domain manifest')
    if gate['scope']=='TARGET':
        context=domain.get('target_context')
        require(isinstance(context,dict),'Target workload/build/period context required')
        for key in ('cohort_id','target_identity','cpu','operating_system'):
            require(isinstance(context.get(key),str) and context[key].strip(),'Target '+key+' required')
        require(context['cpu']=='APQ8064/Krait' and context['operating_system']=='QNX'
                and context.get('emulated') is False,'Actual APQ8064/Krait QNX cohort required')
        for key in ('build_manifest_sha256','workload_manifest_sha256'):
            require(valid_hash(context.get(key)) and
                    any(item['sha256']==context[key] for item in artifacts),
                    'Target build/workload body must be a bound artifact')
        rate,frames,period=context.get('sample_rate_hz'),context.get('callback_frames'),context.get('callback_period_seconds')
        require(type(rate) is int and rate>0 and type(frames) is int and frames>0
                and type(period) in (int,float) and math.isfinite(period) and period>0
                and math.isclose(period,frames/rate,rel_tol=1e-12,abs_tol=0.),
                'Consistent target rate/frame/period geometry required')
        require(context.get('representative_competing_load') is True,'Loaded target cohort required')
        require(context.get('sample_format')=='S16_LE'
                and context.get('channels')==['FL','FR','C','LFE','SL','SR'],
                'Explicit six-slot S16_LE target ABI required')
        require(gate['count_unit']=='target_cohort' and eligible==[context['cohort_id']],
                'One exact loaded target cohort required')
    if gate['scope']=='LISTENING':
        context=domain.get('listening_context')
        require(isinstance(context,dict) and context.get('verdict_unit')=='song_cluster'
                and context.get('repeats_add_trials') is False and context.get('ties_are_nonpreference') is True
                and context.get('population_claim') is False,
                'Independent song-level personal-listening contract required')
        listeners=_ids(context.get('listener_ids'),'actual listener ids')
        require(listeners and type(context.get('listener_count')) is int
                and context['listener_count']==len(listeners),'Actual listener count required')
        require(gate['count_unit']=='song_verdict','Listening counts distinct song verdicts')
    return domain,len(eligible)


def _measurement_value(gate,report):
    value=report.get('value');ratio=report.get('exact_log_ratio')
    case=None
    if ratio is not None:
        require('log_ratio' in gate and isinstance(ratio,dict)
                and set(ratio)=={'numerator','denominator','multiplier'},
                'Exact ratios are only valid for declared dB gates')
        n,d=ratio['numerator'],ratio['denominator'];definition=gate['log_ratio']
        require(all(type(v) in (int,float) and math.isfinite(v) and v>=0 for v in (n,d))
                and type(ratio['multiplier']) is int
                and ratio['multiplier']==definition['multiplier'],'Nonnegative finite exact ratio required')
        if n==0 and d==0:
            if definition['both_zero_is_equal_perfect']:
                derived=0.;case='both_zero_equal_perfect_no_improvement'
            else:
                require(value is None,'Undefined zero/zero cannot carry a finite measured value')
                return None,'zero_over_zero_unknown'
        elif n==0:
            derived=-math.inf;case='exact_zero_numerator'
        elif d==0:
            derived=math.inf;case='exact_zero_denominator'
        else:
            derived=definition['multiplier']*(math.log10(n)-math.log10(d))
        if definition['transform']=='absolute':derived=abs(derived)
        if value is not None:
            require(type(value) in (int,float) and math.isfinite(value) and math.isfinite(derived)
                    and math.isclose(value,derived,rel_tol=1e-12,abs_tol=1e-12),
                    'Finite value differs from exact log ratio')
        value=derived
    else:
        require(type(value) in (int,float) and math.isfinite(value),'Finite measured value or exact dB ratio required')
    physical=gate['numeric_domain']
    if math.isfinite(value):
        require(physical['type']!='integer' or type(value) is int,'Integer metric required')
        require((physical['min'] is None or value>=physical['min'])
                and (physical['max'] is None or value<=physical['max']),'Metric outside physical domain')
    else:
        require(ratio is not None,'Only explicit exact dB ratios may be infinite')
    return value,case


def check_gate(gate,report,root,freeze,candidate_sha256):
    require(isinstance(report,dict) and report.get('schema')==EVIDENCE
            and report.get('gate_id')==gate['id'],'Wrong evidence schema/gate')
    require(report.get('protocol_sha256')==freeze['protocol_sha256']
            and report.get('metric_specification_sha256')==freeze['metric_specification_sha256']
            and report.get('candidate_sha256')==candidate_sha256,'Evidence contract/candidate differs')
    require(report.get('scope')==gate['scope'],'Evidence scope differs')
    require(isinstance(report.get('method'),str) and report['method'].strip(),'Measurement method required')
    require(report.get('state') in ('measured','unverified','blocked'),'Explicit measurement state required')
    if report['state']!='measured':
        require(isinstance(report.get('reason'),str) and report['reason'].strip(),'Missing-evidence reason required')
        return dict(state=report['state'].upper(),reason=report['reason'])
    when=utc(report.get('measurement_utc'));frozen=utc(freeze['utc'])
    require(when<=datetime.now(timezone.utc),'Future measurement timestamp')
    if when<frozen:
        require(gate['scope']=='AUDIT','Candidate performance predates protocol freeze')
        checked=utc(report.get('source_reverified_utc'))
        require(frozen<=checked<=datetime.now(timezone.utc)
                and isinstance(report.get('source_reverified_reason'),str)
                and report['source_reverified_reason'].strip(),'Pre-freeze audit needs current source reverification')
    require(report.get('evidence_origin') in ('computed','actual-target','real-listener','actual-cabin'),
            'Explicit evidence origin required')
    expected={'TARGET':'actual-target','LISTENING':'real-listener','CABIN':'actual-cabin'}.get(gate['scope'])
    require(expected is None or report['evidence_origin']==expected,'Proxy cannot satisfy physical gate')
    domain,count=_domain(gate,report,root,freeze,candidate_sha256)
    common=dict(sample_count=count,count_unit=gate['count_unit'],
                domain_manifest_sha256=report['domain_manifest_sha256'])
    if count<gate['min_count']:
        return dict(common,state='UNVERIFIED',reason='Insufficient independently identified coverage')
    value=report.get('value');case=None
    if gate['kind']=='bool':
        require(type(value) is bool and report.get('exact_log_ratio') is None,'Exact boolean evidence required')
        passed=value is gate['threshold']
    else:
        value,case=_measurement_value(gate,report)
        if case=='zero_over_zero_unknown':
            return dict(common,state='UNVERIFIED',reason='Exact zero/zero log ratio has no defined measurement',
                        value=None,exact_case=case)
        if gate.get('requires_positive_coherent_transfer',False):
            sign=report.get('coherent_sign')
            require(type(sign) is int and sign in (-1,0,1),'Signed coherent-transfer evidence required')
            ratio=report.get('exact_log_ratio')
            require(ratio is None or ((ratio['numerator']==0)==(sign==0)),
                    'Coherent sign and exact amplitude numerator contradict')
            if sign<=0:
                return dict(common,state='FAIL',reason='Nonpositive coherent transfer is not retained amplitude',
                            value=value if math.isfinite(value) else None,exact_case=case)
        passed=value>=gate['threshold'] if gate['kind']=='min' else value<=gate['threshold']
        if gate['id']=='blind_listener_preference_ci95_lower':passed=value>gate['threshold']
    return dict(common,state='PASS' if passed else 'FAIL',value=value if type(value) is bool or math.isfinite(value) else None,
                threshold=gate['threshold'],exact_case=case)


def evaluate(freeze_owner,evidence_owner,candidate_sha256):
    require(valid_hash(candidate_sha256),'Exact candidate manifest SHA256 required')
    protocol,freeze=load_freeze(freeze_owner);root=Path(evidence_owner)
    require(root.is_dir(),'Evidence owner required')
    results=[]
    for gate in protocol['gates']:
        path=root/(gate['id']+'.json')
        if not path.exists():
            result=dict(state='UNVERIFIED',reason='No measurement report')
        else:
            try:
                report,raw=read(path);result=check_gate(gate,report,root,freeze,candidate_sha256)
                result['report_sha256']=sha(raw)
            except (ValueError,KeyError,TypeError,OSError,OverflowError) as error:
                result=dict(state='FAIL',reason='Invalid evidence: '+str(error))
        results.append(dict(id=gate['id'],scope=gate['scope'],
                            acoustic_only=gate.get('acoustic_only',False),**result))
    for scope in COHERENT_SCOPES:
        cohort={r['domain_manifest_sha256'] for r in results
                if r['scope']==scope and 'domain_manifest_sha256' in r}
        if len(cohort)>1:
            for result in results:
                if result['scope']==scope and 'domain_manifest_sha256' in result:
                    result.update(state='FAIL',reason='Incompatible '+scope+' evidence cohorts')
    required=[r for r in results if not r['acoustic_only']]
    install=bool(required) and all(r['state']=='PASS' for r in required)
    acoustic=install and all(r['state']=='PASS' for r in results)
    return dict(schema='upmix-release-acceptance-v2',evaluator_id=EVALUATOR,
                protocol_sha256=freeze['protocol_sha256'],
                metric_specification_sha256=freeze['metric_specification_sha256'],
                candidate_sha256=candidate_sha256,gates=results,
                status='acoustically-validated-release' if acoustic else
                'installation-ready-controlled-car-candidate' if install else 'research-candidate',
                installation_ready=install,acoustically_validated=acoustic,
                integrity_only_disclaimer='Producer measurements, eligibility and physical/listener claims require independent provenance audit')


def main():
    parser=argparse.ArgumentParser(description=__doc__);commands=parser.add_subparsers(dest='command',required=True)
    p=commands.add_parser('seal');p.add_argument('--protocol',required=True);p.add_argument('--specification',required=True);p.add_argument('--output',required=True)
    p=commands.add_parser('evaluate');p.add_argument('--freeze',required=True);p.add_argument('--evidence',required=True);p.add_argument('--candidate-sha256',required=True);p.add_argument('--output',required=True)
    args=parser.parse_args()
    if args.command=='seal':print(json.dumps(seal(args.protocol,args.specification,args.output),indent=2))
    else:
        report=evaluate(args.freeze,args.evidence,args.candidate_sha256);write_new(args.output,report)
        print(json.dumps({k:report[k] for k in ('status','installation_ready','acoustically_validated')},indent=2))


if __name__=='__main__':main()
