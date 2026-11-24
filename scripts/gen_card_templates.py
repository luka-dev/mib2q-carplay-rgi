#!/usr/bin/env python3
"""gen_card_templates - build the template root that altscreen_render/card_rec.c loads.

The CarPlay instruction-card recogniser matches the maneuver icon and the distance
glyphs against templates.  The templates are derived from material that must never be
committed here (the navigation app's own maneuver art, system-font renders, car
captures), so this script only turns reference files that live OUTSIDE the repository
into a template directory that also lives outside it.  Point CARD_REC_TEMPLATES at the
result; the C module and tests/card_rec_test.c load it at run time.

  python3 scripts/gen_card_templates.py --art DIR --out DIR \
      [--car-frame PNG [--car-zone fullscreen] [--car-labels 1,0,m]] [--font TTF]

  --art        directory with the 18 big_direction*.png maneuver images (210 x 210 RGBA)
  --out        template root to write (must be outside this repository)
  --car-frame  a decoded cluster frame with a settled card; its distance glyphs are
               harvested into glyphs_car/ (labels in reading order, --car-labels)
  --font       variable SF Pro font for the bootstrap glyph sets (default: macOS system)

Output layout (P5 PGM + text manifests, format documented in card_rec.h):
  icons/        one NORM x NORM template (+ optional care mask) per maneuver image
  glyphs_sf/    distance glyphs rendered from the system font (bootstrap)
  glyphs_car/   distance glyphs harvested from --car-frame (override glyphs_sf per label)
  ring_digits/  roundabout exit-number digits + confusable letters as reject labels (bootstrap)

Every resampling step uses the same exact integer area average as the runtime, so a
template and a live frame of the same art go through identical arithmetic.
Requires numpy and Pillow (with FreeType variable-font support).
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

REPO = os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
NORM = 32
T_INK = 128
CARD_DEN = 3                     # 210 px art -> 70 px at card scale (fits the car right turn)
DIST_FONT = (36.0, 700)          # SF Pro size / weight fitted to the car '1', '0', 'm'
RING_DIGIT_CAP = 12              # cap height of the ring exit number at card scale (px)
RING_HOLE_PM = 620               # hole radius / outer ring radius for the digit search
RING_CONFUSABLES = 'gqbSBZ'      # letters that read like 9 9 6 5 8 2: reject labels in ring_digits/
DIST_LOOKALIKES = 'BlIS'         # read as 8 i i 8: reject labels '~B' '~l' '~I' '~S' in glyphs_sf/
SYSTEM_FONT = '/System/Library/Fonts/SFNS.ttf'
ZONES = {'fullscreen': (330, 88, 780, 294), 'sportscreen': (108, 88, 348, 294),
         'classicscreen': (540, 88, 360, 294)}
# card gate / layout thresholds needed to cut the car glyphs (same values as card_rec.c)
P = dict(BAND=6, BAND_TOL=8, BLACK_MAX=40, WHITE_RANK=16, LINE_GAP=8, ICON_GAP=16)

# image file -> (template name, output group, CPManeuverType, traffic side)
ICONS = [
    ('big_direction_left', 'left', 'LEFT', 1, '-'),
    ('big_direction_right', 'right', 'RIGHT', 2, '-'),
    ('big_direction_forward', 'forward', 'STRAIGHT', 3, '-'),
    ('big_direction_exit_left', 'exit_left', 'KEEP_LEFT', 13, '-'),
    ('big_direction_exit_right', 'exit_right', 'KEEP_RIGHT', 14, '-'),
    ('big_direction_end', 'end', 'ARRIVE', 12, '-'),
    ('big_directions_uturn', 'uturn', 'UTURN_L', 4, 'R'),
    ('big_directions_uturn_lhs', 'uturn_lhs', 'UTURN_R', 4, 'L'),
    ('big_directions_roundabout', 'roundabout', 'RB_RING', 0, 'R'),
    ('big_directions_roundabout_lhs', 'roundabout_lhs', 'RB_RING', 0, 'L'),
    ('big_directions_roundabout_l', 'roundabout_l', 'RB_LEFT_R', 6, 'R'),
    ('big_directions_roundabout_l_lhs', 'roundabout_l_lhs', 'RB_LEFT_L', 6, 'L'),
    ('big_directions_roundabout_s', 'roundabout_s', 'RB_STRAIGHT_R', 6, 'R'),
    ('big_directions_roundabout_s_lhs', 'roundabout_s_lhs', 'RB_STRAIGHT_L', 6, 'L'),
    ('big_directions_roundabout_r', 'roundabout_r', 'RB_RIGHT_R', 6, 'R'),
    ('big_directions_roundabout_r_lhs', 'roundabout_r_lhs', 'RB_RIGHT_L', 6, 'L'),
    ('big_directions_roundabout_u', 'roundabout_u', 'RB_UTURN_R', 19, 'R'),
    ('big_directions_roundabout_u_lhs', 'roundabout_u_lhs', 'RB_UTURN_L', 19, 'L'),
]
RING_FILES = ('big_directions_roundabout', 'big_directions_roundabout_lhs')


# ---------------------------------------------------------------- integer primitives
def nz(mask1d):
    return np.nonzero(mask1d)[0]


def runs(idx, maxgap):
    out = []
    for v in idx:
        v = int(v)
        if out and v - out[-1][1] - 1 <= maxgap:
            out[-1][1] = v
        else:
            out.append([v, v])
    return out


def _area_weights(n_in, n_out):
    w = np.zeros((n_out, n_in), np.int32)
    for i in range(n_out):
        lo, hi = i * n_in, (i + 1) * n_in
        for k in range(lo // n_out, (hi - 1) // n_out + 1):
            a, b = max(lo, k * n_out), min(hi, (k + 1) * n_out)
            if b > a:
                w[i, k] = b - a
    return w


def area_resample(src, out_w, out_h):
    """Exact integer area average (card_rec_match.c area_resample is the same arithmetic)."""
    h_in, w_in = src.shape
    if w_in % out_w == 0 and h_in % out_h == 0:
        kx, ky = w_in // out_w, h_in // out_h
        blk = src.astype(np.int32).reshape(out_h, ky, out_w, kx).sum(axis=(1, 3))
        k = kx * ky
        return ((blk + k // 2) // k).astype(np.uint8)
    vs = _area_weights(h_in, out_h) @ (src.astype(np.int64) @ _area_weights(w_in, out_w).T)
    den = w_in * h_in
    return ((vs + den // 2) // den).astype(np.uint8)


def pad_square(img):
    h, w = img.shape
    s = max(h, w)
    out = np.zeros((s, s), np.uint8)
    out[(s - h) // 2:(s - h) // 2 + h, (s - w) // 2:(s - w) // 2 + w] = img
    return out


def level_stretch(img, black, white):
    d = max(1, int(white) - int(black))
    v = ((img.astype(np.int32) - int(black)) * 255 + d // 2) // d
    return np.clip(v, 0, 255).astype(np.uint8)


def white_level(img, rank):
    top = np.cumsum(np.bincount(img.ravel(), minlength=256)[::-1])
    return 255 - int(np.searchsorted(top, rank))


def write_pgm(path, img):
    img = np.ascontiguousarray(img.astype(np.uint8))
    with open(path, 'wb') as f:
        f.write(b'P5\n%d %d\n255\n' % (img.shape[1], img.shape[0]))
        f.write(img.tobytes())


def segment_glyphs(block):
    ink = block >= T_INK
    out = []
    for a, b in runs(nz(ink.any(0)), 0):
        r = nz(ink[:, a:b + 1].any(1))
        out.append(dict(x0=a, x1=b, y0=int(r[0]), y1=int(r[-1]),
                        img=block[int(r[0]):int(r[-1]) + 1, a:b + 1]))
    return out


def baseline_of(glyphs):
    return sorted(g['y1'] for g in glyphs)[len(glyphs) // 2]


# ---------------------------------------------------------------- icons
def art_maps(path):
    """Premultiplied luma on black and the 'optional' map: grey art (16 < pm < 224) more
    than 2 px from solid white.  Anti-aliased stroke edges stay scored."""
    a = np.asarray(Image.open(path).convert('RGBA')).astype(np.int32)
    lum = (a[..., 0] * 77 + a[..., 1] * 150 + a[..., 2] * 29) >> 8
    pm = ((lum * a[..., 3] + 127) // 255).astype(np.uint8)
    near = pm >= 224
    for _ in range(2):
        n = near.copy()
        n[1:] |= near[:-1]
        n[:-1] |= near[1:]
        n[:, 1:] |= near[:, :-1]
        n[:, :-1] |= near[:, 1:]
        near = n
    opt = (pm > 16) & (pm < 224) & ~near
    return pm, (opt * 255).astype(np.uint8)


def to_card(img, px=0, py=0, pad=8):
    d = CARD_DEN
    h, w = img.shape
    H, W = (h + 2 * pad * d) // d * d, (w + 2 * pad * d) // d * d
    big = np.zeros((H, W), np.uint8)
    big[pad * d + py:pad * d + py + h, pad * d + px:pad * d + px + w] = img
    return area_resample(big, W // d, H // d)


def icon_norm(card):
    s = level_stretch(card, 0, white_level(card, P['WHITE_RANK']))
    m = s >= T_INK
    r, c = nz(m.any(1)), nz(m.any(0))
    bb = (int(c[0]), int(r[0]), int(c[-1]), int(r[-1]))
    crop = s[bb[1]:bb[3] + 1, bb[0]:bb[2] + 1]
    return area_resample(pad_square(crop), NORM, NORM), bb


def build_icons(art, out):
    os.makedirs(out, exist_ok=True)
    lines = ['# file care name group cp_type side exp_w exp_h hole(cx_pm,cy_pm,r_pm)',
             '# generated by gen_card_templates.py - LOCAL ONLY, never commit']
    for fn, name, group, cp, side in ICONS:
        pm, opt = art_maps(os.path.join(art, fn + '.png'))
        acc = np.zeros((NORM, NORM), np.int32)
        accopt = np.zeros((NORM, NORM), np.int32)
        sizes = []
        for py in range(CARD_DEN):          # every sub-pixel phase of the 1/3 grid
            for px in range(CARD_DEN):
                n, bb = icon_norm(to_card(pm, px, py))
                acc += n
                sizes.append((bb[2] - bb[0] + 1, bb[3] - bb[1] + 1))
                oc = to_card(opt, px, py)
                accopt += area_resample(pad_square(oc[bb[1]:bb[3] + 1, bb[0]:bb[2] + 1]), NORM, NORM)
        k = CARD_DEN * CARD_DEN
        tmpl = ((acc + k // 2) // k).astype(np.uint8)
        care = np.where((accopt + k // 2) // k < 32, 2, 1).astype(np.int32)
        hole = '-'
        if fn in RING_FILES:
            n, bb = icon_norm(to_card(pm))
            al = np.asarray(Image.open(os.path.join(art, fn + '.png')).convert('RGBA'))[..., 3]
            ys, xs = np.nonzero(al > 128)
            pad = 8
            ccx = ((xs.min() + xs.max()) / 2.0) / CARD_DEN + pad
            ccy = ((ys.min() + ys.max()) / 2.0) / CARD_DEN + pad
            outer = (xs.max() - xs.min() + 1) / 2.0 / CARD_DEN
            w, h = bb[2] - bb[0] + 1, bb[3] - bb[1] + 1
            hole = '%d,%d,%d' % (int(round(1000 * (ccx - bb[0]) / w)), int(round(1000 * (ccy - bb[1]) / h)),
                                 int(round(1000 * outer * RING_HOLE_PM / 1000.0 / w)))
            s = max(w, h)
            ox, oy = (s - w) // 2, (s - h) // 2
            yy, xx = np.mgrid[0:NORM, 0:NORM]
            cxp = (xx + 0.5) * s / NORM - ox
            cyp = (yy + 0.5) * s / NORM - oy
            rr = outer * RING_HOLE_PM / 1000.0 + 1.5
            care[((cxp - (ccx - bb[0])) ** 2 + (cyp - (ccy - bb[1])) ** 2) <= rr * rr] = 0
        ew = sorted(s[0] for s in sizes)[len(sizes) // 2]
        eh = sorted(s[1] for s in sizes)[len(sizes) // 2]
        write_pgm(os.path.join(out, name + '.pgm'), tmpl)
        cname = '-'
        if not (care == 2).all():
            cname = name + '_care.pgm'
            write_pgm(os.path.join(out, cname), np.array([0, 128, 255], np.uint8)[care])
        lines.append('%s %s %s %s %d %s %d %d %s' % (name + '.pgm', cname, name, group, cp, side, ew, eh, hole))
        print('icon %-18s exp %dx%d hole %s' % (name, ew, eh, hole))
    with open(os.path.join(out, 'manifest.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n')


# ---------------------------------------------------------------- glyphs
class Renderer:
    def __init__(self, font_path):
        self.font_path = font_path

    def font(self, size, wght):
        f = ImageFont.truetype(self.font_path, size)
        f.set_variation_by_axes([100, min(96, max(17, size / CARD_DEN)), 400, wght])
        return f

    def text(self, text, size, wght, phase=(0, 0)):
        """Rendered at 3x and area-averaged down; returns (img, last row above baseline)."""
        d = CARD_DEN
        f = self.font(size * d, wght)
        W = int(size * d * (len(text) + 2)) // d * d
        H = int(size * d * 2.4) // d * d
        base = int(size * d * 1.6) // d * d
        im = Image.new('L', (W, H), 0)
        ImageDraw.Draw(im).text((d * 4 + phase[0], base + phase[1]), text, fill=255, font=f, anchor='ls')
        return area_resample(np.asarray(im), W // d, H // d), base // d - 1

    def entries(self, text, size, wght, labels):
        """Glyphs of `text` at the 9 sub-pixel phases, paired with `labels`, deduplicated."""
        out, seen = [], set()
        for py in range(CARD_DEN):
            for px in range(CARD_DEN):
                a, brow = self.text(text, size, wght, (px, py))
                gl = segment_glyphs(level_stretch(a, 0, 255))
                if len(gl) != len(labels):
                    raise RuntimeError('render of %r gave %d glyphs, want %d' % (text, len(gl), len(labels)))
                for lab, g in zip(labels, gl):
                    key = (lab, g['img'].shape, g['img'].tobytes())
                    if key not in seen:
                        seen.add(key)
                        out.append((lab, g['img'], g['y0'] - brow, g['y1'] - brow))
        return out


def write_glyphs(out, entries, note):
    os.makedirs(out, exist_ok=True)
    lines = ['# file label top_off bottom_off   (%s)' % note]
    names = {'.': 'dot', ',': 'comma'}
    for i, (lab, img, top, bot) in enumerate(entries):
        fn = 'g%02d_%s.pgm' % (i, names.get(lab, lab.replace('~', 'x').encode('unicode_escape').decode()
                                           .replace('\\', 'u')))
        write_pgm(os.path.join(out, fn), img)
        lines.append('%s %s %d %d' % (fn, lab, top, bot))
    with open(os.path.join(out, 'manifest.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')


def build_glyphs_sf(rd, out):
    size, wght = DIST_FONT
    only = lambda es, keep: [e for e in es if e[0] in keep]     # noqa: E731
    e = rd.entries('0123456789', size, wght, list('0123456789'))
    e += only(rd.entries('0.0', size, wght, ['Z', '.', 'Z']), '.')
    e += only(rd.entries('0,0', size, wght, ['Z', ',', 'Z']), ',')
    e += rd.entries('m', size, wght, ['m'])
    e += only(rd.entries('km', size, wght, ['k', 'Z']), 'k')
    e += only(rd.entries('mi', size, wght, ['Z', 'i']), 'i')
    e += rd.entries('yd', size, wght, ['y', 'd'])
    a, _ = rd.text('ft', size, wght)
    e += rd.entries('ft', size, wght, ['f', 't'] if len(segment_glyphs(level_stretch(a, 0, 255))) == 2 else ['ft'])
    e += rd.entries('м', size, wght, ['м'])
    e += only(rd.entries('км', size, wght, ['к', 'Z']), 'к')
    for ch in DIST_LOOKALIKES:           # reject labels: '~' + the letter (card_rec_text.c)
        e += [('~' + lab, img, top, bot) for lab, img, top, bot in rd.entries(ch, size, wght, [ch])]
    write_glyphs(out, e, 'SF Pro %.1f/%d rendered 3x + area average - bootstrap only' % (size, wght))
    print('glyphs_sf: %d templates' % len(e))


def build_ring_digits(rd, out):
    """Digits plus the confusable letters RING_CONFUSABLES as reject labels: card_rec.c
    REJECTs an exit digit that does not beat every letter by RING_LETTER_MARGIN.  A letter
    is stored with its true baseline offsets and, when its bottom is not on the baseline,
    again as if it were (a single glyph in the hole is measured from its own bottom)."""
    for s10 in range(80, 260, 5):
        a, _ = rd.text('0', s10 / 10.0, 700)
        g = segment_glyphs(level_stretch(a, 0, 255))
        if g and g[0]['img'].shape[0] >= RING_DIGIT_CAP:
            break
    else:
        raise RuntimeError('no ring digit size')
    size = s10 / 10.0
    e = rd.entries('0123456789', size, 700, list('0123456789'))
    for ch in RING_CONFUSABLES:
        for lab, img, top, bot in rd.entries(ch, size, 700, [ch]):
            e.append((lab, img, top, bot))
            if bot != 0:
                e.append((lab, img, top - bot, 0))
    write_glyphs(out, e, 'SF Pro Bold %.1f (cap %d px) - bootstrap for the ring exit number; '
                 'letters %s are reject labels' % (size, RING_DIGIT_CAP, RING_CONFUSABLES))
    print('ring_digits: size %.1f, %d templates' % (size, len(e)))


# ---------------------------------------------------------------- car glyph harvest
def card_text_block(y, zone):
    """Card gate + first-line layout of card_rec.c, reduced to what the harvest needs:
    the stretched line-1 block of the distance text right of the icon."""
    x, y0, w, h = ZONES[zone]
    c = y[y0:y0 + h, x:x + w]
    b = P['BAND']
    band = np.concatenate([c[:b].ravel(), c[-b:].ravel(), c[b:-b, :b].ravel(), c[b:-b, -b:].ravel()])
    black = int(np.searchsorted(np.cumsum(np.bincount(band, minlength=256)), (band.size + 1) // 2))
    if black > P['BLACK_MAX']:
        raise RuntimeError('car frame: zone %s is not a black card' % zone)
    s = level_stretch(c, black, white_level(c[b:-b, b:-b], P['WHITE_RANK']))
    ink = s >= T_INK
    lines = runs(nz(ink.any(1)), P['LINE_GAP'])
    if not lines:
        raise RuntimeError('car frame: empty card')
    l0, l1 = lines[0]
    groups = runs(nz(ink[l0:l1 + 1].any(0)), P['ICON_GAP'] - 1)
    if len(groups) < 2:
        raise RuntimeError('car frame: no distance text right of the icon')
    return s[l0:l1 + 1, groups[1][0]:groups[-1][1] + 1]


def build_glyphs_car(frame, zone, labels, out):
    rgb = np.asarray(Image.open(frame).convert('RGB')).astype(np.int32)
    y = ((rgb[..., 0] * 77 + rgb[..., 1] * 150 + rgb[..., 2] * 29) >> 8).astype(np.uint8)
    gl = segment_glyphs(card_text_block(y, zone))
    if len(gl) != len(labels):
        raise RuntimeError('car frame distance gave %d glyphs, labels %r' % (len(gl), labels))
    base = baseline_of(gl)
    e = [(lab, g['img'], g['y0'] - base, g['y1'] - base) for lab, g in zip(labels, gl)]
    write_glyphs(out, e, "harvested from car frame %s %s '%s'" % (os.path.basename(frame), zone, ''.join(labels)))
    print('glyphs_car: %s' % ' '.join('%s%s' % (lab, img.shape) for lab, img, _, _ in e))


def inside_repo(path):
    p = os.path.realpath(path)
    return p == REPO or p.startswith(REPO + os.sep)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--art', required=True, help='directory of the big_direction*.png maneuver images')
    ap.add_argument('--out', required=True, help='template root to write (outside the repository)')
    ap.add_argument('--car-frame', help='decoded cluster frame (PNG) with a settled card')
    ap.add_argument('--car-zone', default='fullscreen', choices=sorted(ZONES))
    ap.add_argument('--car-labels', default='1,0,m', help='labels of the car distance glyphs, reading order')
    ap.add_argument('--font', default=SYSTEM_FONT, help='variable SF Pro font file')
    a = ap.parse_args(argv)
    for what, path in (('--out', a.out), ('--art', a.art), ('--car-frame', a.car_frame)):
        if path and inside_repo(path):
            print('refusing %s inside the repository (%s): templates and their sources are '
                  'third-party material and stay outside it' % (what, path), file=sys.stderr)
            return 2
    if not os.path.isdir(a.art):
        print('no art directory: %s' % a.art, file=sys.stderr)
        return 2
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, 'README.txt'), 'w') as f:
        f.write('Template root for altscreen_render/card_rec.c (CARD_REC_TEMPLATES).\n'
                'Derived from third-party art and font renders: LOCAL ONLY, never commit.\n')
    rd = Renderer(a.font)
    build_icons(a.art, os.path.join(a.out, 'icons'))
    build_glyphs_sf(rd, os.path.join(a.out, 'glyphs_sf'))
    if a.car_frame:
        build_glyphs_car(a.car_frame, a.car_zone, a.car_labels.split(','), os.path.join(a.out, 'glyphs_car'))
    build_ring_digits(rd, os.path.join(a.out, 'ring_digits'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
