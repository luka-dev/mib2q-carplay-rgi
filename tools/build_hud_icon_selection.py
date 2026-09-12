#!/usr/bin/env python3
"""Review all 54 CarPlay types against measured original HUD resources.

Consumes before/current Java bridge exports plus original V850 G/M results.
All chosen BAP descriptors must exist in the executed MCU oracle; no inferred
shape lookup or generated icon is used.
"""
import csv
import argparse
import hashlib
import json
import struct
from collections import Counter
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from audit_hud_maneuver_mapping import key, java_key

ROOT = Path(__file__).resolve().parents[1]
AUDIT = ROOT / 'output/maneuver-audit'
OUT = ROOT / 'output/maneuver-selection'
CHANGED = {8, 11, 16, 20, 21, 22, 23, 51}
REVIEWS = {
    0: ('FOLLOW_STREET 11/00', 'Оставить', 'Нет команды поворота. Сохраняем режим следования; s0 первого 3D-слота не означает пустой HUD.'),
    1: ('s028; 13/40', 'Оставить', 'Тип явно задаёт левый поворот. Slight и sharp имеют отдельные типы; знак противоречащего угла не меняет сторону.'),
    2: ('s031; 13/C0', 'Оставить', 'Тип явно задаёт правый поворот. Дополнительный излом EXIT или перестроение здесь не нужны.'),
    3: ('s026; 13/00', 'Оставить', 'Явная команда проехать прямо через узел; TURN/00 точнее режима FOLLOW без команды.'),
    4: ('s110/s111; 25/40,C0', 'Оставить', 'Обычная петля разворота. Сторона по знаку доступного угла, иначе по стороне движения.'),
    5: ('FOLLOW_STREET 11/00', 'Оставить', 'Продолжение движения по дороге. Не превращаем отсутствие нового манёвра в команду поворота.'),
    6: ('RHT s031 / LHT s028; 13/C0,40', 'Оставить', 'Отдельная фаза входа на круг. Сохраняем штатный входной поворот; полный круг с произвольным выездом добавил бы неподтверждённое направление.'),
    7: ('RHT s112 / LHT s113; 26,27', 'Оставить', 'Специальная форма выхода с кольца лучше обычного TURN и повторного полного круга.'),
    8: ('s033/s036; 15,16/00', 'Изменить', 'Базовый съезд; угол выбирает только сторону. Один угол CarPlay не обосновывает варианты s034/035/037/038; A0/60 у EXIT пусты.'),
    9: ('s027/s032; 13/20,E0', 'Оставить', 'Для въезда известна сторона. TURN slight — простой доступный знак; SERVICE_ROAD добавляет предположение о параллельной дороге.'),
    10: ('s120; 3/00', 'Оставить', 'Завершение навигации: флаг назначения по центру.'),
    11: ('s026…s032; 13 + угол', 'Изменить', 'Начало маршрута не фиксирует сторону: используем ближайшую допустимую TURN-геометрию по известному углу вместо любого ненулевого угла → 90°.'),
    12: ('s120; 3/00', 'Оставить', 'Прибытие без явно заданной стороны: центральный флаг.'),
    13: ('s027; 13/20', 'Оставить', 'Держаться левее: простая slight-стрелка. FORK_2/FORK_3 требуют уверенности в устройстве развилки; дороги уже передаются отдельно.'),
    14: ('s032; 13/E0', 'Оставить', 'Держаться правее: сохраняем slight-стрелку, не предполагаем обязательное число ветвей развилки.'),
    15: ('FOLLOW_STREET 11/00', 'Оставить', 'В каталоге нет самостоятельной иконки парома. FOLLOW не придумывает направление посадки или съезда.'),
    16: ('s026…s032; 13 + угол', 'Изменить', 'После парома показываем известное направление одной дороги. Уточняем угол обычного TURN, без выдуманной рампы.'),
    17: ('FOLLOW_STREET 11/00', 'Оставить', 'Для смены парома нет отдельного HUD-ресурса и гарантированной геометрии поворота.'),
    18: ('s110/s111; 25/40,C0', 'Оставить', 'Начало маршрута с разворотом: действие уже явно задано, поэтому полная петля.'),
    19: ('RHT s054 / LHT s070; 21,22/80', 'Оставить', 'Разворот именно через круг. Тип задаёт обратный выезд; обычная UTURN-петля потеряла бы кольцо.'),
    20: ('s027/s028/s029; 13/20,40,60', 'Изменить', 'Один левый поворот в конце дороги. Сохраняем slight/normal/sharp по согласованному углу; при missing или конфликте — обычный левый.'),
    21: ('s032/s031/s030; 13/E0,C0,A0', 'Изменить', 'Один правый поворот в конце дороги. Резкость имеет прямой смысл в TURN; больше не округляем всё до 90°.'),
    22: ('s036; 16/00', 'Изменить', 'Сторона явно левая; базовая рампа сохраняется при любом угле и не пропадает на −135°.'),
    23: ('s033; 15/00', 'Изменить', 'Сторона явно правая; базовая рампа сохраняется при любом угле и не пропадает на +135°.'),
    24: ('s002; 3/40', 'Оставить', 'Прибытие слева: флаг на указанной стороне.'),
    25: ('s003; 3/C0', 'Оставить', 'Прибытие справа: флаг на указанной стороне.'),
    26: ('s110/s111; 25/40,C0', 'Оставить', 'Развернуться при возможности — команда разворота, а не отдельная стадия предварительного оповещения PREPARE_U_TURN.'),
    27: ('s120; 3/00', 'Оставить', 'Конец указаний: центральный флаг назначения.'),
    47: ('s029; 13/60', 'Оставить', 'Резкий левый поворот задан самим типом. Эта геометрия допустима у TURN.'),
    48: ('s030; 13/A0', 'Оставить', 'Резкий правый поворот задан самим типом. Неподдерживаемость A0 у EXIT не относится к TURN.'),
    49: ('s027; 13/20', 'Оставить', 'Явный slight left: простая диагональная стрелка без рампы.'),
    50: ('s032; 13/E0', 'Оставить', 'Явный slight right: простая диагональная стрелка без рампы.'),
    51: ('s026…s032; 13 + угол', 'Изменить', 'Общая смена магистрали не задаёт сторону перестроения. Используем известный угол обычного TURN, без SERVICE_ROAD или сложной рампы.'),
    52: ('s024; 12/40', 'Оставить', 'Явная смена магистрали влево: отдельное семейство CHANGE_LANE; EXIT и SERVICE_ROAD имеют другой смысл.'),
    53: ('s025; 12/C0', 'Оставить', 'Явная смена магистрали вправо: сохраняем отдельное семейство CHANGE_LANE.'),
}
for mt in range(28, 47):
    REVIEWS[mt] = ('RHT s046…061 / LHT s062…077; 21,22 + угол', 'Оставить',
                   f'Выезд №{mt-27} задаёт порядок, но не направление. Форма кольца и выезд по углу с шагом 22,5°; отдельного числового значка этого номера в данных HUD нет.')
REVIEWS = {mt: (rule, 'Изменено' if mt in CHANGED else 'Сохранено', reason)
           for mt, (rule, _, reason) in REVIEWS.items()}


def read(path):
    with path.open() as f:
        return list(csv.DictReader(f))


def prepare_new_cases():
    known = {key(r) for r in read(AUDIT/'mcu-chain-output.csv')}
    wanted = {java_key(r) for name in ('before_java_mapping.csv','java_mapping.csv',
                                     'before_selection_examples.csv','selection_examples.csv')
              for r in read(OUT/name)}
    missing = sorted(wanted-known)
    with (OUT/'mcu-new-input.csv').open('w') as f:
        w=csv.writer(f);w.writerow(['case','main','direction','z','streets_hex'])
        w.writerows((f'new{i:03d}',*k) for i,k in enumerate(missing))
    print(f'{len(missing)} additional BAP descriptors to execute with the V850 probe on G/M')


def main():
    before = read(OUT / 'before_java_mapping.csv')
    after = read(OUT / 'java_mapping.csv')
    assert len(before) == len(after) == 7890
    oracle = {}
    new_v850_cases = len(read(OUT/'mcu-new-G-output.csv'))
    for directory, g, m in [(AUDIT, 'mcu-chain-output.csv', 'mcu-chain-M-output.csv'),
                            (OUT, 'mcu-new-G-output.csv', 'mcu-new-M-output.csv')]:
        gr, mr = read(directory/g), read(directory/m)
        assert gr == mr, 'Original V850 G/M mismatch'
        for row in gr:
            oracle[key(row)] = row
    changed_rows = Counter()
    input_fields = ['case','mt','type','angle','junction','traffic','z_input','junction_angles']
    for a, b in zip(before, after):
        assert [a[k] for k in input_fields] == [b[k] for k in input_fields]
        assert a['renderer_hex'] == b['renderer_hex'], 'Renderer command changed unexpectedly'
        assert a['bap_sidestreets'] == b['bap_sidestreets'] and a['bap_z'] == b['bap_z']
        assert java_key(a) in oracle and java_key(b) in oracle
        if java_key(a) != java_key(b):
            changed_rows[int(a['mt'])] += 1
    assert set(changed_rows) == CHANGED
    assert set(REVIEWS) == set(range(54))
    def resolved(row):
        return oracle[java_key(row)]
    # No supported maneuver loses its 3D main icon; FOLLOW's special path and
    # the pre-existing NO_INFO junction gate are handled separately.
    repaired = 0
    for a, b in zip(before, after):
        old, new = int(resolved(a)['selector']), int(resolved(b)['selector'])
        assert not (old and not new), ('newly empty 3D', b)
        if not old and new:
            repaired += 1
        if int(b['mt']) in (8,22,23) and b['junction']=='0':
            assert b['bap_direction']=='0' and int(resolved(b)['selector']) in (33,36)
    full = [dict(r, **{'hud_'+k:v for k,v in resolved(r).items()
                      if k not in ('case','main','direction','z','streets_hex')}) for r in after]
    with (OUT/'java_mapping-hud.csv').open('w') as f:
        writer=csv.DictWriter(f,fieldnames=list(full[0]));writer.writeheader();writer.writerows(full)
    # Same controlled input before/after. Illustrate refined generic/end turns
    # at angles where the old 90-degree reduction lost information.
    before_examples = read(OUT/'before_selection_examples.csv')
    after_examples = read(OUT/'selection_examples.csv')
    assert len(before_examples)==len(after_examples)==108
    for a,b in zip(before_examples,after_examples):
        assert [a[k] for k in input_fields]==[b[k] for k in input_fields]
        assert java_key(a) in oracle and java_key(b) in oracle
    selected=[]
    for mt in range(54):
        examples=[]
        for side in (0,1):
            index=mt*2+side
            a,b=before_examples[index],after_examples[index]
            assert int(b['mt'])==mt and int(b['traffic'])==side
            examples.append(dict(before=a,after=b,before_hud=resolved(a),hud=resolved(b)))
        angle=int(examples[0]['after']['angle']);junction=int(examples[0]['after']['junction'])
        rule,status,reason=REVIEWS[mt]
        selected.append(dict(mt=mt,type=examples[0]['after']['type'],rule=rule,status=status,
                             reason=reason,angle=angle,junction=junction,examples=examples))

    hud=json.loads((AUDIT/'8W6919617G_1952-selectors.json').read_text())
    nodes=[n for n in hud['nodes'] if n['group']==2]
    def composite(result):
        image=Image.new('RGBA',(185,141),'black')
        fields={9:int(result['selector']),10:int(result['marker']),
                **{11+i:v for i,v in enumerate(bytes.fromhex(result['roads_hex']))}}
        for node in nodes:
            value=fields[node['field']]
            if value>=len(node['states']):continue
            for surface in node['states'][value]['surfaces']:
                x,y,w,h=struct.unpack_from('<4H',bytes.fromhex(surface['descriptor_hex']),30)
                assert x+w<=185 and y+h<=141
                layer=Image.open(surface['png']).convert('RGBA');assert layer.size==(w,h)
                image.alpha_composite(layer,(x,y))
        return image

    columns,tw,th,header,footer=6,704,464,220,144
    atlas=Image.new('RGB',(columns*tw,header+9*th+footer),'#10151c');draw=ImageDraw.Draw(atlas)
    normal='/System/Library/Fonts/Supplemental/Arial.ttf';bold='/System/Library/Fonts/Supplemental/Arial Bold.ttf'
    title=ImageFont.truetype(bold,52);font=ImageFont.truetype(bold,24);small=ImageFont.truetype(normal,22)
    note=ImageFont.truetype(normal,19);mono=ImageFont.truetype('/System/Library/Fonts/Menlo.ttc',19)
    draw.text((30,22),'CARPLAY → HUD: ВЫБОР ДЛЯ ВСЕХ 54 ТИПОВ',font=title,fill='white')
    draw.text((32,94),'8 типов уточнены  •  46 сохранены  •  в каждой карточке RHT и LHT  •  BAP: main DEC / direction HEX',font=small,fill='#b7dfe6')
    draw.text((32,135),'Оригинальные слои HUD; MCU G1952/M2325. Контрольные примеры: junctionAngles=[−90,0,+90], z=0; угол выхода указан в карточке.',font=small,fill='#bdcbd8')
    draw.text((32,174),'Номера s соответствуют полному атласу. Варианты боковых дорог и режимы всей панели этим листом не исчерпываются.',font=small,fill='#bdcbd8')
    for i,card in enumerate(selected):
        x,y=(i%columns)*tw,header+(i//columns)*th
        accent='#e9af6f' if card['mt'] in CHANGED else '#7abdad'
        draw.rectangle((x+5,y+4,x+tw-6,y+th-5),fill='#151d27',outline='#384755',width=2)
        draw.rectangle((x+6,y+5,x+tw-7,y+10),fill=accent)
        text=f"{card['mt']:02d}  {card['type'][3:]}"
        assert draw.textlength(text,font=font)<tw-40
        draw.text((x+20,y+22),text,font=font,fill='white')
        draw.text((x+20,y+58),f"{card['status'].upper()}   angle={card['angle']}°   junction={card['junction']}",font=small,fill=accent)
        for side,example in enumerate(card['examples']):
            sx=x+side*(tw//2)
            image=composite(example['hud']).resize((296,226),Image.Resampling.NEAREST)
            atlas.paste(image,(sx+28,y+94))
            r=example['after'];old=example['before'];sel=int(example['hud']['selector']);old_sel=int(example['before_hud']['selector'])
            if not sel:draw.text((sx+66,y+191),'FOLLOW / другой режим',font=note,fill='#d7b994')
            label=f"{'LHT' if side else 'RHT'}  s{sel:03d}  BAP {r['bap_main']}/{int(r['bap_direction']):02X}"
            assert draw.textlength(label,font=mono)<tw//2-35
            draw.text((sx+20,y+331),label,font=mono,fill='#c6e7ed')
            changed=f"было s{old_sel:03d} / {old['bap_main']}/{int(old['bap_direction']):02X}" if java_key(old)!=java_key(r) else 'выбор сохранён'
            draw.text((sx+20,y+363),changed,font=note,fill=accent)
        rule=card['rule']
        assert draw.textlength(rule,font=small)<tw-40
        draw.text((x+20,y+409),rule,font=small,fill='#c4cedb')
    bottom=atlas.height-footer
    draw.text((32,bottom+22),'FOLLOW 11/00: пустой первый 3D-слот является штатным особым случаем; это не утверждение о пустом экране HUD.',font=small,fill='#d5bd96')
    draw.text((32,bottom+66),'EXIT_1…EXIT_19 показаны при одном угле +90°: порядковый номер съезда не позволяет восстановить его направление.',font=small,fill='#bdcbd8')
    atlas.save(OUT/'carplay-hud-selected-54.png',optimize=True)
    with (OUT/'selection-54.csv').open('w') as f:
        w=csv.writer(f);w.writerow(['mt','type','status','rule','reason','example_angle','rht_selector','lht_selector','rht_bap','lht_bap'])
        for c in selected:
            examples=c['examples'];bap=lambda e:f"{e['after']['bap_main']}/{int(e['after']['bap_direction']):02X}"
            w.writerow([c['mt'],c['type'],c['status'],c['rule'],c['reason'],c['angle'],
                        examples[0]['hud']['selector'],examples[1]['hud']['selector'],bap(examples[0]),bap(examples[1])])
    summary=dict(reviewed_types=54,changed_types=sorted(CHANGED),kept_types=46,
                 java_rows=len(after),changed_rows=sum(changed_rows.values()),changed_rows_by_type=dict(changed_rows),
                 descriptors=len({java_key(r) for r in after}),all_descriptors_original_V850_verified=True,
                 new_V850_cases_per_firmware=new_v850_cases,G_M_identical=True,newly_empty_3D=0,repaired_empty_3D_rows=repaired,
                 renderer_commands_unchanged=True,dimensions=list(atlas.size),
                 source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
                    ['java_patch/com/luka/carplay/rgd/ManeuverMapper.java','java_patch/com/luka/carplay/rgd/RendererMapper.java',
                     'java_patch/com/luka/carplay/rgd/SideStreets.java','java_patch/com/luka/carplay/rgd/BAPBridge.java']})
    (OUT/'selection-checks.json').write_text(json.dumps(summary,indent=2)+'\n')
    (OUT/'selection-54.json').write_text(json.dumps(selected,indent=2,ensure_ascii=False)+'\n')
    lines=['# Выбор HUD-иконок для всех типов CarPlay', '',
        '**Проверены все 54 типа: для 8 уточнено направление, 46 сохранены.** '
        'Изменения внесены в ManeuverMapper. Это выбор BAP-иконки, не полная переделка геометрии нашего OpenGL renderer.', '',
        '[Единый лист выбранных иконок, RHT/LHT](../../output/maneuver-selection/carplay-hud-selected-54.png). '
        '[Каталог всех доступных s000–s120](../../output/maneuver-audit/hud-all-maneuvers.png).', '',
        'Основание выбора: сначала смысл ManeuverType, затем действительно доступный угол одного узла. '
        'Число сегментов дороги, параллельные дороги, развилка и сложный двойной поворот не выводятся только из знака угла. '
        'Сохраняется прежний junctionType gate; конфликтующий или неподдерживаемый тип узла не обходится ради картинки.', '',
        'На листе 108 отдельных прогонов настоящего bridge: каждый тип в RHT/LHT, z=0, junctionAngles=[−90,0,+90]. '
        'Углы выбраны для демонстрации; это не запись дорожного движения. Исходный mapper для сравнения примеров '
        'скомпилирован из 28327b6 с проверкой SHA256 и загружен только в отдельном host-процессе.', '',
        '## Изменения', '',
        '- **8/22/23, съезды:** main 15/16, direction 00 → s033/s036. У общего OFF_RAMP угол определяет только сторону; '
        'у 22/23 сторона задана типом. sideStreets=[00] сохранены. s034/035/037/038 больше не выбираются, '
        'неподдерживаемые EXIT/A0 и EXIT/60 тоже исключены.',
        '- **11/16/51, общий выход на дорогу:** сохраняется TURN, направление выбирается по 7 допустимым направлениям. '
        'Границы абсолютного угла: 0…22° прямо, 23…67° slight, 68…112° normal, 113…180° sharp. '
        '±1000 и −1 → прежний fallback прямо; для прочих значений вне ±180° сохранён прежний выбор left/right по знаку. '
        'В TURN нет direction=80; разворот задаётся отдельным типом.',
        '- **20/21, поворот в конце дороги:** сохраняются рассчитанные slight/normal/sharp. '
        'Противоречащий стороне угол и missing → обычный поворот в сторону типа. −1 обрабатывается как unset.', '',
        'Для простого TURN угол описывает единственный поворот. Для EXIT те же числовые направления выбирают другое семейство геометрии, '
        'поэтому правило резкости обычного поворота к рампе не переносится.', '',
        '## Решение по каждому типу', '',
        's — внутренний selector HUD; main в DEC, direction в HEX. Варианты ниже относятся к допустимому junctionType. '
        '«Сохранено» означает лучший обоснованный вариант при текущих данных, а не наличие точной иконки для любой дорожной ситуации.', '',
        '| MT | CarPlay | Решение | HUD / BAP | Почему |','|---:|---|---|---|---|']
    for c in selected:
        lines.append(f"| {c['mt']} | {c['type'][3:]} | {c['status']} | {c['rule']} | {c['reason']} |")
    lines += ['', '## Какие альтернативы не выбраны', '',
        '- SERVICE_ROAD s039/s040 не подставляется вместо ON_RAMP: отдельная параллельная дорога не следует из одного типа въезда.',
        '- FORK_2/FORK_3 s041…s045 не подставляются вместо KEEP: тип не гарантирует конкретную развилку. TURN + sideStreets остаётся общим представлением.',
        '- SQUARE s078…s109 не подставляется вместо круга: в нашей цепочке нет признака прямоугольной площади.',
        '- MICHIGAN/DOUBLE_TURN s116…s119 не строятся из одного exitAngle. Последующие maneuver slots сами по себе не задают геометрию такого сложного знака.',
        '- PREPARE_U_TURN s114/s115 не заменяет U_TURN_WHEN_POSSIBLE: «при возможности» не равнозначно фазе предварительного показа.',
        '- FOLLOW остаётся для NO_TURN/FOLLOW/FERRY без направления. Особая обработка 11/00/z0/empty не позволяет оценивать весь HUD только по s0.', '',
        '## Проверка и ограничения', '',
        f'- Реальный Java bridge: {len(after):,} входов и отдельно 216 ramp-входов. Изменились {sum(changed_rows.values())} строк только у указанных 8 типов.',
        f'- Все 173 текущих уникальных дескриптора и отдельные примеры карточек покрыты исполнением V850 G1952/M2325: предыдущая матрица и {new_v850_cases} дополнительных сочетаний. Результаты двух прошивок одинаковы.',
        f'- Новых пустых 3D-моделей нет; восстановлены {repaired} строк матрицы с ранее неподдерживаемыми острыми EXIT.',
        '- RendererMapperDirectionTest проверяет границы углов, missing, явную сторону и типы рамп. Production Java собрана с target 1.4.',
        '- Команды нашего renderer не изменены. Известные различия MERGE против TURN, кругов, серых дорог и обновления кэша остаются отдельной задачей 1:1.',
        '- Проверен выбор в активном 3D-режиме, а не переключение всех режимов/оптика HUD или фактический кадр штатной приборки. Автомобиль не обновлялся.', '',
        'Артефакты: [проверки](../../output/maneuver-selection/selection-checks.json), '
        '[все входы с HUD-полями](../../output/maneuver-selection/java_mapping-hud.csv), '
        '[54 решения CSV](../../output/maneuver-selection/selection-54.csv), '
        '[исходные/новые примеры JSON](../../output/maneuver-selection/selection-54.json).', '',
        'Исторический аудит в output/maneuver-audit сохранён как исходная точка. Актуальный выбор после этой правки — в этом документе.', '',
        '```sh','bash scripts/audit_maneuvers.sh output/maneuver-selection',
        'python3 tools/build_hud_icon_selection.py --prepare',
        '# Если появились новые BAP descriptors: прогнать их tools/ghidra/HudManeuverChainAudit.java на G/M.',
        'python3 tools/build_hud_icon_selection.py','```','']
    (ROOT/'docs/reference/CARPLAY_HUD_ICON_SELECTION.md').write_text('\n'.join(lines))
    print(json.dumps(summary,indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare',action='store_true')
    if parser.parse_args().prepare:prepare_new_cases()
    else:main()
