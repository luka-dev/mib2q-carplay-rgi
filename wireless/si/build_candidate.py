#!/usr/bin/env python3
"""Build the wireless SI candidate from the pinned re-arm SI image.

Stage-1 graft: three entry trampolines into a separate R+X page.

  * MediaConnectorHandler_prepare  VA 0x1800B0
  * MediaConnectorHandler_cleanup  VA 0x17FC48
  * Job dispatch to listener       VA 0x128DBC

Each entry's first 12 bytes (3 ARM instructions, verified PC-relative-free and
branch-target-free) are replaced by `B <hook>`; the displaced instructions are
relocated into the cave. Default inplace variant retains the 6-entry PHDR
table and replaces its PT_ARM_EXIDX entry with the R+X hook PT_LOAD. Original content
addresses and the re-arm patch are retained. A separate RW root word is the
single published pointer to
si_adapter.so's slot table; zero means stock behaviour. The USB re-arm bytes
at 0x7FDB4 must be present in the input and survive the graft untouched.

Build tool only: refuses unknown input hashes, writes to build/, never touches
deploy/ or the unit.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_INPUT = REPO / "deploy/smartphone_integrator/smartphone_integrator"
DEFAULT_OUTDIR = REPO / "build/si-wireless-stage1"

INPUT_SHA256 = "606236a81c7c4dafda6fdd08fe6eb27c43252796784da25d9760a7d552ee2af3"
INPUT_SIZE = 802798
IMAGE_BASE = 0x100000
REARM_FILE_OFF = 0x7FDB4
REARM_NEW = bytes.fromhex("2e3309e0")

E_PHOFF_OFF = 0x1C
E_PHNUM_OFF = 0x2C
E_PHNUM = 7
PAGE = 0x1000
CAVE_VA = 0x1C6000
ROOT_VA = 0x1C5E00

# slot offsets inside struct si_adapter_table (si_protocol.h)
SLOT_PREPARE, SLOT_CLEANUP, SLOT_JOB = 12, 16, 20

HOOKS = {
    "prepare": {"va": 0x1800B0, "slot": SLOT_PREPARE, "slot_id": 1},
    "cleanup": {"va": 0x17FC48, "slot": SLOT_CLEANUP, "slot_id": 2},
    "job": {"va": 0x128DBC, "slot": SLOT_JOB, "slot_id": 3},
}

# exact displaced prologues (12 bytes each), read from the pinned image
EXPECTED_PROLOGUES = {
    0x1800B0: bytes.fromhex("f04f2de9286090e50050a0e1"),  # PUSH{R4-R11,LR}; LDR R6,[R0,#0x28]; MOV R5,R0
    0x17FC48: bytes.fromhex("f04f2de9286090e50040a0e1"),  # PUSH{R4-R11,LR}; LDR R6,[R0,#0x28]; MOV R4,R0
    0x128DBC: bytes.fromhex("10402de90040a0e1080090e5"),  # PUSH{R4,LR}; MOV R4,R0; LDR R0,[R0,#8]
    0x165390: bytes.fromhex("030052e370402de90240a0e1"),  # CMP R2,#3; PUSH{R4-R6,LR}; MOV R4,R2
    0x169518: bytes.fromhex("000051e304402de50100a013"),  # CMP R1,#0; PUSH{R4}; MOVNE R0,#1
}


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    p.add_argument("--outdir", type=Path, default=DEFAULT_OUTDIR)
    p.add_argument("--apply", action="store_true", help="build the candidate")
    p.add_argument("--stage2", action="store_true", help="ABI 2: original-argument routing and wired preemption")
    p.add_argument("--variant", default="inplace",
                   choices=["inplace", "none", "phnull", "load5", "small", "first", "phnum6", "extend_rw", "replace_phdr"],
                   help="inplace — production; остальные — loader-эксперименты (диагностика)")
    p.add_argument("--verify", type=Path, help="verify an existing candidate against the manifest")
    return p.parse_args()


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# ---------------------------------------------------------------- ARM encoder
def enc_b(addr: int, target: int, cond: int = 0xE) -> int:
    off = (target - (addr + 8)) >> 2
    assert -(1 << 23) <= off < (1 << 23), "branch out of range"
    return (cond << 28) | (0xA << 24) | (off & 0xFFFFFF)


def enc_beq(addr: int, target: int) -> int:
    return enc_b(addr, target, cond=0x0)


def enc_ldr_pc(rt: int, addr: int, lit_addr: int) -> int:
    off = lit_addr - (addr + 8)
    assert 0 <= off < 0x1000, "literal must follow the load within 4 KiB"
    return 0xE59F0000 | (rt << 12) | off


def enc_ldr_imm(rt: int, rn: int, imm12: int) -> int:
    assert 0 <= imm12 < 0x1000
    return 0xE5900000 | (rn << 16) | (rt << 12) | imm12


def enc_cmp_imm(rn: int, imm: int) -> int:
    assert 0 <= imm <= 255
    return 0xE3500000 | (rn << 16) | imm


def enc_mov_imm(rd: int, imm: int) -> int:
    assert 0 <= imm <= 255
    return 0xE3A00000 | (rd << 12) | imm


def enc_stmfd(mask: int) -> int:
    return 0xE92D0000 | mask


def enc_ldmfd(mask: int) -> int:
    return 0xE8BD0000 | mask


def enc_blx_rm(rm: int) -> int:
    return 0xE12FFF30 | rm


def enc_bx_rm(rm: int) -> int:
    return 0xE12FFF10 | rm


MASK_R0R3_R12_LR = 0xF | (1 << 12) | (1 << 14)          # R0-R3, R12, LR
MASK_R1R3_R12_LR = 0xE | (1 << 12) | (1 << 14)          # R1-R3, R12, LR


def words(*ws: int) -> bytes:
    return b"".join(struct.pack("<I", w & 0xFFFFFFFF) for w in ws)


def build_hook_notify(slot_off: int, slot_id: int, entry_va: int, hook_va: int, displaced: bytes) -> bytes:
    """Notify-only trampoline: handler(slot_id), verdict ignored in stage 1,
    then the displaced prologue and a jump back to entry+12."""
    ins: list[int] = []

    def add(w: int) -> None:
        ins.append(w)

    add(enc_stmfd(MASK_R0R3_R12_LR))
    ldr_idx = len(ins); add(0)
    add(enc_ldr_imm(12, 12, 0))                 # dereference root word -> table
    add(enc_cmp_imm(12, 0))
    beq1 = len(ins); add(0)
    add(enc_ldr_imm(12, 12, slot_off))
    add(enc_cmp_imm(12, 0))
    beq2 = len(ins); add(0)
    add(enc_mov_imm(0, slot_id))
    add(enc_blx_rm(12))
    end_idx = len(ins); add(enc_ldmfd(MASK_R0R3_R12_LR))
    for w in struct.unpack_from("<3I", displaced, 0):
        add(w)
    add(enc_b(hook_va + len(ins) * 4, entry_va + 12))
    lit_idx = len(ins); add(ROOT_VA)

    lit_addr = hook_va + lit_idx * 4
    ins[ldr_idx] = enc_ldr_pc(12, hook_va + ldr_idx * 4, lit_addr)
    ins[beq1] = enc_beq(hook_va + beq1 * 4, hook_va + end_idx * 4)
    ins[beq2] = enc_beq(hook_va + beq2 * 4, hook_va + end_idx * 4)
    return words(*ins)


def build_hook_job(slot_off: int, entry_va: int, hook_va: int, displaced: bytes,
                   consumed_result: int = 0) -> bytes:
    """Job-dispatch trampoline: r0 = job. handler(job) non-zero consumes the
    job (dispatcher returns 0), zero falls through to the original dispatch."""
    ins: list[int] = []

    def add(w: int) -> None:
        ins.append(w)

    add(enc_stmfd(MASK_R0R3_R12_LR))            # r0 = job kept for the handler
    ldr_idx = len(ins); add(0)
    add(enc_ldr_imm(12, 12, 0))                 # dereference root word -> table
    add(enc_cmp_imm(12, 0))
    beq1 = len(ins); add(0)
    add(enc_ldr_imm(12, 12, slot_off))
    add(enc_cmp_imm(12, 0))
    beq2 = len(ins); add(0)
    add(enc_blx_rm(12))                          # r0 = handler(job)
    add(enc_cmp_imm(0, 0))
    beq3 = len(ins); add(0)
    consumed = len(ins)
    add(enc_ldmfd(MASK_R0R3_R12_LR))             # pop ALL six saved words, including LR
    add(enc_mov_imm(0, consumed_result))          # predicates return true; void/job calls zero
    add(enc_bx_rm(14))
    orig_idx = len(ins)
    add(enc_ldmfd(MASK_R0R3_R12_LR))
    for w in struct.unpack_from("<3I", displaced, 0):
        add(w)
    add(enc_b(hook_va + len(ins) * 4, entry_va + 12))
    lit_idx = len(ins); add(ROOT_VA)

    lit_addr = hook_va + lit_idx * 4
    ins[ldr_idx] = enc_ldr_pc(12, hook_va + ldr_idx * 4, lit_addr)
    ins[beq1] = enc_beq(hook_va + beq1 * 4, hook_va + orig_idx * 4)
    ins[beq2] = enc_beq(hook_va + beq2 * 4, hook_va + orig_idx * 4)
    ins[beq3] = enc_beq(hook_va + beq3 * 4, hook_va + orig_idx * 4)
    assert consumed == 11
    return words(*ins)


def check_prologue(name: str, h: dict, raw: bytes) -> bytes:
    off = h["va"] - IMAGE_BASE
    prologue = raw[off:off + 12]
    expected = EXPECTED_PROLOGUES[h["va"]]
    if prologue != expected:
        raise AssertionError(f"{name}: prologue drifted: {prologue.hex()} != {expected.hex()}")
    for i in range(3):
        w = struct.unpack_from("<I", prologue, i * 4)[0]
        if (w >> 24) in (0xEA, 0xEB, 0xFA, 0xFB, 0x0A, 0x0B):
            raise AssertionError(f"{name}: branch inside displaced prologue")
        if (w & 0x0F7F0000) in (0x051F0000, 0x059F0000):
            raise AssertionError(f"{name}: pc-relative load in displaced prologue")
    return prologue


def verify_mapping(out: bytes, orig: bytes, phdr_table_off: int, cave_off: int, variant: str = "load5") -> None:
    """Parse the output like a loader and compare sample mappings to the input."""
    phnum = struct.unpack_from("<H", out, E_PHNUM_OFF)[0]
    phdrs = [struct.unpack_from("<8I", out, phdr_table_off + i * 0x20) for i in range(phnum)]
    loads = [p for p in phdrs if p[0] == 1]
    hook_seg = next(p for p in loads if p[2] == CAVE_VA)
    checks = [(0x100034, 0x34), (0x1000F4, 0xF4), (0x114888, 0x14888),
              (0x115198, 0x15198), (0x128DBC, 0x28DBC), (0x1800B0, 0x800B0),
              (0x17FC48, 0x7FC48), (0x1A2B20, 0xA2B20), (0x1BDB28, 0xBCB28),
              (0x1C21D0, 0xC11D0), (0x1C3134, 0xC2134)]
    for va, fo in checks:
        seg = next((p for p in phdrs if p[0] == 1 and p[1] <= fo < p[1] + p[4]), None)
        assert seg, f"no PT_LOAD covers file offset {fo:#x}"
        assert fo - seg[1] + seg[2] == va, f"VA drift at {va:#x}"
    # every content byte identical except the 36 hook bytes and the two
    # header fixups (e_phoff 4 bytes at 0x1C, e_phnum 2 bytes at 0x2C)
    allowed = set()
    for h in HOOKS.values():
        allowed.update(range(h["va"] - IMAGE_BASE, h["va"] - IMAGE_BASE + 12))
    allowed.update(range(0x1C, 0x20))
    allowed.update(range(0x2C, 0x2E))
    if variant == "inplace":
        allowed.update(range(0x34, 0x54))
        allowed.update(range(0xC8, 0xCC))  # RW p_memsz explicitly owns root
    diff = [i for i in range(len(orig)) if orig[i] != out[i]]
    if not all(i in allowed for i in diff):
        bad = [i for i in diff if i not in allowed]
        raise AssertionError(f"unexpected diffs at {['%#x:%#x->%#x' % (i, orig[i], out[i]) for i in bad[:8]]}")


def check_rearm(out: bytes) -> None:
    assert out[REARM_FILE_OFF:REARM_FILE_OFF + 4] == REARM_NEW, "re-arm bytes lost"


def apply(args: argparse.Namespace) -> int:
    raw = args.input.read_bytes()
    if len(raw) != INPUT_SIZE or sha256(raw) != INPUT_SHA256:
        print(f"refusing input {args.input}: sha256/size mismatch", file=sys.stderr)
        return 1
    if raw[REARM_FILE_OFF:REARM_FILE_OFF + 4] != REARM_NEW:
        print("refusing: re-arm bytes missing at 0x7FDB4", file=sys.stderr)
        return 1

    (e_phoff, e_shoff) = struct.unpack_from("<II", raw, 0x1C)
    (e_phentsize,) = struct.unpack_from("<H", raw, 0x2A)
    (e_phnum,) = struct.unpack_from("<H", raw, 0x2C)
    assert (e_phoff, e_phnum, e_phentsize) == (0x34, 6, 0x20), "unexpected header layout"

    phdrs = [struct.unpack_from("<8I", raw, e_phoff + i * e_phentsize) for i in range(e_phnum)]
    rw_load = phdrs[4]
    assert rw_load[1] == 0xBCA24 and rw_load[2] == 0x1BDA24, "unexpected RW segment layout"
    assert rw_load[2] + rw_load[5] <= CAVE_VA, "cave VA collides with the image"

    displaced = {h["va"]: check_prologue(name, h, raw) for name, h in HOOKS.items()}
    variant = args.variant

    # --- cave page ---
    cave = bytearray(PAGE)
    struct.pack_into("<I", cave, 0x00, 0x58484953)   # 'SIHX' little-endian
    abi = 2 if args.stage2 else 1
    struct.pack_into("<I", cave, 0x04, abi)
    # ROOT lives in the .bss page tail (0x1C5E00): the hook page is R+X since
    # the adapter must WRITE the published pointer at runtime.
    root_va = ROOT_VA

    cursor = 0x40
    hook_vas: dict[str, int] = {}
    for name, h in HOOKS.items():
        if name == "job" or args.stage2:
            code = build_hook_job(h["slot"], h["va"], CAVE_VA + cursor,
                                  displaced[h["va"]], int(name == "carplay_device"))
        else:
            code = build_hook_notify(h["slot"], h["slot_id"], h["va"], CAVE_VA + cursor, displaced[h["va"]])
        cave[cursor:cursor + len(code)] = code
        hook_vas[name] = CAVE_VA + cursor
        cursor = (cursor + len(code) + 15) & ~15
    assert cursor <= PAGE, "cave overflow"

    # --- output assembly ---
    out = bytearray(raw)
    for name, h in HOOKS.items():
        off = h["va"] - IMAGE_BASE
        b = enc_b(h["va"], hook_vas[name])
        out[off:off + 12] = words(b, b, b)   # entries +4/+8 also trap into the hook

    tab_off = (len(raw) + 15) & ~15
    if variant == "inplace":
        tab_off = 0x34            # таблица на месте; cave по фиксированному офсету
        cave_off = CAVE_VA - IMAGE_BASE
    else:
        cave_off = (tab_off + E_PHNUM * 0x20 + PAGE - 1) & ~(PAGE - 1)

    def ph(p_type: int, p_off: int, p_vaddr: int, p_filesz: int, p_memsz: int, p_flags: int, p_align: int) -> bytes:
        return struct.pack("<8I", p_type, p_off, p_vaddr, p_vaddr, p_filesz, p_memsz, p_flags, p_align)

    out += bytes(tab_off - len(raw)) if variant != "inplace" else bytearray()
    # the hook PT_LOAD goes between the two original LOADs and DYNAMIC:
    # procnto 6.5 maps PT_LOADs in phdr order and stops scanning at DYNAMIC.
    # The segment must be fully covered by the FILE: pad to cave_off + PAGE
    # (procnto rejects a map that extends past EOF, unlike Linux zero-fill).
    if variant == "inplace":
        # таблица phdr остаётся на 0x34 (e_phoff/e_phnum не меняются); нулевая
        # запись (PT_ARM_EXIDX) заменяется на хук-сегмент: файл и память консистентны
        reordered = [(1, cave_off, CAVE_VA, CAVE_VA, PAGE, PAGE, 5, PAGE)] + list(phdrs[1:])
        tab_off = e_phoff
        assert tab_off == 0x34
    elif variant == "none":
        reordered = list(phdrs)
    elif variant == "phnull":
        reordered = list(phdrs[:5]) + [(0, cave_off, 0, 0, 0, 0, 0, 0), phdrs[5]]
    elif variant == "small":
        reordered = list(phdrs[:5]) + [(1, cave_off, CAVE_VA, 0x30, 0x30, 5, 4), phdrs[5]]
    elif variant == "phnum6":
        # таблица остаётся 6-элементной: хук-сегмент ЗАМЕНЯЕТ PT_ARM_EXIDX (первую запись)
        reordered = [(1, cave_off, CAVE_VA, CAVE_VA, PAGE, PAGE, 5, PAGE)] + list(phdrs[1:])
    elif variant == "replace_phdr":
        # PT_ARM_EXIDX сохранён; хук-сегмент замещает PT_PHDR
        reordered = [phdrs[0], (1, cave_off, CAVE_VA, CAVE_VA, PAGE, PAGE, 5, PAGE)] + list(phdrs[2:])
    elif variant == "extend_rw":
        # вместо нового сегмента: расширяем RW LOAD (файл дополнен до кавы)
        rw = phdrs[4]
        new_filesz = cave_off + PAGE - rw[1]
        ext = (1, rw[1], rw[2], rw[3], new_filesz, new_filesz + 0x100, 6, PAGE)
        reordered = list(phdrs[:4]) + [ext] + list(phdrs[5:])
    elif variant == "first":
        reordered = [(1, cave_off, CAVE_VA, CAVE_VA, PAGE, PAGE, 5, PAGE)] + list(phdrs)
    else:  # load5
        reordered = list(phdrs[:5]) + [(1, cave_off, CAVE_VA, CAVE_VA, PAGE, PAGE, 5, PAGE), phdrs[5]]
    e_phnum_out = len(reordered)
    if variant == "inplace":
        # only phdr entry 0 (PT_ARM_EXIDX) is rewritten in place; entries 1..5 keep
        # their original bytes (ph() would normalise their p_paddr fields)
        hook_entry = ph(1, cave_off, CAVE_VA, PAGE, PAGE, 5, PAGE)
        out[tab_off:tab_off + 0x20] = hook_entry
        # Reserve the root as actual zero-filled BSS, not unowned page
        # padding which a loader/heap implementation might reuse.
        assert rw_load[2] + rw_load[5] <= root_va < CAVE_VA - 4
        struct.pack_into("<I", out, tab_off + 4 * 32 + 20,
                         root_va + 4 - rw_load[2])
        out += bytes(cave_off - len(out))
        out += cave
        assert len(out) == cave_off + PAGE, "file must end exactly at the hook segment end"
    else:
        out += table_bytes
        out += bytes(cave_off - (tab_off + e_phnum_out * 0x20))
        out += cave
        assert len(out) == cave_off + PAGE, "file must end exactly at the hook segment end"

    if variant != "inplace":
        struct.pack_into("<I", out, E_PHOFF_OFF, tab_off)
        struct.pack_into("<H", out, E_PHNUM_OFF, e_phnum_out)

    verify_mapping(bytes(out), raw, tab_off, cave_off, variant)
    check_rearm(bytes(out))

    outdir = args.outdir
    outdir.mkdir(parents=True, exist_ok=True)
    cand = outdir / "smartphone_integrator.si_wireless_stage1"
    cand.write_bytes(out)
    manifest = {
        "tool": "wireless/si/build_candidate.py --apply (stage 1)",
        "variant": variant,
        "date": "2026-10-19",
        "input": {"path": str(args.input), "sha256": INPUT_SHA256, "size": INPUT_SIZE},
        "output": {"path": str(cand), "sha256": sha256(out), "size": len(out)},
        "rearm_bytes": {"file_offset": REARM_FILE_OFF, "va": REARM_FILE_OFF + IMAGE_BASE,
                        "value": REARM_NEW.hex()},
        "hooks": {
            name: {
                "va": h["va"], "file_offset_orig": h["va"] - IMAGE_BASE,
                "hook_va": hook_vas[name], "slot_offset": h["slot"], "slot_id": h["slot_id"],
                "displaced": displaced[h["va"]].hex(),
                "patched_bytes": bytes(out[h["va"] - IMAGE_BASE:h["va"] - IMAGE_BASE + 12]).hex(),
                "returns_to_va": h["va"] + 12,
            }
            for name, h in HOOKS.items()
        },
        "cave": {"va": CAVE_VA, "file_offset": cave_off, "size": PAGE,
                 "root_va": root_va, "sig_offset": 0x00, "sig": "SIHX",
                 "abi_version_offset": 0x04, "abi_version": abi,
                 "used_bytes": cursor},
        "root_storage": {"va": root_va, "size": 4, "rw_phdr_index": 4,
                         "original_memsz": rw_load[5],
                         "reserved_memsz": root_va + 4 - rw_load[2]},
        "elf": {"e_phoff": tab_off, "e_phnum": e_phnum_out,
                "layout": ("phdr entry 0 (PT_ARM_EXIDX) replaced by the R+X hook LOAD in place at 0x34"
                           if variant == "inplace" else
                           "6 original phdrs + 1 RWX PT_LOAD; original content bytes untouched")},
        "slot_table": {
            "owner": "si_adapter.so", "abi_version": abi,
            "offsets": {"magic": 0, "abi_version": 4, "table_size": 8,
                        "prepare": 12, "cleanup": 16, "job": 20,
                        **({"connect": 24, "carplay_device": 28} if args.stage2 else {})},
            "verdicts": {"prepare_cleanup": ("non-zero=consumed void call, 0=original; original r0/r1/r2 arguments"
                                            if args.stage2 else "stage1 notification only; verdict ignored; original path"),
                         "job": "non-zero=consumed(dispatcher returns 0), 0=original"},
        },
        "absolute_addresses": {"job_alloc": 0x1293D8, "job_enqueue": 0x13F4F4,
                               "controller_ao_id": "1, native queue verified on HU 2026-10-20"},
    }
    (outdir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    # Runtime identity checks use safe /proc/<pid>/as reads, never signal probes.
    # Pin the executable header/PHDRs, every modified entry, receiver queue
    # primitives, re-arm, and the complete immutable hook page to THIS build.
    ranges = [(IMAGE_BASE, bytes(out[:0x34])),
              (IMAGE_BASE + tab_off, bytes(out[tab_off:tab_off + e_phnum_out * 32])),
              (IMAGE_BASE + REARM_FILE_OFF, REARM_NEW),
              (CAVE_VA, bytes(cave))]
    for h in HOOKS.values():
        off = h["va"] - IMAGE_BASE
        ranges.append((h["va"], bytes(out[off:off + 12])))
    for va in (0x1293D8, 0x13F4F4, 0x128978, 0x180944, 0x16635C, 0x180704,
               0x129050, 0x1BF248, 0x13F2B4, 0x13F32C):
        # RW data has a different VA/file delta than text. In particular the
        # NOOP vtable is at VA 0x1bf248, file offset 0xbe248.
        off = next(p[1] + va - p[2] for p in reordered
                   if p[0] == 1 and p[2] <= va and va + 32 <= p[2] + p[4])
        ranges.append((va, bytes(out[off:off + 32])))
    if args.stage2:
        for va in (0x16EEF4, 0x16EC90, 0x169984, 0x169850, 0x1698D0, 0x183678,
                   0x165198, 0x17F2B8, 0x12584C, 0x165390):
            off = next(p[1] + va - p[2] for p in reordered
                       if p[0] == 1 and p[2] <= va and va + 32 <= p[2] + p[4])
            ranges.append((va, bytes(out[off:off + 32])))
    header = ["/* Generated by build_candidate.py; do not edit. */",
              '#define SI_GUARD_CANDIDATE_SHA256 "' + sha256(out) + '"']
    for i, (_, data) in enumerate(ranges):
        header.append(f"static const unsigned char si_guard_bytes_{i}[] = {{" +
                      ",".join(f"0x{v:02x}" for v in data) + "};")
    header.append("static const struct si_guard_range si_guard_ranges[] = {")
    header.extend(f"    {{0x{va:x}u, si_guard_bytes_{i}, sizeof(si_guard_bytes_{i})}},"
                  for i, (va, _) in enumerate(ranges))
    header.append("};\n")
    (outdir / "si_candidate_guard.h").write_text("\n".join(header))
    print(f"candidate: {cand}")
    print(f"sha256: {sha256(out)}")
    print(f"cave root VA: {root_va:#x}")
    print("hooks: " + ", ".join(f"{k}={v:#x}" for k, v in hook_vas.items()))
    return 0


def verify(args: argparse.Namespace) -> int:
    cand_path: Path = args.verify
    cand = cand_path.read_bytes()
    man = json.loads((cand_path.parent / "manifest.json").read_text())
    assert sha256(cand) == man["output"]["sha256"], "candidate hash mismatch"
    assert sha256(args.input.read_bytes()) == man["input"]["sha256"], "input hash mismatch"
    verify_mapping(cand, args.input.read_bytes(), man["elf"]["e_phoff"], man["cave"]["file_offset"], man.get("variant", "load5"))
    check_rearm(cand)
    sig = struct.unpack_from("<I", cand, man["cave"]["file_offset"])[0]
    assert sig == 0x58484953, "cave signature missing"
    print("candidate verified: mapping, re-arm, cave signature and manifest agree")
    return 0


def main() -> int:
    args = parse_args()
    if args.stage2:
        HOOKS["connect"] = {"va": 0x165390, "slot": 24, "slot_id": 4}
        HOOKS["carplay_device"] = {"va": 0x169518, "slot": 28, "slot_id": 5}
    if args.verify:
        return verify(args)
    if not args.apply:
        print("nothing to do: pass --apply or --verify PATH", file=sys.stderr)
        return 2
    return apply(args)


if __name__ == "__main__":
    sys.exit(main())
