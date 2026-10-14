"""Join host-monotonic bridge, QEMU backend and virtual USB packet stages."""
from collections import defaultdict, deque
import json
from pathlib import Path

from .bridge_queues import distribution, LIMIT, ROWS


def read_rows(path):
    if not 1 <= path.stat().st_size <= 2*LIMIT: raise ValueError('trace size outside bounds')
    rows = []
    with path.open() as file:
        for line in file:
            if len(line) > 4096: raise ValueError('trace row too large')
            rows.append(json.loads(line))
            if len(rows) > 2*ROWS+1024: raise ValueError('trace row limit exceeded')
    return rows


def key(row):
    return tuple(row['flow']) + tuple(row[k] for k in ('seq','ack','flags','payload_bytes'))


def report(native: Path, bridge: Path):
    rows = read_rows(native)
    begin = [r for r in rows if r.get('event') == 'begin']
    end = [r for r in rows if r.get('event') == 'end']
    if (len(begin) != 1 or len(end) != 1 or begin[0].get('schema') != 1 or
            rows[0].get('event') != 'begin' or rows[-1].get('event') != 'end'):
        raise ValueError('incomplete native trace framing')
    packets = [r for r in rows if r.get('event') == 'packet']
    if end[0].get('rows') != len(packets): raise ValueError('native trace count mismatch')
    if any(r.get('event') not in ('begin','packet','end') for r in rows):
        raise ValueError('unknown native trace event')
    observations = []
    for row in packets:
        if row.get('stage') not in ('socket_rx','usb_accept','usb_complete','usb_tx'):
            raise ValueError('unknown native packet stage')
        if type(row.get('mono_ns')) is not int or row['mono_ns'] < 0:
            raise ValueError('invalid native timestamp')
        observations.append((row['mono_ns'],row['stage'],row))
    bridge_rows = read_rows(bridge)
    if not bridge_rows or bridge_rows[0].get('event')!='begin' or bridge_rows[-1].get('event')!='end':
        raise ValueError('incomplete bridge trace')
    for row in bridge_rows:
        if row.get('event')=='packet' and row.get('flow'):
            if row['direction']=='rx':
                # Use start: QEMU may already read while sendall is completing.
                observations.append((row['send_start_mono_ns'],'bridge_send',row))
            else:
                observations.append((row['read_mono_ns'],'bridge_return',row))
    observations.sort(key=lambda item:item[0])
    previous = defaultdict(deque)
    awaiting_ack = defaultdict(deque)
    statistics = defaultdict(lambda:defaultdict(list))
    unmatched = defaultdict(int)
    links = {'socket_rx':('bridge_send','bridge_to_qemu'),
             'usb_accept':('socket_rx','qemu_to_usb_accept'),
             'usb_complete':('usb_accept','usb_transfer'),
             'bridge_return':('usb_tx','ack_to_bridge')}
    for stamp,stage,row in observations:
        flow=tuple(row['flow'])
        if not any(p in (6030,6031) for p in flow[2:]): continue
        port=str(flow[3] if flow[3] in (6030,6031) else flow[2])
        k=key(row)
        if stage in links:
            parent,label=links[stage]
            pending=previous[(parent,k)]
            if pending:
                before=pending.popleft()
                statistics[port][label].append((stamp-before)/1e6)
            else: unmatched[stage]+=1
        if stage != 'bridge_return': previous[(stage,k)].append(stamp)
        if stage=='usb_complete' and row['payload_bytes'] and flow[3] in (6030,6031):
            awaiting_ack[flow].append(((row['seq']+row['payload_bytes'])&0xffffffff,stamp))
        if stage=='usb_tx' and row['flags']&16 and not row['flags']&4 and flow[2] in (6030,6031):
            reverse=(flow[1],flow[0],flow[3],flow[2])
            remaining=deque()
            for seqend,arrival in awaiting_ack[reverse]:
                if (row['ack']-seqend)&0xffffffff < 0x80000000:
                    statistics[port]['usb_complete_to_ack'].append((stamp-arrival)/1e6)
                else: remaining.append((seqend,arrival))
            awaiting_ack[reverse]=remaining
    return {'schema':1,'native_rows':len(packets),'native_complete':True,
            'native_truncated':end[0].get('truncated') is True,
            'bridge_complete':bridge_rows[-1].get('complete') is True,
            'bridge_truncated':bridge_rows[-1].get('truncated') is True,
            'ports':{p:{s:distribution(v) for s,v in stats.items()} for p,stats in statistics.items()},
            'unmatched_stages':dict(unmatched),
            'unacknowledged_delivered_segments':sum(map(len,awaiting_ack.values())),
            'limitations':['host monotonic clocks; usb_complete means final CDC buffer copied to virtual USB packet',
                'usb_complete is before QNX USB/TCP application processing, not hardware reception or decode',
                'usb_complete_to_ack includes QNX processing, delayed ACK and outbound USB transfer',
                'socket backpressure can keep packets in the host socket before QEMU parses them',
                'native trace itself adds diagnostic overhead; no sender encoder completion inferred']}


if __name__=='__main__':
    import argparse
    from . import lab
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native',type=Path)
    parser.add_argument('bridge',type=Path)
    parser.add_argument('--output',type=Path,default=lab.OUT/'usb-queues-report.json')
    args=parser.parse_args()
    target=args.output.resolve()
    if not target.is_relative_to(lab.OUT.resolve()) or target.suffix!='.json':
        parser.error('report must remain JSON inside the owned vphone lab')
    result=report(args.native,args.bridge)
    target.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
