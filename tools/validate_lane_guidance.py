#!/usr/bin/env python3
"""
Validate static lane-guidance mapping from hook logs.

This mirrors BAPBridge's lane mapping:
  - primary lane arrow -> laneDirection
  - additional distinct lane angles -> laneSideStreets

Input is the decoded RGD log form:
  Lane guidance: idx=0 lanes=5
    lane[0] pos=0 dir=-90 status=2 angles(0)=[]
"""

import argparse
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List


LANE_DIR_NATIVE_ANGLES = [-180, -135, -90, -45, 0, 45, 90, 135, 180]
LANE_DIR_NATIVE_CODES = [0x72, 0x60, 0x40, 0x20, 0x00, 0xE0, 0xC0, 0xA0, 0x92]
MAX_BAP_LANE_SIDESTREETS = 17


GUIDANCE_RE = re.compile(r"Lane guidance: idx=(\d+) lanes=(\d+)")
LANE_RE = re.compile(
    r"lane\[(\d+)\]\s+pos=(\d+)\s+dir=([-0-9]+)\s+status=(\d+)\s+angles\((\d+)\)=\[(.*?)\]"
)


@dataclass
class Lane:
    lane_index: int
    pos: int
    direction: int
    status: int
    angles: List[int]


@dataclass
class LaneGuidance:
    index: int
    expected_count: int
    lanes: List[Lane]


def map_raw_lane_value_to_direction_code(raw: int) -> int:
    if raw == 1000:
        return 0xFF
    best_code = 0
    best_diff = 100000
    for angle, code in zip(LANE_DIR_NATIVE_ANGLES, LANE_DIR_NATIVE_CODES):
        diff = abs(raw - angle)
        if diff < best_diff:
            best_diff = diff
            best_code = code
    return best_code


def map_lane_direction(direction: int, angles: List[int]) -> int:
    if direction == 1000:
        if angles:
            return map_raw_lane_value_to_direction_code(angles[0])
        return 0xFF
    return map_raw_lane_value_to_direction_code(direction)


def map_lane_sidestreets(direction: int, angles: List[int], primary_code: int) -> List[int]:
    if not angles:
        return []
    start = 1 if direction == 1000 else 0
    if start >= len(angles):
        return []

    out: List[int] = []
    for raw in angles[start:]:
        code = map_raw_lane_value_to_direction_code(raw) & 0xFF
        if code == 0xFF or code == primary_code:
            continue
        if code not in out:
            out.append(code)
    out.sort()
    return out


def parse_angles(value: str) -> List[int]:
    value = value.strip()
    if not value:
        return []
    return [int(part.strip()) for part in value.split(",") if part.strip()]


def parse_log(path: Path) -> List[LaneGuidance]:
    events: List[LaneGuidance] = []
    current = None

    for line in path.read_text(errors="replace").splitlines():
        m = GUIDANCE_RE.search(line)
        if m:
            current = LaneGuidance(int(m.group(1)), int(m.group(2)), [])
            events.append(current)
            continue

        m = LANE_RE.search(line)
        if m and current is not None:
            current.lanes.append(
                Lane(
                    lane_index=int(m.group(1)),
                    pos=int(m.group(2)),
                    direction=int(m.group(3)),
                    status=int(m.group(4)),
                    angles=parse_angles(m.group(6)),
                )
            )

    return events


def iter_synthetic() -> Iterable[LaneGuidance]:
    yield LaneGuidance(
        9000,
        4,
        [
            Lane(0, 0, 1000, 2, [0, 45]),
            Lane(1, 1, -45, 2, [0, 45]),
            Lane(2, 2, 0, 1, [0, 45, 90]),
            Lane(3, 3, -90, 0, []),
        ],
    )


def validate(events: Iterable[LaneGuidance], verbose: bool) -> int:
    failures = 0
    warnings = 0
    event_count = 0
    lane_count = 0
    multi_count = 0

    for event in events:
        event_count += 1
        if len(event.lanes) != event.expected_count:
            print(
                f"WARN idx={event.index}: expected {event.expected_count} lanes, "
                f"parsed {len(event.lanes)} from decoded log"
            )
            warnings += 1

        for lane in event.lanes:
            lane_count += 1
            primary = map_lane_direction(lane.direction, lane.angles) & 0xFF
            sidestreets = map_lane_sidestreets(lane.direction, lane.angles, primary)

            if primary == 0xFF:
                print(f"FAIL idx={event.index} lane={lane.lane_index}: unmapped primary direction")
                failures += 1
            if len(sidestreets) > MAX_BAP_LANE_SIDESTREETS:
                print(
                    f"FAIL idx={event.index} lane={lane.lane_index}: "
                    f"{len(sidestreets)} sideStreets exceeds BAP max {MAX_BAP_LANE_SIDESTREETS}"
                )
                failures += 1

            is_multi = len(sidestreets) > 0
            if is_multi:
                multi_count += 1
            if verbose or is_multi:
                side = ",".join(f"0x{x:02x}" for x in sidestreets)
                print(
                    f"idx={event.index} lane={lane.lane_index} pos={lane.pos} "
                    f"dir={lane.direction} angles={lane.angles} -> "
                    f"laneDirection=0x{primary:02x} sideStreets=[{side}] gi={lane.status}"
                )

    print(
        f"validated events={event_count} lanes={lane_count} "
        f"multi_direction_lanes={multi_count} warnings={warnings} failures={failures}"
    )
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("logs", nargs="*", type=Path)
    parser.add_argument("--synthetic", action="store_true", help="include synthetic multi-direction cases")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    events: List[LaneGuidance] = []
    for log in args.logs:
        events.extend(parse_log(log))
    if args.synthetic:
        events.extend(iter_synthetic())

    if not events:
        parser.error("no lane guidance events found")
    return validate(events, args.verbose)


if __name__ == "__main__":
    raise SystemExit(main())
