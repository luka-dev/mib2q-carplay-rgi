#!/usr/bin/env python3
"""Execute pinned original SI record and completion helpers in ARM Unicorn.

Only external UnicodeString operations and ASI delivery are modeled. This
establishes native offsets/control flow, not the real C++ allocator or HMI.
"""
import hashlib
import struct
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.arm_const import *

SOURCE = Path(__file__).resolve().parents[2] / 'deploy/smartphone_integrator/smartphone_integrator'
EXPECTED = '606236a81c7c4dafda6fdd08fe6eb27c43252796784da25d9760a7d552ee2af3'
HEAP, STACK, END = 0x300000, 0x410000, 0x500000


def main():
    assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == EXPECTED
    u = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    u.mem_map(0x100000, 0x100000)
    u.mem_map(HEAP, 0x20000)
    u.mem_map(STACK-0x10000, 0x10000)
    u.mem_map(END, 0x1000)
    with SOURCE.open('rb') as f:
        elf=ELFFile(f)
        for s in elf.iter_segments():
            if s['p_type'] == 'PT_LOAD': u.mem_write(s['p_vaddr'],s.data())
        for s in elf.iter_sections():
            if isinstance(s,RelocationSection):
                symbols=elf.get_section(s['sh_link'])
                for r in s.iter_relocations():
                    sym=symbols.get_symbol(r['r_info_sym'])
                    if r['r_info_type']==21 and sym['st_value']:
                        u.mem_write(r['r_offset'],struct.pack('<I',sym['st_value']))
    u.reg_write(UC_ARM_REG_C1_C0_2,0xf00000)
    u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
    def word(a): return struct.unpack('<I',u.mem_read(a,4))[0]
    def put(a,v): u.mem_write(a,struct.pack('<I',v & 0xffffffff))
    events, strings, notifications = [], [], []
    u.mem_write(0x1c5b88,b'\xff\x7f')
    u.mem_write(0x1c5a54,b'\xff\x7f')
    def intercept(machine,address,size,user):
        if address == END: machine.emu_stop(); return
        r0,r1,r2=[machine.reg_read(UC_ARM_REG_R0+i) for i in range(3)]
        handled=True
        if address==0x115e44: # external UnicodeString constructor
            strings.append(('ctor',r0)); machine.mem_write(r0,bytes(40))
        elif address==0x11573c: # external UnicodeString assignment
            strings.append(('copy',r0,r1)); machine.mem_write(r0,bytes(machine.mem_read(r1,40)))
        elif address==0x12584c: events.append(word(r1))
        elif address==0x156224: machine.mem_write(r0,bytes(8))
        elif address==0x1565c8: pass
        elif address==0x16a2bc: machine.reg_write(UC_ARM_REG_R0,0)
        elif address==0x1762ec: notifications.append((r0,r1))
        else: handled=False
        if handled: machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
        elif 0x115000<=address<0x116000: raise AssertionError(f'unexpected imported call {address:#x}')
    u.hook_add(UC_HOOK_CODE,intercept)
    def fault(machine,access,address,size,value,user):
        raise AssertionError(f'ARM memory fault PC={machine.reg_read(UC_ARM_REG_PC):#x} LR={machine.reg_read(UC_ARM_REG_LR):#x} address={address:#x}')
    u.hook_add(UC_HOOK_MEM_INVALID,fault)
    def call(fn,*args):
        u.reg_write(UC_ARM_REG_SP,STACK-0x100)
        u.reg_write(UC_ARM_REG_LR,END)
        for i,v in enumerate(args): u.reg_write(UC_ARM_REG_R0+i,v)
        try: u.emu_start(fn,0,count=100000)
        except Exception:
            print('FAULT',hex(u.reg_read(UC_ARM_REG_PC)),hex(u.reg_read(UC_ARM_REG_LR)))
            raise
        assert u.reg_read(UC_ARM_REG_PC)==END,hex(u.reg_read(UC_ARM_REG_PC))
    dp, src, record, record2, vector, handler = [HEAP+x for x in (0,0x1000,0x2000,0x2200,0x3000,0x4000)]
    call(0x169850,src)
    assert [x[1]-src for x in strings if x[0]=='ctor']==[36,76,116,156]
    assert word(src+196)==0xffffffff and word(src+200)==0
    for off in (28,68,108,148): u.mem_write(src+8+off,bytes([off])*40)
    put(src+28,2);put(src+32,2);put(src+196,0);put(src+200,2)
    call(0x169850,dp+232) # native default record, used by removal
    for r in (record,record2): call(0x169850,r)
    put(dp+444,2);put(dp+448,vector);put(dp+452,2)
    put(vector,record);put(vector+4,record2)
    call(0x16ec90,dp,101,src+8)
    assert word(dp+440)==1 and word(record)==101 and word(record+28)==2
    assert bytes(u.mem_read(record+36,160))==bytes(u.mem_read(src+36,160))
    assert word(record+196)==0 and word(record+200)==2
    call(0x16ec90,dp,102,src+8)
    assert word(dp+440)==2 and word(record2)==102
    # Removing the first keeps object storage in the unused pool, then reuses it.
    call(0x169984,dp,101)
    assert word(dp+440)==1 and word(vector)==record2 and word(vector+4)==record
    assert word(record)==0 and word(record+196)==0xffffffff
    call(0x16ec90,dp,103,src+8)
    assert word(dp+440)==2 and word(record)==103
    # Native completion helper is used by our no-USB prepare/cleanup shims.
    put(handler+40,dp);put(dp+1256,103);put(0x1c5928,HEAP+0x8000)
    for pending,success,expected in ((1,1,17),(1,0,11),(3,1,20),(2,1,19)):
        put(handler+48,pending)
        for off in range(52,80,4): put(handler+off,0xffffffff)
        put(handler+80,0)
        call(0x17f2b8,handler,success)
        assert events[-1]==expected and word(handler+48)==0
    assert len(notifications)==1
    print('Native ARM: constructors/copy, present add/remove/pool reuse, prepare/retry/error/cleanup completion PASS')


if __name__=='__main__': main()
