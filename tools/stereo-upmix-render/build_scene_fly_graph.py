"""Reduce public FlyWire CSVs to an attributed, reproducible research graph.

No physiology/emulation claim: select a small anatomical subgraph, apply an
explicit neurotransmitter sign assumption, and normalize for stable recurrence.
"""
import argparse
from collections import Counter
import csv
import gzip
import hashlib
import json
from pathlib import Path
import random


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rewire(edges, signs, seed, swaps_per_edge=20):
    """Directed swaps preserve in/out degree and each target's weight multiset.

    Swap only equally signed sources; retain source sign, exclude self/duplicate
    edges. Weights stay with their target. Not a uniform graph sampler.
    """
    rng = random.Random(seed)
    out = [list(e) for e in edges]
    present = {(e[0], e[1]) for e in out}
    wanted, accepted, attempted = swaps_per_edge*len(out), 0, 0
    while accepted < wanted and attempted < 200*wanted:
        attempted += 1
        i, j = rng.sample(range(len(out)), 2)
        a, b, w = out[i]
        c, d, z = out[j]
        if (a == c or b == d or a == d or c == b or signs[a] != signs[c]
                or (c, b) in present or (a, d) in present):
            continue
        present.remove((a, b)); present.remove((c, d))
        out[i], out[j] = [c, b, w], [a, d, z]
        present.add((c, b)); present.add((a, d))
        accepted += 1
    if accepted != wanted:
        raise ValueError("Insufficient successful degree/sign-preserving swaps")
    return sorted(out), dict(seed=seed, accepted=accepted, attempted=attempted,
                              swaps_per_edge=swaps_per_edge)


def build(directory, nodes=64, seed=20261004):
    with gzip.open(directory/'neurons.csv.gz', 'rt') as stream:
        nt = {int(r['root_id']): r['nt_type'] for r in csv.DictReader(stream)}
    with gzip.open(directory/'classification.csv.gz', 'rt') as stream:
        classification = {int(r['root_id']): r for r in csv.DictReader(stream)}
    signs = {k: (1 if v == 'ACH' else -1) for k, v in nt.items()
             if v in ('ACH', 'GABA', 'GLUT')}
    pairs = Counter()
    with gzip.open(directory/'connections.csv.gz', 'rt') as stream:
        for r in csv.DictReader(stream):
            a, b = int(r['pre_root_id']), int(r['post_root_id'])
            if r['neuropil'] == 'AMMC_R' and a != b and a in signs and b in signs:
                pairs[a, b] += int(r['syn_count'])
    pairs = {p: n for p, n in pairs.items() if n >= 5}
    strength = Counter()
    for (a, b), n in pairs.items():
        strength[a] += n
        strength[b] += n
    auditory = {k for k, r in classification.items() if r['sub_class'] == 'auditory'}
    direct = Counter()
    for (a, b), n in pairs.items():
        if a in auditory and classification.get(b, {}).get('flow') == 'intrinsic':
            direct[b] += n
    seeds = sorted(direct, key=lambda k: (-direct[k], k))[:16]
    chosen = set(seeds)
    while len(chosen) < nodes:
        affinity = Counter()
        for (a, b), n in pairs.items():
            if a in chosen and b not in chosen and classification.get(b, {}).get('flow') == 'intrinsic':
                affinity[b] += n
            if b in chosen and a not in chosen and classification.get(a, {}).get('flow') == 'intrinsic':
                affinity[a] += n
        if not affinity:
            raise ValueError('Auditory-seeded neighborhood exhausted')
        chosen.add(min(affinity, key=lambda k: (-affinity[k], k)))
    selected = sorted(chosen)
    if len(selected) != nodes:
        raise ValueError("Insufficient eligible annotated neurons")
    index = {v: k for k, v in enumerate(selected)}
    edges = sorted([index[a], index[b], n] for (a, b), n in pairs.items()
                   if a in index and b in index)
    node_signs = [signs[k] for k in selected]
    shuffled, swaps = rewire(edges, node_signs, seed)
    original_set = {(a, b) for a, b, _ in edges}
    shared = sum((a, b) in original_set for a, b, _ in shuffled)
    if shared >= .8*len(edges):
        raise ValueError("Null graph insufficiently different")
    sources = [{**r, 'sha256': sha(directory/r['name'])}
               for r in json.loads((directory/'downloads.json').read_text())]
    return dict(schema='scene-flywire-graph-v1', snapshot=783, nodes=nodes,
                selection='AMMC_R only; ACH/GABA/GLUT annotated endpoints; no self edges; '
                          'sum synapses per pair >=5; seed16 intrinsic neurons with highest '
                          'synapse count from auditory-subclass inputs; grow to64 by strongest '
                          'total incoming+outgoing synapse count to selected set, intrinsic only; '
                          'ties by root ID; selected node order by root ID',
                sign_assumption='ACH positive, GABA/GLUT negative; simplified model assumption',
                normalization='signed sqrt(synapse_count); each nonzero incoming row L1=0.9',
                sources=sources, attribution='FlyWire Consortium; FAFB v783; '
                'Dorkenwald et al. 2024; dataset DOI 10.5281/zenodo.10676866, CC BY 4.0',
                root_ids=[str(k) for k in selected], signs=node_signs,
                annotations=[{k: classification.get(n, {}).get(k, '')
                              for k in ['class', 'sub_class', 'super_class', 'side']}
                             for n in selected],
                fly_edges=edges, shuffled_edges=shuffled, rewiring=swaps,
                null_shared_edge_count=shared, eligible_region_nodes=len(strength),
                auditory_input_count=len(auditory), seed_targets=[dict(root_id=str(n),
                    auditory_input_synapses=direct[n]) for n in seeds],
                physical_emulation=False, auditory_circuit_validated=False)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--data', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    if a.out.exists():
        raise ValueError("New graph destination required")
    result = build(a.data)
    a.out.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(nodes=result['nodes'], edges=len(result['fly_edges']),
                          overlap=result['null_shared_edge_count'], rewiring=result['rewiring'])))


if __name__ == '__main__':
    main()
