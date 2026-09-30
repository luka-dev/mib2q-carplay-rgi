#!/usr/bin/env python3
"""Restore the original harness's two quiet-stem rows from saved C audio.

CPU only. No network inference, C rerender, threshold tuning or gain change.
Existing row values are retained; the expected set is the prior DEV40 harness.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import shutil

os.environ['CUDA_VISIBLE_DEVICES'] = '-1'
import bsrnn_reference_eval as E


def keys(rows):
    result = {(r['song'], r['stem']) for r in rows}
    if len(result) != len(rows):
        raise RuntimeError('Duplicate stem rows')
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runs', type=Path, required=True)
    p.add_argument('--moisesdb', type=Path, required=True)
    a = p.parse_args()
    root = a.runs/'bsrnn_reference_check40_v1'
    audit_path = root/'coverage_repair.json'
    if audit_path.exists():
        raise RuntimeError('Repair already recorded')
    manifest = json.loads((root/'manifest.json').read_text())
    expected_path = a.runs/'direct_object_full40/baseline/stems.csv'
    with expected_path.open(newline='') as f:
        expected = list(csv.DictReader(f))
    expected_keys = keys(expected)
    if len(expected_keys) != 231 or {s for s, _ in expected_keys} != set(manifest['song_track_ids']):
        raise RuntimeError('Expected original harness changed')
    inputs = {r['track_id']: r for r in json.loads((root/'inputs_progress.json').read_text())}
    expected_missing = {('Ben James - Home', 'other'),
                        ('Holy Magick - Crystal Observatory', 'percussion')}
    records = []
    for point in ['baseline', 'direct_no_bed_body0_cal']:
        all_rows = []
        for song, track_id in manifest['song_track_ids'].items():
            folder = root/point
            row_path = folder/(track_id+'.rows.json')
            rows = json.loads(row_path.read_text())
            for suffix, field in [('.wav','mixture_sha256'),('.inj.wav','injection_sha256'),('.6ch.wav','output_sha256')]:
                if E.ref.sha(folder/(track_id+suffix)) != inputs[track_id][point][field]:
                    raise RuntimeError('Saved audio changed')
            all_rows.extend(rows)
        if expected_keys - keys(all_rows) != expected_missing or keys(all_rows) - expected_keys:
            raise RuntimeError('Unexpected coverage difference')
        for song in sorted({s for s, _ in expected_missing}):
            track_id = manifest['song_track_ids'][song]
            row_path = root/point/(track_id+'.rows.json')
            existing = json.loads(row_path.read_text())
            meta, stems = E.H.load_song(str(a.moisesdb/track_id), 60)
            n = min(v.shape[-1] for v in stems.values())
            stems = {k: v[:, :n] for k, v in stems.items()}
            output = E.T.read_wav6(str(root/point/(track_id+'.6ch.wav')))
            if output.shape != (6, n):
                raise RuntimeError('Saved C duration differs')
            measured = E.rows_from_output(meta, stems, output)
            wanted = {k for k in expected_missing if k[0] == song}
            added = [r for r in measured if (r['song'],r['stem']) in wanted]
            if keys(added) != wanted:
                raise RuntimeError('Original quiet-stem expression did not restore coverage')
            backup = row_path.with_name(track_id+'.rows.before_coverage.json')
            if backup.exists():
                raise RuntimeError('Backup exists')
            shutil.copyfile(row_path, backup)
            E.write(row_path, existing+added)
            records.append(dict(point=point, song=song, added=added,
                                old_rows_sha256=E.ref.sha(backup), new_rows_sha256=E.ref.sha(row_path)))
        complete = [r for song, track_id in manifest['song_track_ids'].items()
                    for r in json.loads((root/point/(track_id+'.rows.json')).read_text())]
        if keys(complete) != expected_keys:
            raise RuntimeError('Final coverage differs')
    E.write(audit_path, dict(scope='CPU restoration of original DEV40 coverage, saved audio unchanged',
            expected_csv_sha256=E.ref.sha(expected_path), expected_rows=231,
            original_expression='(stem*k)**2 sum < 1e-3 * raw_mix**2 sum',
            previous_expression='stem**2 sum < 1e-3 * raw_mix**2 sum',
            repair_script_sha256=E.ref.sha(__file__), records=records))
    print('COVERAGE_RESTORED', len(records), 'saved-output row additions')


if __name__ == '__main__':
    main()
