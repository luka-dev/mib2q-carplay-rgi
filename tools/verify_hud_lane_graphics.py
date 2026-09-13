#!/usr/bin/env python3
"""Verify original HUD lane-cell state tables by executing the ARM GDC dispatcher.

This is INTERNAL packet -> original surfaces, not a BAP interpretation. Raw
firmware remains read-only. The finite declared states come directly from FD62.
"""
from pathlib import Path
import argparse, hashlib, json, struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
from verify_hud_distance_bargraph import FIRMWARES, GDC, GDC_FILE, GDC_SIZE, SCENE, GLOBALS, PACKET, STACK, STOP, ELEMENT_DISPATCH, DRAW_SURFACE, TRACK_SURFACE


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('firmware',type=Path)
    ap.add_argument('output',type=Path)
    args=ap.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    blobs={n:(args.firmware/'blocks'/n).read_bytes() for n in next(iter(FIRMWARES.values()))}
    hashes={n:hashlib.sha256(b).hexdigest() for n,b in blobs.items()}
    version=next((n for n,h in FIRMWARES.items() if h==hashes),None)
    assert version,'Unknown firmware'
    asset=args.firmware.parent/'FL_8W6919617G_1952_APPL_V000'/'navigation_icons'
    resources=json.loads((asset/'raster_resources_manifest.json').read_text())['resources']
    resources={(r['bank'],r['data_offset']):r for r in resources}
    names=json.loads((asset/'maneuvers_manifest.json').read_text())['named_3d_variants']
    names={r['resource_id']:r['name'] for r in names}
    scene=blobs['FD_62FLASHDATA.bin'];page=0x48ad4;group=page+8;nodes=[]
    for _ in range(scene[page+5]):
        size,bit,n=struct.unpack_from('<IBxH',scene,group);node=group+8
        for _ in range(n):
            sz,kind,typ,field,mask,count=struct.unpack_from('<IBBHBxH',scene,node)
            if bit==10:
                states=[]
                for selector in range(count):
                    entry=node+12+selector*8;rel,ns=struct.unpack_from('<IB',scene,entry);surfaces=[]
                    for i in range(ns):
                        off=entry+rel+i*44;raw=scene[off:off+44]
                        r=resources[(raw[39],struct.unpack_from('<I',raw,4)[0])]
                        x,y,w,h=struct.unpack_from('<4H',raw,30)
                        surfaces.append(dict(resource=r['id'],name=names.get(r['id']),x=x,y=y,width=w,height=h,
                            png=str(asset/'raster_resources'/r['png']),descriptor_hex=raw.hex()))
                    states.append(dict(selector=selector,surfaces=surfaces))
                nodes.append(dict(node=hex(node),kind=kind,selector_type=typ,field=field,mask=mask,states=states))
            node+=sz
        assert node==group+size;group+=size
    assert len(nodes)==23
    uc=Uc(UC_ARCH_ARM,UC_MODE_ARM)
    uc.mem_map(GDC,(GDC_SIZE+4095)&~4095);uc.mem_write(GDC,blobs['FD_61FLASHDATA.bin'][GDC_FILE:GDC_FILE+GDC_SIZE])
    uc.mem_map(SCENE,len(scene));uc.mem_write(SCENE,scene)
    for addr,size in [(GLOBALS,0x20000),(PACKET,0x1000),(STACK,0x10000),(STOP,0x1000)]:uc.mem_map(addr,size)
    captured=[]
    def boundary(vm,address,size,_):
        if address==DRAW_SURFACE:captured.append(bytes(vm.mem_read(vm.reg_read(UC_ARM_REG_R0),44)).hex())
        if address in (DRAW_SURFACE,TRACK_SURFACE):
            vm.reg_write(UC_ARM_REG_R0,0);vm.reg_write(UC_ARM_REG_PC,vm.reg_read(UC_ARM_REG_LR))
    uc.hook_add(UC_HOOK_CODE,boundary);passed=0
    for node in nodes:
        shift=(node['mask']&-node['mask']).bit_length()-1
        states=node['states']+[dict(selector=65535 if node['selector_type']==2 else (node['mask']>>shift),surfaces=[])]
        for state in states:
            uc.mem_write(GLOBALS,bytes(0x20000));uc.mem_write(0x6F7750,b'\x02')
            packet=bytearray([255]*128);packet[8]=3;field=node['field']+13
            if node['selector_type']==2:struct.pack_into('<H',packet,field,state['selector'])
            else:packet[field]=(state['selector']<<shift)&node['mask']
            captured.clear();uc.mem_write(PACKET,bytes(packet))
            for reg,value in [(UC_ARM_REG_R0,PACKET),(UC_ARM_REG_R1,SCENE+int(node['node'],16)),(UC_ARM_REG_SP,STACK+0xF000),(UC_ARM_REG_LR,STOP)]:uc.reg_write(reg,value)
            uc.emu_start(ELEMENT_DISPATCH,STOP,count=100000)
            assert uc.reg_read(UC_ARM_REG_PC)==STOP
            assert captured==[s['descriptor_hex'] for s in state['surfaces']],(node['node'],state['selector'],captured)
            passed+=1
    result=dict(firmware=version,sha256=hashes,group=10,cases_passed=passed,scope='Original ARM internal lane packet to surface submissions',nodes=nodes)
    (args.output/(version+'-lane-selectors.json')).write_text(json.dumps(result,indent=2)+'\n')
    print(version,passed,'declared/empty lane states match original ARM execution')

if __name__=='__main__':main()
