#!/usr/bin/env python3
"""Extract small, font-free CurrentPosition data. Not part of the HU build.

Requires fontTools and the five extracted VC fonts. Unicode inputs are pinned
to 17.0.0; --download populates the build cache from unicode.org. Generated
data is checked in, so ordinary Java builds need neither fonts nor Python.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import urllib.request

from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]
UCD_VERSION = '17.0.0'
INPUTS = ['UnicodeData.txt', 'DerivedNormalizationProps.txt',
          'DerivedCoreProperties.txt', 'auxiliary/GraphemeBreakProperty.txt',
          'emoji/emoji-data.txt', 'auxiliary/GraphemeBreakTest.txt',
          'NormalizationTest.txt']
FONTS = ['AudiTypeDisplay-Normal.ttf', 'LT_Univers440_88perc_Arabic.ttf',
         'ZYHei_GB18030_c.ttf', 'SourceHanSansKR-Regular.otf',
         'SourceHanSansJP-Regular.otf']
LOOKUPS = ['AudiTypeDisplay_Normal_' + region + '.lur'
           for region in ('China', 'EU_Arabic', 'Japanese', 'Korea')]
GCB = dict(zip(('Other', 'CR', 'LF', 'Control', 'Extend', 'ZWJ',
                'Regional_Indicator', 'Prepend', 'SpacingMark', 'L', 'V',
                'T', 'LV', 'LVT'), range(14)))


def varuint(value):
    """Unsigned base-128 integer. All encoded fields fit in 22 bits."""
    assert 0 <= value <= 0x3fffff, value
    out = bytearray()
    while value >= 128:
        out.append((value & 127) | 128)
        value >>= 7
    out.append(value)
    return out


def zigzag(value):
    return value * 2 if value >= 0 else -value * 2 - 1


def pack_tables(unknown, tables):
    """VCT2: column-wise delta/varint streams, then JAR's normal deflate.

    Ranges: gap after previous end, end-start, value.
    Decompositions: delta codepoint, zigzag(first-codepoint), second.
    Compositions: delta first, delta second (reset when first changes),
    zigzag(result-first). The decoder writes directly into 63-bit records.
    """
    data = bytearray(b'VCT2') + varuint(unknown)
    for kind, table in enumerate(tables):
        previous = -1 if kind < 2 else 0
        previous_second = 0
        rows = []
        for a, b, c in table:
            assert all(0 <= n <= 0x1fffff for n in (a, b, c))
            if kind < 2:
                row = (a - previous - 1, b - a, c)
                previous = b
            elif kind == 2:
                row = (a - previous, zigzag(b - a), c)
                previous = a
            else:
                row = (a - previous, b - previous_second if a == previous else b,
                       zigzag(c - a))
                previous, previous_second = a, b
            rows.append(row)
        data += varuint(len(rows))
        for col in range(3):
            for row in rows:
                data += varuint(row[col])
    return bytes(data)


def canonical_digest(unknown, tables):
    """Hash the unpacked v1 records to prove a format change lost no data."""
    digest = hashlib.sha256(struct.pack('>II', 0x56435431, unknown))
    for table in tables:
        digest.update(struct.pack('>I', len(table)))
        for row in table:
            digest.update(struct.pack('>III', *row))
    return digest.hexdigest()


def records(path):
    for line in path.read_text().splitlines():
        line = line.split('#')[0].strip()
        if line:
            fields = [s.strip() for s in line.split(';')]
            span = fields[0].split('..')
            yield range(int(span[0], 16), int(span[-1], 16) + 1), fields[1:]


def ranges(values):
    out = []
    for cp, value in sorted(values.items()):
        if out and out[-1][1] + 1 == cp and out[-1][2] == value:
            out[-1][1] = cp
        else:
            out.append([cp, cp, value])
    return out


def subtables(font, tag, wanted=None):
    if tag not in font:
        return
    table = font[tag].table
    indexes = set(range(len(table.LookupList.Lookup))) if wanted is None else {
        i for r in table.FeatureList.FeatureRecord if r.FeatureTag in wanted
        for i in r.Feature.LookupListIndex}
    for i in sorted(indexes):
        lookup = table.LookupList.Lookup[i]
        for sub in lookup.SubTable:
            yield lookup.LookupType, sub


def font_widths(path, primary):
    font = TTFont(path)
    base = {g: m[0] for g, m in font['hmtx'].metrics.items()}
    widths = dict(base)
    # Maximum positive pair expansion per leading glyph. Negative kerning is
    # intentionally ignored: an upper estimate should not underflow the row.
    extra = {}
    for kind, sub in subtables(font, 'GPOS', {'kern'}):
        if kind != 2:
            raise ValueError('unhandled kern lookup')
        def positive(pair):
            return max(0, sum(getattr(getattr(pair, v, None), 'XAdvance', 0) or 0
                              for v in ('Value1', 'Value2')))
        if sub.Format == 1:
            for glyph, pairs in zip(sub.Coverage.glyphs, sub.PairSet):
                extra[glyph] = max(extra.get(glyph, 0),
                                   max([positive(p) for p in pairs.PairValueRecord] or [0]))
        elif sub.Format == 2:
            for glyph in sub.Coverage.glyphs:
                row = sub.Class1Record[sub.ClassDef1.classDefs.get(glyph, 0)]
                extra[glyph] = max(extra.get(glyph, 0), max(positive(p) for p in row.Class2Record))
        else:
            raise ValueError('unhandled pair format')
    if 'kern' in font:
        for table in font['kern'].kernTables:
            for (first, _), value in table.kernTable.items():
                extra[first] = max(extra.get(first, 0), value)
    widths = {g: w + extra.get(g, 0) for g, w in widths.items()}
    # Account for contextual single substitutions in supplementary fonts,
    # including all Arabic initial/medial/final forms. Primary face uses liga;
    # discretionary stylistic/small-cap/denominator features are not enabled.
    substitutions = list(subtables(font, 'GSUB', {'liga', 'clig', 'rlig', 'ccmp', 'locl'} if primary else None))
    for _ in range(32):
        old = dict(widths)
        for kind, sub in substitutions:
            if kind == 1:
                for first, second in sub.mapping.items():
                    widths[first] = max(widths[first], widths[second])
            elif kind == 4:
                for first, ligs in sub.ligatures.items():
                    for lig in ligs:
                        # Charge any expansion to the first component only.
                        bound = widths[lig.LigGlyph] - sum(base[g] for g in lig.Component)
                        widths[first] = max(widths[first], bound)
            elif kind not in (3, 6):
                raise ValueError('unhandled GSUB lookup %d' % kind)
        if widths == old:
            break
    else:
        raise ValueError('GSUB bounds did not converge')
    scale = 27 * 64 / font['head'].unitsPerEm
    return ({cp: math.ceil(widths[g] * scale) for cp, g in font.getBestCmap().items()},
            math.ceil(widths['.notdef'] * scale))


def generate(font_dir, lookup_dir, ucd):
    props = {}
    def bits(points, value):
        for cp in points:
            props[cp] = props.get(cp, 0) | value
    for points, fields in records(ucd / 'auxiliary/GraphemeBreakProperty.txt'):
        bits(points, GCB[fields[0]])
    for points, fields in records(ucd / 'emoji/emoji-data.txt'):
        if fields[0] == 'Extended_Pictographic':
            bits(points, 1 << 15)
    for points, fields in records(ucd / 'DerivedCoreProperties.txt'):
        if fields[0] == 'InCB':
            bits(points, {'Consonant': 1, 'Extend': 2, 'Linker': 3}[fields[1]] << 13)
        elif fields[0] == 'Default_Ignorable_Code_Point':
            bits(points, 1 << 16)
    decomp = []
    range_start = None
    for line in (ucd / 'UnicodeData.txt').read_text().splitlines():
        f = line.split(';')
        cp = int(f[0], 16)
        if int(f[3]):
            bits([cp], int(f[3]) << 5)
        if f[1].endswith(', First>'):
            range_start = cp
        if f[4] in ('R', 'AL'):
            bits([cp], 1 << 17)
        elif f[4] == 'L':
            bits(range(range_start, cp + 1) if f[1].endswith(', Last>') else [cp], 1 << 18)
        d = f[5]
        if d and not d.startswith('<'):
            parts = [int(x, 16) for x in d.split()]
            assert len(parts) <= 2
            decomp.append([cp, parts[0], parts[1] if len(parts) == 2 else 0])
    exclusions = set()
    for points, f in records(ucd / 'DerivedNormalizationProps.txt'):
        if f[0] == 'Full_Composition_Exclusion':
            exclusions.update(points)
    compositions = sorted([a, b, cp] for cp, a, b in decomp if b and cp not in exclusions)
    primary_points = None
    for name in LOOKUPS:
        lur = (lookup_dir / name).read_bytes()
        assert lur[:4] == b'lur\0'
        version, slots, count = struct.unpack_from('<HII', lur, 4)
        assert version == 1 and slots == 5 and len(lur) == 14 + 6 * count
        points = set()
        for i in range(count):
            first, last, slot = struct.unpack_from('<HHH', lur, 14 + 6 * i)
            if slot == 0:
                points.update(range(first, last + 1))
        primary_points = points if primary_points is None else primary_points & points
    combined = {}
    unknown = 0
    for i, filename in reversed(list(enumerate(FONTS))):
        widths, notdef = font_widths(font_dir / filename, i == 0)
        unknown = max(unknown, notdef)
        for cp, width in widths.items():
            # Audi's advance is exact only where ALL regional lookup profiles
            # select slot 0. Otherwise retain the largest possible font bound;
            # no locale detection or runtime profile switching is necessary.
            combined[cp] = width if i == 0 and cp in primary_points else max(combined.get(cp, 0), width)
    tables = [ranges(combined), ranges(props), decomp, compositions]
    return pack_tables(unknown, tables), [len(t) for t in tables], canonical_digest(unknown, tables)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--fonts', type=Path, default=ROOT / 'output/r2/vc-fonts-0379-20260910/fonts')
    p.add_argument('--ucd', type=Path, default=ROOT / ('build/vc-ucd-' + UCD_VERSION))
    p.add_argument('--lookups', type=Path, default=ROOT / 'output/r2/vc-fonts-0379-20260910/lookup-resources')
    p.add_argument('--download', action='store_true')
    p.add_argument('--check', action='store_true')
    args = p.parse_args()
    for name in INPUTS:
        path = args.ucd / name
        if not path.exists() and args.download:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(urllib.request.urlopen('https://www.unicode.org/Public/' + UCD_VERSION + '/ucd/' + name).read())
    data, counts, canonical_sha256 = generate(args.fonts, args.lookups, args.ucd)
    target = ROOT / 'java_resources/com/luka/carplay/rgd/vc-text.bin'
    manifest = dict(format=2, unicode=UCD_VERSION, font_size_px=27,
                    table_records=dict(zip(('advance_ranges', 'unicode_ranges', 'decompositions', 'compositions'), counts)),
                    data_bytes=len(data), data_sha256=hashlib.sha256(data).hexdigest(),
                    canonical_sha256=canonical_sha256, runtime_array_bytes=sum(counts) * 8,
                    inputs={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in [*(args.fonts / n for n in FONTS), *(args.lookups / n for n in LOOKUPS),
                                      *(args.ucd / n for n in INPUTS)]})
    manifest_path = ROOT / 'tools/vc-text-data/manifest.json'
    encoded = json.dumps(manifest, indent=2, sort_keys=True) + '\n'
    if args.check:
        assert target.read_bytes() == data, 'regenerate vc-text.bin'
        assert manifest_path.read_text() == encoded, 'regenerate manifest'
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        manifest_path.write_text(encoded)
    print(json.dumps({k: v for k, v in manifest.items() if k != 'inputs'}, indent=2))


if __name__ == '__main__':
    main()
