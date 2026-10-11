#!/usr/bin/env python3
"""Real C TLV parser, cache reset and slot/text writer -> Java -> stock BAP sender.

Extract source functions verbatim for the host: only time, slot assignment and
linked-lane lookup are fixtures. Does not emulate QNX services or HUD hardware.
Run after scripts/build_java.sh; generated sources/frames stay under build/.

The Java half (tests/RgdNativeContractProbe.java) uses scripts/java/docker.sh and
its stock/dependency overrides; CARPLAY_HOOK_JAR defaults to build/carplay_hook.jar.
RGD_CONTRACT_STAGE=native stops after writing the frames, so the C and Java halves
can run separately (for example in different Docker images). No host JDK is used.
RGD_CONTRACT_OUT moves the output; RGD_CONTRACT_SANITIZE replaces address,undefined
(ASan hangs at random in Docker on kernels with high mmap ASLR entropy).
"""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get('RGD_CONTRACT_OUT') or ROOT / 'build/rgd-native-contract')
SANITIZE = os.environ.get('RGD_CONTRACT_SANITIZE') or 'address,undefined'
BUILD.mkdir(parents=True, exist_ok=True)
hook = (ROOT / 'hook/routeguidance/rgd_hook.c').read_text()
bus = (ROOT / 'hook/framework/bus.c').read_text()
slot_start = hook.index('static void write_slot_data_keys(bus_text_builder_t* b, unsigned idx, const rgd_maneuver_t* man) {')
slot_writer = hook[slot_start:hook.index('\nstatic void write_lane_data_keys(', slot_start)]
text_writer = bus[bus.index('static void bt_append(bus_text_builder_t* b,'):bus.index('\nvoid bus_text_bool(')]
native_state = hook[hook.index('static struct {'):hook.index('/* Forward declarations */')]
reset_start = hook.index('static void rgd_maneuver_map_reset(void) {')
reset = hook[reset_start:hook.index('static const uint64_t RGD_UPD_WRITE_MASK', reset_start)]
c = r'''
#include "routeguidance/rgd_tlv.h"
#include "framework/bus.h"
#include <assert.h>
static uint64_t now_monotonic_ms(void) { return 100; }
'''+native_state+'\n'+reset+r'''
static int rgd_lane_slot_for_iap_index(uint16_t idx, bool create) { (void)idx; (void)create; return -1; }
'''+text_writer+'\n'+slot_writer+r'''
static size_t field(uint8_t *buf,size_t off,uint16_t key,const uint8_t *value,size_t size) {
    write_be16(buf+off,size+4);write_be16(buf+off+2,key);
    if(size)memcpy(buf+off+4,value,size);return off+size+4;
}
static size_t num(uint8_t *buf,size_t off,uint16_t key,int value,int size) {
    uint8_t p[4];if(size==1)p[0]=value;else if(size==2)write_be16(p,value);else write_be32(p,value);
    return field(buf,off,key,p,size);
}
static void sample(const char *path,int which) {
    uint8_t raw[512]={0};size_t n=6;
    n=num(raw,n,MAN_TLV_INDEX,which?50:10,2);
    n=num(raw,n,MAN_TLV_TYPE,which?2:1,1);
    n=num(raw,n,MAN_TLV_JUNCTION_TYPE,0,1);
    if(which!=1)n=num(raw,n,MAN_TLV_EXIT_ANGLE,which?90:-90,2);
    if(!which) {
        for(int i=-90;i<=90;i+=90)n=num(raw,n,MAN_TLV_JUNCTION_ANGLES,i,2);
        n=num(raw,n,MAN_TLV_DISTANCE_BETWEEN,6000,4);
        n=field(raw,n,MAN_TLV_AFTER_ROAD_NAME,(const uint8_t*)"Old road",8);
    }
    raw[0]=raw[1]=0x40;write_be16(raw+2,n);write_be16(raw+4,0x5202);
    rgd_maneuver_t m;assert(rgd_parse_maneuver(raw,n,&m));
    if(which==1)assert(m.exit_angle==1000 && (m.present&RGD_MAN_EXIT_ANGLE));
    uint8_t text[8192];bus_text_builder_t b={text,sizeof(text),0,false,false};
    bus_text_uint(&b,"route_generation",g_rgd.route_generation);bus_text_int(&b,"route_state",1);bus_text_int(&b,"maneuver_count",1);bus_text_str(&b,"maneuver_list","0");
    g_rgd.slot_ver[0]=(which==0 || which==3)?1:34;write_slot_data_keys(&b,0,&m);assert(!b.overflow);
    FILE *f=fopen(path,"wb");assert(f);assert(fwrite(text,1,b.len,f)==b.len);fclose(f);
}
int main(int argc,char **argv) {
    assert(argc==5);
    rgd_maneuver_map_reset();assert(g_rgd.route_generation==100);
    for(int i=0;i<3;i++)sample(argv[i+1],i);
    g_rgd.ver_counter=34;g_rgd.lane_cache[3].present=1;
    rgd_maneuver_map_reset();
    assert(g_rgd.route_generation==101 && g_rgd.ver_counter==0);
    assert(g_rgd.slot_cache[0].present==0 && g_rgd.lane_cache[3].present==0);
    // Same clock, slot and version as the first route; only generation differs.
    sample(argv[4],3);
    return 0;
}
'''
(BUILD / 'native_input.c').write_text(c)
subprocess.run(['cc','-std=c99','-O1','-fsanitize=' + SANITIZE,'-fno-omit-frame-pointer','-DENABLE_LOGGING=0','-I'+str(ROOT/'hook'),str(BUILD/'native_input.c'),str(ROOT/'hook/routeguidance/rgd_tlv.c'),'-o',str(BUILD/'native_input')],check=True)
frames = [BUILD / name for name in ('old-slot.txt','new-no-angle.txt','new-known-angle.txt','new-generation.txt')]
subprocess.run([str(BUILD/'native_input'),*map(str,frames)],check=True)
if os.environ.get('RGD_CONTRACT_STAGE') == 'native':
    print('Native RGI contract frames: ' + ' '.join(frame.name for frame in frames))
    sys.exit(0)

environment = dict(os.environ, JAVA_SKIP_BUILD='1', RGD_CONTRACT_FRAMES=str(BUILD.resolve()))
result = subprocess.run(['bash', str(ROOT / 'scripts/java/docker.sh'), 'test', 'contract'],
                        env=environment, check=True, text=True, capture_output=True)
(BUILD/'native-input-result.txt').write_text(result.stdout)
print(result.stdout,end='')
