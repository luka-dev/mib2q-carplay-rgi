"""Run inside wireless.perf_guard; collect serialized candidate correctness."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/wifi-driver-perf-20261023'
CANDIDATE=OUT/'candidate'

def main():
    global OUT,CANDIDATE
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--candidate',type=Path,default=CANDIDATE)
    ap.add_argument('--out',type=Path,default=OUT)
    a=ap.parse_args();OUT=a.out.resolve();CANDIDATE=a.candidate.resolve()
    OUT.mkdir(parents=True,exist_ok=True)
    sources=[ROOT/p for p in (
        'wireless/driver_re/full_tx_runtime_probe.py','wireless/driver_re/full_rx_runtime_probe.py',
        'wireless/driver_re/qnx_full_driver_loader.py','wireless/driver_re/test_wifi_latency_policy.c',
        'wireless/driver_re/wifi_latency_runtime.c','wireless/driver_re/wifi_latency_policy.c',
        'wireless/driver_re/wifi_latency_policy.h','wireless/integration_latency/driver_pair_probe.c')]
    fingerprint=lambda:{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in sources}
    before=fingerprint()
    rows=[]
    def run(name,args,env=None):
        p=subprocess.run(args,cwd=ROOT,env=env,capture_output=True,text=True)
        path=OUT/(name+'.log');path.write_text(p.stdout+p.stderr)
        rows.append({'name':name,'status':'pass'if p.returncode==0 else'fail','log':str(path),'command':args})
        if p.returncode:raise RuntimeError(f'{name}: {path}')
    binary=OUT/'host-policy-asan'
    run('host-policy-build',['clang','-std=c11','-Wall','-Wextra','-Werror','-pthread','-fsanitize=address,undefined','-fno-omit-frame-pointer','wireless/driver_re/wifi_latency_policy.c','wireless/driver_re/test_wifi_latency_policy.c','-o',str(binary)])
    run('host-policy-asan',[str(binary)])
    env=dict(os.environ,WIFI_LATENCY_DIR=str(CANDIDATE))
    run('full-tx',[sys.executable,'-m','wireless.driver_re.full_tx_runtime_probe'],env)
    run('full-rx',[sys.executable,'-m','wireless.driver_re.full_rx_runtime_probe'],env)
    run('qnx-positive',[sys.executable,'-m','wireless.driver_re.qnx_full_driver_loader','--driver',str(CANDIDATE/'devnp-mrvl_wlan-sdiorm.so'),'--out-dir',str(OUT/'qnx-loader')])
    run('qnx-negative',[sys.executable,'-m','wireless.driver_re.qnx_full_driver_loader','--driver','build/wifi-latency-full-20261022/devnp-mrvl_wlan-sdiorm.so','--out-dir',str(OUT/'qnx-deadlock-control'),'--expect-deadlock'])
    result={'status':'pass','binary_sha256':hashlib.sha256((CANDIDATE/'devnp-mrvl_wlan-sdiorm.so').read_bytes()).hexdigest(),'checks':rows,'limits':'Serialized local fixtures; no HU/RF/CPU or end-to-end timing proof.'}
    if fingerprint()!=before:raise RuntimeError('validation source changed during execution')
    result['source_sha256']=before
    result['harness_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    (OUT/'validation.json').write_text(json.dumps(result,indent=2)+'\n');print(OUT/'validation.json')
if __name__=='__main__':main()
