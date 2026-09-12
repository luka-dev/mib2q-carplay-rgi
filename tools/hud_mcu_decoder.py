#!/usr/bin/env python3
"""Offline reference for HUD BAP -> 3D selectors, using original FD51 tables.

Recovered from V850 functions 5ec3c, 8adac, 8b32e..8b798 and 89036..8910c.
This is the active 3D descriptor conversion after the normal 8acb0 reset;
it does not choose panel modes, process CAN framing, or implement slot 2/3.
Validate against actual V850 execution: python3 tools/hud_mcu_decoder.py FD51.
"""
import argparse
import csv
import hashlib
import json
import struct
from pathlib import Path

FINGERPRINTS = {
    '138351c2f12cd8e45d9f50fd4833775371ec46401b6838f04c24a9a4baaabae2': 'G1952',
    '6e59b62c982873a778bf72b749bf6dc113fe3fe14ee4bb3ae3855d0c624f6fe5': 'M2325',
}
BASE = 0x20000


class HudMcuDecoder:
    def __init__(self, fd51):
        self.rom = Path(fd51).read_bytes()
        self.sha256 = hashlib.sha256(self.rom).hexdigest()
        if self.sha256 not in FINGERPRINTS:
            raise ValueError('Unsupported FD51 fingerprint')
        self.firmware = FINGERPRINTS[self.sha256]
        self.classes = dict(self.rows(0x27e24, 34, '<HBx'))
        self.directions = self.rows(0x27d54, 17, '<H')
        self.exit_directions = self.rows(0x27d76, 19, '<H')
        self.street_directions = self.rows(0x27dfe, 19, '<B')
        self.families = self.rows(0x27d9c, 23, '<BB')
        self.z_markers = self.rows(0x27978, 4, '<BxHBx')

    def rows(self, address, count, fmt):
        result = [struct.unpack_from(fmt, self.rom, address - BASE + i * struct.calcsize(fmt))
                  for i in range(count)]
        return [r[0] for r in result] if len(result[0]) == 1 else result

    def byte(self, address):
        return self.rom[address - BASE]

    def road(self, address):
        return struct.unpack_from('<IB3x', self.rom, address - BASE)

    def decode(self, main, direction, z=0, streets=b''):
        if not all(0 <= v <= 255 for v in (main, direction, z)) or len(streets) > 16:
            raise ValueError('Expected byte fields and at most 16 side streets')
        # 5ec3c moves a bare FOLLOW_STREET into a separate path; the active
        # first 3D descriptor becomes empty. This says nothing about mode choice.
        if (main, direction, z, len(streets)) == (11, 0, 0, 0):
            main = direction = z = 0
        category = self.classes.get(main)
        model = marker = 0
        roads = []
        if category is not None:
            index = self.directions.index(direction) if direction in self.directions else 16
            model, = struct.unpack_from('<I', self.rom, 0x388e4 - BASE + (category * 17 + index) * 8)
        if model:
            marker_model = next((value for family, level, value in self.z_markers
                                 if family == category and level == z), 0)
            marker = self.byte(0x2749a + marker_model)
            family, default = self.families[category]
            if default:
                roads.append(self.road(0x27990 + default * 8))
            excluded = 17  # Original class-4 path initializes the sentinel to 17.
            if category != 4:
                excluded = 19
                if direction in self.exit_directions:
                    excluded = self.exit_directions.index(direction)
                    roads.append(self.road(0x39e24 + (family * 19 + excluded) * 8))
            for street in streets:
                if len(roads) >= 17:
                    break
                # Match the MCU's first accepted entry, including its exclusion.
                for index, value in enumerate(self.street_directions):
                    if value == street and index != excluded:
                        roads.append(self.road(0x39e24 + (family * 19 + index) * 8))
                        break
            roads.sort(key=lambda r: r[1])  # Stable insertion sort in 8b798.
        road_selectors = bytes(self.byte(0x27890 + enum) if enum < 67 else 0 for enum, _ in roads)
        return dict(stored_main=main, stored_direction=direction, model=model,
                    selector=self.byte(0x277ba + (model & 255)) if model & 255 < 121 else 0,
                    marker=marker, road_count=len(roads),
                    roads_hex=(road_selectors + bytes(17-len(roads))).hex(), group_mask=4)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fd51', type=Path)
    parser.add_argument('--out', type=Path, default=Path('output/maneuver-audit'))
    args = parser.parse_args()
    decoder = HudMcuDecoder(args.fd51)
    filename = 'mcu-chain-M-output.csv' if decoder.firmware == 'M2325' else 'mcu-chain-output.csv'
    with (args.out / filename).open() as f:
        rows = list(csv.DictReader(f))
    for row in rows:
        actual = decoder.decode(int(row['main']), int(row['direction']), int(row['z']), bytes.fromhex(row['streets_hex']))
        for name, value in actual.items():
            assert str(value) == row[name], (row['case'], name, value, row[name])
    tables = dict(firmware=decoder.firmware, sha256=decoder.sha256,
                  classes=decoder.classes, directions=decoder.directions,
                  exit_directions=decoder.exit_directions, street_directions=decoder.street_directions,
                  families=decoder.families, z_markers=decoder.z_markers,
                  class_direction_models=decoder.rows(0x388e4, 23 * 17, '<II'),
                  main_selectors=decoder.rows(0x277ba, 121, '<B'),
                  marker_selectors=decoder.rows(0x2749a, 5, '<B'),
                  road_selectors=decoder.rows(0x27890, 67, '<B'),
                  default_roads=decoder.rows(0x27990, 8, '<IB3x'),
                  direction_roads=decoder.rows(0x39e24, 8 * 19, '<IB3x'),
                  original_V850_cases_matched=len(rows))
    (args.out / f'mcu-tables-{decoder.firmware}.json').write_text(json.dumps(tables, indent=2) + '\n')
    print(f'{decoder.firmware}: reference decoder matches original V850 execution on {len(rows)} cases')


if __name__ == '__main__':
    main()
