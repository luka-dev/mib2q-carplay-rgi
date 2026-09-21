"""Read-only diagnostic of current renderer through the existing AmpChain.
Generated WAV/JSON artifacts stay beside this script. No vehicle operations.
"""
import gc
import hashlib
import json
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import welch

ROOT = Path('/Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay')
OUT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'tools/soundamp-virtual-cabin'))
from amp_dsp_chain import AmpChain, SR

BANDS = [(20,60),(60,120),(120,250),(250,500),(500,2000),(2000,8000)]
GROUPS = {'front_mid':[0,1], 'rear':[8,9], 'centre':[10],
          'centre_tweeter':[11], 'woofer':[12,13], 'sub':[14,15]}
VARIANTS = {'gain12288_fb0':(20*np.log10(3),0), 'gain4096_fb0':(0,0),
            'gain4096_fb3':(0,-3)}
renderer = str(OUT / 'renderer')
chain = AmpChain()

def powers(x):
    # Fixed source window; callers compensate the 512-frame upmixer latency.
    f, p = welch(x, SR, nperseg=32768, noverlap=16384, axis=0,
                 detrend=False, scaling='density')
    return np.array([p[(f>=lo)&(f<hi)].sum(axis=0)*(f[1]-f[0])
                     for lo,hi in BANDS]).T

def db(x):
    return 10*np.log10(np.maximum(x,1e-30))

def measure(x, delay):
    stage = chain.process(x)
    sl = slice(5*SR+delay,55*SR+delay)
    y = stage['out'][sl]
    p = powers(y)
    return {'group_power_db':{k:db(p[idx].mean(axis=0)).tolist() for k,idx in GROUPS.items()},
            'output_peak_dbfs':(20*np.log10(np.maximum(np.max(np.abs(y),axis=0),1e-15))).tolist(),
            'stage_peak_dbfs':{k:float(20*np.log10(max(np.max(np.abs(v[sl])),1e-15))) for k,v in stage.items()},
            'pcm6_power_db':db(powers(x[sl])).tolist(),
            'pcm6_peak_dbfs':(20*np.log10(np.maximum(np.max(np.abs(x[sl]),axis=0),1e-15))).tolist()}

results = {}
for source in sorted((ROOT/'tests/fixtures/private-audio/heldout').glob('*.mp3')):
    name = source.stem.replace('_spotdown.org','')
    wav = OUT / (name + '.wav')
    if not wav.exists():
        subprocess.run(['ffmpeg','-v','error','-i',str(source),'-t','60','-ar',str(SR),'-ac','2',str(wav)],check=True)
    x, rate = sf.read(wav,always_2d=True)
    assert rate==SR and len(x)>=56*SR
    row = {'source':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest()}
    dry = np.zeros((len(x),6)); dry[:,:2]=x
    row['dry_bank1'] = measure(dry,0)
    del dry
    for variant,(gain,fb) in VARIANTS.items():
        dest = OUT / (name + '-' + variant + '.wav')
        if not dest.exists():
            cmd=[renderer,'--mode','hssr-universal','--model',str(ROOT/'build/hssr_model.bin'),
                 '--guard-model',str(ROOT/'build/hssr_guard.bin'),'--lfe','assist',
                 '--surround','0.40','--lfe-gain',str(gain),'--front-bass',str(fb),str(wav),str(dest)]
            p=subprocess.run(cmd,capture_output=True,text=True,check=True)
            (OUT/(name+'-'+variant+'.log')).write_text(p.stderr)
        y,rate=sf.read(dest,always_2d=True)
        row[variant]=measure(y,512)
        if variant=='gain4096_fb0':
            # Keep identical L/R/Ls/Rs; isolate amplifier response to removing LFE.
            y[:,3]=0
            row['gain4096_fb0_lfe_zeroed']=measure(y,512)
        del y
        gc.collect()
    results[name]=row
    (OUT/'music-results.json').write_text(json.dumps({'bands':BANDS,'groups':GROUPS,
        'window_seconds':[5,55],'latency_compensation':512,'variants':VARIANTS,
        'notes':['AmpChain identity complex block and silent B hypotheses',
                 'dry_bank1 is L/R-only six-channel input, NOT stock bank0',
                 'arithmetic mean electrical powers per group; no acoustic sum',
                 'Sound Check not applied; fixed identical programme scale'],
        'results':results},indent=2))
    print(name,'done',flush=True)

print('\nMean paired output dB differences vs dry L/R-only BANK1 (not stock):')
for v in list(VARIANTS)+['gain4096_fb0_lfe_zeroed']:
    print(v)
    for group in GROUPS:
        if group in ('sub','rear'): continue
        d=np.array([np.array(r[v]['group_power_db'][group])-r['dry_bank1']['group_power_db'][group] for r in results.values()])
        print(group,np.round(d.mean(axis=0),2).tolist())
    tilt=[]
    for r in results.values():
        delta=lambda g,b:r[v]['group_power_db'][g][b]-r['dry_bank1']['group_power_db'][g][b]
        tilt.append(delta('woofer',1)-delta('front_mid',4))
    print('woofer60-120 / front_mid500-2000 tilt delta mean,min,max', np.round([np.mean(tilt),min(tilt),max(tilt)],2))
