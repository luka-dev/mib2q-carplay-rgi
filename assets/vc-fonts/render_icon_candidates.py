"""Diagnostic specimen: exact extracted fonts, no automatic font fallback."""
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parent
GROUPS = [
    ('AudiTypeDisplayHigh-Normal.ttf', 'DIRECT AUDI FONT',
     'Present in all five Audi faces; physical VC/BAP test still pending.',
     '\uf8ff►◀◆◊▪'),
    ('SourceHanSansJP-Regular.otf', 'ADDITIONAL JP/KR FONT - VC FALLBACK UNVERIFIED',
     'Present in both extracted Source Han fonts, absent from Audi faces. Monochrome glyphs.',
     '◉◎◈❖★☆☀☁☂☃☎☏☜☞☝☟♥♡♠♣♪♫✓⚠➡⊙⊕◐◑❶❷❸'),
]


def main():
    img = Image.new('RGB', (1240, 785), '#15191f')
    draw = ImageDraw.Draw(img)
    draw.text((24, 18), 'VC 0379 - pictogram candidates from firmware fonts',
              font=ImageFont.load_default(size=24), fill='white')
    draw.text((24, 53), 'Font specimens, not VC screenshots. U+F8FF below is the Audi rings, not the macOS Apple glyph.',
              font=ImageFont.load_default(size=15), fill='#a6b2c4')
    y = 98
    audit = []
    for filename, title, subtitle, chars in GROUPS:
        catalog = json.loads((ROOT / 'catalogs' / (Path(filename).stem + '.json')).read_text())
        cmap = {e['codepoint']: e for e in catalog['unicode']}
        assert all(ord(c) in cmap for c in chars)
        draw.text((24, y), title, font=ImageFont.load_default(size=20), fill='#78c8ff')
        draw.text((24, y + 30), subtitle, font=ImageFont.load_default(size=15), fill='#a6b2c4')
        y += 65
        for i, c in enumerate(chars):
            x, top = 20 + (i % 12) * 100, y + (i // 12) * 120
            draw.rounded_rectangle((x + 3, top, x + 96, top + 110), radius=6, fill='#242b35')
            face = ImageFont.truetype(str(ROOT / 'fonts' / filename), 44)
            box = draw.textbbox((0, 0), c, font=face)
            if box[2] - box[0] > 82:
                face = ImageFont.truetype(str(ROOT / 'fonts' / filename), int(44 * 82 / (box[2] - box[0])))
                box = draw.textbbox((0, 0), c, font=face)
            draw.text((x + (100 - (box[2] - box[0])) / 2 - box[0], top + 15 - box[1]), c, font=face, fill='white')
            draw.text((x + 9, top + 80), f'U+{ord(c):04X}', font=ImageFont.load_default(size=14), fill='#78c8ff')
        y += ((len(chars) + 11) // 12) * 120 + 24
        audit.append({'font': filename, 'characters': [cmap[ord(c)] for c in chars]})
    img.crop((0, 0, 1240, y)).save(ROOT / 'previews' / 'icon-candidates.png')
    (ROOT / 'icon-candidates.json').write_text(json.dumps(audit, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
