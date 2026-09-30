#!/usr/bin/env python3
"""Synthetic route/ducking and fail-closed checks for the NN-free copied host."""
import argparse
import hashlib
import json
import subprocess
import wave
from pathlib import Path
import numpy as np

SR = 48000


def write(path, x):
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(x.shape[0]); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(np.rint(np.clip(x.T, -.999, .999)*32768).astype('<i2').tobytes())


def read(path):
    with wave.open(str(path), 'rb') as w:
        assert w.getframerate() == SR and w.getnchannels() == 6 and w.getsampwidth() == 2
        return np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 6).T.astype(float)/32768


def verify(executable, out):
    out.mkdir(parents=True, exist_ok=False)
    t = np.arange(SR*6)/SR
    eligible = np.array([.12*np.sin(2*np.pi*1000*t), .09*np.sin(2*np.pi*1800*t)])
    vocal = .10*np.sin(2*np.pi*3000*t)*((t >= 2)&(t < 4))
    mix = eligible + vocal
    write(out/'mixture.wav', mix)
    write(out/'eligible.wav', eligible)
    args = ['--mode','hssr-universal','--lfe','assist','--lfe-gain','0','--surround','.3235',
            '--front-bass','-3','--front-retention','.5','--bed-delays','--bed-attack','off',
            '--rear-air','-12','--rear-body','0','--mask-drive','1']
    command = [str(executable), *args, '--rear-audio', str(out/'eligible.wav'), str(out/'mixture.wav'), str(out/'output.wav')]
    p = subprocess.run(command, capture_output=True, text=True)
    if p.returncode or 'old_neural_invocations=0, old_blobs=0' not in p.stderr:
        raise RuntimeError(p.stderr)
    y = read(out/'output.wav')
    # Interior seconds avoid startup, synthesis delay and entry boundaries.
    before = y[4:, SR:SR*2]
    singing = y[4:, SR*3:SR*4]
    ratios = (singing**2).sum(1)/(before**2).sum(1)
    assert np.max(np.abs(ratios-1)) < .01, ratios
    assert (before**2).sum() > 1 and (y[:2]**2).sum() > 1
    assert np.max(np.abs(y[2])) == 0, 'C should stay zero in this route'
    assert abs(np.corrcoef(before)[0,1]) < .01, 'Distinct L/R carriers collapsed'
    assert np.max(np.abs(y)) < .99, 'Unexpected saturation'
    # Voice-only mixture with a zero eligible output must not create a rear bed.
    write(out/'voice.wav', np.array([vocal,vocal])); write(out/'zero.wav', eligible*0)
    zero_cmd = [str(executable),*args,'--rear-audio',str(out/'zero.wav'),str(out/'voice.wav'),str(out/'voice6.wav')]
    q = subprocess.run(zero_cmd,capture_output=True,text=True)
    if q.returncode: raise RuntimeError(q.stderr)
    voice6=read(out/'voice6.wav')
    assert np.max(np.abs(voice6[4:])) == 0
    rejects={}
    for name,extra in [('q6d',['--model','forbidden.bin']),('guard',['--guard-model','forbidden.bin']),
                       ('mask',['--mask-model','forbidden.bin']),('missing_audio',[])]:
        c=[str(executable),*args]
        if name != 'missing_audio': c+=['--rear-audio',str(out/'eligible.wav')]
        c+=extra+[str(out/'mixture.wav'),str(out/(name+'.wav'))]
        r=subprocess.run(c,capture_output=True,text=True)
        rejects[name]=r.returncode
        assert r.returncode != 0, name
    write(out/'short.wav',eligible[:,:-1])
    r=subprocess.run([str(executable),*args,'--rear-audio',str(out/'short.wav'),str(out/'mixture.wav'),str(out/'short6.wav')],capture_output=True,text=True)
    rejects['wrong_length']=r.returncode
    assert r.returncode != 0
    result=dict(verified=True, scope='synthetic C audio-only route; no music/network/scene/device result',
                executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                entry_rear_energy_ratios=ratios.tolist(), distinct_rear_correlation=float(np.corrcoef(before)[0,1]),
                voice_only_rear_max=0, legacy_neural_calls=0,legacy_blobs=0, rejected=rejects,
                stdout=p.stdout,stderr=p.stderr)
    (out/'verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print('UNIFIED_AUDIO_HOST_VERIFIED',json.dumps({k:result[k] for k in ['entry_rear_energy_ratios','distinct_rear_correlation','rejected']}))


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--executable',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();verify(a.executable,a.out)
