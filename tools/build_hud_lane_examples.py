#!/usr/bin/env python3
"""Composite verified original HUD lane selectors and untouched firmware sprites."""
from pathlib import Path
import csv,json,struct
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'output/r2/hud-lanes-2026-09-13'

def main():
    g=list(csv.DictReader((OUT/'lane-G.csv').open()));m=list(csv.DictReader((OUT/'lane-M.csv').open()));assert g==m
    tables=json.loads((OUT/'8W6919617G_1952-lane-selectors.json').read_text())
    other=json.loads((OUT/'8W6919617M_2325-lane-selectors.json').read_text());assert tables['nodes']==other['nodes']
    rows={};images={}
    for row in g:
        packet=bytes.fromhex(row['packet_hex']);canvas=Image.new('RGBA',(185,187),'black');surfaces=[]
        for node in tables['nodes']:
            i=node['field']-36
            if node['selector_type']==2:sel=struct.unpack_from('<H',packet,i)[0]
            else:
                mask=node['mask'];shift=(mask&-mask).bit_length()-1;sel=(packet[i]&mask)>>shift
            if sel>=len(node['states']):continue
            for s in node['states'][sel]['surfaces']:
                p=s['png']
                if p not in images:images[p]=Image.open(p).convert('RGBA')
                assert images[p].size==(s['width'],s['height'])
                canvas.alpha_composite(images[p],(s['x'],s['y']))
                surfaces.append({k:v for k,v in s.items() if k not in ('png','descriptor_hex')})
        row['surfaces']=surfaces;row['image']=canvas.crop((0,146,169,185)).convert('RGB');rows[row['name']]=row
    expected={'single_status_0':'lg_dr_e0','single_status_1':'lg_dr_e0','single_status_2':'lg_dr_e0_recommended'}
    for name,wanted in expected.items():assert [s['name'] for s in rows[name]['surfaces'] if s['name']]==[wanted]
    shared=[s['name'] for s in rows['shared_status_2']['surfaces'] if s['name']]
    assert len(shared)==2 and sum(s.endswith('_recommended') for s in shared)==1
    assert not rows['hidden']['surfaces'] and not rows['empty']['surfaces']
    for name in ['eight_best_0','eight_best_7','eight_best_3','eight_best_4']:
        ell=[s for s in rows[name]['surfaces'] if s['resource']=='r0675'];assert len(ell)==1
        # Overflow replaces a whole cell with the original ellipsis sprite.
        assert len({(s['x'],s['y']) for s in ell})==1
    pairs=[('single_status_0','Статус 0 · не рекомендована'),('single_status_1','Статус 1 · рекомендована'),
           ('single_status_2','Статус 2 · основная рекомендация'),('shared_status_2','Одна полоса · направо + прямо'),
           ('three_directions','Одна полоса · три направления'),('count_5','Пять полос помещаются целиком'),
           ('eight_best_0','8 полос · основная крайняя слева'),('eight_best_7','8 полос · основная крайняя справа'),
           ('eight_best_3','8 полос · основная четвёртая слева'),('eight_best_4','8 полос · основная пятая слева'),
           ('count_2','Две полосы · центрирование'),('count_3','Три полосы · центрирование')]
    font='/System/Library/Fonts/Supplemental/Arial.ttf'
    def f(n):return ImageFont.truetype(font,n)
    sheet=Image.new('RGB',(1450,1300),'#151515');draw=ImageDraw.Draw(sheet)
    draw.text((25,20),'Как HUD рисует полосы',font=f(34),fill='#fff')
    draw.text((25,68),'Оригинальные спрайты и код HUD G1952 / M2325 · увеличение ×4',font=f(23),fill='#aaa')
    for i,(name,label) in enumerate(pairs):
        x=25+(i%2)*715;y=123+(i//2)*188
        draw.text((x,y),label,font=f(22),fill='#ddd')
        sheet.paste(rows[name]['image'].resize((676,156),Image.Resampling.NEAREST),(x,y+28))
    draw.text((25,1260),'Офлайн-реконструкция: активная панель полос; не фотография физической проекции.',font=f(20),fill='#999')
    sheet.save(OUT/'hud-lanes.png')
    data={k:{kk:vv for kk,vv in v.items() if kk!='image'} for k,v in rows.items()}
    (OUT/'lane-results.json').write_text(json.dumps(dict(cases=len(g),firmware_results_identical=True,gdc_states_verified_each=tables['cases_passed'],examples=data),indent=2)+'\n')
    print('PASS:',len(g),'native layouts identical across firmware, colour and overflow assertions pass')

if __name__=='__main__':main()
