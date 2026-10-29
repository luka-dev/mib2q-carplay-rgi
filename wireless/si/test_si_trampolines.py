"""Execute the generated ARM hooks in Unicorn; no QNX VM or native process."""
import random
import struct

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_CPSR
import build_candidate as bc

REGS = [UC_ARM_REG_R0 + i for i in range(13)] + [UC_ARM_REG_SP, UC_ARM_REG_LR]
BASE, HEAP, STACK, HANDLER, RETURN = 0x100000, 0x300000, 0x410000, 0x500000, 0x500100


def run_arm_tests(cand, manifest):
    rng = random.Random(47426351)
    total = 0
    for name, hook in manifest["hooks"].items():
        for mode in ("no_root", "no_slot", "original", "consumed"):
            for _ in range(16):
                uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
                uc.mem_map(BASE, 0x100000)
                uc.mem_map(HEAP, 0x10000)
                uc.mem_map(STACK - 0x10000, 0x10000)
                uc.mem_map(HANDLER, 0x1000)
                # Hook code is RX; RW offsets in the ELF differ from VA-base,
                # but these hooks use only this explicitly initialized root.
                uc.mem_write(BASE, cand)
                initial = [rng.getrandbits(32) for _ in REGS]
                initial[0], initial[13], initial[14] = HEAP, STACK - 0x100, RETURN
                for reg, value in zip(REGS, initial):
                    uc.reg_write(reg, value)
                uc.mem_write(HEAP + 8, struct.pack("<I", 1))
                uc.mem_write(HEAP + 40, struct.pack("<I", 0x76543210))
                table = HEAP + 0x100
                root = 0 if mode == "no_root" else table
                slot = HANDLER if mode in ("original", "consumed") else 0
                uc.mem_write(manifest["cave"]["root_va"], struct.pack("<I", root))
                uc.mem_write(table + hook["slot_offset"], struct.pack("<I", slot))
                # Actual ARM callback: clobber all caller-saved registers,
                # return the verdict, then BX LR.
                verdict = int(mode == "consumed")
                uc.mem_write(HANDLER, bc.words(bc.enc_mov_imm(0, verdict),
                    bc.enc_mov_imm(1, 0xa1), bc.enc_mov_imm(2, 0xa2),
                    bc.enc_mov_imm(3, 0xa3), bc.enc_mov_imm(12, 0xac), bc.enc_bx_rm(14)))
                observed = []
                routes = name == "job" or manifest["cave"]["abi_version"] >= 2
                terminal = RETURN if routes and verdict else hook["returns_to_va"]

                def trace(machine, address, size, _):
                    if address == HANDLER:
                        assert machine.reg_read(UC_ARM_REG_SP) % 8 == 0
                        observed.append(tuple(machine.reg_read(UC_ARM_REG_R0+i) for i in range(3)))
                    if address == terminal:
                        machine.emu_stop()

                uc.hook_add(UC_HOOK_CODE, trace)
                uc.emu_start(hook["va"], 0, count=200)
                assert uc.reg_read(UC_ARM_REG_PC) == terminal, (name, mode, "did not return")
                expected_calls = [tuple(initial[:3]) if routes else
                                  (hook["slot_id"], initial[1], initial[2])] if slot else []
                assert observed == expected_calls, (name, mode, "handler args", observed)
                actual = [uc.reg_read(r) for r in REGS]
                if routes and verdict:
                    expected = initial.copy()
                    expected[0] = int(name == "carplay_device")
                else:
                    # Independent oracle: execute original native prologue
                    # without our trampoline/callback and compare registers.
                    stock = Uc(UC_ARCH_ARM, UC_MODE_ARM)
                    stock.mem_map(BASE, 0x100000)
                    stock.mem_map(HEAP, 0x10000)
                    stock.mem_map(STACK - 0x10000, 0x10000)
                    stock.mem_write(hook["va"], bytes.fromhex(hook["displaced"]))
                    stock.mem_write(HEAP + 8, struct.pack("<I", 1))
                    stock.mem_write(HEAP + 40, struct.pack("<I", 0x76543210))
                    for reg, value in zip(REGS, initial): stock.reg_write(reg, value)
                    stock.emu_start(hook["va"], hook["returns_to_va"], count=3)
                    if name in ("connect", "carplay_device"):
                        # The displaced CMP supplies the flags consumed by
                        # native connect's following conditional return.
                        assert (uc.reg_read(UC_ARM_REG_CPSR) & 0xf0000000) == (stock.reg_read(UC_ARM_REG_CPSR) & 0xf0000000), (name, mode, "condition flags")
                    expected = [stock.reg_read(r) for r in REGS]
                    active_sp = expected[13]
                    active_size = initial[13] - active_sp
                    assert bytes(uc.mem_read(active_sp, active_size)) == bytes(
                        stock.mem_read(active_sp, active_size))
                assert actual == expected, (name, mode, "register/SP/LR corruption", actual, expected)
                total += 1
    return total
