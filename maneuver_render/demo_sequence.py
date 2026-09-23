# Renders the README demo GIF: stands in for BAPBridge on :19800, drives maneuver_render
# through a scripted route and asks for one PPM screenshot per frame (30 fps).
# Usage (macOS, after `make`), from an empty output dir:
#   cp <repo>/maneuver_render/resources/flag_atlas.rgba .
#   python3 <repo>/maneuver_render/demo_sequence.py & sleep 0.5; <repo>/build/maneuver_render
#   for f in snap_*.ppm; do ln -s $f ${f##*_}; done   # snap counter outgrows %03d; order by label
#   ffmpeg -framerate 30 -pattern_type glob -i 'f*.ppm' -vf "fps=25,split[a][b];\
#     [a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:\
#     bayer_scale=4:diff_mode=rectangle" maneuver_demo.gif
import socket, struct, time

PKT = 48
FPS = 30
ICON_APPROACH, ICON_TURN, ICON_EXIT, ICON_ROUNDABOUT, ICON_ARRIVED = 1, 2, 5, 6, 7
FILL, BLINK_LOW = 1, 2

def pkt(cmd, flags=0, payload=b""):
    return bytes([cmd, flags]) + payload.ljust(46, b"\0")

def maneuver(icon, angle=0, direction=0, junctions=(), level=16, state=FILL):
    p = bytearray(46)
    p[0] = icon
    p[1] = direction & 0xFF
    p[2:4] = struct.pack(">h", angle)
    p[5] = len(junctions)
    for i, j in enumerate(junctions):
        p[6 + 2 * i:8 + 2 * i] = struct.pack(">h", j)
    p[42] = state
    p[43] = 1                   # perspective on
    p[44] = level
    p[45] = 1
    return pkt(0x01, 0x01 | 0x02 | 0x20, bytes(p))

def progress(level, state=FILL):
    return pkt(0x06, 0x20, bytes([level, 1, state]))

BLINK_HIGH = 3
BLINK_S = 0.6                   # BAPBridge ACTION_BLINK_INTERVAL_MS

def timeline(hold=0.8, fill=3.5, blink=3.0):
    """Per-frame (level, state): settle full, count down to 0, then blink at 0."""
    t = [(16, FILL)] * int(hold * FPS)
    n = int(fill * FPS)
    t += [(round(16 - 16 * f / (n - 1)), FILL) for f in range(n)]
    t += [(0, BLINK_LOW if int(f / FPS / BLINK_S) % 2 == 0 else BLINK_HIGH)
          for f in range(int(blink * FPS))]
    return t

ROUTE = [
    (dict(icon=ICON_APPROACH, junctions=(90, -90)), timeline(blink=0)),
    (dict(icon=ICON_TURN, angle=90, junctions=(-90, 0)), timeline()),
    (dict(icon=ICON_ROUNDABOUT, angle=0, junctions=(90, 45, 0, -45, -90)), timeline()),
    (dict(icon=ICON_EXIT, direction=1), timeline()),
    (dict(icon=ICON_ROUNDABOUT, angle=-90, junctions=(120, 80, 40, 0, -40, -90, -140)), timeline()),
    (dict(icon=ICON_TURN, angle=-135, junctions=(90, 30)), timeline()),
    (dict(icon=ICON_ARRIVED, direction=1), timeline(fill=2.5, blink=0) + [(0, FILL)] * int(2.5 * FPS)),
]

srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", 19800))
srv.listen(1)
c, _ = srv.accept()
c.setblocking(False)

def drain():
    try:
        while c.recv(4096):
            pass
    except BlockingIOError:
        pass

time.sleep(1.5)
drain()
n = 0
for m, frames in ROUTE:
    c.sendall(maneuver(level=frames[0][0], state=frames[0][1], **m))
    last = frames[0]
    for f in frames:
        if f != last:
            c.sendall(progress(*f))
            last = f
        time.sleep(1 / FPS)
        c.sendall(pkt(0x02, 0, b"f%04d" % n))
        n += 1
        drain()
time.sleep(0.5)
c.sendall(pkt(0x03))
print("frames", n)
