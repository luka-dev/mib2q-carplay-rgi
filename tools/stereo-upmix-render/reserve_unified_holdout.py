#!/usr/bin/env python3
"""Reserve whole artists from TRAIN before the fresh unified model goes broad.

Metadata only. This is candidate-independent, not globally pristine: historical
students saw the original TRAIN pool. Do not claim it closes the untouched DEV
acceptance gate. No audio or model output participates in selection.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path

FAMILIES = ('guitar', 'keys', 'piano', 'strings', 'winds')
MINIMUM = (3, 3, 3, 2, 2)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def reserve(a):
    if a.out.exists():
        raise FileExistsError('Preserve frozen partition')
    metadata = json.loads(a.metadata.read_bytes())
    old = json.loads(a.monitor.read_bytes())
    fit = json.loads(a.fit.read_bytes())
    rows = {r['id']: r for r in metadata['rows']}
    if len(rows) != 176 or set(rows) != set(old['train_inventory']):
        raise ValueError('Original176 TRAIN metadata/inventory mismatch')
    if metadata['split_sha256'] != old['split_sha256'] or metadata['split_sha256'] != fit['split_sha256']:
        raise ValueError('Original split changed')
    for song, r in rows.items():
        if r['metadata_sha256'] != old['train_inventory'][song]['metadata_sha256']:
            raise ValueError('Metadata changed')
    excluded_songs = {r['song'] for r in old['evaluation']['train']} | {r['song'] for r in fit['windows']}
    artist = lambda r: r['artist'].strip().casefold()
    excluded_artists = {artist(rows[s]) for s in excluded_songs}
    groups = defaultdict(list)
    for r in rows.values():
        if not artist(r) or not r['title'].strip():
            raise ValueError('Artist/title required')
        if artist(r) not in excluded_artists:
            groups[artist(r)].append(r)
    ordered = sorted(groups, key=lambda k: hashlib.sha256(('unified-holdout-v1:' + k).encode()).hexdigest())
    # Deterministic bounded subset-sum; counts are metadata presence, not measured
    # activity. First path to each capped state wins, never acoustic cherry-picking.
    states = {(0, 0, 0, 0, 0, 0): ()}
    target = (20, *MINIMUM)
    for name in ordered:
        songs = groups[name]
        counts = [sum(f in r['groups'] for r in songs) for f in FAMILIES]
        added = {}
        for key, path in sorted(states.items()):
            n = key[0] + len(songs)
            if n > 20:
                continue
            next_key = (n, *(min(cap, prev + count) for cap, prev, count in zip(MINIMUM, key[1:], counts)))
            if next_key not in states and next_key not in added:
                added[next_key] = path + (name,)
        states.update(added)
        if target in states:
            break
    if target not in states:
        raise ValueError('No whole-artist20 with declared family coverage')
    selected_artists = set(states[target])
    selected = sorted(r['id'] for r in rows.values() if artist(r) in selected_artists)
    train = sorted(set(rows) - set(selected))
    if len(selected) != 20 or len(train) != 156 or set(selected) & excluded_songs:
        raise ValueError('Partition cardinality/exclusion differs')
    if {artist(rows[s]) for s in train} & selected_artists:
        raise ValueError('Artist leakage')
    output = dict(
        version='unified-candidate-holdout-v1', train=train, holdout=selected,
        holdout_artists=sorted(selected_artists), excluded_fit_monitor_artists=sorted(excluded_artists),
        metadata_family_coverage=dict(Counter(f for s in selected for f in rows[s]['groups'])),
        selection='metadata-only whole artists; hash order plus bounded subset-sum; guitar/keys/piano>=3 songs, strings/winds>=2; no audio read or model scores',
        scope='fresh unified candidate has seen only four excluded TRAIN crops; no historical network weights copied; later training must exclude all20 and their artists',
        limits='Historical models trained on these original TRAIN songs. Candidate-independent is not globally untouched DEV. Labels do not prove active family coverage in60s excerpts. The strict untouched20 goal remains open.',
        evaluation='hold sealed until fixed terminal model passes full inspected DEV40; never choose checkpoints/gains on this partition',
        split_sha256=metadata['split_sha256'], sidecar_split_sha256=metadata['sidecar_split_sha256'],
        input_hashes={name:sha(path) for name, path in [('metadata',a.metadata),('old_monitor',a.monitor),('four_crop_fit',a.fit)]},
        script_sha256=sha(__file__), holdout_metadata={s:rows[s] for s in selected})
    a.out.write_text(json.dumps(output, indent=2, ensure_ascii=True) + '\n')
    print('RESERVED metadata-only156TRAIN/20candidate-holdout', output['metadata_family_coverage'])


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('metadata', 'monitor', 'fit', 'out'):
        p.add_argument('--'+name, type=Path, required=True)
    reserve(p.parse_args())
