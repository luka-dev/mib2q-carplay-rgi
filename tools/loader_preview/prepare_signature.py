#!/usr/bin/env python3
"""Bake the selected PRISM signature from local system fonts at panel resolution.

The font file is not bundled. Runtime decodes the alpha texture from the compiled PRISM header.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

LOADER = Path(__file__).resolve().parents[2] / "altscreen_render" / "loader"
TEXT = "@LuKa_dev"


def render(font_name, weight, width, tracking, height=16):
    font = ImageFont.truetype(f"/System/Library/Fonts/{font_name}", 160)
    values = {b"Width": width, b"Optical Size": 17, b"GRAD": 400,
              b"Weight": weight, b"YAXS": weight}
    font.set_variation_by_axes([values.get(a["name"], a["default"])
                                for a in font.get_variation_axes()])
    canvas = Image.new("RGBA", (1600, 320))
    draw = ImageDraw.Draw(canvas)
    x = 16.0
    for i, char in enumerate(TEXT):
        draw.text((x, 210), char, font=font, anchor="ls", fill="white")
        advance = font.getlength(char)
        if i+1 < len(TEXT):
            advance = font.getlength(char+TEXT[i+1])-font.getlength(TEXT[i+1])
        x += advance+160*tracking  # Keep pair kerning as well as tracking.
    label = canvas.crop(canvas.getbbox())
    # Bake coverage at the actual panel size. A 142px-high atlas minified with
    # GL_LINEAR only samples 2x2 texels and makes small letter strokes sparkle.
    label = label.resize((round(label.width*height/label.height), height), Image.Resampling.LANCZOS)
    return label


def main():
    label = render("SFNSRounded.ttf", 500, 100, 0.045, 18)
    label.save(LOADER / "sig_prism_soft.png")
    print(f"SOFT / SF Rounded Medium: {label.width}x{label.height}, native panel coverage")


if __name__ == "__main__":
    main()
