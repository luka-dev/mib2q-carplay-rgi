#!/usr/bin/env python3
"""Conservative block-local affine def/use audit of the full decoded B span.

NOT whole-program alias analysis. Calls/branches/loop joins are explicit cuts;
unknown address arithmetic is reported, not interpreted as absence of access.
Tracks register copies, integer offsets, postincrements and local stack spills.
"""
import argparse
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import re
import struct

from audit_amp_b_mode_control import CENSUS_START, CENSUS_END, CENSUS_SHA256
from inspect_amp_visa import load_shortwords, decode


@dataclass(frozen=True)
class Affine:
    base: str | None
    offset: int

    def json(self): return {'base': self.base, 'offset': self.offset}


def affine_add(a, b, subtract=False):
    if a is None or b is None: return None
    sign = -1 if subtract else 1
    if b.base is None: return Affine(a.base, a.offset+sign*b.offset)
    if not subtract and a.base is None: return Affine(b.base, a.offset+b.offset)
    if subtract and a.base == b.base: return Affine(None, a.offset-b.offset)
    return None


def analyze_instructions(instructions):
    """(pc,size,text,target), aligned order. Symbol names are NOT object proof."""
    bypc = {r[0]: r for r in instructions}; starts = {instructions[0][0]}; cuts = []
    for index, (pc, size, text, target) in enumerate(instructions):
        control = re.search(r'\b(?:CJUMP|CALL|JUMP|RTS|RTI)\b', text)
        if control:
            after = index+(3 if '(DB)' in text else 1)
            if after < len(instructions): starts.add(instructions[after][0])
            if target in bypc: starts.add(target)
            cuts.append({'pc': hex(pc), 'instruction': text, 'reason': 'control-flow/unknown call effects'})
        loop = re.search(r'DO (0x[0-9a-f]+) UNTIL', text)
        if loop:
            starts.add(pc+size)
            end = int(loop[1], 16)
            if end in bypc: starts.add(end+bypc[end][1])
            cuts.append({'pc': hex(pc), 'instruction': text, 'reason': 'loop back-edge'})
    regs = {}; memory = {}; reads = []; stores = []; unresolved = []; calls = []
    def new_block(pc):
        r = {f'{kind}{i}': Affine(f'block:{pc:x}:{kind}{i}', 0)
             for kind in ('R', 'I', 'M', 'S') for i in range(16)}
        # The actual ABI constants used throughout this image; arbitrary M
        # registers stay symbolic. No assumption that R4 is a full B object.
        r.update({f'M{i}': Affine(None, v) for i, v in [(5, 0), (6, 1), (7, -1), (13, 0), (14, 1), (15, -1)]})
        return r
    for pc, size, text, target in instructions:
        if pc in starts: regs = new_block(pc); memory = {}
        old = regs.copy(); writes = {}; post = {}; pending_stores = []
        def value(token):
            if token in old: return old[token]
            try: return Affine(None, int(token, 0))
            except ValueError: return None
        def address(spec):
            if ',' not in spec: return value(spec)
            a, b = spec.split(',')
            if a.startswith('I'):
                advanced = affine_add(value(a), value(b))
                post[a] = advanced if not conditional or advanced == value(a) else None
                return value(a)
            return affine_add(value(a), value(b))
        def load(spec):
            addr = address(spec)
            row = {'pc': hex(pc), 'instruction': text, 'address': None if addr is None else addr.json()}
            reads.append(row)
            if addr is None: unresolved.append(row | {'reason': 'non-affine read address'})
            if addr in memory: return memory[addr]
            return Affine(f'load:{pc:x}:{spec}', 0)
        if text.startswith(('CJUMP ', 'CALL ')):
            calls.append({'pc': hex(pc), 'target': None if target is None else hex(target),
                          'arguments_before_delay_slots': {r: None if old[r] is None else old[r].json() for r in ('R4', 'R8', 'R12')}})
        # Old operands for all parallel clauses. Float operations deliberately
        # destroy integer-address provenance rather than reinterpreting values.
        for clause in re.split(r' \|\| |, (?=(?:[FRIS]\d+|[DP]M\()=?)', text):
            conditional = clause.startswith('IF ')
            clause = re.sub(r'^IF (?:NOT \w+|\w+) ', '', clause)
            m = re.fullmatch(r'([RIMS]\d+)=DM\(([^)]+)\)', clause)
            if m:
                out = load(m[2]); writes[m[1]] = out if not conditional or out == old[m[1]] else None
                continue
            m = re.fullmatch(r'DM\(([^)]+)\)=([\w+\-x]+)', clause)
            if m:
                addr, out = address(m[1]), value(m[2]); pending_stores.append((addr, out, conditional))
                stores.append({'pc': hex(pc), 'instruction': text, 'address': None if addr is None else addr.json(),
                               'value': None if out is None else out.json()})
                continue
            clause = re.sub(r'^(\w+)([+\-])=(\w+)$', r'\1=\1\2\3', clause)
            m = re.match(r'([RFIMS]\d+)=(.*)', clause)
            if not m: continue
            dest, expr = m.groups(); out = None
            if not dest.startswith('F'):
                q = re.fullmatch(r'MODIFY\((\w+),([\w+\-]+)\)', expr)
                if q: out = affine_add(value(q[1]), value(q[2]))
                elif expr.startswith('PASS '): out = value(expr[5:])
                elif re.fullmatch(r'\w+[+\-]\w+', expr):
                    a, op, b = re.split(r'([+\-])', expr); out = affine_add(value(a), value(b), op == '-')
                else: out = value(expr)
            name = 'R'+dest[1:] if dest.startswith('F') else dest
            writes[name] = out if not conditional or out == old[name] else None
        regs.update(post); regs.update(writes)
        for addr, out, conditional in pending_stores:
            # Track only exact local I6-relative spills. Any other store may
            # alias that frame, so invalidate rather than assume disjointness.
            fp = old['I6']
            if addr is not None and fp is not None and addr.base == fp.base and addr.base is not None:
                if conditional: memory.pop(addr, None)
                else: memory[addr] = out
            else: memory.clear()
    interest = set(range(1156, 1174)) | set(range(1097, 1115))
    candidates = [r for r in reads if r['address'] and r['address']['base'] is not None and r['address']['offset'] in interest]
    return {'instructions': len(instructions), 'basic_blocks': len(starts),
            'reads': reads, 'stores': stores, 'call_arguments': calls,
            'candidate_table_relative_reads': candidates, 'non_affine_reads': unresolved, 'cuts': cuts,
            'scope': 'block-local nominal linear-address def/use; no incoming-base, DAG circular/SIMD, or complete alias proof; offsets alone do not establish a B object',
            'unresolved_pointer_consumers': [1162, 1163, 1164, 1165],
            'dead_field_proven': False}


def audit(loader):
    mem = load_shortwords(loader)
    words = [mem[p] for p in range(CENSUS_START, CENSUS_END)]
    digest = hashlib.sha256(struct.pack(f'<{len(words)}H', *words)).hexdigest()
    if digest != CENSUS_SHA256: raise ValueError('B code span fingerprint mismatch')
    rows = []; pc = CENSUS_START
    while pc < CENSUS_END:
        size, text, target = decode(mem, pc)
        if 'UNKNOWN' in text: raise ValueError(f'unknown instruction at{pc:x}')
        rows.append((pc, size, text, target)); pc += size
    if pc != CENSUS_END: raise ValueError('unaligned B code span')
    return {'loader_sha256': hashlib.sha256(loader).hexdigest(), 'code_span_sha256': digest,
            'start': hex(CENSUS_START), 'end': hex(CENSUS_END), **analyze_instructions(rows)}


def smoothing_caller_domains(alias_report, configs, modes):
    """Finite caller-domain proof, conditional on retained counts/normal maps.

No intrinsic upper bound exists in12FAF2. This enumerates its callers in the
hash-validated span, not arbitrary external/indirect calls or mutated pointers.
"""
    expected = {
        '0x12faf2': [0x12dc8a, 0x12dc92, 0x12dc9d, 0x12dca8, 0x12dcb2, 0x12f454, 0x132d23],
        '0x12dbde': [0x12b8ba, 0x12ec4d, 0x13138c, 0x1313e6],
        '0x12f411': [0x12a87c, 0x12ebc9, 0x12ec04, 0x12ee46, 0x132fd5],
        '0x132d0b': [0x12a864, 0x12aa02, 0x12aa97, 0x12aadf, 0x12aaf2, 0x12b8f0,
                     0x12ec3a, 0x12eef6, 0x131490, 0x1314da, 0x13156d, 0x1315f3, 0x1330d7],
        '0x12eb9b': [0x12a98a, 0x12aa1a, 0x12aaaf, 0x12eea2, 0x12eecb, 0x133015],
    }
    for target, wanted in expected.items():
        actual = [int(c['pc'], 16) for c in alias_report['call_arguments'] if c['target'] == target]
        if actual != wanted: raise ValueError(f'caller closure changed for{target}')
    pair_bases = [332, 376, 444, 495, 546, 615]
    pointer_scalar = {0x12aa02: 1171, 0x12aa97: 1172, 0x12aadf: 1173,
                      0x12aaf2: 1157, 0x12eef6: 1156, 0x1330d7: 1158}
    inline_scalar = {0x12a864: 422, 0x12b8f0: 719, 0x131490: 592,
                     0x1314da: 421, 0x13156d: 676, 0x1315f3: 97}
    def offsets_for_pointer(p):
        return sorted({299+m[str(p)] for m in modes['pointer_maps'].values() if str(p) in m})
    common = []
    for pc, p in pointer_scalar.items():
        common.append({'pc': hex(pc), 'via': '132D0B→132D23', 'object_offsets': offsets_for_pointer(p),
                       'reason': f'dereferenced mode pointer{p}, not address of that pointer cell'})
    for pc, offset in inline_scalar.items():
        common.append({'pc': hex(pc), 'via': '132D0B→132D23', 'object_offsets': [offset],
                       'reason': 'inline scalar field; object97 is below registered config base299'})
    common.append({'pc': '0x12ec3a', 'via': '12EB9B→132D0B→132D23',
                   'object_offsets': [p+2 for p in pair_bases], 'reason': 'all six fixed pair-bank bases +2'})
    for pc, p in [(0x12a87c, 1159), (0x132fd5, 1160)]:
        common.append({'pc': hex(pc), 'via': '12F411→12F454', 'object_offsets': offsets_for_pointer(p),
                       'reason': f'dereferenced mode pointer{p}'})
    for pc, delta in [(0x12ebc9, 0), (0x12ec04, 1)]:
        common.append({'pc': hex(pc), 'via': '12EB9B→12F411→12F454',
                       'object_offsets': [p+delta for p in pair_bases], 'reason': f'fixed pair-bank +{delta}'})
    common.append({'pc': '0x12ee46', 'via': '12F411→12F454', 'object_offsets': [331], 'reason': 'inline common four-tail delay'})
    scenarios = []
    for source in ('loader', 'b01'):
        for bank in configs['sources'][source]['banks']:
            raw = struct.pack('<829I', *bank['words'])
            if hashlib.sha256(raw).hexdigest() != bank['sha256']: raise ValueError('retained config fingerprint mismatch')
            config = struct.unpack('<829f', raw); rows = list(common)
            for pc, count_fields in [(0x12b8ba, [698]), (0x12ec4d, [p+3 for p in pair_bases]),
                                     (0x13138c, [593]), (0x1313e6, [677])]:
                for field in count_fields:
                    value = config[field-299]
                    if not math.isfinite(value) or not 0 <= value <= 128: raise ValueError('unsupported retained section count')
                    count = math.trunc(value+.5)
                    rows.append({'pc': hex(pc), 'via': '12DBDE→five12FAF2 calls/section',
                        'count_object_offset': field, 'section_count': count,
                        'object_offsets': list(range(field+1, field+1+5*count)),
                        'reason': 'all retained sections, even if an active/error path would skip some'})
            all_offsets = sorted({o for row in rows for o in row['object_offsets']})
            collision = sorted(set(all_offsets)&set(range(1162, 1166)))
            scenarios.append({'source': source, 'bank': bank['bank'], 'callsites': rows,
                'argument_offsets_min_max': [min(all_offsets), max(all_offsets)],
                'pointer_cell_collisions': collision, 'smoothing_skips_offsets': [o for o in all_offsets if o < 299]})
    return {'direct_callers': expected, 'pair_bank_object_bases': pair_bases,
            'helper_has_upper_bound': False,
            'lower_bound_pc': '0x12fafc/12fafe',
            'indexed_target_read_pc': '0x12fb0b', 'indexed_step_read_pc': '0x12fb16',
            'registered_offset_formula': 'R4 - DM(B903A), unsigned lower-bound check only',
            'scenarios': scenarios,
            'conditional_smoothing_path_excludes_1162_1165': all(not s['pointer_cell_collisions'] for s in scenarios),
            'assumptions': ['retained loader/B01 filter counts', 'normal decoded mode-pointer maps, including mixed apply order',
                'unchanged fixed caller/pair bases and config base299', 'no indexed/source-RAM mutation, corrupt state or external call'],
            'unproved': ['other dynamic B paths', 'incoming bases across every control-flow join', 'arbitrary indirect/external caller',
                        'mutated section counts/pointers', 'actual live state'], 'dead_field_proven': False}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--loader', type=Path, required=True); p.add_argument('--out', type=Path, required=True)
    p.add_argument('--config-report', type=Path); p.add_argument('--mode-report', type=Path)
    args = p.parse_args(); result = audit(args.loader.read_bytes())
    if bool(args.config_report) != bool(args.mode_report): p.error('both config/mode reports required together')
    if args.config_report:
        result['smoothing_caller_domains'] = smoothing_caller_domains(result,
            json.loads(args.config_report.read_text()), json.loads(args.mode_report.read_text()))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: result[k] for k in ('instructions', 'basic_blocks', 'dead_field_proven')}))
