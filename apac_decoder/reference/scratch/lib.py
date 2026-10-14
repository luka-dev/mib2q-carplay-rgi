import struct, sys
import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '../../..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
LIB=_REPO + "/apac_decoder/reference/.build/libs/libSPF_CP_PlugInWrapper.so"
DATA=open(LIB,'rb').read()
# parse program headers (ELF64 LE)
e_phoff=struct.unpack_from('<Q',DATA,0x20)[0]
e_phentsize=struct.unpack_from('<H',DATA,0x36)[0]
e_phnum=struct.unpack_from('<H',DATA,0x38)[0]
SEGS=[]
for i in range(e_phnum):
    o=e_phoff+i*e_phentsize
    p_type,p_flags,p_offset,p_vaddr,p_paddr,p_filesz,p_memsz,p_align=struct.unpack_from('<IIQQQQQQ',DATA,o)
    if p_type==1: SEGS.append((p_vaddr,p_filesz,p_offset))
def off(va):
    for v,fsz,o in SEGS:
        if v<=va<v+fsz: return o+(va-v)
    raise KeyError(hex(va))
def rd(va,n): 
    o=off(va); return DATA[o:o+n]
def u64(va): return struct.unpack('<Q',rd(va,8))[0]
def u32(va): return struct.unpack('<I',rd(va,4))[0]
def u16(va): return struct.unpack('<H',rd(va,2))[0]
# relocations: .rela.dyn R_AARCH64_RELATIVE  (type 1027)
def sect(name):
    e_shoff=struct.unpack_from('<Q',DATA,0x28)[0]
    e_shentsize=struct.unpack_from('<H',DATA,0x3a)[0]
    e_shnum=struct.unpack_from('<H',DATA,0x3c)[0]
    e_shstrndx=struct.unpack_from('<H',DATA,0x3e)[0]
    def sh(i):
        o=e_shoff+i*e_shentsize
        return struct.unpack_from('<IIQQQQIIQQ',DATA,o)
    strtaboff=sh(e_shstrndx)[4]
    for i in range(e_shnum):
        s=sh(i)
        nm=DATA[strtaboff+s[0]:DATA.index(b'\0',strtaboff+s[0])].decode()
        if nm==name: return s
    return None
_REL=None
def relmap():
    global _REL
    if _REL is None:
        _REL={}
        s=sect('.rela.dyn')
        o,sz,esz=s[4],s[5],s[9]
        for i in range(sz//24):
            r_off,r_info,r_add=struct.unpack_from('<QQq',DATA,o+i*24)
            if (r_info&0xffffffff)==1027: _REL[r_off]=r_add
    return _REL
def ptr(va):
    r=relmap()
    if va in r: return r[va]
    return u64(va)
