#!/usr/bin/env python3
"""Execute the actual graft callback ARM with peer-bearing SDK events.
Does not emulate a real Bluetooth connection or validate SDK field semantics.
"""
from pathlib import Path
import struct
import subprocess
import tempfile
from elftools.elf.elffile import ELFFile
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC

HERE=Path(__file__).resolve().parent
STATE,INPUT,STACK,END=0x510000,0x600000,0x710000,0x720000
def main():
    with tempfile.TemporaryDirectory(prefix='rf-peer-arm-') as temp:
        root=Path(temp);src=root/'callback.c'
        src.write_text('#include "'+str(HERE/'graft.c')+'"\n'
            'const u32 test_layout[]={__builtin_offsetof(struct GraftState,channel),'
            '__builtin_offsetof(struct GraftState,queue),'
            '__builtin_offsetof(struct GraftState,peer_valid),sizeof(struct BridgeFrame)};\n')
        flags=['clang','-target','armv7-none-eabi','-march=armv7-a','-marm','-O2','-ffreestanding','-fno-builtin',
               '-fno-unwind-tables','-fno-asynchronous-unwind-tables','-fno-stack-protector','-fno-pic']
        subprocess.run([*flags,'-c',str(src),'-o',str(root/'graft.o')],check=True)
        subprocess.run([*flags,'-c',str(HERE/'eir.c'),'-o',str(root/'eir.o')],check=True)
        subprocess.run(['ld.lld','-flavor','gnu','-m','armelf','-T',str(HERE/'graft.ld'),
                        '--no-undefined',str(root/'graft.o'),str(root/'eir.o'),'-o',str(root/'graft.elf')],check=True)
        with (root/'graft.elf').open('rb') as f:
            e=ELFFile(f);section=e.get_section_by_name('.graft');code=section.data();base=section['sh_addr']
            symbols=e.get_section_by_name('.symtab')
            fn=symbols.get_symbol_by_name('graft_rf_callback')[0]['st_value']
            lo=symbols.get_symbol_by_name('test_layout')[0]['st_value']-base
            channel,queue,valid,stride=struct.unpack_from('<4I',code,lo)
        u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0x508000,0x8000);u.mem_write(base,code)
        u.mem_map(STATE,0x20000);u.mem_map(INPUT,0x1000);u.mem_map(STACK-0x10000,0x10000);u.mem_map(END,0x1000)
        def stop(m,a,s,o):
            if a==END:m.emu_stop()
        u.hook_add(UC_HOOK_CODE,stop)
        def word(off):return struct.unpack('<I',u.mem_read(STATE+off,4))[0]
        def event(kind,pointer=0,wrong=False):
            u.mem_write(INPUT,struct.pack('<BBHHBBHHI',kind,0,0,0,0,0,0,0,pointer))
            u.reg_write(UC_ARM_REG_R0,STATE+channel+int(wrong));u.reg_write(UC_ARM_REG_R1,INPUT)
            u.reg_write(UC_ARM_REG_SP,STACK-0x100);u.reg_write(UC_ARM_REG_LR,END)
            u.emu_start(fn,0,count=10000);assert u.reg_read(UC_ARM_REG_PC)==END
        # Place remote at the exact end of mapped memory, with address at +13.
        peer=INPUT+0x1000-19;address=b'\x01\x23\x45\x67\x89\xab';u.mem_write(peer+13,address)
        event(2,peer,wrong=True);assert word(28)==0
        event(2,peer);frame=bytes(u.mem_read(STATE+queue,14))
        assert frame==struct.pack('<BBHI',1,0,6,1)+address and word(28)==1
        assert bytes(u.mem_read(STATE+valid,1))==b'\x01'
        event(4);assert bytes(u.mem_read(STATE+queue+stride,8))==struct.pack('<BBHI',3,0,0,1)
        assert bytes(u.mem_read(STATE+valid,1))==b'\x00'
        event(2);assert bytes(u.mem_read(STATE+queue+2*stride,8))==struct.pack('<BBHI',1,0,0,3)
        u.mem_write(STATE+28,struct.pack('<I',8));event(2,peer);assert word(40)==1
        print('RFCOMM callback ARM: peer6, disconnect, missing identity, wrong channel, queue overflow PASS')
if __name__=='__main__':main()
