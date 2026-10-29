#!/usr/bin/env python3
import argparse,hashlib,json,os,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--reference-project',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];helper=root/'deploy/binpatch/wifi_runtime.py';res=root/'deploy/wifi/runtime';ref=a.reference_project
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
meta=json.loads((res/'manifest.json').read_text());rows=[]
interpreters=[shutil.which('python3')];mp=shutil.which('micropython')
if mp:interpreters.append(mp)
for py in interpreters:
 with tempfile.TemporaryDirectory(prefix='wifi-install-') as temporary:
  t=Path(temporary);d=t/'devnp-mrvl_wlan-sdiorm.so';stack=t/'io-pkt-v6-hc';r=t/'resources';saved=Path(str(d)+'.carplay-stock')
  def fresh(data=None):
   for f in t.iterdir():
    if f.is_dir():shutil.rmtree(f)
    else:f.unlink()
   shutil.copytree(res,r);d.write_bytes(data if data is not None else (r/'stock.so').read_bytes());shutil.copyfile(ref/'build/wifi-latency-re-20261022/io-pkt-v6-hc',stack)
  def run(name,action,code=0,train='MHI2Q_US_AUG22_P5087'):
   args=[py,str(helper),action,str(r),str(d)]
   if action in ['install','check']:
    args.append(str(stack))
    if train is not None:args.append(train)
   out=subprocess.run(args,capture_output=True,text=True)
   assert out.returncode==code,(name,py,out.returncode,out.stdout,out.stderr)
   rows.append({'name':name,'interpreter':Path(py).name,'status':'pass','returncode':code,'output':out.stdout.strip()})
  fresh();run('stock-preflight','check');run('stock-install','install');assert sha(d)==meta['target_sha256'] and sha(saved)==meta['stock_sha256'];first=saved.stat().st_mtime_ns
  run('idempotent','install',3);assert saved.stat().st_mtime_ns==first
  run('rollback','restore');assert sha(d)==meta['stock_sha256'] and saved.exists();run('rollback-idempotent','restore',3)
  for version,path in [('legacy-ca529','build/wifi-driver-re-20261018/devnp-mrvl_wlan-sdiorm.PATCHED.so'),('regressed-ff67','build/wifi-latency-full-20261022/devnp-mrvl_wlan-sdiorm.so'),('189a','build/wifi-latency-full-20261023/devnp-mrvl_wlan-sdiorm.so'),('662c','build/wifi-latency-reviewfix-20261023/devnp-mrvl_wlan-sdiorm.so')]:
   old=(ref/path).read_bytes();fresh(old);previous=sha(d);run(version+'-upgrade','install');assert sha(d)==meta['target_sha256'];assert sha(Path(str(d)+'.carplay-preupgrade.'+previous))==previous and sha(saved)==meta['stock_sha256']
  fresh((r/'driver.so').read_bytes());run('already-target-without-backup','install',3);assert sha(saved)==meta['stock_sha256']
  fresh(b'unknown-newer-runtime');before=d.read_bytes();run('unknown-newer-preserved','install',1);assert d.read_bytes()==before and not saved.exists();run('unknown-rollback-preserved','restore',1);assert d.read_bytes()==before
  fresh();saved.write_bytes(b'stale');before=d.read_bytes();run('stale-stock-backup','install',1);assert d.read_bytes()==before and saved.read_bytes()==b'stale'
  fresh();stack.write_bytes((root/'deploy/wifi/stack-on-device-reference').read_bytes());run('exact-on-device-stripped-stack','install');assert sha(d)==meta['target_sha256']
  fresh();stack.unlink();run('missing-io-pkt','install',1);assert sha(d)==meta['stock_sha256']
  fresh();stack.write_bytes(b'wrong');run('mismatched-io-pkt','install',1);assert sha(d)==meta['stock_sha256']
  fresh();(r/'driver.so').write_bytes(b'tamper');run('tampered-payload','install',1);assert sha(d)==meta['stock_sha256']
  fresh();stage=Path(str(d)+'.carplay-new');stage.write_bytes((r/'driver.so').read_bytes());run('preexisting-stage-preserved','install',1);assert sha(stage)==meta['target_sha256'] and sha(d)==meta['stock_sha256']
  fresh();Path(str(d)+'.carplay-install-lock').mkdir();run('busy-lock-preserved','install',1);assert sha(d)==meta['stock_sha256']
  fresh();bad=Path(str(d)+'.carplay-preupgrade.'+sha(d));bad.write_bytes(b'bad');run('wrong-preupgrade-preserved','install',1);assert bad.read_bytes()==b'bad' and sha(d)==meta['stock_sha256']
  fresh();run('er-p5092-matching-hash-install','install',0,'MHI2Q_ER_AUG22_P5092');assert sha(d)==meta['target_sha256'] and sha(saved)==meta['stock_sha256']
  fresh();run('no-train-arg-install','install',0,None);assert sha(d)==meta['target_sha256'] and sha(saved)==meta['stock_sha256']
  fresh(b'future-runtime');saved.write_bytes((r/'stock.so').read_bytes());run('future-with-valid-old-backup','install',1);run('future-with-valid-old-backup-rollback','restore',1);assert d.read_bytes()==b'future-runtime'
for mode in ['verified','missing-helper','unknown']:
 with tempfile.TemporaryDirectory(prefix='wifi-uninstall-') as temporary:
  t=Path(temporary);app=t/'mnt/app';system=t/'mnt/system';app.mkdir(parents=True);system.mkdir(parents=True);d=app/'devnp-mrvl_wlan-sdiorm.so';before=(res/'driver.so').read_bytes() if mode!='unknown' else b'future-runtime';d.write_bytes(before);saved=Path(str(d)+'.carplay-stock');saved.write_bytes((res/'stock.so').read_bytes())
  mod=t/'mod';mod.mkdir();source=(root/'uninstall_MoreIncredibleBash/mod/custom.sh').read_text().replace('/mnt/',str(t)+'/mnt/').replace('/tmp/',str(t)+'/tmp/');(t/'tmp').mkdir();(mod/'custom.sh').write_text(source)
  if mode!='missing-helper':
   (mod/'carplay/wifi').mkdir(parents=True);shutil.copyfile(helper,mod/'carplay/wifi_runtime.py');shutil.copytree(res,mod/'carplay/wifi/runtime')
  out=subprocess.run([shutil.which('mksh') or '/bin/sh',str(mod/'custom.sh')],env=dict(os.environ,CARPLAY_SAVE_WAIT='0',CARPLAY_MICROPYTHON=mp or interpreters[0]),capture_output=True,text=True);assert out.returncode==0,out.stderr
  assert (sha(d)==meta['stock_sha256']) if mode=='verified' else (d.read_bytes()==before)
  assert saved.exists();rows.append({'name':'standalone-uninstall-'+mode,'status':'pass','output':out.stdout})
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps({'status':'pass','cases':len(rows),'target_sha256':meta['target_sha256'],'rows':rows},indent=2)+'\n');print('PASS',len(rows),'cases',a.output)
