#!/usr/bin/env python3
"""Real host renderer / TCP check of independent atomic lane redraw and clear."""
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import time
from PIL import Image, ImageChops

ROOT=Path(__file__).resolve().parents[2]


def packet(command,payload=b'',flags=0):
    return bytes((command,flags))+payload.ljust(46,b'\0')


def main():
    subprocess.run(['make','-C',str(ROOT/'maneuver_render'),'BUILD_DIR=../build/lane-panel-host','all'],check=True)
    with tempfile.TemporaryDirectory(prefix='lane-wire-',dir=ROOT/'build') as folder:
        folder=Path(folder)
        shutil.copyfile(ROOT/'maneuver_render/resources/flag_atlas.rgba',folder/'flag_atlas.rgba')
        with socket.socket() as listener, (folder/'renderer.log').open('w') as log:
            listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(10)
            proc=subprocess.Popen([str(ROOT/'build/lane-panel-host/maneuver_render'),
                '--port='+str(listener.getsockname()[1])],cwd=folder,stdout=log,stderr=log)
            try:
                with listener.accept()[0] as peer:
                    peer.settimeout(10)
                    while True:
                        event=b''
                        while len(event)<48:
                            chunk=peer.recv(48-len(event));assert chunk, 'Renderer disconnected before READY'
                            event+=chunk
                        if event[0]==0x81:break
                    serial=0

                    def snap(label):
                        nonlocal serial
                        target=folder/f'snap_{serial:03d}_{label}.ppm';serial+=1
                        peer.sendall(packet(2,label.encode()))
                        deadline=time.monotonic()+5
                        while time.monotonic()<deadline:
                            try:
                                with Image.open(target) as im:return im.copy()
                            except (OSError,ValueError):time.sleep(.02)
                        raise AssertionError(f'Renderer did not capture {label}')

                    def batch(token,showing=True,count=8,best=7,event=73):
                        p=bytearray(46);struct.pack_into('>I',p,0,token)
                        p[4:7]=bytes((count,1,int(showing)));struct.pack_into('>i',p,8,event)
                        begin=packet(12,p);records=[]
                        for i in range(count):
                            p=bytearray(46);struct.pack_into('>I',p,0,token);p[4]=i
                            struct.pack_into('>H',p,5,i);p[7]=2 if i==best else 0;p[8]=1
                            struct.pack_into('>hh',p,9,45 if i==best else 0,45 if i==best else 0)
                            records.append(packet(13,p))
                        return begin,records,packet(14,struct.pack('>I',token))

                    def settled(label):
                        previous=snap(label)
                        for _ in range(30):
                            time.sleep(.2);current=snap(label)
                            if ImageChops.difference(previous,current).getbbox() is None:return current
                            previous=current
                        raise AssertionError('Native maneuver/framing did not settle: '+label)

                    p=bytearray(46);p[0]=2;struct.pack_into('>h',p,2,90)
                    p[43:46]=bytes((1,4,1));peer.sendall(packet(1,p,7));time.sleep(1.8)
                    before=snap('ordinary')
                    begin,records,commit=batch(1)
                    peer.sendall(begin+b''.join(records[:-1]))
                    assert ImageChops.difference(before,snap('partial')).getbbox() is None
                    # Real packet fragmentation, then atomic commit.
                    wire=records[-1]+commit
                    for i in range(0,len(wire),7):peer.sendall(wire[i:i+7])
                    time.sleep(.3);small=snap('small');diff=ImageChops.difference(before,small)
                    assert diff.getbbox() is not None, 'Committed lane event did not trigger redraw'
                    assert diff.crop((0,0,diff.width,round(132*diff.height/181))).getbbox() is not None, 'Camera did not dolly for lanes'
                    peer.sendall(packet(8,struct.pack('>4H',0,0,328,180)));time.sleep(.4)
                    wide=snap('wide');assert ImageChops.difference(small,wide).getbbox() is not None
                    peer.sendall(packet(8,struct.pack('>4H',59,27,210,153)));time.sleep(.4)
                    assert ImageChops.difference(small,snap('small_again')).getbbox() is None
                    begin,records,commit=batch(11,best=6)
                    peer.sendall(begin+b''.join(records)+commit);time.sleep(.35)
                    changed=snap('lane_update');diff=ImageChops.difference(small,changed)
                    assert diff.getbbox() is not None, 'Same-event recommendation did not redraw'
                    assert diff.crop((0,0,diff.width,round(132*diff.height/181))).getbbox() is None, 'Lane update changed the maneuver'
                    begin,records,commit=batch(12,event=99)
                    peer.sendall(begin+b''.join(records)+commit);time.sleep(.35)
                    assert ImageChops.difference(small,snap('new_lane_event')).getbbox() is None, 'Lane event id triggered maneuver travel'
                    begin,records,commit=batch(2,count=2)
                    peer.sendall(begin+records[0]+commit)
                    time.sleep(.35) # camera returns smoothly after guidance clears
                    assert ImageChops.difference(before,snap('malformed')).getbbox() is None
                    begin,records,commit=batch(3);peer.sendall(begin+b''.join(records)+commit);time.sleep(.2)
                    begin,records,commit=batch(4,showing=False,count=0);peer.sendall(begin+commit)
                    time.sleep(.35)
                    assert ImageChops.difference(before,snap('hidden')).getbbox() is None
                    # Repeat on/off quickly without a single maneuver command.
                    for token in range(20,26,2):
                        begin,records,commit=batch(token);peer.sendall(begin+b''.join(records)+commit);time.sleep(.06)
                        begin,records,commit=batch(token+1,showing=False,count=0);peer.sendall(begin+commit)
                        time.sleep(.35)
                        assert ImageChops.difference(before,snap('quick_hide')).getbbox() is None, 'Rapid hide left stale lanes/framing'
                    begin,records,commit=batch(30);peer.sendall(begin+b''.join(records)+commit);time.sleep(.35)
                    assert ImageChops.difference(small,snap('reappear')).getbbox() is None
                    # The same guidance survives a native maneuver transition;
                    # its hide/show remains actionable while the camera travels.
                    p[1]=255;struct.pack_into('>h',p,2,-90)
                    peer.sendall(packet(1,p,7));time.sleep(.12)
                    begin,records,commit=batch(31,showing=False,count=0);peer.sendall(begin+commit);time.sleep(.08)
                    begin,records,commit=batch(32);peer.sendall(begin+b''.join(records)+commit);time.sleep(2.0)
                    next_lanes=settled('next_with_lanes')
                    begin,records,commit=batch(33,showing=False,count=0);peer.sendall(begin+commit);time.sleep(.35)
                    next_plain=snap('next_plain')
                    assert ImageChops.difference(next_plain,before).getbbox() is not None, 'Maneuver transition did not complete'
                    assert ImageChops.difference(next_lanes,next_plain).getbbox() is not None, 'Mid-transition lane reappearance lost'
                    begin,records,commit=batch(34);peer.sendall(begin+b''.join(records)+commit);time.sleep(.35)
                    assert ImageChops.difference(next_lanes,snap('next_reappear')).getbbox() is None, 'Lane state depended on maneuver transition phase'
                    begin,records,commit=batch(5);peer.sendall(begin+b''.join(records)+commit);time.sleep(.2)
                    peer.sendall(packet(7));assert snap('clear').getbbox() is None
                    peer.sendall(packet(3));proc.wait(timeout=5)
                    assert proc.returncode==0
            finally:
                if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
    print('PASS: real TCP atomic/fragmented commit, same-maneuver updates and rapid show/hide/reappear, independent event switch, hide/reappear during native travel, camera dolly/restore, 5/8 stage roundtrip, malformed/hidden/CLEAR cleanup')


if __name__=='__main__':main()
