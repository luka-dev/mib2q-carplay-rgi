"""Bind the selected driver's repeated profiles, safety checks and tradeoffs."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/wifi-driver-perf-20261023'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    cmp_path=OUT/'selected-comparison/comparison.json';val_path=OUT/'selected-validation/validation.json'
    comparison=json.loads(cmp_path.read_text());validation=json.loads(val_path.read_text())
    final=OUT/'candidate-final/devnp-mrvl_wlan-sdiorm.so';identity=sha(final)
    assert comparison['status']==validation['status']=='pass'
    assert comparison['candidate_sha256']==validation['binary_sha256']==identity
    assert comparison['repeated_instruction_profiles_identical']
    profile=lambda p:json.loads(Path(p).read_text())
    baseline=profile(comparison['reports'][0]['path']);candidate=profile(comparison['reports'][1]['path'])
    intermediate_path=OUT/'cookie-comparison/profile-0-baseline/profile.json';intermediate=profile(intermediate_path)
    h=sha(Path(__file__).with_name('driver_perf.py'))
    assert baseline['harness_sha256']==candidate['harness_sha256']==intermediate['harness_sha256']==h
    assert intermediate['binary_sha256']==sha(OUT/'candidate/devnp-mrvl_wlan-sdiorm.so')
    for key in ('stock_tx_ablation_completion32x4','stock_tx_ablation_callback_dispatch'):
        assert baseline['tx'][key]==candidate['tx'][key]
    assert baseline['rx']==candidate['rx'],'TX optimization unexpectedly changed RX profile'
    for path,h in validation['source_sha256'].items():assert sha(ROOT/path)==h,path
    def delta(a,b):return {'before_instructions':a,'after_instructions':b,'reduction_percent':round(100*(a-b)/a,3)}
    hint={k:delta(intermediate['tx'][k]['instruction_total'],candidate['tx'][k]['instruction_total'])for k in intermediate['tx']if'first_request_retained_cookies_'in k}
    for label,index in [('cold_completion',0),('warm_completion',32)]:
        hint[label]=delta(intermediate['tx']['full_completion32x4']['samples'][index]['instructions'],candidate['tx']['full_completion32x4']['samples'][index]['instructions'])
    hint['callback']=delta(intermediate['tx']['full_callback_dispatch']['instruction_p50'],candidate['tx']['full_callback_dispatch']['instruction_p50'])
    result={'status':'pass','selected_binary':str(final.relative_to(ROOT)),'binary_sha256':identity,'binary_bytes':final.stat().st_size,
        'baseline_sha256':comparison['baseline_sha256'],'prefix_only_sha256':intermediate['binary_sha256'],
        'comparison':{'path':str(cmp_path.relative_to(ROOT)),'sha256':sha(cmp_path)},'validation':{'path':str(val_path.relative_to(ROOT)),'sha256':sha(val_path)},
        'intermediate_profile':{'path':str(intermediate_path.relative_to(ROOT)),'sha256':sha(intermediate_path)},
        'tx_deltas':comparison['tx_deltas'],'isolated_cookie_hint_deltas':hint,'rx_profile_identical':True,
        'optimizations':['Newest-first context lookup starts at monotonic permanent allocated-prefix high-water, not128 unusedslots.','Mutex-protected free-cookie hint always checks ownership flag; misses retain original bounded scan. Quarantined/captured cookies are never recycled.'],
        'retained_safeguards':['Trylock native completion fallback','Stock bitmap/flags after exhaustion','Serial max drains then native fallback without wrap','Recursive global mutex, module pin, immutable context generations and captured cookies'],
        'scope':'ARM instruction/event cost, not walltime or HU CPU. Repeated A/B/B/A profiles identical. Actual QNX safety and paired progress tests pass.',
        'limits':baseline['limits']+['RX remains more instruction-expensive than native cleanRX; no speculative RX lock/age fastpath added.','Four unrelated orphanQEMU workloads load the host. No guest timing or physicalCPU/latency ranking is claimed.'],
        'next_physical_check':'HU traffic at real completion/reconnect rates, CarPlay main/Alt motion and5.1; measure one-coreCPU, latencytails and resource fallback.'}
    result['harness_sha256']=sha(Path(__file__))
    (OUT/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
