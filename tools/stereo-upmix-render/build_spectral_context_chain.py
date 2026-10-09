#!/usr/bin/env python3
"""Build the isolated spectral causal chain without writing Track A or active sources."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import build_unified_audio_host as R


def build(a):
    if a.out.exists():raise FileExistsError('Preserve earlier chain build')
    cc=shutil.which(a.compiler)
    if not cc:raise ValueError('Missing compiler')
    root=Path(__file__).resolve().parents[2];runtime=root/'tools/stereo-upmix-render/unified_spectral_runtime_v1';audio=root/'hook/audio'
    own=root/'tools/stereo-upmix-render/unified_spectral_chain_v1/spectral_chain_cli.c'
    a.out.mkdir(parents=True);copies=a.out/'copies'
    R.build(copies,a.renderer_compiler or a.compiler)
    # Warnings remain errors for new code. Only the unchanged copied legacy
    # traps/dead controller helpers need unused-parameter/function exceptions.
    sources=[(own,False),(runtime/'unified_runtime.c',False),(runtime/'unified_audio_stream.c',False)]
    sources += [(copies/name,True) for name in ('stereo_upmix.c','hssr_dsp.c','hssr_neon.c','hssr_mask.c')]
    sources += [(audio/n,False) for n in ('soundcheck.c','scene_engine_math.c','scene_clusters.c','scene_engine.c','scene_mask.c')]
    common=['-std=c99','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-DSPA_HSSR_TEST_MASK','-DSPA_HSSR_TEST_DIRECT_NO_BED','-I',str(audio),'-I',str(runtime)]
    commands=[];objects=[]
    for i,(src,copied) in enumerate(sources):
        obj=a.out/f'{i}_{src.stem}.o';cmd=[cc,*common]
        if copied:cmd+=['-Wno-unused-function','-Wno-unused-parameter']
        cmd+=['-c',str(src),'-o',str(obj)];commands.append(cmd)
        r=subprocess.run(cmd,capture_output=True,text=True)
        (a.out/f'{i}.compiler.log.txt').write_text(r.stdout+r.stderr)
        if r.returncode:raise RuntimeError('Compilation failed; log preserved: '+str(src))
        objects.append(obj)
    dest=a.out/('upmix_spectral_chain.exe' if 'mingw' in cc else 'upmix_spectral_chain')
    cmd=[cc,*map(str,objects),'-pthread','-lm','-o',str(dest)]
    if 'mingw' in cc:cmd.insert(1,'-static')
    commands.append(cmd);r=subprocess.run(cmd,capture_output=True,text=True)
    (a.out/'link.log.txt').write_text(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError('Link failed; log preserved')
    identity={str(src):R.sha(src) for src,_ in sources}
    result=dict(builder_sha256=R.sha(Path(__file__)),renderer_builder_sha256=R.sha(Path(R.__file__)),
                sources=identity,commands=commands,compiler=subprocess.check_output([cc,'--version'],text=True),
                copied_renderer_manifest_sha256=R.sha(copies/'manifest.json'),executable_sha256=R.sha(dest),
                scope='one causal model plus real copied NN-free C; front1536sample delay;256KB worker stack; no hook/unit change or device/acoustic qualification')
    (a.out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print('SPECTRAL_CAUSAL_CHAIN_BUILT',dest,result['executable_sha256'])


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',default='cc');p.add_argument('--renderer-compiler');build(p.parse_args())
