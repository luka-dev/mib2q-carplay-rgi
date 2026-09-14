#!/usr/bin/env python3
"""Draw a fictional navigation frame for the local transition experiment."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
SCALE = 3


def render(width, height):
    im = Image.new('RGB', (width*SCALE, height*SCALE), '#151c23')
    d = ImageDraw.Draw(im)
    def line(points, color, stroke):
        d.line([(round(x*SCALE), round(y*SCALE)) for x,y in points], fill=color,
               width=round(stroke*SCALE), joint='curve')
    def polygon(points, color):
        d.polygon([(round(x*SCALE), round(y*SCALE)) for x,y in points], fill=color)
    def text(at, value, size, color, anchor='la'):
        f = ImageFont.truetype('/System/Library/Fonts/SFNS.ttf', round(size*SCALE))
        d.text((at[0]*SCALE,at[1]*SCALE), value, font=f, fill=color, anchor=anchor)
    def rect(box, radius, color):
        d.rounded_rectangle(tuple(round(v*SCALE) for v in box), radius=radius*SCALE, fill=color)
    cx,cy=width*.61,height*.64
    # Fictional map: local grid, a park, water and one highlighted route.
    polygon([(width*.73,0),(width,0),(width,height),(width*.88,height),
             (width*.83,height*.57),(width*.78,height*.4)], '#142d3b')
    polygon([(cx-120,cy-118),(cx-23,cy-151),(cx+24,cy-104),
             (cx-43,cy-67),(cx-125,cy-74)], '#1d3430')
    for j in range(-8,12):
        x=cx+j*47
        road=[(x-80,0),(x+height*.38,height)]
        line(road,'#0d151c',11);line(road,'#303b46',6)
    for j in range(-6,8):
        y=cy+j*38
        road=[(0,y+width*.10),(width,y-width*.16)]
        line(road,'#0d151c',12);line(road,'#303b46',7)
    boulevard=[(0,height*.91),(cx-140,cy+58),(cx,cy),(cx+70,cy-70),(width,height*.22)]
    line(boulevard,'#101923',21);line(boulevard,'#45505b',14)
    route=[(cx-46,height+30),(cx-22,cy+65),(cx,cy),(cx+36,cy-72),(cx+151,cy-102)]
    line(route,'#072347',13);line(route,'#229bff',8);line(route,'#7cc8ff',2)
    text((cx-88,cy-53),'GROVE PARK',9,'#728e80')
    text((cx+56,cy+36),'Harbor Ave',10,'#95a5b5')
    if width>500:
        text((cx-157,cy+92),'Riverside Dr',11,'#95a5b5')
        text((cx+155,cy-52),'Marina',10,'#7394ac')
    # Route position marker.
    mx,my=cx,cy
    d.ellipse(((mx-16)*SCALE,(my-16)*SCALE,(mx+16)*SCALE,(my+16)*SCALE),fill='#1e5684')
    polygon([(mx,my-12),(mx-9,my+11),(mx,my+6),(mx+9,my+11)],'#edf9ff')
    panelw=205 if width>500 else 183
    rect((15,14,15+panelw,79),15,'#080d13')
    line([(36,61),(36,35),(47,35)],'#eef6ff',3)
    polygon([(43,28),(52,35),(43,42)],'#eef6ff')
    text((63,21),'250 m',23,'#f4f8ff')
    text((63,51),'Harbor Avenue',12,'#a7b5c7')
    rect((15,height-55,170,height-14),13,'#080d13')
    text((28,height-47),'9 min',17,'#f0f6ff')
    text((91,height-42),'3.2 km',12,'#99aabe')
    rect((width-44,15,width-15,44),14,'#0b131b')
    text((width-29,21),'N',13,'#d0dfe8',anchor='ma')
    return im.resize((width,height),Image.Resampling.LANCZOS)


if __name__ == '__main__':
    render(1440,542).save(HERE/'stream_demo.png')
    print('Fictional full-panel map: 1440x542')
