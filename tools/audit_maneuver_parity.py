#!/usr/bin/env python3
"""Validate current bridge packets against measured original HUD descriptors.

Consumes the existing G/M V850 execution oracles, freshly exported Java matrix
and actual native OpenGL captures. Does not infer HUD selections from shapes.
Produces one 54-type sheet containing original HUD layers and renderer frames.
"""
import argparse
import csv
import hashlib
import json
import struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from audit_hud_maneuver_mapping import key, java_key

ROOT = Path(__file__).resolve().parents[1]
ICONS = ['NONE', 'FOLLOW', 'TURN', 'UTURN', 'MERGE', 'EXIT', 'ROUNDABOUT',
         'ARRIVED', 'LANE_CHANGE', 'ROUNDABOUT_EXIT']


def read(path):
    with path.open() as f:
        return list(csv.DictReader(f))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('out', type=Path, nargs='?', default=ROOT/'output/maneuver-parity')
    args = ap.parse_args()
    out = args.out.resolve()
    oracle_files = [ROOT/'output/maneuver-audit'/name for name in
                    ('mcu-chain-output.csv', 'mcu-chain-M-output.csv')]
    oracle_files += [ROOT/'output/maneuver-selection'/name for name in
                     ('mcu-new-G-output.csv', 'mcu-new-M-output.csv')]
    g = {key(r): r for p in oracle_files[::2] for r in read(p)}
    m = {key(r): r for p in oracle_files[1::2] for r in read(p)}
    rows = read(out/'java_mapping.csv')
    keys = sorted({java_key(r) for r in rows})
    for k in keys:
        assert k in g and k in m, ('Missing original CPU execution evidence', k)
        assert {n: v for n, v in g[k].items() if n != 'case'} == {
            n: v for n, v in m[k].items() if n != 'case'}, ('G/M mismatch', k)
    family = {1: 0, 3: 7, 11: 1, 12: 8, 13: 2, 15: 5, 16: 5,
              21: 6, 22: 6, 25: 3, 26: 9, 27: 9}
    # Independent selector expectations measured from original FD51 LUTs.
    turn_angles = {26: 0, 27: -45, 28: -90, 29: -135, 30: 135, 31: 90, 32: 45}
    def degrees(direction):
        return -direction*360/256 if direction < 128 else (256-direction)*360/256
    merged = []
    for r in rows:
        k = java_key(r); result = g[k]
        b = bytes.fromhex(r['renderer_hex']); main, direction, z, streets = k
        icon = b[2]; angle = struct.unpack_from('>h', b, 4)[0]/2
        assert len(b) == 48 and b[0] == 1 and b[1] & 4
        mt, raw = int(r['mt']), int(r['angle'])
        circle_phase = mt in (6, 7) and main != 1
        raw_circle = (mt in (6, 7) or 28 <= mt <= 46) and main != 1
        assert icon == (6 if circle_phase else family[main]), (r['mt'], main, icon)
        decoded_roads = [struct.unpack_from('>h', b, 8+2*i)[0]/2 for i in range(b[7])]
        expected_roads = [degrees(v) for v in bytes.fromhex(streets)]
        if icon in (1, 2, 6):
            expected_roads = [int(v) for v in r['junction_angles'].split(';') if v and -180 <= int(v) <= 180][:18]
        assert decoded_roads == expected_roads
        selector = int(result['selector'])
        assert bool(b[1] & 16) == (raw_circle and -180 <= raw <= 180), (r, "snap policy")
        if raw_circle:
            assert angle == (raw if -180 <= raw <= 180 else 0)
            assert b[6] == int(r['traffic'])
        elif main == 13:
            expected = turn_angles[selector]
            if mt not in (8,9,22,23) and -180 <= raw <= 180 and (mt in (11,16,51) or expected*raw > 0):
                expected = raw
            assert angle == expected, (r, selector)
        elif main in (21, 22):
            assert angle == degrees(direction)
            assert b[6] == (main == 22)
        elif main in (15, 16):
            assert direction == 0 and selector == (33 if main == 15 else 36)
            assert struct.unpack('b', b[3:4])[0] == (1 if main == 15 else -1)
        elif main == 12:
            assert selector in (24, 25) and b[3] == (255 if selector == 24 else 1)
        elif main == 25:
            assert selector in (110, 111) and angle == (-180 if selector == 110 else 180)
        elif main in (26, 27):
            assert selector == (112 if main == 26 else 113) and b[6] == (main == 27)
        elif main == 1:
            assert selector == 0 and b[7] == 0
        merged.append(dict(r, **{'hud_'+n: v for n, v in result.items()
                                if n not in ('case', 'main', 'direction', 'z', 'streets_hex')}))
    with (out/'java_mapping-hud.csv').open('w') as f:
        w = csv.DictWriter(f, fieldnames=list(merged[0])); w.writeheader(); w.writerows(merged)
    for name, oracle in [('mcu-chain-output.csv', g), ('mcu-chain-M-output.csv', m)]:
        with (out/name).open('w') as f:
            w = csv.DictWriter(f, fieldnames=list(g[keys[0]])); w.writeheader()
            w.writerows(oracle[k] for k in keys)

    captures = json.loads((out/'renderer-captures.json').read_text())
    assert sha(Path(captures['binary'])) == captures['binary_sha256'], 'Captures are from another binary'
    shots = {r['row']['renderer_hex']: r['png'] for r in captures['captures']}
    hud_file = ROOT/'output/maneuver-audit/8W6919617G_1952-selectors.json'
    nodes = [n for n in json.loads(hud_file.read_text())['nodes'] if n['group'] == 2]
    def composite(result):
        im = Image.new('RGBA', (185, 141), 'black')
        fields = {9: int(result['selector']), 10: int(result['marker']),
                  **{11+i: v for i, v in enumerate(bytes.fromhex(result['roads_hex']))}}
        for n in nodes:
            value = fields[n['field']]
            if value >= len(n['states']):
                continue
            for surface in n['states'][value]['surfaces']:
                x, y, w, h = struct.unpack_from('<4H', bytes.fromhex(surface['descriptor_hex']), 30)
                layer = Image.open(surface['png']).convert('RGBA')
                assert layer.size == (w, h)
                im.alpha_composite(layer, (x, y))
        return im

    canonical = [r for r in rows if r['case'] == 'canonical']
    assert len(canonical) == 108
    columns, tw, th, header = 6, 624, 468, 168
    sheet = Image.new('RGB', (columns*tw, header+9*th+75), '#11161d')
    d = ImageDraw.Draw(sheet)
    font = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', 20)
    bold = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial Bold.ttf', 22)
    title = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial Bold.ttf', 42)
    mono = ImageFont.truetype('/System/Library/Fonts/Menlo.ttc', 17)
    d.text((24, 20), '54 манёвра: HUD и maneuver_renderer после исправлений', font=title, fill='white')
    d.text((24, 80), 'Слева — оригинальные слои HUD (стрелка + дороги); справа — реальные кадры нашего OpenGL renderer.', font=bold, fill='#a8dce6')
    d.text((24, 116), 'Каждый тип: RHT и LHT. Renderer сохраняет полное кольцо на входе/выходе и точные совместимые углы; HUD выбирает готовые иконки.', font=font, fill='#c4d0da')
    for mt in range(54):
        pair = [r for r in canonical if int(r['mt']) == mt]
        x, y = (mt % columns)*tw, header+(mt//columns)*th
        d.rectangle((x+2, y+2, x+tw-3, y+th-3), outline='#364654')
        d.text((x+13, y+10), f"{mt:02d} {pair[0]['type'][3:]}", font=bold, fill='white')
        d.text((x+13, y+40), f"Вход: угол {pair[0]['angle']}°, junction={pair[0]['junction']}", font=font, fill='#9baebd')
        for side, r in enumerate(pair):
            result = g[java_key(r)]; yy = y+73+side*190
            selector = int(result['selector'])
            desc = f"{'LHT' if side else 'RHT'}  BAP {r['bap_main']}/{int(r['bap_direction']):02X}  HUD s{selector:03d}"
            d.text((x+13, yy), desc, font=mono, fill='#edc77e')
            im = composite(result); sheet.paste(im, (x+33, yy+31))
            if r['bap_main'] == '11':
                d.text((x+24, yy+89), 'FOLLOW: отдельный', font=font, fill='#a9bbc6')
                d.text((x+24, yy+114), 'режим HUD', font=font, fill='#a9bbc6')
            im = Image.open(out/shots[r['renderer_hex']]).convert('RGB')
            im.thumbnail((328, 156)); sheet.paste(im, (x+274, yy+28))
            d.text((x+278, yy+164), ICONS[int(r['renderer_icon'])], font=mono, fill='#80c2ff')
    d.text((24, header+9*th+18), 'Форма, цвет, перспектива и анимация различаются. HUD: фиксированный активный 3D-режим; это не запись автомобиля.', font=font, fill='#b8c4ce')
    sheet.save(out/'hud-renderer-54.png', optimize=True)
    # Focused review of the user's ramp shape requirement, with measured old
    # EXIT/00 HUD layers next to the current TURN and actual native frames.
    ramp_rows = [next(r for r in canonical if int(r['mt']) == mt and r['traffic'] == '0')
                 for mt in (22, 23, 8, 9)]
    ramp_sheet = Image.new('RGB', (1040, 1130), '#11161d')
    rd = ImageDraw.Draw(ramp_sheet)
    rd.text((20, 16), 'Рампы: один поворот без выпрямления в конце', font=bold, fill='white')
    for x, label in [(20, 'Прежний HUD'), (350, 'Текущий HUD'), (680, 'Текущий renderer')]:
        rd.text((x, 61), label, font=bold, fill='#a8dce6')
    for i, r in enumerate(ramp_rows):
        y = 100 + i*250
        result = g[java_key(r)]
        left = int(r['bap_direction']) == 32
        # ON_RAMP was already TURN; its previous image stays the same.
        old = result if r['mt'] == '9' else g[(16 if left else 15, 0, 0, '00')]
        rd.text((20, y), f"MT {r['mt']} {r['type'][3:]} / вход {r['angle']}°", font=bold, fill='white')
        ramp_sheet.paste(composite(old), (50, y+45))
        ramp_sheet.paste(composite(result), (380, y+45))
        frame = Image.open(out/shots[r['renderer_hex']]).convert('RGB')
        frame.thumbnail((328, 181))
        ramp_sheet.paste(frame, (680, y+28))
        rd.text((40, y+209), f"s{int(old['selector']):03d}", font=mono, fill='#edc77e')
        rd.text((370, y+209), f"s{int(result['selector']):03d} / TURN", font=mono, fill='#edc77e')
        rd.text((710, y+209), 'TURN / один изгиб', font=mono, fill='#80c2ff')
    rd.text((20, 1100), 'HUD — оригинальные слои прошивки; renderer — OpenGL. Это не запись автомобиля.', font=font, fill='#b8c4ce')
    ramp_sheet.save(out/'ramps-hud-renderer.png', optimize=True)
    geometry = Image.new('RGB', (1200, 680), '#11161d')
    gd = ImageDraw.Draw(geometry)
    gd.text((20, 16), 'Полные круги, исходные дороги и sharp-повороты', font=bold, fill='white')
    examples = [(6, 0, 0, 'canonical'), (7, 0, 0, 'canonical'),
                (28, -67, 0, 'matrix'), (28, -67, 1, 'matrix'),
                (47, -157, 0, 'matrix'), (48, 157, 0, 'matrix')]
    for i, (mt, angle, side, case) in enumerate(examples):
        r = next(r for r in rows if int(r['mt']) == mt and int(r['angle']) == angle
                 and int(r['traffic']) == side and r['case'] == case
                 and int(r['junction']) == (1 if mt in (6,7,28) else 0))
        x, y = (i%3)*400, 65+(i//3)*300
        gd.text((x+20, y), f"MT {mt} / {angle}° / {'LHT' if side else 'RHT'}", font=bold, fill='white')
        frame = Image.open(out/shots[r['renderer_hex']]).convert('RGB')
        frame.thumbnail((368, 203)); geometry.paste(frame, (x+16, y+37))
        gd.text((x+20, y+253), 'Совмещение с дорогой' if mt == 28 else
                'Полное кольцо' if mt in (6,7) else 'Точный угол, подъём стрелки', font=font, fill='#a8dce6')
    geometry.save(out/'restored-geometry.png', optimize=True)
    report = dict(java_rows=len(rows), unique_descriptors=len(keys), all_covered_by_original_v850=True,
                  firmware_results_identical=True, packet_checks='single-bend ramps, raw circle angles/roads and gated snap, BAP fallback/side, compatible raw turns, half-degree transport',
                  capture_cases=len(captures['captures']), capture_unique_commands=captures['unique_commands'],
                  binary_sha256=captures['binary_sha256'],
                  scope='Original V850 active 3D mode 3, G1952/M2325; renderer macOS OpenGL, no vehicle test',
                  sources={str(p.relative_to(ROOT)): sha(p) for p in [*oracle_files, hud_file, out/'java_mapping.csv']})
    (out/'parity-checks.json').write_text(json.dumps(report, indent=2)+'\n')
    sources = [*sorted((ROOT/'java_patch/com/luka/carplay/rgd').glob('*.java')),
               *sorted((ROOT/'maneuver_render').glob('*.c')), *sorted((ROOT/'maneuver_render').glob('*.h'))]
    (out/'source-hashes.json').write_text(json.dumps({str(p.relative_to(ROOT)): sha(p) for p in sources}, indent=2)+'\n')
    print(f'PASS: {len(rows)} Java cases, {len(keys)} original V850 descriptors in both firmwares; 54-type comparison sheet')


if __name__ == '__main__':
    main()
