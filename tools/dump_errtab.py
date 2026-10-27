import struct,sys,string
d=open(sys.argv[1],'rb').read()
phoff,=struct.unpack_from('<I',d,0x1c); pes,pn=struct.unpack_from('<HH',d,0x2a)
segs=[]
for i in range(pn):
    o=phoff+i*pes; typ,off,va,pa,fsz,msz=struct.unpack_from('<IIIIII',d,o)
    if typ==1: segs.append((va,off,fsz))
def v2o(v):
    for va,off,fsz in segs:
        if va<=v<va+fsz: return off+(v-va)
ok=set(string.printable)-set('\t\r\x0b\x0c')
def cstr(v,maxlen=200):
    o=v2o(v)
    if o is None: return None
    e=d.find(b'\x00',o,o+maxlen)
    if e<0: return None
    s=d[o:e].decode('latin1')
    if not s or any(c not in ok for c in s): return None
    return s
start=int(sys.argv[2],16)
i=0
while True:
    o=start+i*8
    c,p=struct.unpack_from('<iI',d,o)
    s=cstr(p)
    if s is None:
        print('--- table ends at index %d (file off 0x%x): code=%d ptr=0x%08x'%(i,o,c,p)); break
    print('%8d\t%s'%(c,s))
    i+=1
    if i>4000: print('--- stopped at 4000'); break
