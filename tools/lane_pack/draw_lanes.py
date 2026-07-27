#!/usr/bin/env python3
"""
Procedurally draw Audi-style lane-guidance arrows.
Targets olymp1/800x246/lane/arrows/1 (41×39 PNGs).

Key visual properties of the originals:
  * Stem origin offsets to the side OPPOSITE the arrow direction.
  * Stem rises HIGH (close to top of tile), then bends sharply toward tip.
  * Arrowhead is integrated with the bend, like a "j" hook for sharp turns.
  * Slight turns have gentler S-curve from low stem.

Direction codes (BAP convention):
  0x00=straight, 0x20=slight right, 0x40=right, 0x60=sharp right,
  0x72=u-turn (right-side curl), 0x92=u-turn (left-side curl),
  0xa0=sharp left, 0xc0=left, 0xe0=slight left.
"""
import math
import os
from PIL import Image, ImageDraw

TILE_W, TILE_H = 41, 39
SCALE = 8
W, H  = TILE_W * SCALE, TILE_H * SCALE

COLOR_BG       = (50, 50, 50, 255)
COLOR_REGULAR  = (210, 210, 210, 255)
COLOR_HILITE   = (89, 171, 230, 255)       # #59ABE6

S        = SCALE
CX       = W / 2
BOT      = H - 3 * S
TOP      = 5 * S
STEM_W   = 7.5 * S       # match olymp1 stem weight
HEAD_LEN = 13 * S        # slightly larger arrowhead
HEAD_W   = 16 * S


def _path_thick(draw, points, width, color):
    last = None
    for p in points:
        if last is not None:
            draw.line([last, p], fill=color, width=int(width))
        draw.ellipse(
            [p[0] - width / 2, p[1] - width / 2,
             p[0] + width / 2, p[1] + width / 2],
            fill=color,
        )
        last = p


def _bezier(p0, p1, p2, n=24):
    pts = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        x = u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0]
        y = u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1]
        pts.append((x, y))
    return pts


def _arrowhead(draw, tip, sx, sy, color, length=HEAD_LEN, width=HEAD_W):
    px, py = -sy, sx
    base = (tip[0] - sx * length * 0.55, tip[1] - sy * length * 0.55)
    a = (base[0] + px * width / 2, base[1] + py * width / 2)
    b = (base[0] - px * width / 2, base[1] - py * width / 2)
    draw.polygon([tip, a, b], fill=color)


def draw_normal_arrow(draw, angle_deg: float, color: tuple):
    """
    Stem on opposite side of turn; rises high; curves toward tip; arrowhead.
    angle_deg: 0=straight, +ve=right turn, -ve=left turn.
    """
    rad = math.radians(angle_deg)
    sx, sy = math.sin(rad), -math.cos(rad)

    if abs(angle_deg) < 0.1:
        # Straight: centered tall stem, arrowhead at top.
        tip_y = TOP + HEAD_LEN * 0.5
        draw.rounded_rectangle(
            [CX - STEM_W / 2, tip_y - STEM_W * 0.3, CX + STEM_W / 2, BOT],
            radius=STEM_W / 2,
            fill=color,
        )
        _arrowhead(draw, (CX, TOP), 0, -1, color)
        return

    # Side and severity
    sign = 1 if angle_deg > 0 else -1            # +1 right, -1 left
    severity = abs(angle_deg) / 90.0              # 0..1 (0=straight, 1=90°)
    severity = min(severity, 1.0)

    # ── Two distinct visual modes that match the original olymp1 sprites ──
    # SHARP (severity ≥ 0.7, ≈ 60°-90°): J-hook profile.
    #   Tall vertical stem on opposite side, 90° corner near top, fully
    #   horizontal arrowhead landing at the far edge of the tile.
    # SLIGHT/MEDIUM (severity < 0.7): single smooth S-curve from
    #   bottom-center area arching up-and-out, arrowhead aligned to the
    #   end-of-curve tangent.
    if severity >= 0.7:
        # ----- J-hook (sharp) -----
        stem_x = CX - sign * 9 * S          # stem far on opposite side
        stem_top_y = TOP + 6 * S             # stem reaches near top
        tip_x = CX + sign * 16 * S           # tip near far edge of tile
        tip_y = TOP + 5 * S                  # tip near top
        head_dir = (sign, 0.0)               # purely horizontal arrowhead

        # Stem
        draw.rounded_rectangle(
            [stem_x - STEM_W / 2, stem_top_y, stem_x + STEM_W / 2, BOT],
            radius=STEM_W / 2,
            fill=color,
        )
        # Bezier with corner control point — gives the L/J shape.
        head_back = (tip_x - head_dir[0] * HEAD_LEN * 0.55,
                     tip_y - head_dir[1] * HEAD_LEN * 0.55)
        ctrl = (stem_x, tip_y)               # corner at stem_x, tip_y
        pts = _bezier((stem_x, stem_top_y), ctrl, head_back, n=22)
        _path_thick(draw, pts, STEM_W, color)
        _arrowhead(draw, (tip_x, tip_y), head_dir[0], head_dir[1], color)
    else:
        # ----- Smooth S-curve (slight / medium) -----
        # Single flowing path — no separate stem rect, just one bezier
        # from bottom-center-ish to upper-side, with the arrowhead at
        # the end pointing along the curve tangent.
        t = severity / 0.7                   # 0..1 across the slight band
        # Bottom of curve sits a touch off-center, opposite the turn
        bot_x = CX - sign * (1 + t * 4) * S
        bot_y = BOT - STEM_W * 0.3
        # Control point pulls the curve up — the more severe, the more out
        ctrl_x = CX - sign * (4 + t * 6) * S
        ctrl_y = TOP + (1 - t) * 6 * S + 6 * S
        # End (tip) on the side of turn direction, near top
        tip_x = CX + sign * (4 + t * 11) * S
        tip_y = TOP + (1 - t) * 5 * S + 4 * S

        # Bezier
        pts = _bezier((bot_x, bot_y), (ctrl_x, ctrl_y), (tip_x, tip_y), n=24)
        _path_thick(draw, pts, STEM_W, color)
        # Arrowhead direction = tangent at end (last segment direction)
        if len(pts) >= 2:
            ax = pts[-1][0] - pts[-3][0]
            ay = pts[-1][1] - pts[-3][1]
            mag = math.hypot(ax, ay) or 1
            sx_t, sy_t = ax / mag, ay / mag
        else:
            sx_t, sy_t = math.sin(rad), -math.cos(rad)
        _arrowhead(draw, (tip_x, tip_y), sx_t, sy_t, color)


def draw_uturn(draw, side: int, color: tuple):
    """
    U-turn: stem rises from the side OPPOSITE the turn direction, curls
    180° at top, comes back down on the SAME side as the turn, arrowhead
    pointing down.  Matches Audi olymp1 0x72 (right-side u-turn — arrow
    on right) and 0x92 (left-side — arrow on left).

    side = +1: u-turn to the RIGHT (stem on left, arrow on right).
    side = -1: u-turn to the LEFT  (stem on right, arrow on left).
    """
    radius = 5 * S
    pivot_y = TOP + radius
    up_x   = CX - side * radius   # stem opposite the turn
    down_x = CX + side * radius   # arrow on the turn side

    # Going-up stem
    draw.rounded_rectangle(
        [up_x - STEM_W / 2, pivot_y, up_x + STEM_W / 2, BOT],
        radius=STEM_W / 2,
        fill=color,
    )

    # Curl: bezier through TOP
    p_start = (up_x, pivot_y)
    p_top   = (CX, TOP)
    p_end   = (down_x, pivot_y)
    curl_pts = _bezier(p_start, (up_x, TOP), p_top, n=14) + \
               _bezier(p_top,   (down_x, TOP), p_end, n=14)
    _path_thick(draw, curl_pts, STEM_W, color)

    # Coming-down stem (shorter — leaves room for arrowhead)
    down_bot = BOT - HEAD_LEN * 0.45
    draw.rounded_rectangle(
        [down_x - STEM_W / 2, pivot_y, down_x + STEM_W / 2, down_bot],
        radius=STEM_W / 2,
        fill=color,
    )

    # Arrowhead pointing DOWN
    _arrowhead(draw, (down_x, down_bot + STEM_W * 0.5), 0, 1, color)


DIRECTION_CODES = {
    0x00: ("normal",    0.0),
    0x20: ("normal",   30.0),
    0x40: ("normal",   55.0),
    0x60: ("normal",   90.0),
    0x72: ("uturn",    +1),
    0x92: ("uturn",    -1),
    0xa0: ("normal",  -90.0),
    0xc0: ("normal",  -55.0),
    0xe0: ("normal",  -30.0),
}


def render_sprite(angle_code: int, recommended: bool) -> Image.Image:
    img = Image.new("RGBA", (W, H), COLOR_BG)
    drw = ImageDraw.Draw(img, "RGBA")
    color = COLOR_HILITE if recommended else COLOR_REGULAR

    kind, param = DIRECTION_CODES[angle_code]
    if kind == "normal":
        draw_normal_arrow(drw, param, color)
    elif kind == "uturn":
        draw_uturn(drw, param, color)

    return img.resize((TILE_W, TILE_H), Image.LANCZOS)


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR    = os.path.join(SCRIPT_DIR, "out")
REF_DIR    = os.path.join(SCRIPT_DIR, "reference")


def render_all() -> list:
    os.makedirs(OUT_DIR, exist_ok=True)
    rendered = []
    for code in sorted(DIRECTION_CODES):
        for rec in (False, True):
            name = f"lg_dr_{code:02x}{'_recommended' if rec else ''}.png"
            img = render_sprite(code, rec)
            img.save(os.path.join(OUT_DIR, name))
            rendered.append((code, rec, name, img))
    return rendered


def write_preview_strip(rendered):
    pw = TILE_W * len(rendered)
    strip = Image.new("RGBA", (pw, TILE_H), (0, 0, 0, 0))
    for i, (_, _, _, img) in enumerate(rendered):
        strip.paste(img, (i * TILE_W, 0))
    strip.save(os.path.join(OUT_DIR, "_preview_strip.png"))


def write_compare_grid(rendered):
    """4-row side-by-side: orig regular | proc regular | orig hi | proc hi."""
    if not os.path.isdir(REF_DIR):
        return None
    codes_in_order = sorted(DIRECTION_CODES.keys())
    PAD = 4
    cw = TILE_W + PAD
    rh = TILE_H + PAD
    canvas = Image.new(
        "RGBA",
        (PAD + cw * len(codes_in_order), PAD + rh * 4),
        (40, 40, 40, 255),
    )

    def paste_row(row_idx: int, src_dir: str, suffix: str):
        y = PAD + row_idx * rh
        for col, c in enumerate(codes_in_order):
            x = PAD + col * cw
            f = os.path.join(src_dir, f"lg_dr_{c:02x}{suffix}.png")
            if os.path.exists(f):
                canvas.paste(Image.open(f), (x, y))

    paste_row(0, REF_DIR, "")               # original regular
    paste_row(1, OUT_DIR, "")               # procedural regular
    paste_row(2, REF_DIR, "_recommended")   # original highlighted
    paste_row(3, OUT_DIR, "_recommended")   # procedural highlighted
    out = os.path.join(OUT_DIR, "_compare.png")
    canvas.save(out)
    return out


def main():
    rendered = render_all()
    write_preview_strip(rendered)
    cmp_path = write_compare_grid(rendered)
    print(f"[lane_pack] {len(rendered)} sprites → {OUT_DIR}/")
    print(f"[lane_pack] strip:   {OUT_DIR}/_preview_strip.png")
    if cmp_path:
        print("[lane_pack] compare (rows: orig | proc | orig-hi | proc-hi):")
        print(f"            {cmp_path}")
    else:
        print(f"[lane_pack] (no reference dir at {REF_DIR}, skipping compare)")


if __name__ == "__main__":
    main()
