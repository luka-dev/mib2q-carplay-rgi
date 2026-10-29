#!/usr/bin/env python3
"""Stage-1 host tests for the SI wireless graft (wireless/si/).

Covers, in order:
  1. candidate build from the pinned re-arm SI (build_candidate.py --apply)
  2. candidate self-verification (--verify) and negative hash rejection
  3. content diff limited to the 36 hook bytes + 2 header fixups
  4. trampoline disassembly (capstone when available) matches the design:
     args saved, root load, slot dispatch, displaced prologue, return to entry+12
  5. cave signature/root layout, inplace PHDR and R+X hook segment
  6. actual ARM execution with original prologue as oracle
  7. native C image/readiness guard, envv and malformed inputs (host cc)

Run from the repository root:  python3 wireless/si/test_si_stage1.py
"""
from __future__ import annotations

import json
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SI = REPO / "wireless/si"
sys.path.insert(0, str(SI))
import build_candidate as bc  # noqa: E402
from test_si_trampolines import run_arm_tests
from test_si_runtime_guard import run_guard_tests

INPUT = REPO / "deploy/smartphone_integrator/smartphone_integrator"
FAILURES: list[str] = []


def check(name: str, cond: bool, detail: str = "") -> None:
    status = "PASS" if cond else "FAIL"
    print(f"[{status}] {name}" + (f" — {detail}" if detail and not cond else ""))
    if not cond:
        FAILURES.append(name)


def run_candidate_cycle(workdir: Path) -> tuple[Path, bytes, bytes, dict]:
    raw = INPUT.read_bytes()
    out_dir = workdir / "cand"
    r = subprocess.run([sys.executable, str(SI / "build_candidate.py"), "--apply",
                        "--input", str(INPUT), "--outdir", str(out_dir)],
                       capture_output=True, text=True)
    check("candidate builds", r.returncode == 0, r.stderr)
    cand_path = out_dir / "smartphone_integrator.si_wireless_stage1"
    cand = cand_path.read_bytes()
    man = json.loads((out_dir / "manifest.json").read_text())

    # self-verify mode
    r2 = subprocess.run([sys.executable, str(SI / "build_candidate.py"),
                         "--verify", str(cand_path), "--input", str(INPUT)],
                        capture_output=True, text=True)
    check("candidate self-verify", r2.returncode == 0, r2.stderr)

    # negative test: a corrupted input must be refused
    bad = workdir / "bad_input.bin"
    bad_bytes = bytearray(raw)
    bad_bytes[0x100] ^= 0xFF
    bad.write_bytes(bytes(bad_bytes))
    r3 = subprocess.run([sys.executable, str(SI / "build_candidate.py"), "--apply",
                         "--input", str(bad), "--outdir", str(workdir / "bad")],
                        capture_output=True, text=True)
    check("wrong input hash refused", r3.returncode != 0)

    # content diff
    allowed = set()
    for h in bc.HOOKS.values():
        allowed.update(range(h["va"] - bc.IMAGE_BASE, h["va"] - bc.IMAGE_BASE + 12))
    allowed.update(range(0x1C, 0x20))
    allowed.update(range(0x2C, 0x2E))
    allowed.update(range(0x34, 0x54))  # phdr entry 0: PT_QNX -> hook LOAD
    allowed.update(range(0xC8, 0xCC))  # RW p_memsz owns the root
    diff = [i for i in range(len(raw)) if raw[i] != cand[i]]
    check("diff limited to hooks + hook PHDR + RW memsz", all(i in allowed for i in diff),
          f"{len(diff)} diffs")
    check("appended page present", len(cand) == man["cave"]["file_offset"] + bc.PAGE)
    return cand_path, cand, raw, man


def check_trampolines(cand: bytes, man: dict) -> None:
    try:
        from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
    except ImportError:
        print("[SKIP] capstone not installed — trampoline disassembly not checked")
        return
    md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
    cave_off, cave_va = man["cave"]["file_offset"], man["cave"]["va"]

    def disasm(va: int, count: int = 24) -> list[str]:
        off = cave_off + (va - cave_va)
        return [f"{i.mnemonic} {i.op_str}" for i in md.disasm(cand[off:off + count * 4], va)]

    # prepare: push args, root load, slot 12, handler(1), pop, displaced, b entry+12
    ins = disasm(0x1C6040)
    check("prepare: args saved", ins[0].startswith("push {r0, r1, r2, r3"))
    check("prepare: root loaded", ins[1].startswith("ldr ip, [pc"))
    check("prepare: slot offset 12", "ldr ip, [ip, #0xc]" in ins)
    check("prepare: slot id 1", "mov r0, #1" in ins)
    check("prepare: handler called", "blx ip" in ins)
    check("prepare: displaced prologue", "ldr r6, [r0, #0x28]" in ins and "mov r5, r0" in ins)
    check("prepare: returns to entry+12", any(t == "b #0x1800bc" for t in ins))

    ins = disasm(0x1C6080)
    check("cleanup: slot offset 16", "ldr ip, [ip, #0x10]" in ins)
    check("cleanup: slot id 2", "mov r0, #2" in ins)
    check("cleanup: returns to entry+12", any(t == "b #0x17fc54" for t in ins))

    ins = disasm(0x1C60C0)
    check("job: handler with job in r0", "blx ip" in ins and "mov r0, #1" not in ins[:6])
    check("job: consumed path returns 0", "mov r0, #0" in ins and "bx lr" in ins)
    check("job: displaced prologue", "push {r4, lr}" in ins and "ldr r0, [r0, #8]" in ins)
    check("job: returns to entry+12", any(t == "b #0x128dc8" for t in ins))

    # trampoline slot offsets must match the manifest contract
    offs = man["slot_table"]["offsets"]
    check("manifest slot offsets match protocol",
          offs["prepare"] == 12 and offs["cleanup"] == 16 and offs["job"] == 20)
    ins = disasm(0x1C6040)
    check("prepare loads slot 12", "ldr ip, [ip, #0xc]" in ins)
    ins = disasm(0x1C6080)
    check("cleanup loads slot 16", "ldr ip, [ip, #0x10]" in ins)
    ins = disasm(0x1C60C0)
    check("job loads slot 20", "ldr ip, [ip, #0x14]" in ins)

    # entry bytes: three identical B-to-hook branches
    for name, h in man["hooks"].items():
        va, hook = h["va"], h["hook_va"]
        off = va - bc.IMAGE_BASE
        w = struct.unpack_from("<I", cand, off)[0]
        expect = bc.enc_b(va, hook)
        check(f"entry {name}: B hook", w == expect, f"{w:#x} != {expect:#x}")


def check_elf_and_cave(cand: bytes, man: dict) -> None:
    tab = man["elf"]["e_phoff"]
    phnum = struct.unpack_from("<H", cand, bc.E_PHNUM_OFF)[0]
    check("phnum = 6 (inplace)", phnum == 6)
    phdrs = [struct.unpack_from("<8I", cand, tab + i * 0x20) for i in range(phnum)]
    hook_seg = phdrs[0]
    check("hook segment R+X @cave as entry 0", hook_seg[0] == 1 and hook_seg[6] == 5
          and hook_seg[2] == man["cave"]["va"])
    check("original phdr entries 1..5 preserved",
          [p[0] for p in phdrs[1:]] == [6, 3, 1, 1, 2])
    root = man["cave"]["root_va"]
    rw = phdrs[4]
    check("root explicitly owned by writable zero-filled BSS",
          rw[6] == 6 and rw[2] + rw[4] <= root and root + 4 <= rw[2] + rw[5])
    check("RW p_memsz reserves root without changing file size",
          rw[5] == root + 4 - rw[2] and rw[4] == 0x5ea0)
    check("bss tail on the RW segment's last mapped page",
          (root & ~0xFFF) == ((0x1BDA24 + 0x8370 - 1) & ~0xFFF))
    coff, cova = man["cave"]["file_offset"], man["cave"]["va"]
    sig_off = coff + man["cave"]["sig_offset"]
    check("cave signature", struct.unpack_from("<I", cand, sig_off)[0] == 0x58484953)
    check("cave abi version", struct.unpack_from("<I", cand, coff + 4)[0] == 1)
    check("cave root slot removed (R+X page)",
          struct.unpack_from("<I", cand, coff + 8)[0] == 0)
    check("root not inside hook segment",
          not (man["cave"]["va"] <= root < man["cave"]["va"] + bc.PAGE))
    # root must be writable: inside the RWX segment, not in the read-only image
    check("root outside hook segment (bss)", root == 0x1C5E00)


def check_envv_native() -> None:
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        print("[SKIP] no host compiler — envv native test skipped")
        return
    with tempfile.TemporaryDirectory() as td:
        exe = Path(td) / "test_envv"
        r = subprocess.run([cc, "-std=c99", "-Wall", "-Wextra", "-Werror",
                            "-I", str(SI), str(SI / "test_envv_native.c"),
                            str(SI / "si_envv.c"), "-o", str(exe)],
                           capture_output=True, text=True)
        check("envv native test compiles", r.returncode == 0, r.stderr)
        if r.returncode == 0:
            r2 = subprocess.run([str(exe)], capture_output=True, text=True)
            check("envv native tests pass", r2.returncode == 0, r2.stdout + r2.stderr)
            print(r2.stdout.strip())


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        cand_path, cand, raw, man = run_candidate_cycle(Path(td))
        check_trampolines(cand, man)
        check_elf_and_cave(cand, man)
        cases = run_arm_tests(cand, man)
        check(f"ARM execution: {cases} cases, SP/LR/args/native prologue", cases == 192)
        cases = run_guard_tests(SI, cand, man, Path(td))
        check(f"native C image/readiness guard: {cases} cases", cases >= 29)
    check_envv_native()
    subprocess.run([sys.executable, str(SI / "test_si_payload.py")], check=True)
    subprocess.run([sys.executable, str(SI / "test_si_startup.py")], check=True)

    print()
    if FAILURES:
        print(f"FAILURES: {len(FAILURES)}: {', '.join(FAILURES)}")
        return 1
    print("stage-1 host tests: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
