#!/usr/bin/env python3
"""Focused original HUD resource sheet and measured ramp descriptor comparisons."""
import csv
import json
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from audit_hud_maneuver_mapping import key

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'output/maneuver-audit'


def main():
    rows = list(csv.DictReader((OUT / 'ramp_mapping.csv').open()))
    def row(mt, angle, side, streets=''):
        return next(r for r in rows if (int(r['mt']), int(r['angle']), int(r['traffic']), r['junction_angles'])
                    == (mt, angle, side, streets))
    def descriptor(r):
        return tuple(r[k] for k in ('bap_main', 'bap_direction', 'bap_z', 'bap_sidestreets'))
    checks = []
    for side in (0, 1):
        for sign in (-1, 1):
            for magnitude in (45, 90, 135):
                angle = sign * magnitude
                on = row(9, angle, side)
                slight = row(49 if sign < 0 else 50, angle, side)
                keep = row(13 if sign < 0 else 14, angle, side)
                off = row(22 if sign < 0 else 23, angle, side)
                change = row(52 if sign < 0 else 53, angle, side)
                assert descriptor(on) == descriptor(slight) == descriptor(keep)
                assert on['renderer_icon'] == '4' and slight['renderer_icon'] == '2'
                assert keep['renderer_hex'] == slight['renderer_hex']
                assert off['renderer_hex'] == change['renderer_hex']
                assert off['bap_main'] != change['bap_main']
                assert descriptor(off) == descriptor(row(8, angle, side))
                for streets in ('0', '0;' + str(angle)):
                    assert descriptor(off) == descriptor(row(int(off['mt']), angle, side, streets))
                checks.append(dict(traffic=side, angle=angle,
                    on_equals_slight_equals_keep_descriptor=list(descriptor(on)),
                    on_renderer_icon=4, slight_renderer_icon=2,
                    off_equals_change_renderer_command=off['renderer_hex'],
                    off_descriptor=list(descriptor(off)), change_descriptor=list(descriptor(change))))
    (OUT / 'ramp-equivalence-checks.json').write_text(json.dumps(dict(
        input_rows=len(rows), cases_passed=len(checks),
        scope='Actual Java bridge descriptors and renderer commands; no BAP-to-HUD lookup assumed.',
        cases=checks), indent=2) + '\n')

    hud = json.loads((OUT / '8W6919617G_1952-selectors.json').read_text())
    measured = {key(r): r for r in csv.DictReader((OUT / 'mcu-chain-output.csv').open())}
    def composite(result):
        fields = {9: int(result['selector']), 10: int(result['marker']),
                  **{11+i: v for i, v in enumerate(bytes.fromhex(result['roads_hex']))}}
        # Keep original node order: road outlines, road fills, then arrow/marker.
        # This reconstructs navigation layers only, not all panel compositor modes.
        canvas = Image.new('RGBA', (169, 140), 'black')
        layers = []
        for node in hud['nodes']:
            if node['group'] != 2:
                continue
            selector = fields[node['field']]
            if selector >= len(node['states']):
                continue
            for surface in node['states'][selector]['surfaces']:
                x, y, width, height = struct.unpack_from('<4H', bytes.fromhex(surface['descriptor_hex']), 30)
                layer = Image.open(surface['png']).convert('RGBA')
                assert layer.size == (width, height)
                canvas.alpha_composite(layer, (x, y))
                layers.append(dict(field=node['field'], selector=selector,
                                   resource=surface['resource'], x=x, y=y))
        return canvas, layers
    font_path = '/System/Library/Fonts/Supplemental/Arial.ttf'
    title = ImageFont.truetype(font_path, 25)
    font = ImageFont.truetype(font_path, 20)
    small = ImageFont.truetype(font_path, 16)
    sheet = Image.new('RGB', (1320, 920), '#10151c')
    draw = ImageDraw.Draw(sheet)
    draw.text((18, 12), 'BAP → оригинальный MCU → слои 3D HUD', font=title, fill='white')
    draw.text((18, 48), 'Измерено на G1952 и M2325. Стрелка и дороги по данным MCU; позиции слоёв из FD62.', font=small, fill='#73ddeb')
    examples = [
        (15, 0, '00', 'Съезд: прямой выход', 'EXIT_RIGHT + 00'),
        (15, 224, '00', 'Съезд: slight right', 'EXIT_RIGHT + E0'),
        (15, 192, '00', 'Съезд: right', 'EXIT_RIGHT + C0'),
        (15, 160, '00', 'Съезд: sharp right', 'EXIT_RIGHT + A0'),
        (13, 224, '', 'Въезд / slight / keep', 'TURN + E0'),
        (12, 192, '', 'Смена магистрали вправо', 'CHANGE_LANE + C0'),
        (17, 224, '', 'Боковая дорога вправо', 'SERVICE_ROAD_RIGHT + E0'),
        (16, 32, '00', 'Съезд: slight left', 'EXIT_LEFT + 20'),
    ]
    manifests = []
    for index, (main, direction, streets, label, detail) in enumerate(examples):
        x, y = (index % 4) * 330, 90 + (index // 4) * 365
        result = measured[(main, direction, 0, streets)]
        im, layers = composite(result)
        draw.rectangle((x+3, y, x+326, y+358), outline='#465362')
        draw.text((x+12, y+10), label, font=font, fill='white')
        draw.text((x+12, y+40), detail, font=small, fill='#73ddeb')
        sheet.paste(im.resize((270, 224), Image.Resampling.NEAREST), (x+30, y+68))
        if int(result['selector']) == 0:
            draw.text((x+52, y+155), 'ПУСТОЙ 3D-СЛОЙ', font=font, fill='#ffc471')
        resources = '+'.join(s['resource'] for s in layers if s['field'] == 9) or 'empty'
        draw.text((x+12, y+303), f"s{result['selector']} / {resources} / streets={streets or '[]'}", font=small, fill='#ffc471')
        roads = bytes.fromhex(result['roads_hex'])[:int(result['road_count'])]
        draw.text((x+12, y+329), 'road selectors: ' + ', '.join(map(str, roads)), font=small, fill='#b5c1ce')
        manifests.append(dict(result, label=label, surfaces=layers))
        if (main, direction) == (15, 224):
            im.resize((676, 560), Image.Resampling.NEAREST).save(OUT / 'hud-exit-e0-composite.png')
    draw.text((18, 836), 'Слои собраны офлайн; это не фотография HUD. Активный 3D-режим 3 задан явно.', font=font, fill='white')
    draw.text((18, 872), 's39 — SERVICE_ROAD, не EXIT. Для EXIT_RIGHT+A0 и EXIT_LEFT+60 прошивка возвращает s0.', font=font, fill='#ffc471')
    sheet.save(OUT / 'hud-ramp-shapes.png')
    (OUT / 'hud-ramp-layer-composites.json').write_text(json.dumps(manifests, indent=2, ensure_ascii=False) + '\n')
    print(f'{len(checks)} equivalence checks; {OUT / "hud-ramp-shapes.png"}')


if __name__ == '__main__':
    main()
