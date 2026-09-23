# Renders the README demo GIF: stands in for BAPBridge on :19800, drives maneuver_render
# through a scripted route and asks for one PPM screenshot per frame (30 fps).
# Usage (macOS, after `make`), from an empty output dir:
#   cp <repo>/maneuver_render/resources/flag_atlas.rgba .
#   python3 <repo>/maneuver_render/demo_sequence.py & sleep 0.5; <repo>/build/maneuver_render
#   ffmpeg -framerate 30 -pattern_type glob -i 'snap_*.ppm' -vf "fps=25,split[a][b];\
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

# (maneuver packet, seconds, progress countdown from, to)
ROUTE = [
    (dict(icon=ICON_APPROACH, junctions=(90, -90)), 3.0, 16, 10),
    (dict(icon=ICON_TURN, angle=90, junctions=(-90, 0)), 3.5, 16, 0),
    (dict(icon=ICON_ROUNDABOUT, angle=0, junctions=(90, 45, 0, -45, -90)), 3.5, 16, 0),
    (dict(icon=ICON_EXIT, direction=1), 3.0, 16, 0),
    (dict(icon=ICON_ROUNDABOUT, angle=-90, junctions=(120, 80, 40, 0, -40, -90, -140)), 3.5, 16, 0),
    (dict(icon=ICON_TURN, angle=-135, junctions=(90, 30)), 3.0, 16, 0),
    (dict(icon=ICON_ARRIVED, direction=1), 3.5, 0, 0),
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
for m, secs, lv0, lv1 in ROUTE:
    c.sendall(maneuver(level=lv0, **m))
    frames = int(secs * FPS)
    last = None
    for f in range(frames):
        lv = round(lv0 + (lv1 - lv0) * f / max(1, frames - 1))
        st = BLINK_LOW if lv <= 2 and lv0 else FILL
        if (lv, st) != last:
            c.sendall(progress(lv, st))
            last = (lv, st)
        time.sleep(1 / FPS)
        c.sendall(pkt(0x02, 0, b"f%04d" % n))
        n += 1
        drain()
time.sleep(0.5)
c.sendall(pkt(0x03))
print("frames", n)
