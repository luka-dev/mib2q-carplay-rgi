#!/usr/bin/env python3
"""Prepare measured Java descriptors for the V850 probe, or validate its CSVs.

No shape-based BAP lookup is used. Run HudManeuverChainAudit.java in Ghidra
between `prepare` and `check`; see HUD_RAMP_3D_CHAIN.md for the execution scope.
"""
import argparse
import csv
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'output/maneuver-audit'


def read(name):
    with (OUT / name).open() as f:
        return list(csv.DictReader(f))


def key(row):
    return (int(row['main']), int(row['direction']), int(row['z']),
            bytes.fromhex(row['streets_hex']).hex())


def java_key(row):
    return (int(row['bap_main']), int(row['bap_direction']), int(row['bap_z']),
            bytes.fromhex(row['bap_sidestreets']).hex())


def prepare():
    cases = {}
    def add(k, source):
        cases.setdefault(k, set()).add(source)
    for main in range(39):
        for direction in [*range(0, 256, 16), 255]:
            add((main, direction, 0, ''), 'LUT')
    for name in ('java_mapping.csv', 'ramp_mapping.csv'):
        for r in read(name):
            if name == 'java_mapping.csv' and r['case'] != 'canonical':
                continue
            add(java_key(r), f'{name}:{r["mt"]}')
    for main in (12, 13, 15, 16, 17, 18):
        for z in (1, 2):
            add((main, 32 if main == 16 else 224, z, '00'), 'z')
    for streets in ('00', '00e0', '2020e0', bytes(range(0, 256, 16)).hex()):
        add((15, 224, 0, streets), 'roads')
    # Include every distinct descriptor in the full Java matrix, preserving
    # canonical/ramp case IDs from the initial 698-case investigation.
    for r in read('java_mapping.csv'):
        add(java_key(r), f'java_mapping.csv:{r["mt"]}')
    for main in (15, 16):
        add((main, 0, 0, '00'), 'straight-exit')
    fields = ['case', 'main', 'direction', 'z', 'streets_hex']
    rows = [dict(zip(fields, [f'mcu{i:04d}', *k]), sources=sorted(v))
            for i, (k, v) in enumerate(cases.items())]
    with (OUT / 'mcu-chain-input.csv').open('w') as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction='ignore')
        w.writeheader(); w.writerows(rows)
    (OUT / 'mcu-chain-input-sources.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(f'Prepared {len(rows)} unique descriptors')


def check():
    inputs = read('mcu-chain-input.csv')
    g, m = read('mcu-chain-output.csv'), read('mcu-chain-M-output.csv')
    assert g == m, 'G1952 / M2325 results differ'
    assert len(inputs) == len(g)
    assert [key(r) for r in inputs] == [key(r) for r in g]
    results = {key(r): r for r in g}
    assert len(results) == len(inputs)
    for r in g:
        assert int(r['selector']) <= 120 and int(r['road_count']) <= 17
        roads = bytes.fromhex(r['roads_hex'])
        assert len(roads) == 17 and max(roads) <= 64
        assert int(r['group_mask']) == 4
    coverage = {}
    for filename in ('java_mapping.csv', 'ramp_mapping.csv'):
        rows = read(filename)
        assert all(java_key(r) in results for r in rows)
        coverage[filename] = {'rows': len(rows), 'unique_descriptors': len({java_key(r) for r in rows})}
        merged = [dict(r, **{'hud_' + k: v for k, v in results[java_key(r)].items()
                            if k not in ('case', 'main', 'direction', 'z', 'streets_hex')}) for r in rows]
        with (OUT / filename.replace('.csv', '-hud.csv')).open('w') as f:
            w = csv.DictWriter(f, fieldnames=list(merged[0])); w.writeheader(); w.writerows(merged)
    expected = {(13, 224, ''): 32, (15, 0, '00'): 33, (15, 192, '00'): 34,
                (15, 224, '00'): 35, (15, 160, '00'): 0, (16, 96, '00'): 0,
                (16, 0, '00'): 36, (16, 32, '00'): 37, (16, 64, '00'): 38,
                (17, 224, ''): 39, (18, 32, ''): 40, (12, 192, ''): 25}
    for (main, direction, streets), selector in expected.items():
        assert int(results[(main, direction, 0, streets)]['selector']) == selector
    report = dict(cases_per_firmware=len(g), firmware_results_identical=True, coverage=coverage,
                  scope='Original V850 importer, converter, model transfer and field packer; forced active 3D mode 3; '
                        'BAP cache accessor is the sole substituted function; slots 2/3 empty; no hardware/CAN capture.',
                  regressions_checked=len(expected),
                  sha256={name: hashlib.sha256((OUT / name).read_bytes()).hexdigest()
                          for name in ('mcu-chain-input.csv', 'mcu-chain-output.csv', 'mcu-chain-M-output.csv')})
    (OUT / 'mcu-chain-checks.json').write_text(json.dumps(report, indent=2) + '\n')
    source = (ROOT / 'java_patch/com/luka/carplay/rgd/ManeuverMapper.java').read_text().split('/* BAP Direction constants */')[0]
    names = {int(value): name for name, value in re.findall(r'public static final int (\w+) = (\d+);', source)}
    lines = ['# BAP mainElement → 3D HUD selectors', '',
             'Измерено исполнением V850 G1952/M2325. Условия: z=0, sideStreets=[], активный 3D-режим 3; '
             'один первый descriptor, остальные слоты пусты. Все числа direction — HEX, sN — внутренний индекс GDC.', '',
             'Проверены direction 00,10,…,F0 и FF. В таблице перечислены непустые состояния; '
             'остальные проверенные направления дают s0. Стрелка и дороги зависят от разных таблиц; '
             'полные road selectors находятся в CSV. Выбор режима панели здесь не проверяется.', '',
             '| Main | BAP | Direction HEX → selector |', '|---:|---|---|']
    for main, name in sorted(names.items()):
        groups = {}
        for direction in [*range(0, 256, 16), 255]:
            selector = int(results[(main, direction, 0, '')]['selector'])
            if selector:
                groups.setdefault(selector, []).append(f'{direction:02X}')
        mappings = '; '.join(f'{",".join(directions)} → s{selector}' for selector, directions in groups.items())
        lines.append(f'| {main} | {name} | {mappings or "∅"} |')
    lines += ['', '**FOLLOW_STREET 11/00 с z=0 и пустыми sideStreets имеет отдельное правило импортера:** '
              'первый descriptor очищается и используются следующие байты. s0 в этой таблице не означает, '
              'что весь HUD будет пустым в обычном режиме движения.', '',
              'ARRIVED 3/00 выбирает s120, а не прежний предполагаемый s1. '
              'EXIT_RIGHT использует s33–35; s39 принадлежит SERVICE_ROAD_RIGHT.', '',
              '- [Полная цепочка и рампы](HUD_RAMP_3D_CHAIN.md).',
              '- [Все 54 iAP2 типа, RHT/LHT](MANEUVER_MAPPING_TABLE.md).',
              '- [Измеренные селекторы и дороги](../../output/maneuver-audit/mcu-chain-output.csv).',
              '- [Воспроизводимый декодер по таблицам прошивки](../../tools/hud_mcu_decoder.py).', '']
    (ROOT / 'docs-obsidian/Sources/reference/HUD_BAP_3D_SELECTORS.md').write_text('\n'.join(lines))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare', 'check'))
    {'prepare': prepare, 'check': check}[parser.parse_args().action]()
