#!/usr/bin/env python3
"""Bind a local render-only review screen to exact binaries. No quality approval."""
import argparse
import json
from pathlib import Path
from prepare_hssr_native_teacher import digest

ROOT=Path(__file__).resolve().parents[2]
V11='ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--report',type=Path,required=True)
    p.add_argument('--renderer',type=Path,required=True)
    p.add_argument('--profile',choices=['quiet-bass','cabin'],required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();report=json.loads(a.report.read_text())
    expected={'v11-float','quiet-bass-only','quiet-plus1','quiet-minus9',
              'cabin-assist','cabin-no-assist','cabin-plus1','cabin-minus9'}
    ids={r['id'] for r in report['rows']}
    if (report['model_sha256']!=V11 or report['renderer_sha256']!=digest(a.renderer)
            or len(ids)!=14 or len(report['rows'])!=14*len(expected)):
        raise ValueError('incomplete or mismatched render screen')
    for item in ids:
        if {r['variant'] for r in report['rows'] if r['id']==item}!=expected:
            raise ValueError('missing variant')
    legacy=report['legacy_compatibility']
    if {r['id'] for r in legacy}!=ids or not all(r['byte_exact'] for r in legacy):
        raise ValueError('legacy compatibility not verified')
    over=sum(c['preclip_samples'] for r in report['rows']
        for c in r['input_matrix_buses']['channels'].values())
    if over:raise ValueError('known matrix overload')
    peak=max(c['peak'] for r in report['rows'] for stage in ['pcm','input_matrix_buses']
             for c in r[stage]['channels'].values())
    if peak>10**(-1/20)+.00015:raise ValueError('linked -1dBFS margin not retained')
    result=dict(kind='V13-render-only-digital-safety',profile=a.profile,model_sha256=V11,
        renderer_sha256=digest(a.renderer),offline_screen_passed=True,
        build_sha256=dict(hook=digest(ROOT/'build/libcarplay_hook.so'),
            apac=digest(ROOT/'build/libcarplay_apac.so'),renderer=digest(a.renderer)),
        report_sha256={str(a.report):digest(a.report)},new_trained_model_selected=False,
        vehicle_approved=False,separation_quality_approved=False,
        limitations=['Known matrix/sample peak only; no acoustic or speaker-safety guarantee.',
                    'Rear selectivity still requires mixture-conditioned review and listening.',
                    'Existing 7-song regression corpus, not a fresh blind test.'])
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(a.output)

if __name__=='__main__':main()
