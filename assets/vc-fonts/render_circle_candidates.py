"""Exact-font circular-symbol specimens, with explicit coverage tiers."""
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
GROUPS = [
    ('AudiTypeDisplayHigh-Normal', 'Audi: direct glyphs (small dots / raised rings)', '•·°˚'),
    ('SourceHanSansJP-Regular', 'Additional JP/KR fonts: VC fallback NOT yet tested',
     '○◯◦●◉◎⦿⊙☉◌◐◑◒◓⊕⊖⊗⊘⏀⏁⏂⓪①❶Ⓣ'),
]


def main():
    width, cell, row_height = 1240, 100, 112
    height = 95 + sum(74 + ((len(chars)+11)//12)*row_height for _,_,chars in GROUPS)
    img = Image.new('RGB',(width,height),'#15191f')
    draw = ImageDraw.Draw(img)
    draw.text((22,17),'VC 0379 - circular symbols from actual firmware fonts',font=ImageFont.load_default(size=23),fill='white')
    draw.text((22,52),'Not VC screenshots. Circle-with-line symbols below are mathematical/dentistry signs, not clocks.',font=ImageFont.load_default(size=14),fill='#a6b2c4')
    y = 95
    report = []
    for name,title,chars in GROUPS:
        catalog=json.loads((ROOT/'catalogs'/f'{name}.json').read_text())
        cmap={e['codepoint']:e for e in catalog['unicode']}
        assert all(ord(c) in cmap for c in chars)
        face=ImageFont.truetype(str(ROOT/catalog['font']['file']),44)
        draw.text((22,y),title,font=ImageFont.load_default(size=19),fill='#78c8ff')
        y+=47
        for i,c in enumerate(chars):
            x,top=20+i%12*cell,y+i//12*row_height
            draw.rounded_rectangle((x+3,top,x+96,top+102),radius=6,fill='#242b35')
            # Common baseline preserves raised position of degree/ring-above.
            draw.text((x+50,top+60),c,font=face,anchor='ms',fill='white')
            draw.text((x+9,top+79),f'U+{ord(c):04X}',font=ImageFont.load_default(size=14),fill='#78c8ff')
        y+=((len(chars)+11)//12)*row_height+27
        report.append(dict(font=name,characters=[cmap[ord(c)] for c in chars]))
    img.save(ROOT/'previews/circle-candidates.png')
    (ROOT/'circle-candidates.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')


if __name__=='__main__':
    main()
