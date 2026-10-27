#!/usr/bin/env python3
"""One zoomable PNG with every verified main 3D HUD selector and BAP examples.

Uses measured V850 results and original FD62 resources/positions. No artwork
is synthesized. The JSON sidecar retains every selected descriptor and layer.
"""
import argparse
import csv
import hashlib
import json
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from audit_hud_maneuver_mapping import key

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'output/maneuver-audit'
FONT = '/System/Library/Fonts/Supplemental/Arial.ttf'
BOLD = '/System/Library/Fonts/Supplemental/Arial Bold.ttf'
MONO = '/System/Library/Fonts/Menlo.ttc'


def family(selector):
    ranges = [
        (0, 0, 'ПУСТОЙ 3D-СЛОЙ', '#8c9aa9'),
        (1, 4, 'МЕСТО НАЗНАЧЕНИЯ', '#dfb774'),
        (5, 20, 'АЗИМУТ / OFF ROAD', '#91a8be'),
        (21, 23, 'ПОДГОТОВКА К МАНЁВРУ', '#c2abd9'),
        (24, 25, 'ПЕРЕСТРОЕНИЕ', '#c2abd9'),
        (26, 32, 'ПОВОРОТ', '#40c8df'),
        (33, 35, 'СЪЕЗД ВПРАВО', '#f7b46b'),
        (36, 38, 'СЪЕЗД ВЛЕВО', '#f7b46b'),
        (39, 40, 'БОКОВАЯ ДОРОГА', '#b9ca7d'),
        (41, 42, 'РАЗВИЛКА: 2 НАПРАВЛЕНИЯ', '#c2abd9'),
        (43, 45, 'РАЗВИЛКА: 3 НАПРАВЛЕНИЯ', '#c2abd9'),
        (46, 61, 'КРУГ / RHT', '#6bbcae'),
        (62, 77, 'КРУГ / LHT', '#6bbcae'),
        (78, 93, 'ПЛОЩАДЬ / RHT', '#8faede'),
        (94, 109, 'ПЛОЩАДЬ / LHT', '#8faede'),
        (110, 111, 'РАЗВОРОТ', '#d7a0b7'),
        (112, 112, 'ВЫХОД С КРУГА / RHT', '#6bbcae'),
        (113, 113, 'ВЫХОД С КРУГА / LHT', '#6bbcae'),
        (114, 115, 'ПОДГОТОВКА К РАЗВОРОТУ', '#d7a0b7'),
        (116, 117, 'MICHIGAN TURN', '#d7a0b7'),
        (118, 119, 'ДВОЙНОЙ ПОВОРОТ', '#d7a0b7'),
        (120, 120, 'ПРИБЫТИЕ / ПРЯМО', '#dfb774'),
    ]
    return next((name, color) for lo, hi, name, color in ranges if lo <= selector <= hi)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', type=Path, default=OUT)
    args = ap.parse_args()
    out = args.out
    with (out / 'mcu-chain-output.csv').open() as f:
        rows = list(csv.DictReader(f))
    with (out / 'mcu-chain-M-output.csv').open() as f:
        assert rows == list(csv.DictReader(f)), 'G/M results differ'
    measured = {key(r): r for r in rows}
    hud = json.loads((out / '8W6919617G_1952-selectors.json').read_text())
    nodes = [n for n in hud['nodes'] if n['group'] == 2]
    main_node = next(n for n in nodes if n['node'] == '0x8da20')
    assert len(main_node['states']) == 121
    assert {int(r['selector']) for r in rows} == set(range(121))
    # Destination and some roundabout layers exceed the 169x140 ramp area.
    # Take the union of all original bounds so no arrow/flag gets cropped.
    extents = [struct.unpack_from('<4H', bytes.fromhex(s['descriptor_hex']), 30)
               for n in nodes for state in n['states'] for s in state['surfaces']]
    native_w = max(x+w for x, y, w, h in extents)
    native_h = max(y+h for x, y, w, h in extents)
    images = {}

    def composite(result):
        fields = {9: int(result['selector']), 10: int(result['marker']),
                  **{11+i: v for i, v in enumerate(bytes.fromhex(result['roads_hex']))}}
        canvas = Image.new('RGBA', (native_w, native_h), 'black')
        surfaces = []
        for node in nodes:
            selector = fields[node['field']]
            if selector >= len(node['states']):
                continue
            for surface in node['states'][selector]['surfaces']:
                x, y, w, h = struct.unpack_from('<4H', bytes.fromhex(surface['descriptor_hex']), 30)
                assert x + w <= native_w and y + h <= native_h, (surface['resource'], x, y, w, h)
                if surface['png'] not in images:
                    images[surface['png']] = Image.open(surface['png']).convert('RGBA')
                layer = images[surface['png']]
                assert layer.size == (w, h)
                canvas.alpha_composite(layer, (x, y))
                surfaces.append(dict(field=node['field'], selector=selector,
                                     resource=surface['resource'], x=x, y=y,
                                     width=w, height=h))
        return canvas, surfaces

    cards = []
    for selector in range(121):
        candidates = [r for r in rows if int(r['selector']) == selector
                      and r['z'] == '0' and r['streets_hex'] == '']
        result = min(candidates, key=lambda r: (int(r['main']), int(r['direction'])))
        # Show ramps with the straight side road our bridge actually sends.
        if 33 <= selector <= 38:
            k = key(result)
            result = measured[(*k[:3], '00')]
            assert int(result['selector']) == selector
        title, accent = family(selector)
        aliases = sorted({int(r['main']) for r in rows if int(r['selector']) == selector
                          and r['direction'] == result['direction'] and r['z'] == '0'
                          and r['streets_hex'] == result['streets_hex']})
        cards.append(dict(id=f's{selector:03d}', title=title, accent=accent,
                          result=result, main_aliases=aliases, section='main'))
    extras = [
        ('Z1', 'СЪЕЗД ВПРАВО / Z=1', (15, 224, 1, '00')),
        ('Z2', 'СЪЕЗД ВПРАВО / Z=2', (15, 224, 2, '00')),
        ('Z3', 'СЪЕЗД ВЛЕВО / Z=1', (16, 32, 1, '00')),
        ('Z4', 'СЪЕЗД ВЛЕВО / Z=2', (16, 32, 2, '00')),
        ('X-R', 'SHARP EXIT RIGHT: ПУСТО', (15, 160, 0, '00')),
        ('X-L', 'SHARP EXIT LEFT: ПУСТО', (16, 96, 0, '00')),
        ('F0', 'FOLLOW: ОСОБОЕ ПРАВИЛО', (11, 0, 0, '')),
    ]
    for label, title, k in extras:
        cards.append(dict(id=label, title=title, accent='#df8b81', result=measured[k],
                          main_aliases=[], section='special'))
    assert len(cards) == 128

    columns, tile_w, tile_h, header, footer = 8, 512, 472, 266, 130
    width = columns * tile_w
    height = header + (len(cards) // columns) * tile_h + footer
    atlas = Image.new('RGB', (width, height), '#10151c')
    draw = ImageDraw.Draw(atlas)
    fonts = dict(title=ImageFont.truetype(BOLD, 54), header=ImageFont.truetype(FONT, 29),
                 id=ImageFont.truetype(BOLD, 33), label=ImageFont.truetype(BOLD, 21),
                 code=ImageFont.truetype(MONO, 22), small=ImageFont.truetype(MONO, 18),
                 note=ImageFont.truetype(FONT, 20))
    draw.text((32, 24), 'HUD — ВСЕ 3D-МАНЁВРЫ И ИХ ЗНАЧЕНИЯ BAP', font=fonts['title'], fill='white')
    draw.text((34, 99), '121 основной selector: s000–s120  •  4 z-маркера  •  примеры неподдерживаемых сочетаний',
              font=fonts['header'], fill='#9dd8e4')
    draw.text((34, 145), 'BAP = main (DEC) / direction (HEX). S — боковые дороги BAP в HEX. roads — внутренние selectors дорог в DEC.',
              font=fonts['header'], fill='#c8d2dc')
    draw.text((34, 191), 'G1952 = M2325. Оригинальные слои HUD и координаты FD62; активный 3D-режим. В каждой карточке один проверенный пример.',
              font=fonts['header'], fill='#c8d2dc')
    text_checks = 0

    def tile_text(x, y, text, font, fill):
        nonlocal text_checks
        assert draw.textlength(text, font=font) <= tile_w - 40, text
        draw.text((x+22, y), text, font=font, fill=fill)
        text_checks += 1

    for index, card in enumerate(cards):
        x, y = (index % columns) * tile_w, header + (index // columns) * tile_h
        result = card['result']
        draw.rectangle((x+5, y+4, x+tile_w-6, y+tile_h-5), fill='#151c25', outline='#354350', width=2)
        draw.rectangle((x+6, y+5, x+tile_w-7, y+9), fill=card['accent'])
        tile_text(x, y+17, card['id'], fonts['id'], card['accent'])
        tile_text(x, y+59, card['title'], fonts['label'], '#edf2f8')
        im, surfaces = composite(result)
        image_x = x+(tile_w-2*native_w)//2
        atlas.paste(im.resize((2*native_w, 2*native_h), Image.Resampling.NEAREST), (image_x, y+89))
        if int(result['selector']) == 0:
            draw.text((x+198, y+209), 'ПУСТО', font=fonts['label'], fill='#dfa77a')
        streets = bytes.fromhex(result['streets_hex']).hex(' ').upper() or '-'
        code = f"BAP {int(result['main']):02d}/{int(result['direction']):02X}  z={result['z']}  S=[{streets}]"
        tile_text(x, y+380, code, fonts['code'], '#f2c184')
        roads = list(bytes.fromhex(result['roads_hex'])[:int(result['road_count'])])
        resource_names = list(dict.fromkeys(s['resource'] for s in surfaces if s['field'] == 9))
        resources = '+'.join(resource_names) or '-'
        tile_text(x, y+414, f"roads={','.join(map(str,roads)) or '-'}  {resources}", fonts['small'], '#b8c7d6')
        if card['id'] == 'F0':
            note = 'Первый слот очищен; это не весь HUD'
        elif card['section'] == 'special':
            note = f"main s{int(result['selector']):03d} / marker {result['marker']}"
        elif int(result['selector']) == 0:
            note = 'Пустое состояние основной 3D-группы'
        elif len(card['main_aliases']) > 1:
            note = 'Также main: ' + ', '.join(str(v) for v in card['main_aliases'] if v != int(result['main']))
        else:
            note = 'main=DEC, direction=HEX; s не BAP-код'
        tile_text(x, y+441, note, fonts['note'], '#879aab')
        card['surfaces'] = surfaces
        card['roads'] = roads
        card['image_bounds'] = [image_x, y+89, 2*native_w, 2*native_h]
        card['tile_bounds'] = [x, y, tile_w, tile_h]

    bottom = height - footer
    draw.text((34, bottom+17), 'Для обсуждения называй номер s и BAP-пару. Пример: s035 = EXIT_RIGHT, main=15, direction=E0, S=[00].',
              font=fonts['header'], fill='#9dd8e4')
    draw.text((34, bottom+64), 'Серые дороги зависят от descriptor; их варианты не исчерпываются одним примером. Это офлайн-сборка ресурсов, не фотография панели.',
              font=fonts['header'], fill='#a8b7c6')
    filename = 'hud-all-maneuvers.png'
    atlas.save(out / filename, optimize=True)
    manifest = dict(image=filename, dimensions=[width,height], main_selectors=121,
                    total_cards=len(cards), text_width_checks=text_checks,
                    scope='Original main/marker/road resources; representative measured descriptor per main selector; '
                          'all 121 main states included; original active 3D mode, not full panel-mode simulation.',
                    source_sha256={name: hashlib.sha256((out/name).read_bytes()).hexdigest() for name in
                                   ('mcu-chain-output.csv','mcu-chain-M-output.csv','8W6919617G_1952-selectors.json')},
                    cards=cards)
    (out / 'hud-all-maneuvers.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False)+'\n')
    print(f'{out/filename}: {width}x{height}; 121 main states, 128 cards; {text_checks} text-width checks passed')


if __name__ == '__main__':
    main()
