#!/usr/bin/env python3
"""Bake numeric distance/normal data for the existing logo's glass bevel.

R: signed distance (positive inside, 0.5 + 4*d in UV units).
GB: outward unit normal, encoded from [-1,1] into [0,1].
A: original artwork coverage. This is shader data, not replacement artwork.
"""
from pathlib import Path
import numpy as np
from PIL import Image
from scipy.ndimage import distance_transform_edt, gaussian_filter

LOADER = Path(__file__).resolve().parents[2] / "altscreen_render" / "loader"


def main():
    alpha = np.asarray(Image.open(LOADER / "apple_logo.png").convert("RGBA"))[:, :, 3]
    inside = alpha >= 128
    # Both the leaf and lower-left lobe touch the artwork's canvas boundary.
    # Include the transparent exterior before calculating distance/derivatives;
    # otherwise the last row has a false inward gradient and a specular spur.
    padding = 64
    padded = np.pad(inside, padding, constant_values=False)
    distance = distance_transform_edt(padded) - distance_transform_edt(~padded)
    distance = gaussian_filter(distance, 1.2) / alpha.shape[0]
    dy, dx = np.gradient(distance)
    crop = (slice(padding, -padding), slice(padding, -padding))
    distance, dy, dx = distance[crop], dy[crop], dx[crop]
    norm = np.maximum(np.hypot(dx, dy), 1e-9)
    out = np.empty((*alpha.shape, 4), dtype=np.uint8)
    out[:, :, 0] = np.rint(np.clip(0.5 + distance*4, 0, 1)*255)
    out[:, :, 1] = np.rint(np.clip(0.5 - dx/norm*0.5, 0, 1)*255)
    out[:, :, 2] = np.rint(np.clip(0.5 - dy/norm*0.5, 0, 1)*255)
    out[:, :, 3] = alpha
    Image.fromarray(out).save(LOADER / "logo_material.png")


if __name__ == "__main__":
    main()
