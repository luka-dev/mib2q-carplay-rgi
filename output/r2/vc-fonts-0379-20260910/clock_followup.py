"""Read-only firmware diagnostics; bitmap and PUA specimen generation."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import unicodedata

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('font_audit', ROOT.parents[2] / 'tools/vc_font_audit.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def clock_atlases():
    fw = Path(json.loads((ROOT / 'manifest.json').read_text())['firmware'])
    stage = fw / 'extracted/stage2_fs'
    found = []
    for stem in ('atlas_early_LA', 'atlas_full_LA'):
        source = stage / 'STAGE2.2/data/gtf' / (stem + '.atlas')
        index = stage / 'STAGE2.1/data/gtf' / (stem + '.json')
        b = source.read_bytes()
        assert b[:3] == b'TC\0'
        count = struct.unpack_from('<I', b, 43)[0]
        bin_offset = 47 + count * 8
        assert b[bin_offset:bin_offset+4] == b'BIN\0'
        v, fmt, w, h, channels, _, _, n = struct.unpack_from('<8I', b, bin_offset + 4)
        assert v == 4 and fmt == 4 and channels == 2 and n == count
        entries = {}
        for i in range(n):
            uid, uw, vh, u, v, iw, ih = struct.unpack_from('<I4f2H', b, bin_offset+36+i*24)
            assert round(uw*w)==iw and round(vh*h)==ih
            entries[uid] = (round(u*w), round(v*h), iw, ih)
        pixel_offset = bin_offset+36+n*24
        atlas = Image.frombytes('LA', (w,h), b[pixel_offset:pixel_offset+w*h*2]).convert('RGBA')
        for entry in json.loads(index.read_text())['images']:
            if not any(word in entry['filename'].lower() for word in ('clock', 'watch')):
                continue
            uid = entry['UID']
            x,y,iw,ih = entries[uid]
            assert 0<=x<x+iw<=w and 0<=y<y+ih<=h
            target = ROOT / 'previews' / (stem+'-'+Path(entry['filename']).stem+'.png')
            atlas.crop((x,y,x+iw,y+ih)).save(target)
            found.append(dict(source=str(source), source_sha256=hashlib.sha256(b).hexdigest(),
                index=str(index), filename=entry['filename'], uid=uid,
                bin_offset=bin_offset, entry_offset=bin_offset+36+(uid-1)*24,
                pixel_offset=pixel_offset, atlas_size=[w,h], crop=[x,y,iw,ih],
                output=str(target.relative_to(ROOT))))
    (ROOT / 'clock-images.json').write_text(json.dumps(found,indent=2),encoding='utf-8')
    canvas = Image.new('RGB', (1050, 115+len(found)*145), '#15191f')
    draw = ImageDraw.Draw(canvas)
    draw.text((24,20),'Actual clock graphics from VC firmware atlases',font=ImageFont.load_default(size=23),fill='white')
    draw.text((24,55),'Images, NOT Unicode glyphs. No mapping to a BAP text character has been found.',font=ImageFont.load_default(size=16),fill='#a6b2c4')
    for i,e in enumerate(found):
        y=115+i*145
        draw.text((24,y),e['filename'],font=ImageFont.load_default(size=21),fill='#78c8ff')
        draw.text((24,y+32),f"Atlas UID {e['uid']} | {e['crop'][2]} x {e['crop'][3]} pixels | shown 4x",font=ImageFont.load_default(size=15),fill='#a6b2c4')
        im=Image.open(ROOT/e['output']).convert('RGBA')
        im=im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST)
        canvas.paste(im,(710,y),im)
    canvas.save(ROOT/'previews/clock-images.png')
    print(f'Extracted {len(found)} named clock/watch atlas sprites.')


def main():
    b = (ROOT / 'fonts/arial.font').read_bytes()
    assert b[:4] == b'FONT' and struct.unpack_from('<I', b, 4)[0] == len(b) - 8
    chunks = {}
    off = 12
    while off < len(b):
        name, size = struct.unpack_from('<4sI', b, off)
        assert off + 8 + size <= len(b)
        chunks[name.decode()] = dict(offset=off, length=size)
        off += 8 + size
    assert off == len(b)
    g = chunks['GLYP']
    count = struct.unpack_from('<I', b, g['offset'] + 8)[0]
    assert g['length'] == 4 + count * 2
    cps = struct.unpack_from(f'<{count}H', b, g['offset'] + 12)
    assert list(cps) == sorted(set(cps))
    w, h = struct.unpack_from('<II', b, chunks['RWFM']['offset'] + 8)
    d = chunks['RWD4']
    raw = b[d['offset'] + 8:d['offset'] + 8 + d['length']]
    assert len(raw) * 2 == w * h
    pixels = bytes(n * 17 for v in raw for n in (v >> 4, v & 15))
    strip = Image.frombytes('L', (w, h), pixels)
    strip.save(ROOT / 'previews/arial-bitmap-strip.png')
    # Rows of the source strip, no glyph resizing or claimed per-glyph boundaries.
    canvas = Image.new('RGB', (1200, 80 + ((w+599)//600) * 94), '#15191f')
    draw = ImageDraw.Draw(canvas)
    draw.text((12, 14), 'Arial .font - full decoded 4-bit bitmap strip', font=ImageFont.load_default(size=22), fill='white')
    draw.text((12, 44), 'Source x offsets below; first rows include metadata marks. Not a VC UI font-binding claim.', font=ImageFont.load_default(size=14), fill='#a6b2c4')
    for i,x in enumerate(range(0,w,600)):
        draw.text((6, 80+i*94), f'x={x}', font=ImageFont.load_default(size=13), fill='#78c8ff')
        crop = strip.crop((x, 0, min(x+600,w), h))
        canvas.paste(crop.resize((crop.width*2,h*2), Image.Resampling.NEAREST), (0,100+i*94))
    canvas.save(ROOT / 'previews/arial-bitmap-all.png')
    info = dict(sha256=hashlib.sha256(b).hexdigest(), chunks=chunks,
                codepoint_count=count, bitmap_width=w, bitmap_height=h,
                unicode=[dict(codepoint=cp,hex=f'U+{cp:04X}',char=chr(cp),name=unicodedata.name(chr(cp),'<private-use>')) for cp in cps])
    (ROOT / 'catalogs/arial-bitmap.json').write_text(json.dumps(info,ensure_ascii=False,indent=2),encoding='utf-8')
    for name in ('LT_Univers440_88perc_Arabic','ZYHei_GB18030_c'):
        cat = json.loads((ROOT / 'catalogs' / f'{name}.json').read_text())
        entries = [e for e in cat['unicode'] if e['category']=='Co']
        audit.sheet(ROOT / cat['font']['file'], entries, ROOT / 'previews' / f'{name}-private-use.png', name + ' - all private-use glyphs')
    print(f'Arial validated: {count} codepoints, {w}x{h} 4-bit bitmap; {len(chunks)} chunks cover entire file.')
    clock_atlases()


if __name__ == '__main__':
    main()
