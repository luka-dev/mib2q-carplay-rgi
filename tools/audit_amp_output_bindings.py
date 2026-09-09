#!/usr/bin/env python3
"""Read-only J525 image audit: V850 parameter routing and SHARC graph records.

No firmware patching or acoustic simulation. Three VISA handlers validate the
selector, split-half mixer and packed output EQ. Physical labels remain inferred.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from extract_amp_sharc_app import (
    SOURCE_SHA256, SOURCE_SIZE, SHARC_OFFSET, SHARC_END,
    bit_reverse_bytes, validate,
)
from inspect_amp_visa import load_shortwords, decode


def hmi_3d_mode_to_hu_level(mode: int) -> int:
    """MU1316 DSISound enum -> native SoundSettings 0x22 value.

    Java ThreeDMode maps UI indices to 1/2/4/8. MIBRoot 0x1E2CD0
    finds their index in [0,1,2,4,8,...]; 0x1EB1F0 subtracts one.
    This does not by itself validate the RCC/MOST serialization to J525.
    """
    return (1, 2, 4, 8).index(mode)


def complex_process_mask(control_flags: int, sideband_flags: int) -> int:
    """Instruction-level R13 selection at VISA 0x127021..0x12704E.

    Precondition: descriptor[40]&1, descriptor[5]&1 (initialized/enabled).
    The meanings of these modes must not be equated with the MMI 3D enum.
    """
    mask = 2
    if control_flags & 1:
        mask = 1
    if control_flags & 2:
        mask = 0xF4BA
    if control_flags & 4:
        mask = 0xDFF75E5 if sideband_flags & 1 else 0xFFF7FE5
    return mask


def processing_row(level: int, processing_enabled: bool, b01_valid: bool) -> int:
    """Steady target row in V850 0xAA648; not a claim about the MMI UI enum."""
    if level not in range(4):
        raise ValueError('supported control levels are 0..3')
    return level + 1 if processing_enabled and b01_valid else 0


def gain_from_tenths_db(data: bytes, value: int) -> float:
    if not -1000 <= value <= 200:
        raise ValueError('outside audited gain range')
    return struct.unpack_from('<f', data, 0x69399 + ~(value << 2))[0]


def output_eq_layout(counts: list[int], capacity: int = 144) -> list[dict]:
    """VISA 0x1284D6: packed coefficients, TCB batches of <=12 biquads.

    A zero-count channel uses one separate identity stage without advancing
    the packed coefficient cursor. Its TCB still contributes to the DMA chain.
    """
    if any(n < 0 for n in counts) or sum(counts) > capacity:
        raise ValueError('invalid or excessive EQ count')
    start = 0
    result = []
    for n in counts:
        full, remainder = divmod(n, 12)
        result.append({'start_record': start, 'count': n,
                       'coefficient_word': hex(0xC2AB0 + 5 * start),
                       'accelerator_word': hex(0xC2D80 + 14 * start),
                       'tcb_stage_counts': [12] * full + ([remainder] if remainder else []) if n else [1],
                       'separate_identity_stage': n == 0})
        start += n
    return result


def words(data: bytes, address: int, count: int) -> list[int]:
    return list(struct.unpack_from(f"<{count}I", data, address))


def loader_dm(decoded: bytes, blocks: list) -> dict[int, int]:
    """Only explicit 32-bit writes/zero fills, not cross-width memory aliases."""
    result: dict[int, int] = {}
    for block in blocks:
        if block.tag == 1:
            result.update((block.address + i, 0) for i in range(block.count))
        elif block.tag == 4:
            for i, value in enumerate(words(decoded, block.offset + 12, block.count)):
                result[block.address + i] = value
    return result


def source_offset(ram_relative: int, profile: int = 0) -> int:
    # Copy loops: 0xC23C0 (profile/global), 0xB64E0 (fixed calibration).
    regions = [
        (-0x13900, -0x13590, 0x100034 + profile * 0x370),
        (-0x13590, -0x1330C, 0x1021B4),
        (-0x1330C, -0x1250C, 0x102438),
    ]
    for start, end, file_start in regions:
        if start <= ram_relative < end:
            return file_start + ram_relative - start
    raise ValueError(f"unmapped RAM field {ram_relative:#x}")


def frequency_lookup(data: bytes, selector: int) -> dict:
    address = (0x6BCE4 + selector * 4 if selector < 1000 else
               0x6CC84 + ((selector - 1000) // 10) * 4)
    value = struct.unpack_from("<f", data, address)[0]
    # Numerical interpretation, checked against multiple known selector values.
    hz = math.atan2(1.0, value) * 48000.0 / math.pi
    return {"lookup_file_offset": hex(address), "cot_value": value,
            "frequency_hz_from_cot": hz}


def filter_record(data: bytes, index: int, file_base: int | None = None,
                  dsp_base: int = 0xC2AB0) -> dict:
    address = (source_offset(-0x132FC) if file_base is None else file_base) + index * 8
    frequency, gain, q, kind, reserved = struct.unpack_from("<HhBBH", data, address)
    result = {"index": index, "file_offset": hex(address),
              "sharc_coefficient_address": hex(dsp_base + index * 5),
              "frequency_selector": frequency, "gain_tenths_db": gain,
              "q_code": q, "type_code": kind, "reserved": reserved}
    if kind:
        result.update(frequency_lookup(data, frequency))
    # Type families follow the dispatch in 0xA96C8 and numerator generators.
    result["family"] = ("bypass" if kind == 0 else "peak" if kind == 1 else
                        "lowpass" if 2 <= kind <= 13 else
                        "highpass" if 14 <= kind <= 25 else "other")
    return result


# Candidate physical assignments: unique signal role + output passband + Audi
# nineteen-driver inventory. NOT proof of amplifier channel-ID-to-T38 wiring.
CANDIDATES = [
    (["R20", "R103"], [11, 12], "front-left shared mid/treble"),
    (["R22", "R104"], [13, 14], "front-right shared mid/treble"),
    (["R221"], [24, 32], "upper A-pillar right"),
    (["R220"], [23, 31], "upper A-pillar left"),
    (["R276"], [25, 33], "dashboard side left"),
    (["R277"], [26, 34], "dashboard side right"),
    (["R210"], [28, 36], "right effect"),
    (["R209"], [27, 35], "left effect"),
    (["R14", "R15"], [4, 5], "rear-left door shared treble/bass"),
    (["R16", "R17"], [16, 17], "rear-right door shared treble/bass"),
    (["R208"], [6, 18], "center midrange"),
    (["R219"], [3, 15], "center tweeter"),
    (["R21"], [7, 8], "front-left bass"),
    (["R23"], [9, 10], "front-right bass"),
    (["R211"], None, "subwoofer winding, T38 pair order unresolved"),
    (["R211"], None, "subwoofer winding, T38 pair order unresolved"),
]


def audit(data: bytes, profile: int = 0) -> dict:
    if len(data) != SOURCE_SIZE or hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("wrong CodeFlash size or SHA-256")
    if profile not in (0, 1):
        raise ValueError("audit only covers selectable A07 profiles 0 and 1")
    raw = data[SHARC_OFFSET:SHARC_END]
    decoded = bit_reverse_bytes(raw)
    blocks = validate(raw, decoded)
    dm = loader_dm(decoded, blocks)
    visa = load_shortwords(decoded)
    for address, expected in {
        0x126977: 'I4=DM(M3,I1)', 0x126979: 'I5=DM(M4,I1)',
        0x12893F: 'IF LT F1=F1-F1 || M4=M5',
        0x128943: 'IF GE F1=F1-F1 || M4=M5',
        0x128577: 'R2=PASS R6 || R13=DM(+18,I1)',
        0x1285B8: 'R13=R13+R9 || DM(I0,M6)=M14',
        0x1285E6: 'R13=R13+R8 || DM(I0,M6)=R12',
    }.items():
        if decode(visa, address)[1] != expected:
            raise ValueError(f'VISA semantic anchor changed at {address:#x}')
    perm = list(data[0x68B40:0x68B50])
    inverse = list(data[0x68B30:0x68B40])
    if sorted(perm) != list(range(16)) or any(inverse[perm[i]] != i for i in range(16)):
        raise ValueError("channel permutations do not invert")
    split_address = source_offset(-0x12997)
    split = [list(data[split_address + mode * 16:split_address + (mode + 1) * 16])
             for mode in range(2)]
    route_address = source_offset(-0x12977)
    route = list(data[route_address:route_address + 16])
    counts = list(data[0x102438:0x102448])
    records = [filter_record(data, i) for i in range(144)]
    count_starts = [sum(counts[:i]) for i in range(16)]
    eq_layout = output_eq_layout(counts)
    delay_address = source_offset(-0x12E7C)
    delay = list(struct.unpack_from("<16H", data, delay_address))
    gain_address = source_offset(-0x12967)
    gains = list(struct.unpack_from("<16b", data, gain_address))
    channels = []
    for channel, slot in enumerate(perm):
        candidate_ids, candidate_pins, candidate_name = CANDIDATES[channel]
        start = count_starts[slot]
        channels.append({
            "v850_config_index": channel, "sharc_processing_slot": slot,
            "normal_route_selector": route[channel],
            "ten_bus_source_modes": [mode[route[channel]] for mode in split],
            "route_parameter_word": hex(0xC2114 + slot),
            "delay_parameter_word": hex(0xE4674 + slot),
            "delay_raw_samples_candidate": delay[channel],
            "gain_parameter_word": hex(0xC393A + slot),
            "gain_db": gains[channel],
            "eq_count": counts[slot],
            "packed_eq_start": start,
            "packed_eq_records": records[start:start + counts[slot]],
            # Legacy keys retained for earlier report consumers. The output
            # grouping is now validated; input-bus grouping is still inferred.
            "packed_eq_start_inferred": start,
            "packed_eq_records_inferred": records[start:start + counts[slot]],
            "physical_candidate": {"status": "inference_not_direct_pinout_proof",
                "speaker_ids": candidate_ids, "t38_pins": candidate_pins,
                "description": candidate_name},
        })
    # Explicit schedule in loader, terminated by a zero word.
    nodes = []
    cursor = 0xC5794
    while dm[cursor] != 0:
        address = dm[cursor]
        class_address, ports_address, port_count = [dm[address + i] for i in range(3)]
        if dm[address + 3] != 0xC4F92:
            raise ValueError("unexpected graph descriptor context")
        nodes.append({"schedule_word": hex(cursor), "descriptor": hex(address),
                      "class_descriptor": hex(class_address),
                      "process_visa_address_candidate": hex(dm[class_address + 1]),
                      "port_ids": [dm[ports_address + i] for i in range(port_count)]})
        cursor += 1
        if len(nodes) > 128:
            raise ValueError("unterminated graph schedule")
    rows_address = source_offset(-0x135F8, profile)
    input_count_address = source_offset(-0x13900, profile)
    input_counts = list(data[input_count_address:input_count_address + 8])
    input_records = [filter_record(data, i, source_offset(-0x138F8, profile), 0xE0264)
                     for i in range(sum(input_counts))]
    return {
        "source_sha256": SOURCE_SHA256, "profile": profile,
        "scope": "static image, not live amplifier readback",
        "v850_config_to_sharc_slot": perm, "sharc_slot_to_v850_config": inverse,
        "split_modes": split, "split_file_offset": hex(split_address),
        "normal_route": route, "route_file_offset": hex(route_address),
        "output_eq_capacity": 144, "output_eq_counts": counts,
        "output_eq_count_sum": sum(counts),
        "eq_grouping_status": "output packed grouping validated in VISA 0x1284d6 and coefficient converter 0x125d17; not hardware-executed",
        "output_eq_layout": eq_layout,
        "channels": channels,
        "input_bus_eq_counts": input_counts,
        "input_bus_eq_packed_inferred": [input_records[sum(input_counts[:i]):sum(input_counts[:i + 1])]
                                         for i in range(8)],
        "ten_bus_pair_gain_rows_db": [list(struct.unpack_from("<20b", data, rows_address + 20 * i)) for i in range(5)],
        "pair_gain_row_file_offset": hex(rows_address),
        "pair_mixer_layout": "out[j] = in[j]*gain[2*j] + in[N+j]*gain[2*j+1]; inputs split halves, gains interleaved",
        "level_parameter_values": list(struct.unpack_from('<4f', data, 0x6EA68)),
        "enabled_level_target_rows": [processing_row(i, True, True) for i in range(4)],
        "disabled_or_invalid_b01_target_row": processing_row(0, False, True),
        "gain_lookup_minus100_db": gain_from_tenths_db(data, -1000),
        "gain_lookup_minus99_db": gain_from_tenths_db(data, -990),
        "pair_gain_row_state_warning": "level 0 with processing enabled and B01 valid selects row 1, not bypass row 0. MU1316 Java/native code verifies UI OFF=DSI 1 -> HU SoundSettings 0x22 value 0; the RCC/MOST link to J525 field 4 and live gates remain unverified. ASAD=0xC0 applies an odd-gain surround correction and clamp; transitions are not steady targets.",
        "b01_copy": {"writer": "0xacfb4", "source": "0x12000c", "destination": "0xc3c6a", "words": words(data, 0x120008, 1)[0]},
        "graph_status": "loader schedule recovered; 0x128910 selector, 0x12694c split-half mixer, 0x1284d6 packed output EQ validated statically; other handlers not fully decoded",
        "graph_schedule_count": len(nodes), "graph_nodes": nodes,
        "not_recovered": ["direct SHARC/hardware output to T38 binding", "complete VISA DSP semantics",
                          "3D-off ten-bus transfer function", "speaker/cabin acoustic response"],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--profile", type=int, default=0)
    parser.add_argument("--output", type=Path, help="derived JSON report; never a firmware output")
    args = parser.parse_args()
    report = audit(args.image.read_bytes(), args.profile)
    text = json.dumps(report, indent=2) + "\n"
    if args.output:
        if args.output.resolve() == args.image.resolve():
            raise SystemExit("refusing to overwrite source firmware")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
        print(f"{args.output}: {report['graph_schedule_count']} graph nodes; "
              f"{report['output_eq_count_sum']} counted EQ stages; 16 channel mappings")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
