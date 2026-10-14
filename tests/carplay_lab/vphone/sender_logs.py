"""Bounded native airplayd observations; no encoder or frame scheduling changes."""
import argparse
import json
import re
import time
import tempfile
from pathlib import Path

from . import lab


def summarize(value):
    result=value.get('result',{})
    entries=result.get('entries',[])
    if not isinstance(entries,list) or len(entries)>5000: raise ValueError('invalid sender log bounds')
    stats,overflow,encoder=[],[],[]
    for row in entries:
        message=row.get('message','')
        if not isinstance(message,str) or len(message)>65536: raise ValueError('invalid sender log entry')
        if 'fvdp_statsReport: stats/sec:' in message:
            stats.append({'date':row.get('date'),'counters':{
                name:int(count) for name,count in re.findall(r'\|\s*(\w+)\s+(\d+)',message)}})
        if re.search(r'dropped \d+ frames due to sink overflow',message):
            overflow.append({'date':row.get('date'),'message':message})
        if 'ParavirtualizedVideoEncoder_CreateInstanceWithSpecification' in message:
            encoder.append({'date':row.get('date'),'message':message})
    return {'log_entries':len(entries),'truncated':result.get('truncated') is True,
            'stats_per_second':stats,'sink_overflow_notifications':overflow,'encoder_instances':encoder,
            'limitations':['stats for both virtual displays can be interleaved and have private processor identities',
                'aggregate stats/sec counters do not identify per-frame encoder completion or BPF arrival',
                'sink overflow is sender-side evidence; its cause requires transport correlation',
                'configured 30 fps and idle encodes do not prove 30 unique map frames per second']}


def capture(seconds=20, max_lines=1800):
    if type(seconds) is not int or not 1<=seconds<=25 or type(max_lines) is not int or not 1<=max_lines<=5000:
        raise ValueError('sender log capture outside bounds')
    settings=lab.profile()
    lab.require_machine(settings,'running')
    before=time.time_ns()
    reply=lab.rpc(settings,'logs.syslog',{'seconds':seconds,'process':'airplayd','max_lines':max_lines})
    return {'machine':settings['machine'],'host_start_ns':before,'host_end_ns':time.time_ns(),
            'reply':reply,'summary':summarize(reply)}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds',type=int,default=20)
    parser.add_argument('--max-lines',type=int,default=1800)
    parser.add_argument('--output',type=Path,default=lab.OUT/'sender-log.json')
    args=parser.parse_args()
    target=args.output.resolve()
    if not target.is_relative_to(lab.OUT.resolve()) or target.suffix!='.json':
        parser.error('sender log must remain JSON inside the owned vphone lab')
    value=capture(args.seconds,args.max_lines)
    target.parent.mkdir(parents=True,exist_ok=True)
    temporary=None
    try:
        with tempfile.NamedTemporaryFile(mode='w',dir=target.parent,prefix='.sender-log-',delete=False) as file:
            temporary=Path(file.name)
            json.dump(value,file,indent=2)
            file.write('\n')
        temporary.replace(target)
    finally:
        if temporary is not None:temporary.unlink(missing_ok=True)
    print(json.dumps(value['summary'],indent=2))
