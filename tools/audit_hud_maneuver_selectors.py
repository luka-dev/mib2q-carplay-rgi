#!/usr/bin/env python3
"""Verify HUD maneuver selectors using original ARM code and FD62 state tables.

Exports every raw (including empty) state in page3 groups2/20, excluding bargraph.
This establishes INTERNAL packet -> surfaces, not BAP -> internal packet.
Requires Unicorn and Pillow. Firmware is hash-pinned, read-only.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
from verify_hud_distance_bargraph import FIRMWARES, GDC, GDC_FILE, GDC_SIZE, SCENE, GLOBALS, PACKET, STACK, STOP, ELEMENT_DISPATCH, DRAW_SURFACE, TRACK_SURFACE


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('firmware', type=Path)
    ap.add_argument('out', type=Path)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    blobs = {n: (args.firmware / 'blocks' / n).read_bytes() for n in next(iter(FIRMWARES.values()))}
    hashes = {n: hashlib.sha256(v).hexdigest() for n, v in blobs.items()}
    firmware = next((n for n, h in FIRMWARES.items() if h == hashes), None)
    if firmware is None:
        raise SystemExit('Unsupported/mixed firmware fingerprint')
    scene = blobs['FD_62FLASHDATA.bin']
    # Resource identities are keyed by bank/offset. G/M scene and bank2 icon
    # payloads are identical; the G extraction provides PNGs for both versions.
    asset_root = args.firmware.parent / 'FL_8W6919617G_1952_APPL_V000' / 'navigation_icons'
    manifest = json.loads((asset_root / 'raster_resources_manifest.json').read_text())
    resources = {(r['bank'], r['data_offset']): r for r in manifest['resources']}
    names = json.loads((asset_root / 'maneuvers_manifest.json').read_text())
    aliases = {r['id']: r.get('source_name_aliases', []) for r in names['categories'][0]['resources']}
    nodes = []
    page = 0x48ad4
    group = page + 8
    for _ in range(scene[page + 5]):
        group_size, bit, count = struct.unpack_from('<IBxH', scene, group)
        node = group + 8
        for _ in range(count):
            size, kind, selector_type, field, mask, nstates = struct.unpack_from('<IBBHBxH', scene, node)
            if bit in (2, 20) and field != 32 and kind in (0, 1):
                states = []
                for selector in range(nstates):
                    entry = node + 12 + selector * 8
                    relative, nsurfaces = struct.unpack_from('<IB', scene, entry)
                    surfaces = []
                    for i in range(nsurfaces):
                        offset = entry + relative + i * 44
                        raw = scene[offset:offset + 44]
                        assert len(raw) == 44 and offset + 44 <= node + size
                        data_offset = struct.unpack_from('<I', raw, 4)[0]
                        r = resources.get((raw[39], data_offset))
                        surfaces.append(dict(descriptor_offset=hex(offset), descriptor_hex=raw.hex(),
                            resource=r['id'] if r else None,
                            png=str(asset_root / 'raster_resources' / r['png']) if r else None,
                            aliases=aliases.get(r['id'], []) if r else []))
                    states.append(dict(selector=selector, surfaces=surfaces))
                nodes.append(dict(node=hex(node), group=bit, kind=kind, field=field,
                    packet_byte=13 + field, mask=mask, selector_type=selector_type, states=states))
            node += size
        assert node == group + group_size
        group += group_size
    assert group == page + struct.unpack_from('<I', scene, page)[0]
    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    uc.mem_map(GDC, (GDC_SIZE + 4095) & ~4095)
    uc.mem_write(GDC, blobs['FD_61FLASHDATA.bin'][GDC_FILE:GDC_FILE + GDC_SIZE])
    uc.mem_map(SCENE, len(scene)); uc.mem_write(SCENE, scene)
    uc.mem_map(GLOBALS, 0x20000); uc.mem_map(PACKET, 0x1000)
    uc.mem_map(STACK, 0x10000); uc.mem_map(STOP, 0x1000)
    captured = []

    def boundary(vm, address, size, unused):
        if address == DRAW_SURFACE:
            captured.append(bytes(vm.mem_read(vm.reg_read(UC_ARM_REG_R0), 44)).hex())
        if address in (DRAW_SURFACE, TRACK_SURFACE):
            vm.reg_write(UC_ARM_REG_R0, 0)
            vm.reg_write(UC_ARM_REG_PC, vm.reg_read(UC_ARM_REG_LR))
    uc.hook_add(UC_HOOK_CODE, boundary)
    cases = 0
    for node in nodes:
        assert node['selector_type'] == 1 and node['mask'] == 255
        for state in node['states'] + [dict(selector=255, surfaces=[])]:
            uc.mem_write(GLOBALS, bytes(0x20000))
            uc.mem_write(0x6F7750, b'\x02')
            packet = bytearray([255] * 128); packet[8] = 3
            packet[node['packet_byte']] = state['selector']
            uc.mem_write(PACKET, bytes(packet)); captured.clear()
            uc.reg_write(UC_ARM_REG_R0, PACKET)
            uc.reg_write(UC_ARM_REG_R1, SCENE + int(node['node'], 16))
            uc.reg_write(UC_ARM_REG_SP, STACK + 0xF000); uc.reg_write(UC_ARM_REG_LR, STOP)
            uc.emu_start(ELEMENT_DISPATCH, STOP, count=100000)
            assert uc.reg_read(UC_ARM_REG_PC) == STOP
            assert captured == [s['descriptor_hex'] for s in state['surfaces']], (node['node'], state['selector'])
            cases += 1
    result = dict(firmware=firmware, sha256=hashes, cases_passed=cases,
        scope='Original ARM element dispatch/state walker. BAP-to-internal-packet conversion NOT executed.', nodes=nodes)
    (args.out / (firmware + '-selectors.json')).write_text(json.dumps(result, indent=2) + '\n')
    font = ImageFont.truetype('/System/Library/Fonts/Menlo.ttc', 12)
    for label, field in [('hud_2d', 89), ('hud_3d', 9)]:
        relevant = [n for n in nodes if n['field'] == field]
        nstates = max(len(n['states']) for n in relevant)
        for start in range(0, nstates, 48):
            count = min(48, nstates - start)
            sheet = Image.new('RGB', (6 * 190, 36 + ((count + 5) // 6) * 156), '#11151b')
            draw = ImageDraw.Draw(sheet)
            draw.text((10, 8), f'{label}: INTERNAL packet[{13 + field}] selectors; NOT BAP mainElement', font=font, fill='white')
            for selector in range(start, start + count):
                x, y = ((selector - start) % 6) * 190, 36 + ((selector - start) // 6) * 156
                surfaces = [s for n in relevant if selector < len(n['states']) for s in n['states'][selector]['surfaces']]
                # Catalog images, each layer independently, not a reconstructed
                # screen: this deliberately does not guess the road-layer fields.
                tilew = 180 // max(1, len(surfaces))
                for index, surface in enumerate(surfaces):
                    if surface['png']:
                        im = Image.open(surface['png']).convert('RGBA')
                        box = im.getbbox()
                        if box: im = im.crop(box)
                        im.thumbnail((tilew - 4, 106))
                        sheet.paste(im, (x + 5 + index * tilew + (tilew - im.width)//2, y + 4), im)
                ids = '+'.join(s['resource'] or '?' for s in surfaces) or 'EMPTY'
                draw.text((x+5, y+114), f'{selector:3d}  {ids}', font=font, fill='#8edbed')
                alias = surfaces[0]['aliases'][0] if surfaces and surfaces[0]['aliases'] else ''
                draw.text((x+5, y+132), alias[:25], font=font, fill='white')
            sheet.save(args.out / f'{label}_{start:03d}.png')
    assert hashes == {n: hashlib.sha256((args.firmware / 'blocks' / n).read_bytes()).hexdigest() for n in blobs}
    print(f'{firmware}: {len(nodes)} elements, {cases} selector cases PASS')


if __name__ == '__main__':
    main()
