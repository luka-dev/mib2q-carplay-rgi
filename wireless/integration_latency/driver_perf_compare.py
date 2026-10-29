"""Run under perf_guard: repeat instruction profiling in alternating A/B order."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/wifi-driver-perf-20261023'

def main():
    global OUT
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline',type=Path,default=ROOT/'build/wifi-driver-finished-20261023')
    ap.add_argument('--candidate',type=Path,default=OUT/'candidate')
    ap.add_argument('--out',type=Path,default=OUT)
    a=ap.parse_args();OUT=a.out.resolve();OUT.mkdir(parents=True,exist_ok=True)
    reports=[]
    for index,name in enumerate(('baseline','optimized','optimized','baseline')):
        folder=a.baseline.resolve()if name=='baseline'else a.candidate.resolve()
        target=OUT/f'profile-{index}-{name}'
        subprocess.run([sys.executable,'-m','wireless.integration_latency.driver_perf','--candidate',str(folder),'--out',str(target)],cwd=ROOT,check=True)
        reports.append((name,target/'profile.json',json.loads((target/'profile.json').read_text())))
    # Exact instruction totals must be repeatable despite reversed execution
    # order. No host-time metric is used to rank these binaries.
    for name in ('baseline','optimized'):
        rs=[d for n,p,d in reports if n==name]
        assert rs[0]['tx']==rs[1]['tx'] and rs[0]['rx']==rs[1]['rx'],name
    baseline=reports[0][2];candidate=reports[1][2];deltas={}
    for key,value in baseline['tx'].items():
        if not isinstance(value,dict)or'instruction_total'not in value:continue
        a=value['instruction_total'];b=candidate['tx'][key]['instruction_total']
        deltas[key]={'baseline_instructions':a,'candidate_instructions':b,'reduction_percent':round(100*(a-b)/a,3)}
    result={'status':'pass','baseline_sha256':baseline['binary_sha256'],'candidate_sha256':candidate['binary_sha256'],
        'order':['baseline','optimized','optimized','baseline'],'repeated_instruction_profiles_identical':True,
        'reports':[{'variant':n,'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}for n,p,d in reports],
        'tx_deltas':deltas,'limits':baseline['limits'],'method':baseline['method']}
    result['harness_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in (Path(__file__),Path(__file__).with_name('driver_perf.py'))}
    (OUT/'comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(OUT/'comparison.json')
if __name__=='__main__':main()
