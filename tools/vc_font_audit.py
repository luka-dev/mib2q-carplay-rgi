#!/usr/bin/env python3
"""Extract VC font resources, enumerate cmap coverage, and render exact-font sheets.

Requires fontTools and Pillow. Firmware inputs are read-only. No OS font fallback
is used for specimen glyphs; labels deliberately use Pillow's built-in font.
"""
import argparse
import gzip
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import unicodedata as ud

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont


CANDIDATES = '[]{}()<>«»‹›⌈⌉⌊⌋⟦⟧⟨⟩「」『』【】〔〕〖〗〘〙⌚⌛⏱⏲⏳◷◴◵◶◉◌◍◐◑◒◓⊙◎○●□■◇◆◊►◀▯▪→↑↗↘↙↖↔⋮⋯…•·★☆⚑⚐≈≃≅∼∆Δτ'


def codepoint(cp, glyph, font):
    return dict(codepoint=cp, hex=f'U+{cp:04X}', char=chr(cp),
                java=f'\\u{cp:04X}' if cp <= 0xffff else None,
                name=ud.name(chr(cp), '<private-use>' if ud.category(chr(cp)) == 'Co' else '<unnamed>'),
                category=ud.category(chr(cp)), glyph_name=glyph,
                glyph_id=font.getGlyphID(glyph) if font is not None else None)


def sheet(font_path, entries, target, title, page=1):
    cols, rows, cell_w, cell_h = 12, max(1, math.ceil(len(entries) / 12)), 100, 112
    img = Image.new('RGB', (cols * cell_w + 40, rows * cell_h + 110), '#15191f')
    draw = ImageDraw.Draw(img)
    label = ImageFont.load_default(size=14)
    draw.text((20, 16), title.replace('—', '-'), font=ImageFont.load_default(size=20), fill='#ffffff')
    draw.text((20, 47), f'Page {page} | exact firmware font; no fallback | codepoint / glyph ID', font=label, fill='#a6b2c4')
    face = ImageFont.truetype(str(font_path), 46)
    for i, e in enumerate(entries):
        x, y = 20 + (i % cols) * cell_w, 83 + (i // cols) * cell_h
        draw.rounded_rectangle((x + 2, y + 2, x + cell_w - 3, y + cell_h - 3), radius=6, fill='#242b35')
        cp = e['codepoint']
        if e['category'] not in ('Cc', 'Cf', 'Zl', 'Zp'):
            glyph = chr(cp)
            glyph_face = face
            box = draw.textbbox((0, 0), glyph, font=glyph_face)
            if box[2] - box[0] > cell_w - 14:
                glyph_face = ImageFont.truetype(str(font_path), max(10, int(46 * (cell_w - 14) / (box[2] - box[0]))))
                box = draw.textbbox((0, 0), glyph, font=glyph_face)
            draw.text((x + (cell_w - box[2] + box[0]) / 2 - box[0], y + 9 - box[1]), glyph, font=glyph_face, fill='#f1f5fa')
        draw.text((x + 8, y + 76), f'{cp:04X}', font=label, fill='#78c8ff')
        draw.text((x + 8, y + 93), f"gid {e['glyph_id']}", font=ImageFont.load_default(size=11), fill='#a6b2c4')
    img.save(target)


def examples(font_path, cmap, target):
    rows = [('Current brackets', '[Exit 12]'), ('Double angle quotes', '«Exit 12»'),
            ('Single angle quotes', '‹Exit 12›'), ('Braces', '{Exit 12}'),
            ('Diamond separators', '◆ Exit 12 ◆'), ('Current ETA', 'ETA 22:42 | 37 min'),
            ('Approximate duration', '≈ 37 min'), ('Arrow + arrival clock', '→ 22:42'),
            ('Approximate arrival + duration', '≈ 22:42 | 37 min')]
    img = Image.new('RGB', (1240, 90 + 95 * len(rows)), '#15191f')
    draw = ImageDraw.Draw(img)
    draw.text((26, 18), 'AudiTypeDisplayHigh-Normal - actual firmware glyphs', font=ImageFont.load_default(size=23), fill='white')
    draw.text((26, 53), 'Specimens only; not a screenshot from the VC / no OS font fallback', font=ImageFont.load_default(size=15), fill='#a6b2c4')
    face = ImageFont.truetype(str(font_path), 39)
    for i, (name, value) in enumerate(rows):
        y = 95 + i * 95
        draw.text((26, y + 20), name, font=ImageFont.load_default(size=18), fill='#78c8ff')
        missing = [c for c in value if ord(c) not in cmap]
        if missing:
            draw.text((390, y + 20), 'UNSUPPORTED: ' + ' '.join(f'U+{ord(c):04X}' for c in missing), font=ImageFont.load_default(size=18), fill='#ff928b')
        else:
            draw.text((390, y + 4), value, font=face, fill='white')
    img.save(target)


def audit_lur(source, output):
    """Validated structural interpretation; numeric font slots are not UI bindings."""
    resources = []
    target = output / 'lookup-resources'
    target.mkdir(exist_ok=True)
    for p in sorted(source.rglob('*.lur')):
        if not p.name.startswith('AudiTypeDisplay'):
            continue
        data = p.read_bytes()
        if len(data) < 14 or data[:4] != b'lur\0':
            raise ValueError(f'Unexpected lookup header: {p}')
        version, slots, count = struct.unpack_from('<HII', data, 4)
        if len(data) != 14 + count * 6:
            raise ValueError(f'Unexpected lookup size: {p}')
        ranges = [struct.unpack_from('<HHH', data, 14 + 6*i) for i in range(count)]
        if not all(a <= z and slot < slots and (i == 0 or ranges[i-1][1] < a)
                   for i, (a,z,slot) in enumerate(ranges)):
            raise ValueError(f'Invalid/overlapping lookup ranges: {p}')
        shutil.copy2(p, target / p.name)
        resources.append(dict(source=str(p), file=p.name, sha256=hashlib.sha256(data).hexdigest(),
            version=version, font_slot_count=slots, range_count=count,
            range_offset=14, range_record_bytes=6,
            ranges=[dict(first=a,last=z,font_slot=slot) for a,z,slot in ranges],
            candidates={f'U+{ord(c):04X}': next((slot for a,z,slot in ranges if a<=ord(c)<=z), None) for c in CANDIDATES}))
    (output / 'lookup-ranges.json').write_text(json.dumps(resources, indent=2), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('firmware', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--refresh', action='store_true', help='Regenerate an existing audit output directory')
    args = parser.parse_args()
    source = args.firmware.resolve() / 'extracted'
    if not source.is_dir():
        parser.error('Expected an extracted/ directory in the firmware root')
    args.output.mkdir(parents=True, exist_ok=args.refresh)
    font_dir, catalog_dir, preview_dir = [args.output / name for name in ('fonts', 'catalogs', 'previews')]
    for d in (font_dir, catalog_dir, preview_dir):
        d.mkdir(exist_ok=args.refresh)
    manifest, audi_cmaps, seen = [], [], {}
    for p in sorted(source.rglob('*')):
        if p.suffix.lower() not in ('.ttf', '.otf', '.ftgz'):
            continue
        dest = font_dir / p.name
        data = p.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if p.name in seen:
            if seen[p.name] == digest:
                continue
            dest = font_dir / (p.parent.parent.name + '_' + p.name)
        seen[p.name] = digest
        shutil.copy2(p, dest)
        info = dict(file=str(dest.relative_to(args.output)), source=str(p), bytes=len(data), sha256=digest)
        if p.suffix == '.ftgz':
            raw = gzip.decompress(data)
            expanded = dest.with_suffix('.font')
            expanded.write_bytes(raw)
            info.update(kind='gzip-wrapped raster font, not TrueType cmap', expanded_bytes=len(raw), expanded_sha256=hashlib.sha256(raw).hexdigest())
            manifest.append(info)
            continue
        f = TTFont(dest)
        cmap = f.getBestCmap() or {}
        entries = [codepoint(cp, name, f) for cp, name in sorted(cmap.items()) if name != '.notdef']
        tables = [dict(platform=t.platformID, encoding=t.platEncID, format=t.format, unicode=t.isUnicode(), count=len(t.cmap),
                       mappings={f'U+{cp:04X}': g for cp, g in sorted(t.cmap.items())})
                  for t in f['cmap'].tables if hasattr(t, 'cmap')]
        mapped_names = {g for t in f['cmap'].tables if hasattr(t, 'cmap') for g in t.cmap.values()}
        unencoded = [dict(glyph_id=i, glyph_name=g) for i, g in enumerate(f.getGlyphOrder()) if g not in mapped_names]
        info.update(family=f['name'].getDebugName(1), style=f['name'].getDebugName(2), version=f['name'].getDebugName(5),
                    sfnt_tables={str(tag):dict(offset=table.offset, length=table.length) for tag,table in f.reader.tables.items()},
                    unicode_count=len(entries), glyph_count=f['maxp'].numGlyphs,
                    private_use_count=sum(e['category']=='Co' for e in entries),
                    candidates={f'U+{ord(c):04X}': ord(c) in cmap for c in CANDIDATES},
                    cmap_tables=[{k:v for k,v in t.items() if k != 'mappings'} for t in tables])
        catalog = dict(font=info, unicode=entries, all_cmap_tables=tables, glyphs_without_cmap=unencoded)
        (catalog_dir / (p.stem + '.json')).write_text(json.dumps(catalog, indent=2, ensure_ascii=False), encoding='utf-8')
        (catalog_dir / (p.stem + '.txt')).write_text('\n'.join(
            f"{e['hex']}\t{e['char']!r}\t{e['category']}\t{e['name']}\t{e['glyph_name']}\tgid={e['glyph_id']}" for e in entries) + '\n', encoding='utf-8')
        if p.name.startswith('Audi'):
            audi_cmaps.append({e['codepoint'] for e in entries})
            for i in range(0, len(entries), 96):
                sheet(dest, entries[i:i+96], preview_dir / f'{p.stem}-all-{i//96+1:02d}.png', p.stem + ' — all encoded characters', i//96+1)
            symbols = [e for e in entries if e['category'][0] in 'PS' or e['category'] == 'Co']
            for i in range(0, len(symbols), 96):
                sheet(dest, symbols[i:i+96], preview_dir / f'{p.stem}-symbols-{i//96+1:02d}.png', p.stem + ' — punctuation, symbols, private use', i//96+1)
            if p.name == 'AudiTypeDisplayHigh-Normal.ttf':
                examples(dest, cmap, preview_dir / 'signpost-eta-options.png')
        manifest.append(info)
        print(f"{p.name}: {len(entries)} codepoints, {info['private_use_count']} private-use, {len(unencoded)} unmapped glyphs")
        f.close()
    common = set.intersection(*audi_cmaps)
    (args.output / 'manifest.json').write_text(json.dumps(dict(firmware=str(args.firmware.resolve()), fonts=manifest,
        common_audi_codepoint_count=len(common), common_audi_candidates=[codepoint(cp, None, None)
        for cp in sorted({ord(c) for c in CANDIDATES} & common)]), indent=2, ensure_ascii=False), encoding='utf-8')
    print('Common Audi candidates:', ''.join(c for c in CANDIDATES if ord(c) in common))
    audit_lur(source, args.output)
    print('Output:', args.output.resolve())


if __name__ == '__main__':
    main()
