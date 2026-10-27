/* SPDX-License-Identifier: GPL-3.0-or-later
 * Replace ONLY MirrorGate.class in the unit's own JAR. Every other entry's
 * compressed bytes and directory metadata remain untouched. Stored ZIP entries
 * avoid the unit's known deflate/ZIP compatibility issue. Output is a NEW file. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define TARGET "com/sq5/aa/luka/MirrorGate.class"
static void fail(const char *s) { fprintf(stderr,"jarpatch: %s\n",s); exit(2); }
static uint16_t r16(const unsigned char *p) { return p[0]|p[1]<<8; }
static uint32_t r32(const unsigned char *p) { return r16(p)|(uint32_t)r16(p+2)<<16; }
static void w16(unsigned char *p,uint16_t v) { p[0]=v; p[1]=v>>8; }
static void w32(unsigned char *p,uint32_t v) { w16(p,v); w16(p+2,v>>16); }
static uint32_t crc32(const unsigned char *p,size_t n) {
    uint32_t c=~0u; size_t i; unsigned j;
    for(i=0;i<n;++i) { c^=p[i]; for(j=0;j<8;++j) c=(c>>1)^(0xedb88320u&-(c&1)); }
    return ~c;
}
static unsigned char *load(const char *name,size_t *size) {
    FILE *f=fopen(name,"rb"); long n; unsigned char *p;
    if(!f) fail("cannot open input");
    if(fseek(f,0,SEEK_END)||(n=ftell(f))<1||n>8*1024*1024) fail("input size");
    rewind(f); p=malloc(n); if(!p) fail("memory");
    if(fread(p,1,n,f)!=(size_t)n) fail("input read"); fclose(f); *size=n; return p;
}
int main(int argc,char **argv) {
    unsigned char *zip,*cls,*central,*hit=NULL,local[30]={0}; size_t zn,cn,eocd,at,end;
    uint32_t cd,cdsize,newcrc,expect,add; unsigned count,i; FILE *out; char *ep;
    /* "jarpatch --crc JAR" prints the installed MirrorGate CRC (live.sh: adopt a jar that already carries
     * the cluster-aware class, e.g. a newer aa-luka build, instead of refusing it as an external edit). */
    int crconly=argc==3&&!strcmp(argv[1],"--crc");
    cls=NULL; cn=0; expect=0; (void)ep;
    if(!crconly) {
        if(argc!=5) fail("usage: jarpatch original.jar replacement.class expected_old_crc_hex output.jar | jarpatch --crc JAR");
        if(!strcmp(argv[1],argv[4])) fail("in-place output forbidden");
        expect=strtoul(argv[3],&ep,16); if(!*argv[3]||*ep) fail("CRC argument");
    }
    zip=load(crconly?argv[2]:argv[1],&zn);
    if(!crconly) {
        cls=load(argv[2],&cn);
        if(cn<8||cn>65535||r32(cls)!=0xbebafeca||cls[6]!=0||cls[7]!=48) fail("expected Java 1.4 class");
    }
    if(zn<22) fail("truncated ZIP");
    eocd=zn-22;
    for(;;--eocd) {
        if(r32(zip+eocd)==0x06054b50u && eocd+22+r16(zip+eocd+20)==zn) break;
        if(!eocd || zn-eocd>65557) fail("EOCD missing");
    }
    if(r16(zip+eocd+4)||r16(zip+eocd+6)||r16(zip+eocd+8)!=r16(zip+eocd+10)) fail("split ZIP unsupported");
    count=r16(zip+eocd+10); cdsize=r32(zip+eocd+12); cd=r32(zip+eocd+16);
    if(count==65535||(uint64_t)cd+cdsize!=eocd) fail("central directory bounds/ZIP64");
    central=zip+cd; at=cd; end=eocd;
    for(i=0;i<count;++i) {
        size_t name,extra,comment,next; unsigned char *h;
        if(at>end||end-at<46) fail("directory truncated");
        h=zip+at; if(r32(h)!=0x02014b50) fail("directory signature");
        name=r16(h+28); extra=r16(h+30); comment=r16(h+32); next=at+46+name+extra+comment;
        if(next>end||(r16(h+8)&1)||r32(h+42)>=cd) fail("unsupported/encrypted entry");
        if(name==strlen(TARGET)&&!memcmp(h+46,TARGET,name)) {
            if(hit) fail("duplicate target class"); hit=h;
        }
        at=next;
    }
    if(at!=end||!hit) fail("target missing/directory mismatch");
    if(crconly) { printf("%08lx\n",(unsigned long)r32(hit+16)); free(zip); return 0; }
    if(r32(hit+16)!=expect) fail("installed MirrorGate differs from reviewed baseline; no output written");
    newcrc=crc32(cls,cn); add=30+strlen(TARGET)+(uint32_t)cn;
    w32(local,0x04034b50); w16(local+4,20); memcpy(local+10,hit+12,4);
    w32(local+14,newcrc); w32(local+18,cn); w32(local+22,cn); w16(local+26,strlen(TARGET));
    w16(hit+6,20); w16(hit+8,0); w16(hit+10,0); w32(hit+16,newcrc);
    w32(hit+20,cn); w32(hit+24,cn); w32(hit+42,cd);
    w32(zip+eocd+16,cd+add);
    out=fopen(argv[4],"wb"); if(!out) fail("cannot create output");
#define WRITE(p,n) do { if(fwrite((p),1,(n),out)!=(size_t)(n)) fail("write failure"); } while(0)
    WRITE(zip,cd); WRITE(local,30); WRITE(TARGET,strlen(TARGET)); WRITE(cls,cn);
    WRITE(central,zn-cd);
    if(fclose(out)) fail("close failure");
    printf("Replaced only %s; %u entries retained; old_crc=%08lx new_crc=%08lx\n",TARGET,count,(unsigned long)expect,(unsigned long)newcrc);
    free(zip); free(cls); return 0;
}
