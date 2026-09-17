#!/usr/bin/env python3
"""Offline independent shaping check for generated conservative advances.

Uses real extracted fonts and Pillow/RAQM. This validates table extraction;
it is not a substitute for calibrating the stock EAL rasterizer on the VC.
"""
import importlib.util
from pathlib import Path
import random
import unicodedata

from PIL import ImageFont, features

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('vc_generator', ROOT / 'tools/generate_vc_text_data.py')
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)


def main():
    assert features.check('raqm'), 'Pillow with RAQM shaping is required'
    fonts = ROOT / 'output/r2/vc-fonts-0379-20260910/fonts'
    rng = random.Random(170027)
    count = 0
    for i, name in enumerate(gen.FONTS):
        widths, _ = gen.font_widths(fonts / name, i == 0)
        face = ImageFont.truetype(str(fonts / name), 27)
        if i == 0:
            alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ÉÀÖéàöЖШДфяим123456789‹›.,- '
        elif i == 1:
            alphabet = 'ابتثجحخدذرزسشصضطظعغفقكلمنهويأإآةىَُِّْ '
        elif i == 2:
            alphabet = '北京市朝阳区建国门外大街向东直到下一条路右转然后进入停车场'
        elif i == 3:
            alphabet = '서울특별시종로구세종대로광화문광장에서우회전하세요'
        else:
            alphabet = '東京都千代田区丸の内一丁目中央通りから次の交差点で右折'
        assert all(ord(c) in widths for c in alphabet), name
        for _ in range(1000):
            text = ''.join(rng.choice(alphabet) for _ in range(rng.randrange(1, 33)))
            shaped = face.getlength(text)
            bound = sum(widths[ord(c)] for c in text) / 64
            if unicodedata.category(text[0]).startswith('M'):
                bound += widths.get(0x25CC, 27 * 64) / 64
            # Font rasterizer quantization at 27 px may differ by one 1/64px
            # per glyph; the HU fitting budget has a separate 6px reserve.
            assert shaped <= bound + len(text) / 64, (name, text, shaped, bound)
            count += 1
    data = (ROOT / 'java_resources/com/luka/carplay/rgd/vc-text.bin').read_bytes()
    assert data[:4] == b'VCT2'
    pos = 4
    def read_var():
        nonlocal pos
        result = 0
        for shift in range(0, 28, 7):
            value = data[pos]
            pos += 1
            result |= (value & 127) << shift
            if not value & 128:
                return result
        raise AssertionError('invalid varint')
    unknown, n = read_var(), read_var()
    gaps, spans, values = [[read_var() for _ in range(n)] for _ in range(3)]
    table = []
    previous_end = -1
    for gap, span, value in zip(gaps, spans, values):
        start = previous_end + 1 + gap
        previous_end = start + span
        table.append((start, previous_end, value))
    def advance(cp):
        return next((v for a, b, v in table if a <= cp <= b), unknown)
    for text in ('Main Street', 'North Pennsylvania Avenue', '‹North Pennsylvania Avenue›',
                 'улица Большая Дмитровка', 'Avenida de la Constitución', 'Avenue des Champs-Élysées'):
        shaped = ImageFont.truetype(str(fonts / gen.FONTS[0]), 27).getlength(text)
        assert shaped <= sum(advance(ord(c)) for c in text) / 64
    print('VC font metric bounds: %d shaped samples + decorated road fixtures PASS (host RAQM, not EAL calibration)' % count)


if __name__ == '__main__':
    main()
