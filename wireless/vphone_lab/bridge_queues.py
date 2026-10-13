"""Bounded host-clock observations of the owned NCM bridge, without payloads."""
from collections import defaultdict, deque
import json
from pathlib import Path
import struct

from wireless.lab_common.screen_wire_pacing import tcp

LIMIT = 64*1024*1024
ROWS = 131072


def transport(frame):
    item = tcp(frame)
    if item is None: return {}
    flow, seq, flags, payload = item
    offset = 14+(40 if frame[12:14] == b'\x86\xdd' else (frame[14]&15)*4)
    return {'flow':[flow[0].hex(),flow[1].hex(),flow[2],flow[3]],
            'seq':seq,'ack':struct.unpack_from('>I',frame,offset+8)[0],
            'flags':flags,'payload_bytes':len(payload)}


class Journal:
    def __init__(self, path: Path, metadata: dict):
        self.file = path.open('w')
        path.chmod(0o600)
        self.count = 0
        self.file.write(json.dumps({'event':'begin',**metadata})+'\n')

    def packet(self, direction, frame, capture_ns, wall_ns, read_ns, start_ns, end_ns):
        if self.count >= ROWS: return
        self.file.write(json.dumps({'event':'packet','direction':direction,
            'capture_wall_ns':capture_ns,'read_wall_ns':wall_ns,'read_mono_ns':read_ns,
            'send_start_mono_ns':start_ns,'send_end_mono_ns':end_ns,
            'frame_bytes':len(frame),**transport(frame)},separators=(',',':'))+'\n')
        self.count += 1

    def check(self, start_ns, end_ns):
        self.file.write(json.dumps({'event':'endpoint_check','start_mono_ns':start_ns,'end_mono_ns':end_ns})+'\n')

    def close(self, complete):
        self.file.write(json.dumps({'event':'end','complete':complete,
            'rows':self.count,'truncated':self.count>=ROWS})+'\n')
        self.file.close()


def distribution(values):
    ordered = sorted(values)
    def percentile(fraction):
        if not ordered: return None
        index = fraction*(len(ordered)-1)
        lo = int(index)
        return ordered[lo]+(ordered[min(lo+1,len(ordered)-1)]-ordered[lo])*(index-lo)
    return {'samples':len(values),'median_ms':percentile(.5),'p95_ms':percentile(.95),
            'p99_ms':percentile(.99),'max_ms':max(values) if values else None}


def report(path: Path) -> dict:
    if not 1 <= path.stat().st_size <= LIMIT: raise ValueError('queue trace outside bounds')
    statistics = defaultdict(lambda: defaultdict(list))
    pending = defaultdict(deque)
    checks, metadata, footer, count, negative_age = [], None, None, 0, 0
    with path.open() as file:
        for line in file:
            if len(line)>4096: raise ValueError('queue trace row too large')
            row=json.loads(line)
            event=row.get('event')
            if footer is not None: raise ValueError('queue trace contains events after its footer')
            if event=='begin':
                if metadata is not None: raise ValueError('duplicate queue trace header')
                metadata=row;continue
            if event=='end': footer=row;continue
            if event=='endpoint_check':
                checks.append((row['end_mono_ns']-row['start_mono_ns'])/1e6);continue
            if event!='packet': raise ValueError('unknown queue trace event')
            count+=1
            if count>ROWS or metadata is None or footer is not None: raise ValueError('queue trace framing/count invalid')
            read,start,end=(row[k] for k in ['read_mono_ns','send_start_mono_ns','send_end_mono_ns'])
            if any(type(v) is not int or v<0 for v in [read,start,end]) or not read<=start<=end:
                raise ValueError('host monotonic stage times invalid')
            direction=row['direction']
            if direction not in ('rx','tx'):raise ValueError('unknown bridge direction')
            flow=row.get('flow')
            key=tuple(flow) if flow else None
            group=str(flow[3]) if direction=='rx' and flow else 'other_'+direction
            stats=statistics[group]
            stats['user_queue'].append((start-read)/1e6)
            stats['send_call'].append((end-start)/1e6)
            age=(row['read_wall_ns']-row['capture_wall_ns'])/1e6
            if direction=='rx':
                if age>=0:stats['capture_to_read'].append(age)
                else:negative_age+=1
            if direction=='rx' and key and flow[3] in (6030,6031) and row['payload_bytes']:
                if len(pending[key])>=8192: raise ValueError('too many unacknowledged screen segments')
                pending[key].append(((row['seq']+row['payload_bytes'])&0xffffffff,end))
            elif direction=='tx' and key and flow[2] in (6030,6031) and row['flags']&16:
                reverse=(flow[1],flow[0],flow[3],flow[2])
                queue=pending[reverse]
                remaining=deque()
                for seq_end,sent in queue:
                    if (row['ack']-seq_end)&0xffffffff < 0x80000000:
                        if read>=sent:statistics[str(flow[2])]['qnx_ack_observed'].append((read-sent)/1e6)
                    else:remaining.append((seq_end,sent))
                pending[reverse]=remaining
    if metadata is None or footer is None or footer.get('rows')!=count:
        raise ValueError('incomplete queue trace')
    return {'metadata':metadata,'complete':footer.get('complete') is True,
            'truncated':footer.get('truncated') is True,'rows':count,
            'ports':{port:{stage:distribution(values) for stage,values in stages.items()}
                     for port,stages in statistics.items()},
            'endpoint_checks':distribution(checks),'negative_wall_clock_age_samples':negative_age,
            'unacknowledged_screen_segments':sum(map(len,pending.values())),
            'limitations':['capture-to-read includes BPF buffering and bridge scheduling, not phone-to-Mac NCM latency',
                'send completion means QEMU host socket acceptance, not guest receipt',
                'QNX ACK observed is an upper bound including guest processing and return bridge scheduling',
                'host realtime capture age is invalid across wall-clock adjustments',
                'no packet loss, encoder completion or physical display latency inferred']}


if __name__ == '__main__':
    import argparse
    from . import lab
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace',type=Path)
    parser.add_argument('--output',type=Path,default=lab.OUT/'bridge-queues-report.json')
    args=parser.parse_args()
    target=args.output.resolve()
    if not target.is_relative_to(lab.OUT.resolve()) or target.suffix!='.json':
        parser.error('queue report must remain JSON inside the owned vphone lab')
    value=report(args.trace)
    target.write_text(json.dumps(value,indent=2)+'\n')
    print(json.dumps(value,indent=2))
