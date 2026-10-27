import sys,struct,os
d=open(sys.argv[1],'rb').read(); out=sys.argv[2]; os.makedirs(out,exist_ok=True)
def boxes(buf,off,end):
    while off+8<=end:
        sz=struct.unpack_from('>I',buf,off)[0]; ty=buf[off+4:off+8]
        if sz==1: sz=struct.unpack_from('>Q',buf,off+8)[0]; body=off+16
        else: body=off+8
        if sz<8: break
        yield ty,body,off+sz
        off+=sz
def find(buf,path,off=0,end=None):
    end=end or len(buf)
    for ty,b,e in boxes(buf,off,end):
        if ty==path[0]:
            if len(path)==1: return b,e
            r=find(buf,path[1:],b if ty not in (b'stsd',) else b+8,e)
            if r: return r
    return None
stbl=find(d,[b'moov',b'trak',b'mdia',b'minf',b'stbl'])
sb,se=stbl
tabs={}
for ty,b,e in boxes(d,sb,se): tabs[ty]=(b,e)
# cookie: inside stsd -> sample entry -> dapa
sdb,sde=tabs[b'stsd']
ent=sdb+8                      # version/flags + entry_count
esz=struct.unpack_from('>I',d,ent)[0]; fmt=d[ent+4:ent+8]
cookie=None
for ty,b,e in boxes(d,ent+8+28,ent+esz):   # skip SampleEntry+AudioSampleEntry v0 header
    if ty==b'dapa': cookie=d[b-8:e]
    if ty==b'wave':
        for t2,b2,e2 in boxes(d,b,e):
            if t2==b'dapa': cookie=d[b2-8:e2]
print('sample entry fmt',fmt,'cookie',len(cookie) if cookie else None)
open(os.path.join(out,'cookie.bin'),'wb').write(cookie)
# sizes
zb,_=tabs[b'stsz']; ssz,cnt=struct.unpack_from('>II',d,zb+4)
sizes=[ssz]*cnt if ssz else [struct.unpack_from('>I',d,zb+12+4*i)[0] for i in range(cnt)]
# chunk offsets
if b'stco' in tabs:
    cb,_=tabs[b'stco']; n=struct.unpack_from('>I',d,cb+4)[0]
    chunks=[struct.unpack_from('>I',d,cb+8+4*i)[0] for i in range(n)]
else:
    cb,_=tabs[b'co64']; n=struct.unpack_from('>I',d,cb+4)[0]
    chunks=[struct.unpack_from('>Q',d,cb+8+8*i)[0] for i in range(n)]
# stsc
cb,_=tabs[b'stsc']; n=struct.unpack_from('>I',d,cb+4)[0]
stsc=[struct.unpack_from('>III',d,cb+8+12*i) for i in range(n)]
spc=[]
for i,(first,per,_) in enumerate(stsc):
    last=stsc[i+1][0]-1 if i+1<len(stsc) else len(chunks)
    for c in range(first,last+1): spc.append(per)
s=0
for ci,co in enumerate(chunks):
    off=co
    for _ in range(spc[ci] if ci<len(spc) else spc[-1]):
        if s>=len(sizes): break
        open(os.path.join(out,'p%05d.bin'%s),'wb').write(d[off:off+sizes[s]])
        off+=sizes[s]; s+=1
print('packets',s)
