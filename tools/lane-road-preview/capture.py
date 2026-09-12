#!/usr/bin/env python3
"""Capture the actual GLES-compatible preview and assemble review sheets."""
from pathlib import Path
import hashlib
import json
import subprocess
from PIL import Image, ImageDraw, ImageFont, ImageChops, ImageStat

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'output/lane-road-aligned'
TITLES = [
    ('Съезд справа', '4 полосы · одна предпочтительная'),
    ('Съезд из двух полос', '5 полос · допустимая + предпочтительная'),
    ('Четвёртая от левого края', '8 полос · все полосы до настоящего края видны'),
    ('Правый край широкой дороги', '8 полос · скрыты полосы слева'),
    ('Поворот налево', '5 полос · запас для наконечника'),
    ('Неизвестные направления', '±1000 слева · справа две поворачивающие, одна рекомендована'),
    ('Нет рекомендации', 'не назначаем полосу самостоятельно'),
    ('Рекомендации по разным краям', 'один настоящий край · продолжение с другой стороны'),
    ('Две полосы налево', 'общее ответвление · две полосы и внешний белый край'),
    ('Три полосы налево', 'широкое ответвление · разделители продолжаются в поворот'),
    ('Три налево — средняя основная', 'продолжается в средней · не прижимаем маршрут к краю'),
    ('Три налево — правая основная', 'продолжается в правой · порядок полос сохранён'),
    ('Прямо и направо из одной полосы', 'общее открытое пересечение · обводка под прямой дорогой'),
    ('Защита: пересечение прямой полосы', 'обычный манёвр · направление внешней полосы не меняем'),
    ('Защита: неизвестная внешняя полоса', 'обычный манёвр · не предполагаем разрешённый поворот'),
    ('Защита: неполный список', 'обычный манёвр · нет подтверждения полного набора'),
    ('Защита: пропуск позиции', 'обычный манёвр · не придумываем отсутствующую полосу'),
    ('Защита: другой манёвр', 'стрелка берётся из манёвра · противоречивые полосы не привязываем'),
]

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run(['make', '-C', 'tools/lane-road-preview', 'all', 'test'], cwd=ROOT, check=True)
    binary = ROOT / 'build/lane-road-preview/preview'
    with (OUT / 'capture.log').open('w') as log, (OUT / 'renderer.log').open('w') as errors:
        subprocess.run([str(binary), '--capture', str(OUT)], cwd=ROOT,
                       stdout=log, stderr=errors, check=True, timeout=60)
    font_path = '/System/Library/Fonts/Supplemental/Arial.ttf'
    title_font = ImageFont.truetype(font_path, 23)
    body_font = ImageFont.truetype(font_path, 16)
    small_font = ImageFont.truetype(font_path, 13)
    sheet = Image.new('RGB', (1120, 132+len(TITLES)*276), '#11161d')
    d = ImageDraw.Draw(sheet)
    d.text((24,18), 'LANE ROAD / нативный визуальный эксперимент', font=title_font, fill='#dbe7f2')
    d.text((24,55), 'Схематичные данные · реальный OpenGL, материалы и стрелка maneuver_render', font=body_font, fill='#8393a5')
    d.text((510,97), 'Обрезка 210 × 153', font=body_font, fill='#9cacbc')
    d.text((824,97), 'Полная зона 328 × 180', font=body_font, fill='#9cacbc')
    captures = []
    for c, (title, subtitle) in enumerate(TITLES):
        y = 132+c*276
        d.line((24,y,1096,y), fill='#26303c')
        d.text((24,y+27), f'{c+1:02d}  {title}', font=body_font, fill='#e3edf5')
        # Keep descriptive text within its column.
        words=subtitle.split(); line=''; yy=y+59
        for word in words:
            if d.textlength(line+' '+word,font=small_font)>400:
                d.text((24,yy),line,font=small_font,fill='#879aaa');yy+=20;line=word
            else: line=(line+' '+word).strip()
        d.text((24,yy),line,font=small_font,fill='#879aaa')
        for col, kind in enumerate(('crop', 'wide')):
            ppm = OUT / f'{c:02d}-{kind}.ppm'
            im = Image.open(ppm); width, height = im.size
            im.save(OUT/f'{c:02d}-{kind}-source.png')
            bounds = (59,27,269,180) if kind=='crop' else (0,0,328,180)
            im = im.crop(tuple(round(v*(width/328 if i%2==0 else height/181)) for i,v in enumerate(bounds)))
            png=OUT/f'{c:02d}-{kind}.png';im.save(png)
            native=im.resize((210,153) if kind=='crop' else (328,180),Image.Resampling.LANCZOS)
            native.save(OUT/f'{c:02d}-{kind}-native.png')
            tile=im.copy();tile.thumbnail((304,236),Image.Resampling.LANCZOS)
            sheet.paste(tile,(466+col*326+(304-tile.width)//2,y+20+(236-tile.height)//2))
            captures.append(dict(case=c,area=kind,png=png.name,source_rect=bounds))
            ppm.unlink()
    sheet.save(OUT/'comparison.png')
    hero=Image.new('RGB',(1000,530),'#11161d'); hd=ImageDraw.Draw(hero)
    for col,c in enumerate((1,3)):
        hd.text((25+col*500,15),TITLES[c][0],font=title_font,fill='#e3edf5')
        im=Image.open(OUT/f'{c:02d}-crop.png');im.thumbnail((480,420))
        hero.paste(im,(10+col*500+(480-im.width)//2,65))
    hd.text((25,496),'Яркая сплошная — настоящий край. Скрытая сторона продолжается и исчезает фейдом.',font=body_font,fill='#9bacbb')
    hero.save(OUT/'overview.png')
    framed=Image.new('RGB',(1384,914),'#11161d');fd=ImageDraw.Draw(framed)
    native=Image.new('RGB',(704,474),'#11161d');nd=ImageDraw.Draw(native)
    for i,c in enumerate((1,3,2,4)):
        x,y=20+(i%2)*682,20+(i//2)*442
        fd.text((x,y),TITLES[c][0],font=title_font,fill='#e3edf5')
        im=Image.open(OUT/f'{c:02d}-crop-source.png')
        assert im.size==(656,362), f'Expected stock Retina framebuffer, got {im.size}'
        framed.paste(im,(x,y+43))
        nx,ny=16+(i%2)*344,16+(i//2)*232
        nd.text((nx,ny),TITLES[c][0],font=small_font,fill='#e3edf5')
        native.paste(im.resize((328,181),Image.Resampling.LANCZOS),(nx,ny+25))
    fd.text((20,886),'Красная рамка: (59,27) 210×153 внутри исходного 328×181. Ширина стрелки и FOV сохранены.',font=body_font,fill='#9bacbb')
    framed.save(OUT/'framing.png');native.save(OUT/'native-size.png')
    # Same visible lanes/camera/glyphs, different total lane count. The omitted
    # adjacent continuation lane is identical. Fixed progress must be repeatable.
    a=Image.open(OUT/'00-crop.png');b=Image.open(OUT/'03-crop.png')
    assert ImageChops.difference(a,b).getbbox() is None, 'Equivalent scenes changed across captures'
    frame_results=[]
    for line in (OUT/'capture.log').read_text().splitlines():
        if line.startswith('FRAME '):
            t=line.split();assert t[3]=='PASS',line
            frame_results.append(dict(case=int(t[1]),area=t[2],bounds=list(map(float,t[5:9])),
                                      camera=list(map(float,t[10:12])),visible_lanes=int(t[13]),
                                      secondary_width=float(t[15]),hidden_side=int(t[17]),mesh_hash=t[19]))
    assert len(frame_results)==2*len(TITLES)
    for c in range(len(TITLES)):
        small,wide=frame_results[c*2:c*2+2]
        for field in ('camera','secondary_width','mesh_hash'):
            assert small[field]==wide[field], f'Case {c}: stage changed {field}'
        assert all(r['hidden_side'] in (-1,0,1) for r in (small,wide))
        assert wide['visible_lanes']>=small['visible_lanes']
    # The packed eight-lane small stage has room for secondary context after
    # fitting the fixed primary composition. Do not fade immediately at lane 4.
    assert frame_results[2*2]['visible_lanes']>=5, 'Unused small-stage lane space'
    exits=[]
    for line in (OUT/'capture.log').read_text().splitlines():
        if line.startswith('EXIT '):
            t=line.split();exits.append(dict(case=int(t[1]),area=t[2],lanes=int(t[4]),glyph_clearance=float(t[6])))
    assert len(exits)==2*len(TITLES)
    for entry in exits:
        assert entry['glyph_clearance']>=.022-1e-5,entry
        assert entry['lanes']=={0:1,1:2,3:1,4:1,5:2,8:2,9:3,10:3,11:3,12:1}.get(entry['case'],0),entry
    for c in range(len(TITLES)):
        assert exits[c*2]['lanes']==exits[c*2+1]['lanes']
    heads=[]
    for line in (OUT/'capture.log').read_text().splitlines():
        if line.startswith('HEAD '):
            t=line.split();heads.append(dict(case=int(t[1]),area=t[2],minimum=float(t[4]),maximum=float(t[6])))
    assert len(heads)==2*len(TITLES)
    assert all(h['minimum']>=-.1301 and h['maximum']<=.1301 for h in heads)
    alpha=[]
    for line in (OUT/'capture.log').read_text().splitlines():
        if line.startswith('ALPHA '):
            t=line.split();alpha.append(dict(case=int(t[1]),area=t[2],invalid_pixels=int(t[4]),max_excess=int(t[6])))
    assert len(alpha)==2*len(TITLES)
    assert all(a['invalid_pixels']==0 and a['max_excess']<=2 for a in alpha), alpha
    safety=[]
    expected_reasons={6:3,13:7,14:7,15:1,16:2,17:5}
    for line in (OUT/'capture.log').read_text().splitlines():
        if line.startswith('SAFETY '):
            t=line.split();entry=dict(case=int(t[1]),area=t[2],reason=int(t[4]))
            assert entry['reason']==expected_reasons.get(entry['case'],0),entry
            safety.append(entry)
    assert len(safety)==2*len(TITLES)
    glyphs=[]
    for line in (OUT/'capture.log').read_text().splitlines():
        if line.startswith('GLYPH '):
            t=line.split();glyphs.append(dict(case=int(t[1]),area=t[2],lane=int(t[4]),geometry_hash=t[6]))
    for c in range(len(TITLES)):
        small={g['lane']:g['geometry_hash'] for g in glyphs if g['case']==c and g['area']=='crop'}
        wide={g['lane']:g['geometry_hash'] for g in glyphs if g['case']==c and g['area']=='wide'}
        for lane in small.keys() & wide.keys():
            assert small[lane]==wide[lane], f'Case {c}, lane {lane}: stage changed glyph geometry'
    for f in frame_results:
        if f['case'] in expected_reasons: assert f['visible_lanes']==0,f
    # Different rejected lane inputs must not affect the independent maneuver.
    for kind in ('crop','wide'):
        base=Image.open(OUT/f'13-{kind}.png')
        for c in (14,15,16):
            assert ImageChops.difference(base,Image.open(OUT/f'{c:02d}-{kind}.png')).getbbox() is None
    frames=[]
    for kind,title in (('crop','Малая зона'),('wide','Большая зона — та же стрелка')):
        canvas=Image.new('RGB',(696,422),'#11161d');cd=ImageDraw.Draw(canvas)
        cd.text((20,10),title,font=title_font,fill='#e3edf5')
        canvas.paste(Image.open(OUT/f'02-{kind}-source.png'),(20,50));frames.append(canvas)
    frames[0].save(OUT/'stage-stability.gif',save_all=True,append_images=frames[1:],duration=1100,loop=0)
    # Exercise actual intermediate GPU frames, including a left/right change
    # and lane-road -> safety fallback. Include fully idle frames after the
    # native light settles; both endpoints must exactly match settled scenes.
    transition_dir=OUT/'transition';transition_dir.mkdir(exist_ok=True)
    transitions=[]
    for current,nxt in ((1,2),(3,11),(5,13)):
        for ppm in transition_dir.glob('*.ppm'):ppm.unlink()
        with (transition_dir/'capture.log').open('w') as log, (transition_dir/'renderer.log').open('w') as errors:
            subprocess.run([str(binary),'--capture-transition',str(transition_dir),str(current),str(nxt)],
                           cwd=ROOT,stdout=log,stderr=errors,check=True,timeout=60)
        files=sorted(transition_dir.glob('*-crop.ppm'),key=lambda f:int(f.name.split('-')[0]))
        moving=[Image.open(f).convert('RGB') for f in files]
        assert 25<len(moving)<901, 'Native handoff did not run through camera settle'
        endpoint_error=[]
        for frame,c in ((moving[0],current),(moving[-1],nxt)):
            diff=ImageChops.difference(frame,Image.open(OUT/f'{c:02d}-crop-source.png'))
            peak=max(hi for lo,hi in diff.getextrema());mean=max(ImageStat.Stat(diff).mean)
            assert peak==0, f'Transition {current}->{nxt}: endpoint moved ({peak}, {mean})'
            endpoint_error.append(dict(max_channel_error=peak,mean_channel_error=mean))
        hashes={hashlib.sha256(frame.tobytes()).hexdigest() for frame in moving}
        assert len(hashes)>25, 'Transition did not advance'
        travel=[]
        for line in (transition_dir/'capture.log').read_text().splitlines():
            if line.startswith('TRAVEL '):
                t=line.split();travel.append(dict(frame=int(t[1]),x=float(t[3]),y=float(t[4]),rotation=float(t[6]),phase=int(t[8]),fill=float(t[10]),handoff=int(t[12])))
        assert len(travel)==len(moving)
        distance=max((f['x']**2+f['y']**2)**.5 for f in travel)
        rotation=max(abs(f['rotation']) for f in travel)
        assert distance>1 and rotation>.5, 'Camera did not follow the joined route through its turn'
        assert travel[-1]['phase']==0 and all(abs(travel[-1][k])<1e-5 for k in ('x','y','rotation'))
        departing=[f['fill'] for f in travel if f['handoff']]
        arriving=[f['fill'] for f in travel[1:] if not f['handoff']]
        assert len(set(departing))>5 and departing[-1]==0, 'Old fill did not visibly retreat'
        assert all(b<=a for a,b in zip(departing,departing[1:])), 'Incoming target leaked into departure'
        assert arriving[0]==0 and len(set(arriving))>5, 'New fill appeared instantly at commit'
        assert all(b>=a for a,b in zip(arriving,arriving[1:])), 'New fill did not grow continuously'
        assert arriving[-1]==travel[0]['fill']==.75
        transitions.append(dict(current=current,next=nxt,frames=len(moving),engine='native joined-route',
                                exact_endpoints=True,endpoint_error=endpoint_error,valid_rgba=True,
                                progress_handoff='retreat to zero, hold through push, refill on commit',
                                max_camera_travel=distance,max_rotation=rotation))
        if current==1:
            moving[0].save(OUT/'maneuver-transition.gif',save_all=True,append_images=moving[1:],
                           duration=[650]+[33]*(len(moving)-2)+[650],loop=0)
            contact=Image.new('RGB',(656*3,362*2))
            for slot,i in enumerate((0,15,30,45,60,len(moving)-1)):
                contact.paste(moving[i],((slot%3)*656,(slot//3)*362))
            contact.save(transition_dir/'contact-sheet.png')
        for ppm in transition_dir.glob('*.ppm'):ppm.unlink()
    manifest=dict(scope='Standalone visual spike; synthetic lane states; real existing renderer shaders/mesh; no live iOS/HUD deployment',
                  binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  baseline='3f87f64256e79994555c9569aebebb044d8c5952',captures=captures,
                  source_size=[328,181],framebuffer_size=[656,362],fov_degrees=40,arrow_width=.14,
                  framing=frame_results,exits=exits,projected_heads=heads,window_alpha=alpha,safety=safety,glyphs=glyphs,
                  transitions=transitions)
    (OUT/'captures.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(OUT/'comparison.png')

if __name__=='__main__': main()
