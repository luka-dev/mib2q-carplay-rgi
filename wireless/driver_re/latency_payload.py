"""Build and merge the complete MU1316 latency payload into a stock ARM DSO.

Original LOADs, notes, symbols, imports and PLT are retained. The extended PHDR
table stays in the header page; its displaced hash and note move to the new RX
segment. Payload-local R_ARM_RELATIVE entries join the original DT_REL prefix;
all other payload relocation kinds are refused.
"""
from __future__ import annotations
import hashlib
import io
import json
from pathlib import Path
import struct
import subprocess

from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
from elftools.elf.elffile import ELFFile
from .reorder_machine import DriverImage, compile_candidate, compile_window_candidate, compile_harden_candidate
from .reorder_machine import PATCH_ADDRESS, WINDOW_PATCH_ADDRESS, HARDEN_PATCH_ADDRESS
from .callout_machine import STACK_SHA256

CODE_LOAD, CODE_VA, STATE_VA = 0x67000, 0x67200, 0x80000
ENTRY_HOOKS = {
    'rx': (0x473a8, 'wl_rx_runtime_receive'),
    'create': (0x47d10, 'wl_rx_runtime_create'),
    'delete': (0x46e8c, 'wl_rx_runtime_delete'),
    'flush': (0x472fc, 'wl_rx_runtime_flush'),
    'attach': (0x15034, 'wl_tx_runtime_attach'),
    'detach': (0x15a38, 'wl_tx_runtime_detach'),
}
INLINE_HOOKS = {0x47748: ('wl_rx_runtime_tail_trampoline', False, 14),
                0xf188: ('wl_tx_gate_trampoline', False, 1),
                0xf1a8: ('wl_tx_runtime_request', True, 14)}
SOURCES = ('wifi_latency_runtime.c', 'wifi_latency_policy.c', 'wifi_latency_rx_runtime.c',
           'wifi_latency_rx_runtime.S', 'rx_timer_churn_policy.S')


def align(n, a=4096): return (n+a-1)//a*a
def digest(b): return hashlib.sha256(b).hexdigest()
def branch(source, target, *, link=False, condition=14):
    delta = target-source-8
    if delta % 4 or not -(1 << 25) <= delta < (1 << 25): raise ValueError('ARM branch range/alignment')
    return struct.pack('<I', (condition << 28) | (0x0b000000 if link else 0x0a000000) | ((delta//4) & 0xffffff))
def run(args):
    p = subprocess.run(args, capture_output=True, text=True, timeout=60)
    if p.returncode: raise RuntimeError(p.stdout+p.stderr)
def va_offset(elf, va, size=1):
    found=[int(s['p_offset']+va-s['p_vaddr']) for s in elf.iter_segments()
           if s['p_type']=='PT_LOAD' and s['p_vaddr'] <= va and va+size <= s['p_vaddr']+s['p_filesz']]
    if len(found)!=1: raise ValueError(f'not uniquely file-backed {va:#x}')
    return found[0]
def fnv(data):
    h=2166136261
    for v in data: h=((h^v)*16777619)&0xffffffff
    return h


def compile_payload(stock, stack, out):
    out.mkdir(parents=True, exist_ok=True)
    image=DriverImage(stock)
    if digest(stack)!=STACK_SHA256: raise ValueError('unsupported io-pkt binary')
    elf=ELFFile(io.BytesIO(stack))
    syms={s.name:(int(s['st_value']),int(s['st_size'])) for s in elf.get_section_by_name('.symtab').iter_symbols()}
    defs={
        'WL_IO_MUTEX_LOCK':'pthread_mutex_lock','WL_IO_MUTEX_UNLOCK':'pthread_mutex_unlock',
        'WL_IO_MUTEX_TRYLOCK':'pthread_mutex_trylock',
        'WL_IO_DLSYM':'dlsym','WL_IO_DLOPEN':'dlopen','WL_IO_TICKS':'hardclock_ticks',
        'WL_IO_HZ':'hz','WL_IO_CONTEXT_CALLBACK':'stk_context_callback_2',
    }
    guards=[]
    for name in ('pthread_mutex_lock','pthread_mutex_unlock','pthread_mutex_trylock','dlsym','dlopen','callout_init',
                 'callout_reset','callout_stop','softclock','stk_context_callback_2'):
        va,n=syms[name]
        if not n and name in ('pthread_mutex_lock','pthread_mutex_unlock','pthread_mutex_trylock','dlsym','dlopen'):
            n=12 # pinned ARM executable's import thunks
        if not n: raise ValueError(f'no native function size: {name}')
        off=va_offset(elf,va,n)
        guards.append((va,n,fnv(stack[off:off+n])))
    header=['#ifndef WIFI_LATENCY_BUILD_H','#define WIFI_LATENCY_BUILD_H']
    header += [f'#define {k} {syms[v][0]:#x}u' for k,v in defs.items()]
    header += [f'#define WL_STACK_GUARD_COUNT {len(guards)}u',
               'static const struct { uint32_t address, length, hash; } wl_stack_guards[] = {']
    header += [f'    {{{a:#x}u,{n}u,{h:#x}u}},' for a,n,h in guards]
    header += ['};','#endif','']
    (out/'wifi_latency_build.h').write_text('\n'.join(header))
    assembly=['.syntax unified','.arm','.section .text.wl_native_base,"ax",%progbits',
              '.global wl_native_base','.hidden wl_native_base','.type wl_native_base,%function',
              'wl_native_base:','adr r0,wl_native_base','ldr r1,1f','sub r0,r0,r1','bx lr',
              f'1: .word {CODE_VA:#x}', '.size wl_native_base,.-wl_native_base',
              '.section .text.wl_gateways,"ax",%progbits']
    cs=Cs(CS_ARCH_ARM, CS_MODE_ARM)
    gateway_targets={}
    for name,(va,_target) in ENTRY_HOOKS.items():
        raw=stock[va:va+4]
        inst=list(cs.disasm(raw,va))
        if len(inst)!=1 or inst[0].mnemonic!='push' or 'pc' in inst[0].op_str:
            raise ValueError(f'unsafe entry replay at{va:#x}: {inst}')
        assembly += [f'.global wl_orig_{name}',f'.hidden wl_orig_{name}',f'.type wl_orig_{name},%function',
                     f'wl_orig_{name}:',f'.word {struct.unpack("<I",raw)[0]:#x}',
                     f'.global wl_resume_{name}',f'wl_resume_{name}:','.word 0xea000000',
                     f'.size wl_orig_{name},.-wl_orig_{name}']
        gateway_targets[f'wl_resume_{name}']=va+4
    assembly += ['.global wl_tx_gate_trampoline','.hidden wl_tx_gate_trampoline',
                 '.type wl_tx_gate_trampoline,%function','wl_tx_gate_trampoline:',
                 'push {r0-r3,r12,lr}','mov r0,r4','mov r1,r5','ldr r2,[r6,#24]',
                 'bl wl_tx_runtime_gate','cmp r0,#0','pop {r0-r3,r12,lr}',
                 '.global wl_tx_gate_stock_skip','wl_tx_gate_stock_skip:','.word 0x0a000000',
                 '.global wl_tx_gate_early_continue','wl_tx_gate_early_continue:','.word 0xea000000',
                 '.size wl_tx_gate_trampoline,.-wl_tx_gate_trampoline','']
    gateway_targets.update(wl_tx_gate_stock_skip=0xf1ac,wl_tx_gate_early_continue=0xf18c)
    (out/'gateways.S').write_text('\n'.join(assembly))
    root=Path(__file__).parent
    objects=[]
    flags=['clang','--target=armv7-none-eabi','-march=armv7-a','-marm','-O2',
           '-mfloat-abi=soft','-fno-vectorize','-fno-slp-vectorize','-ffreestanding',
           '-fno-builtin','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables',
           '-fPIC','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-Wall','-Wextra','-Werror',
           '-I',str(root),'-I',str(out)]
    for src in [*(root/s for s in SOURCES),out/'gateways.S']:
        obj=out/(src.name.replace('.','_')+'.o')
        run([*flags,'-c',str(src),'-o',str(obj)]); objects.append(obj)
    script=f'''PHDRS {{ code PT_LOAD FLAGS(5); state PT_LOAD FLAGS(6); dynamic PT_DYNAMIC FLAGS(6); }}
SECTIONS {{
 . = {CODE_VA:#x};
 .text : {{ KEEP(*(.text.wl_native_base)) *(.text*) *(.rodata*) }} :code
 . = {STATE_VA:#x};
 .data : {{ *(.data*) *(.got*) }} :state
 .dynamic : {{ *(.dynamic) }} :state :dynamic
 .rel.dyn : {{ *(.rel.dyn) }} :state
 .dynsym : {{ *(.dynsym) }} :state
 .dynstr : {{ *(.dynstr) }} :state
 .hash : {{ *(.hash) }} :state
 .gnu.hash : {{ *(.gnu.hash) }} :state
 .bss (NOLOAD) : {{ *(.bss*) *(COMMON) }} :state
 /DISCARD/ : {{ *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.note*) *(.eh_frame*) }}
}}
'''
    (out/'payload.ld').write_text(script)
    target=out/'latency_payload.elf'
    run(['ld.lld','-shared','-Bsymbolic','--no-undefined','--hash-style=sysv',
         '-T',str(out/'payload.ld'),*(str(p) for p in objects),'-o',str(target)])
    payload=bytearray(target.read_bytes())
    pe=ELFFile(io.BytesIO(payload))
    tab=pe.get_section_by_name('.symtab')
    symbols={s.name:int(s['st_value']) for s in tab.iter_symbols() if s.name and s['st_shndx']!='SHN_UNDEF'}
    undefined=[s.name for s in tab.iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF']
    if undefined: raise ValueError(f'payload unresolved symbols{undefined}')
    for sym,dest in gateway_targets.items():
        va=symbols[sym];cond=0 if sym=='wl_tx_gate_stock_skip' else 14
        off=va_offset(pe,va,4); payload[off:off+4]=branch(va,dest,condition=cond)
    target.write_bytes(payload)
    return bytes(payload),symbols,guards


def build_latency(stock,stack,out):
    image=DriverImage(stock)
    payload,symbols,guards=compile_payload(stock,stack,out)
    pe=ELFFile(io.BytesIO(payload)); original=ELFFile(io.BytesIO(stock))
    loads=[s for s in pe.iter_segments() if s['p_type']=='PT_LOAD']
    if len(loads)!=2: raise ValueError('payload LOAD count')
    code,state=loads
    if code['p_vaddr']!=CODE_VA or state['p_vaddr']!=STATE_VA: raise ValueError('payload placement')
    if code['p_vaddr']+code['p_memsz']>STATE_VA: raise ValueError('payload code/state overlap')
    for s in original.iter_segments():
        if s['p_type']=='PT_LOAD' and s['p_vaddr']+s['p_memsz']>CODE_LOAD: raise ValueError('stock/payload overlap')
    reloc=pe.get_section_by_name('.rel.dyn')
    payload_relocations=[]
    if reloc:
        for r in reloc.iter_relocations():
            if r['r_info_type']!=23: raise ValueError(f'unsupported payload relocation {r.entry}')
            if not STATE_VA <= r['r_offset'] < STATE_VA+state['p_memsz']:
                raise ValueError('payload relocation outside writable state')
            payload_relocations.append((int(r['r_offset']),23))
    dynamic=original.get_section_by_name('.dynamic')
    tags={t.entry.d_tag:int(t.entry.d_val) for t in dynamic.iter_tags() if t.entry.d_tag!='DT_NULL'}
    old_rel_off=va_offset(original,tags['DT_REL'],tags['DT_RELSZ'])
    old_rel=stock[old_rel_off:old_rel_off+tags['DT_RELSZ']]
    relative_bytes=tags['DT_RELCOUNT']*8
    for i in range(0,relative_bytes,8):
        if struct.unpack_from('<II',old_rel,i)[1]!=23:
            raise ValueError('non-relative relocation in DT_RELCOUNT prefix')
    merged=(old_rel[:relative_bytes]+b''.join(struct.pack('<II',*r) for r in payload_relocations)
            +old_rel[relative_bytes:])
    state_data=bytearray(state.data());rel_delta=align(len(state_data),4)
    state_data.extend(bytes(rel_delta-len(state_data)));state_data.extend(merged)
    # Existing BSS must remain zero. Append the combined relocation table after
    # all payload state, not inside its uninitialized region.
    if rel_delta < state['p_memsz']:
        state_data=bytearray(state.data())
        state_data.extend(bytes(align(int(state['p_memsz']),4)-len(state_data)))
        rel_delta=len(state_data);state_data.extend(merged)
    code_off=align(len(stock))
    # Keep the table in the original ELF header page for QNX6.5 compatibility.
    # The two extra entries displace only the GNU build-id note and start of
    # SysV hash. Preserve both in the new read/execute LOAD and repoint users.
    hash_section=original.get_section_by_name('.hash')
    hash_data=hash_section.data()
    hash_delta=512+len(code.data())
    new_hash=CODE_LOAD+hash_delta
    code_bytes=bytearray(512)
    note=next(s for s in original.iter_segments() if s['p_type']=='PT_NOTE' and s['p_offset']==0xf4)
    code_bytes[:len(note.data())]=note.data()
    code_bytes.extend(code.data());code_bytes.extend(hash_data)
    state_off=align(code_off+len(code_bytes))
    if code_off != CODE_LOAD:
        raise ValueError('unexpected pinned payload file/virtual layout')
    old_phoff=int(original['e_phoff']);n=int(original['e_phnum'])
    phdr=[list(struct.unpack_from('<8I',stock,old_phoff+32*i)) for i in range(n)]
    for p in phdr:
        if p[0]==4 and p[1]==int(note['p_offset']):
            p[1]=code_off;p[2]=p[3]=CODE_LOAD
    phdr += [[1,code_off,CODE_LOAD,CODE_LOAD,len(code_bytes),len(code_bytes),5,4096],
             [1,state_off,STATE_VA,STATE_VA,len(state_data),len(state_data),6,4096]]
    if len(phdr)*32>512: raise ValueError('PHDR prefix exhausted')
    result=bytearray(stock)
    result.extend(bytes(code_off-len(result)))
    prefix=b''.join(struct.pack('<8I',*p) for p in phdr)
    result.extend(code_bytes)
    result.extend(bytes(state_off-len(result)));result.extend(state_data)
    result[old_phoff:old_phoff+len(prefix)]=prefix
    struct.pack_into('<H',result,44,len(phdr))
    permitted=set(range(old_phoff,old_phoff+len(prefix)))|set(range(44,46))
    for i,section in enumerate(original.iter_sections()):
        if section.name in ('.hash','.note.gnu.build-id'):
            va=new_hash if section.name=='.hash' else CODE_LOAD
            off=code_off+hash_delta if section.name=='.hash' else code_off
            header_off=int(original['e_shoff'])+i*int(original['e_shentsize'])+12
            struct.pack_into('<II',result,header_off,va,off)
            permitted.update(range(header_off,header_off+8))
    for i,t in enumerate(dynamic.iter_tags()):
        if t.entry.d_tag in ('DT_REL','DT_RELSZ','DT_RELCOUNT','DT_HASH'):
            off=int(dynamic['sh_offset'])+8*i+4
            value={'DT_REL':STATE_VA+rel_delta,'DT_RELSZ':len(merged),
                   'DT_RELCOUNT':tags['DT_RELCOUNT']+len(payload_relocations),
                   'DT_HASH':new_hash}[t.entry.d_tag]
            struct.pack_into('<I',result,off,value);permitted.update(range(off,off+4))
    patches=[]
    fragments=[('expiry',PATCH_ADDRESS,compile_candidate(out)[0]),
               ('window',WINDOW_PATCH_ADDRESS,compile_window_candidate(out)[0]),
               ('harden',HARDEN_PATCH_ADDRESS,compile_harden_candidate(out)[0])]
    for name,(va,target) in ENTRY_HOOKS.items():fragments.append((name,va,branch(va,symbols[target])))
    for va,(target,link,condition) in INLINE_HOOKS.items():
        fragments.append((target,va,branch(va,symbols[target],link=link,condition=condition)))
    for name,va,b in fragments:
        off=va_offset(original,va,len(b));old=stock[off:off+len(b)]
        result[off:off+len(b)]=b;permitted.update(range(off,off+len(b)))
        patches.append(dict(name=name,va=hex(va),offset=off,original_hex=old.hex(),patched_hex=b.hex()))
    if any(i not in permitted for i,(a,b) in enumerate(zip(stock,result)) if a!=b):
        raise ValueError('unexpected mutation of original binary')
    check=ELFFile(io.BytesIO(result))
    for s in check.iter_segments():
        if s['p_type']=='PT_LOAD' and (s['p_vaddr']-s['p_offset'])%s['p_align']:raise ValueError('LOAD congruence')
    meta=dict(profile='latency',stock_sha256=digest(stock),io_pkt_sha256=digest(stack),
              sha256=digest(result),size=len(result),patches=patches,
              payload_symbols=symbols,stack_guards=guards,payload_relocations=len(payload_relocations),
              merged_rel_va=STATE_VA+rel_delta,merged_rel_size=len(merged),
              code_load_va=CODE_LOAD,state_load_va=STATE_VA,code_file_offset=code_off,state_file_offset=state_off,
              sources={s:digest(Path(__file__).with_name(s).read_bytes()) for s in
                       (*SOURCES,'wifi_latency_runtime.h','wifi_latency_rx_runtime.h',
                        'wifi_latency_policy.h','rx_timer_churn_policy.h','latency_payload.py')},
              requirements=['pinned MU1316 driver and io-pkt; hz119',
                            'runtime module self-pin required; failure preserves stock policy',
                            'bounded sidecars/cookies; exhaustion falls back to stock policy',
                            'separate io-sdiorm coalescing is not part of this driver'])
    (out/'latency_manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
    return bytes(result),meta
