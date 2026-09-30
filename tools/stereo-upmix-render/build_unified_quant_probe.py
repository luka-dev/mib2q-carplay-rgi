#!/usr/bin/env python3
"""Build isolated diagnostic C copies: only weight or activation rounding.

Never edits production/runtime sources. No mixed-precision deployment recipe.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def main(a):
    if a.out.exists():raise FileExistsError('Preserve prior quantization probe build')
    a.out.mkdir(parents=True)
    src=a.source/'unified_runtime.c';original=src.read_text()
    line='if (w->storage) sum=(float)dot_int8(q,w->q+i*w->cols,w->cols)*scale*w->scale[i];'
    if original.count(line)!=1:raise ValueError('Frozen quantized affine pattern differs')
    weights_only=original[:original.index('static int round_even(')]+original[original.index('static void linear('):]
    setup_start=weights_only.index('    const Tensor *w=l->weight; unsigned i,j; int8_t q[MAX_INPUT]; float scale=1;')
    setup_end=weights_only.index('    for (i=0;i<w->rows;i++) {',setup_start)
    weights_only=weights_only[:setup_start]+'    const Tensor *w=l->weight; unsigned i,j;\n'+weights_only[setup_end:]
    weights_only=weights_only.replace(line,'if (w->storage) for (j=0;j<w->cols;j++) sum+=x[j]*(float)w->q[i*w->cols+j]*w->scale[i];')
    variants={
        'weights_only':weights_only,
        'activations_only':original.replace('if (w->storage) {\n        float maximum=0;','if (1) {\n        float maximum=0;').replace('sum+=x[j]*w->data[i*w->cols+j];','sum+=((float)q[j]*scale)*w->data[i*w->cols+j];'),
    }
    if variants['activations_only']==original:raise ValueError('Activation diagnostic copy not changed')
    records={}
    for tag,source in variants.items():
        folder=a.out/tag;folder.mkdir()
        (folder/'unified_runtime.c').write_text(source)
        for name in ('unified_runtime.h','unified_runtime_cli.c'):shutil.copyfile(a.source/name,folder/name)
        executable=folder/('upmix_quant_'+tag+('.exe' if a.windows else ''))
        command=[a.compiler,'-O3','-std=c99','-Wall','-Wextra','-Werror',str(folder/'unified_runtime.c'),str(folder/'unified_runtime_cli.c'),'-lm','-o',str(executable)]
        completed=subprocess.run(command,capture_output=True,text=True)
        (folder/'compiler.log').write_text(completed.stdout+completed.stderr)
        if completed.returncode:raise RuntimeError('Diagnostic compile failed')
        records[tag]=dict(source_sha256=hashlib.sha256(source.encode()).hexdigest(),executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),command=command)
    manifest=dict(original_runtime_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),builds=records,
                  scope='diagnostic C copies only: weights_only reads int8 blob but leaves vector inputs float; activations_only reads float blob but rounds every matrix input; no model/source mutation or deployment quality claim')
    (a.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',default='cc');p.add_argument('--windows',action='store_true');main(p.parse_args())
