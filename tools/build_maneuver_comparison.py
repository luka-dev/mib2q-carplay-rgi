#!/usr/bin/env python3
"""Create the audit table and side-by-side reference sheets from measured data.

HUD selections come from execution of the original V850 BAP decoder in
active 3D mode. Internal selector-to-surface mapping is ARM-verified.
"""
import argparse
import csv
import hashlib
import json
import re
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from audit_hud_maneuver_mapping import key, java_key

ICONS = ['NONE', 'APPROACH', 'TURN', 'UTURN', 'MERGE', 'EXIT', 'ROUNDABOUT', 'ARRIVED']
MAIN = {0:'NO_SYMBOL',1:'NO_INFO',3:'ARRIVED',11:'FOLLOW_STREET',12:'CHANGE_LANE',13:'TURN',15:'EXIT_RIGHT',16:'EXIT_LEFT',21:'ROUNDABOUT_RHT',22:'ROUNDABOUT_LHT',25:'UTURN',26:'EXIT_ROUNDABOUT_RHT',27:'EXIT_ROUNDABOUT_LHT'}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('out', type=Path, nargs='?', default=Path('output/maneuver-audit'))
    args = ap.parse_args()
    out = args.out.resolve(); repo = Path(__file__).resolve().parents[1]
    rows = list(csv.DictReader((out/'java_mapping.csv').open()))
    canonical = [r for r in rows if r['case'] == 'canonical']
    measured = {key(r): r for r in csv.DictReader((out/'mcu-chain-output.csv').open())}
    captures = json.loads((out/'renderer-captures.json').read_text())['captures']
    shots = {r['row']['renderer_hex']: r['png'] for r in captures}
    hud = json.loads((out/'8W6919617G_1952-selectors.json').read_text())
    nodes = [n for n in hud['nodes'] if n['field'] == 9]
    def surfaces(selector):
        if selector is None: return []
        return [s for n in nodes if selector < len(n['states']) for s in n['states'][selector]['surfaces']]
    pairs = []
    for r in canonical:
        result = measured[java_key(r)]
        selector = int(result['selector'])
        layers = surfaces(selector)
        # Contract example from actual bridge descriptor, with absent slots empty.
        # This is NOT the raw output of the reconstructed stock JXE serializer.
        side = bytes.fromhex(r['bap_sidestreets'])
        contract = bytes([int(r['bap_main']), int(r['bap_direction']), int(r['bap_z']), len(side)]) + side + bytes(8)
        pairs.append(dict(r, descriptor_contract_example=contract.hex(' '),
            hud_selector=selector, hud_resources='+'.join(s['resource'] for s in layers),
            hud_model=result['model'], hud_marker=result['marker'], hud_roads_hex=result['roads_hex'],
            hud_road_count=result['road_count'], hud_verification='original_V850_active_3D_mode3_G1952_M2325',
            renderer_png=shots[r['renderer_hex']]))
    with (out/'canonical_comparison.csv').open('w') as f:
        writer = csv.DictWriter(f, fieldnames=list(pairs[0])); writer.writeheader(); writer.writerows(pairs)
    font = ImageFont.truetype('/System/Library/Fonts/Menlo.ttc', 13)
    small = ImageFont.truetype('/System/Library/Fonts/Menlo.ttc', 11)
    def sheets(selected, name, count):
        files=[]
        for start in range(0, len(selected), count):
            subset=selected[start:start+count]
            sheet=Image.new('RGB',(1080,58+((len(subset)+1)//2)*226),'#11151b'); d=ImageDraw.Draw(sheet)
            d.text((12,9),'HUD = measured original main layers; renderer = real OpenGL frame',font=font,fill='white')
            d.text((12,30),'BAP -> V850 -> ARM verified, active 3D mode. Road layers omitted on this sheet.',font=small,fill='#f0bb60')
            for i,r in enumerate(subset):
                x=(i%2)*540; y=58+(i//2)*226
                d.rectangle((x+2,y+1,x+537,y+223),outline='#384554')
                d.text((x+10,y+8),f"{r['mt']:>2} {r['type'][3:]}",font=font,fill='white')
                d.text((x+10,y+29),f"angle={r['angle']}  {'LHT' if r['traffic']=='1' else 'RHT'}  BAP={r['bap_main']}/{int(r['bap_direction']):02X}",font=small,fill='#8edbed')
                layers=surfaces(r['hud_selector'])
                tilew=210//max(1,len(layers))
                for j,s in enumerate(layers):
                    im=Image.open(s['png']).convert('RGBA'); box=im.getbbox()
                    if box: im=im.crop(box)
                    im.thumbnail((tilew-8,115))
                    sheet.paste(im,(x+10+j*tilew+(tilew-im.width)//2,y+53),im)
                if not layers: d.text((x+25,y+95),'EMPTY 3D LAYER',font=font,fill='#f0bb60')
                im=Image.open(out/r['renderer_png']).convert('RGB');im.thumbnail((290,155))
                sheet.paste(im,(x+237,y+47))
                d.text((x+10,y+181),f"HUD s={r['hud_selector']}: {r['hud_resources']}",font=small,fill='#f0bb60')
                d.text((x+237,y+205),f"{ICONS[int(r['renderer_icon'])]} dir={r['renderer_direction']} a={r['renderer_angle']}",font=small,fill='white')
            filename=f'{name}_{start//count+1:02d}.png';sheet.save(out/filename);files.append(filename)
        return files
    pages=sheets([r for r in pairs if r['traffic']=='0'],'comparison_rht',10)
    lht=sheets([r for r in pairs if r['traffic']=='1' and int(r['mt']) in (4,6,7,8,9,19,28,52)],'comparison_lht',8)
    lines=['# Полная таблица iAP2 → BAP → maneuver_render', '',
        'Сгенерировано `tools/build_maneuver_comparison.py` из исполнения реального Java-кода. Все 54 типа; RHT и LHT.', '',
        '**BAP → HUD selector и дороги получены исполнением оригинального V850-кода G1952/M2325.** '
        'Исполнением ARM проверена связь внутреннего индекса с поверхностью. Принудительно выбран активный 3D-режим 3; '
        'это не эмуляция всех режимов панели и не запись CAN.', '',
        'Углы ниже — явно заданные примеры. Для EXIT_1…EXIT_19 намеренно использован один угол +90°: '
        'номер съезда сам по себе не определяет его направление. Входные junctionAngles=[−90,0,+90], z=0. '
        'Для типов 6/7/19/28…46 junctionType=1, для остальных 0. Полная сетка с другими входами лежит в CSV.', '',
        'BAP: `main / direction HEX`; renderer: `ICON / signed direction / angle degrees`. '
        '`sN` — **внутренний selector HUD packet[22]**, не BAP-код.', '',
        '| MT | iAP2 | Угол | BAP RHT → LHT | Sidestreets RHT → LHT (hex) | Renderer RHT → LHT | HUD 3D RHT → LHT |',
        '|---:|---|---:|---|---|---|---|']
    def paired(a,b): return a if a==b else a+' → '+b
    for a,b in zip(pairs[::2],pairs[1::2]):
        bap=lambda r:f"{r['bap_main']}/{int(r['bap_direction']):02X}"
        cr=lambda r:f"{ICONS[int(r['renderer_icon'])]}/{r['renderer_direction']}/{r['renderer_angle']}"
        hu=lambda r:f"s{r['hud_selector']} {r['hud_resources'] or '∅'}"
        lines.append(f"| {a['mt']} | {a['type'][3:]} | {a['angle']} | {paired(bap(a),bap(b))} | {paired(a['bap_sidestreets'] or '∅',b['bap_sidestreets'] or '∅')} | {paired(cr(a),cr(b))} | {paired(hu(a),hu(b))} |")
    lines += ['', '## Изображения', '', 'Оригинальные 3D-слои HUD показаны без реконструкции серых дорог. '
              'Это сравнение формы стрелки с полным кадром renderer; не попиксельная проверка всего экрана.', '']
    for i,name in enumerate(pages):
        lines.append(f'- [RHT: типы {i*10}–{min(53,i*10+9)}](../../output/maneuver-audit/{name})')
    for name in lht: lines.append(f'- [LHT: круги, разворот и рампы](../../output/maneuver-audit/{name})')
    lines+=['','## Машинные данные','',
        '- [Все 7 890 прогонов Java](../../output/maneuver-audit/java_mapping.csv)',
        '- [108 пар RHT/LHT с примерами контракта](../../output/maneuver-audit/canonical_comparison.csv)',
        '- [Все входы Java + измеренные HUD selector/marker/roads](../../output/maneuver-audit/java_mapping-hud.csv)',
        '- [Проверка V850 G1952/M2325](../../output/maneuver-audit/mcu-chain-checks.json)',
        '- [122 захвата renderer / 48 команд](../../output/maneuver-audit/renderer-captures.json)',
        '', '**`stock_fct23_hex` содержит артефакт реконструированного JXE:** вместо пустых строк там встречается '
        '`isNativeLittleEndian`. Это не запись с CAN. Для исходных sidestreets использовать `bap_sidestreets`; '
        '`descriptor_contract_example` — расчётный пакет из реальных полей bridge с пустыми отсутствующими слотами. '
        'Подробности и выводы: [MANEUVER_HUD_RENDERER_AUDIT.md](MANEUVER_HUD_RENDERER_AUDIT.md).','']
    (repo/'docs-obsidian/Sources/reference/MANEUVER_MAPPING_TABLE.md').write_text('\n'.join(lines))
    inputs=['java_patch/com/luka/carplay/rgd/ManeuverMapper.java','java_patch/com/luka/carplay/rgd/RendererMapper.java',
        'java_patch/com/luka/carplay/rgd/SideStreets.java','java_patch/com/luka/carplay/rgd/BAPBridge.java',
        'java_patch/com/luka/carplay/rgd/RendererServer.java','java_patch/com/luka/carplay/rgd/RouteGuidance.java',
        'hook/routeguidance/rgd_tlv.c','hook/routeguidance/rgd_hook.c','maneuver_render/maneuver.c',
        'maneuver_render/main.c','maneuver_render/protocol.h','maneuver_render/maneuver.h',
        'maneuver_render/render.c','maneuver_render/route_path.c','maneuver_render/platform_macos.c']
    (out/'source-hashes.json').write_text(json.dumps({p:hashlib.sha256((repo/p).read_bytes()).hexdigest() for p in inputs},indent=2)+'\n')
    print(f'54 types, 108 pairs, {len(pages)+len(lht)} comparison sheets')


if __name__=='__main__': main()
