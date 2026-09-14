#!/usr/bin/env python3
"""Real C TLV parser, cache reset and slot/text writer -> Java -> stock BAP sender.

Extract source functions verbatim for the host: only time, slot assignment and
linked-lane lookup are fixtures. Does not emulate QNX services or HUD hardware.
Run after scripts/build_java.sh; generated sources/frames stay under build/.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/rgd-native-contract'
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
subprocess.run(['cc','-std=c99','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DENABLE_LOGGING=0','-I'+str(ROOT/'hook'),str(BUILD/'native_input.c'),str(ROOT/'hook/routeguidance/rgd_tlv.c'),'-o',str(BUILD/'native_input')],check=True)
frames = [BUILD / name for name in ('old-slot.txt','new-no-angle.txt','new-known-angle.txt','new-generation.txt')]
subprocess.run([str(BUILD/'native_input'),*map(str,frames)],check=True)
java = r'''
import com.luka.carplay.rgd.*;
import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;
import java.nio.file.*;
import java.lang.reflect.*;
import java.util.Arrays;
public class NativeInputProbe {
    static void parse(RouteGuidance rg,String file) throws Exception {
        byte[] bytes=Files.readAllBytes(Paths.get(file));
        Method m=RouteGuidance.class.getDeclaredMethod("parse",CarplayBus.Data.class);m.setAccessible(true);
        m.invoke(rg,CarplayBus.parseText(bytes,bytes.length));
    }
    static RouteGuidance.State state(RouteGuidance rg) throws Exception {return (RouteGuidance.State)ManeuverChainAudit.get(rg,"state");}
    static byte[] hud(ManeuverChainAudit audit,RouteGuidance rg) throws Exception {
        audit.sendBap.invoke(audit.bridge,state(rg));return audit.sender.input[0].sideStreets;
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);ManeuverChainAudit audit=new ManeuverChainAudit();
        RouteGuidance old=new RouteGuidance();parse(old,args[0]);parse(old,args[1]);
        RouteGuidance.State s=state(old);
        System.out.println("ACTUAL C -> JAVA, missing angle: angle="+s.mTurnAngle[0]+", exit="+s.mExitAngle[0]+", HUD roads="+Arrays.toString(hud(audit,old)));
        if(s.mTurnAngle[0]!=1000 || s.mExitAngle[0]!=1000)throw new AssertionError("C sentinel lost");
        parse(old,args[2]);RouteGuidance fresh=new RouteGuidance();parse(fresh,args[2]);
        byte[] inherited=hud(audit,old),clean=hud(audit,fresh);
        System.out.println("ACTUAL C -> JAVA, known new angle: angle="+s.mTurnAngle[0]+", inherited roads="+Arrays.toString(s.mJunctionAngles[0])+", road="+s.mAfterRoad[0]+", step="+s.mDistance[0]);
        System.out.println("ACTUAL JAVA -> BAP, same new input: reused-slot sideStreets="+Arrays.toString(inherited)+", fresh-slot sideStreets="+Arrays.toString(clean));
        if(!Arrays.equals(inherited,clean) || s.mJunctionAngles[0]!=null || s.mAfterRoad[0]!=null || s.mDistance[0]!=-1)
            throw new AssertionError("new native slot inherited omitted fields");
        RouteGuidance direct=new RouteGuidance();parse(direct,args[0]);parse(direct,args[2]);
        if(!Arrays.equals(hud(audit,direct),clean))throw new AssertionError("known-angle slot reassignment differs from a fresh slot");
        RouteGuidance reset=new RouteGuidance();parse(reset,args[0]);parse(reset,args[3]);
        if(state(reset).routeGeneration!=101 || state(reset).mVer[0]!=2
                || state(reset).mAfterRoad[0]!=null || state(reset).mJunctionAngles[0]!=null)
            throw new AssertionError("native reset generation not received");
        System.out.println("Native RGI contract: source sentinel, slot replacement, same-clock route reset and identical clean HUD descriptors PASS");
    }
}
'''
(BUILD/'NativeInputProbe.java').write_text(java)
tools = ROOT.parent.parent/'Tools/jxe2jar'
jdk=tools/'jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home'
cp=':'.join(map(str,[ROOT/'build/carplay_hook.jar',tools/'out/MU1316-final.jar',tools/'libs/org.osgi.framework-1.10.0.jar',tools/'libs/org.osgi.util.tracker-1.5.4.jar']))
subprocess.run([str(jdk/'bin/javac'),'-encoding','UTF-8','-cp',cp,'-d',str(BUILD),str(ROOT/'tests/ManeuverChainAudit.java'),str(BUILD/'NativeInputProbe.java')],check=True)
result=subprocess.run([str(jdk/'bin/java'),'-Xverify:none','-cp',str(BUILD)+':'+cp,'NativeInputProbe',*map(str,frames)],check=True,text=True,capture_output=True)
(BUILD/'native-input-result.txt').write_text(result.stdout)
print(result.stdout,end='')
