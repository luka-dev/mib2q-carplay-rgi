#!/usr/bin/env python3
"""
Extract all PNG images from VC cluster firmware images.
Searches for PNG signature (89 50 4E 47) and extracts complete PNG blobs.
Also looks for nearby ASCII strings that might be filenames.
"""
import os
import sys
import struct

PNG_SIG = b'\x89PNG\r\n\x1a\n'
IEND_MARKER = b'IEND'

def find_nearby_name(data, pos, search_range=256):
    """Look backwards from pos for a plausible filename string."""
    start = max(0, pos - search_range)
    chunk = data[start:pos]
    # Look for strings ending in .png, .bmp, .tga, etc.
    for ext in [b'.png', b'.bmp', b'.tga', b'.jpg']:
        idx = chunk.rfind(ext)
        if idx != -1:
            # Walk back to find start of string
            s = idx
            while s > 0 and chunk[s-1] >= 0x20 and chunk[s-1] < 0x7F:
                s -= 1
                if idx - s > 120:
                    break
            name = chunk[s:idx+4].decode('ascii', errors='replace')
            name = name.replace('\\', '/').replace('\x00', '')
            # Clean: only keep last path component
            if '/' in name:
                name = name.rsplit('/', 1)[-1]
            if len(name) > 3:
                return name
    # Also look for generic identifiers
    for pattern in [b'arrow', b'Arrow', b'AIO', b'aio', b'navi', b'Navi', b'turn', b'Turn']:
        idx = chunk.rfind(pattern)
        if idx != -1:
            s = idx
            while s > 0 and chunk[s-1] >= 0x20 and chunk[s-1] < 0x7F:
                s -= 1
                if idx - s > 80:
                    break
            e = idx + len(pattern)
            while e < len(chunk) and chunk[e] >= 0x20 and chunk[e] < 0x7F:
                e += 1
                if e - idx > 80:
                    break
            label = chunk[s:e].decode('ascii', errors='replace').strip()
            if len(label) > 2:
                return f"[{label}]"
    return None

def extract_pngs(img_path, out_dir, prefix):
    with open(img_path, 'rb') as f:
        data = f.read()

    pos = 0
    idx = 0
    extracted = []

    while True:
        start = data.find(PNG_SIG, pos)
        if start == -1:
            break

        # Find IEND chunk
        iend = data.find(IEND_MARKER, start + 8)
        if iend == -1:
            # No IEND - try to find next PNG or take reasonable chunk
            next_png = data.find(PNG_SIG, start + 8)
            end = next_png if next_png != -1 else min(start + 1024*1024, len(data))
        else:
            end = iend + 8  # IEND + 4 byte CRC

        png_data = data[start:end]

        # Validate: check IHDR chunk exists
        has_ihdr = png_data[12:16] == b'IHDR' if len(png_data) > 16 else False

        # Try to get dimensions from IHDR
        width = height = 0
        if has_ihdr and len(png_data) > 24:
            width = struct.unpack('>I', png_data[16:20])[0]
            height = struct.unpack('>I', png_data[20:24])[0]

        # Look for nearby name
        nearby = find_nearby_name(data, start)

        # Generate filename
        size_str = f"{width}x{height}" if width > 0 else "unknown"
        if nearby and not nearby.startswith('['):
            fname = f"{prefix}_{idx:03d}_{nearby}"
        else:
            fname = f"{prefix}_{idx:03d}_{size_str}.png"

        # Save
        fpath = os.path.join(out_dir, fname)
        with open(fpath, 'wb') as f:
            f.write(png_data)

        note = f" name_hint={nearby}" if nearby else ""
        has_iend = iend != -1 and iend < end
        status = "OK" if (has_ihdr and has_iend) else "MAYBE_BROKEN"

        extracted.append({
            'file': fname,
            'offset': start,
            'size': len(png_data),
            'dims': f"{width}x{height}",
            'status': status,
            'hint': nearby,
            'has_ihdr': has_ihdr,
            'has_iend': has_iend,
        })

        print(f"  {fname}: {len(png_data)} bytes, {size_str}, {status}{note}")

        idx += 1
        pos = start + 8

    return extracted

def main():
    base = "/Users/luka/Desktop/AUDI_2/Firmwares/AU_C1_AU491_0379_1310_prod_8S0906961AL_A4-A5-Q5_2018/KI_FPK_AU491"
    out = "/Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay/c_render/vc_aio_arrows"

    images = [
        (f"{base}/gss-applications/8/default/app.img", "app"),
        (f"{base}/gss-stage2/8/default/stage2_ifs.img", "stage2"),
        (f"{base}/gss-stage2-nand/8/default/stage2_nand.img", "nand"),
    ]

    all_extracted = []
    for img_path, prefix in images:
        name = os.path.basename(img_path)
        print(f"\n=== {name} ({os.path.getsize(img_path) / 1024 / 1024:.1f} MB) ===")
        items = extract_pngs(img_path, out, prefix)
        all_extracted.extend(items)

    # Summary
    print(f"\n{'='*60}")
    print(f"Total: {len(all_extracted)} PNGs extracted to {out}/")

    # Flag potential AIO arrows
    arrow_candidates = [e for e in all_extracted if e['hint'] and
                       any(k in str(e['hint']).lower() for k in ['arrow', 'aio', 'turn', 'navi'])]
    if arrow_candidates:
        print(f"\nPotential AIO arrow candidates:")
        for e in arrow_candidates:
            print(f"  {e['file']}: {e['dims']}, hint={e['hint']}")

    broken = [e for e in all_extracted if e['status'] == 'MAYBE_BROKEN']
    if broken:
        print(f"\nPotentially broken ({len(broken)}):")
        for e in broken:
            print(f"  {e['file']}: ihdr={e['has_ihdr']}, iend={e['has_iend']}")

if __name__ == '__main__':
    main()
