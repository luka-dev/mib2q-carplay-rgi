#!/usr/bin/env python3
"""Check duplicated constants at shipping Java/C transport boundaries (no HU)."""
import ast
import re
import struct
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def numeric(expr):
    expr = re.sub(r"\b(0x[0-9a-fA-F]+|[0-9]+)[uUlL]+\b", r"\1", expr)
    expr = re.sub(r"\((?:byte|int)\)", "", expr).strip()
    tree = ast.parse(expr, mode="eval")
    def value(node):
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.BinOp):
            a, b = value(node.left), value(node.right)
            if isinstance(node.op, ast.Mult): return a * b
            if isinstance(node.op, ast.LShift): return a << b
            if isinstance(node.op, ast.BitOr): return a | b
        raise ValueError(f"unsupported integer constant: {expr}")
    return value(tree.body)


def constants(path, java=False):
    text = (ROOT / path).read_text()
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    pattern = r"\b(?:int|byte)\s+(\w+)\s*=\s*([^;]+);" if java else r"^#define\s+(\w+)\s+([^\n]+)"
    return dict(re.findall(pattern, text, re.M))


checks = 0

def compare(c_path, java_path, mapping, shared=()):
    global checks
    c, j = constants(c_path), constants(java_path, True)
    pairs = list(mapping.items())
    pairs += [(key, key) for key in j if key.startswith(shared) and key in c]
    for cn, jn in pairs:
        cv, jv = numeric(c[cn]), numeric(j[jn])
        if cv != jv:
            raise SystemExit(f"Protocol mismatch: {c_path}:{cn}={cv}, {java_path}:{jn}={jv}")
        checks += 1


compare("hook/framework/bus_protocol.h", "java_patch/com/luka/carplay/bus/CarplayBus.java", {
    "BUS_TCP_PORT": "PORT", "BUS_MAGIC": "MAGIC", "BUS_HEADER_SIZE": "HEADER_SIZE",
    "BUS_MAX_PAYLOAD": "MAX_PAYLOAD", "BUS_FLAG_STICKY": "FLAG_STICKY",
    "BUS_FLAG_BINARY": "FLAG_BINARY", "BUS_FLAG_REPLAY": "FLAG_REPLAY",
}, ("CMD_", "EVT_"))
compare("maneuver_render/protocol.h", "java_patch/com/luka/carplay/rgd/RendererServer.java", {
    "CR_TCP_PORT": "PORT", "CR_PKT_SIZE": "PKT_SIZE",
}, ("CMD_", "EVT_", "MAN_FLAG_"))
compare("altscreen_render/protocol.h", "java_patch/com/luka/carplay/core/AltscreenControlServer.java", {
    "ALTCTL_PORT": "PORT", "ALTEVT_VIDEO_ON": "EVT_VIDEO_ON", "ALTEVT_VIDEO_OFF": "EVT_VIDEO_OFF",
    "ALTCMD_VIEW_AREA": "CMD_VIEW_AREA",
})
print(f"Local transport constants: {checks} matching Java/C ports, opcodes, flags, sizes and limits")
jar = ROOT / "build/carplay_hook.jar"
if not jar.exists():
    raise SystemExit("Build Java first to check the shipping class-file target")
with zipfile.ZipFile(jar) as archive:
    classes = [name for name in archive.namelist() if name.endswith(".class")]
    if not classes:
        raise SystemExit("Empty patch JAR")
    for name in classes:
        magic, minor, major = struct.unpack(">IHH", archive.read(name)[:8])
        if magic != 0xCAFEBABE or (minor, major) != (0, 48):
            raise SystemExit(f"Unexpected Java 1.4 target: {name} version={major}.{minor}")
    print(f"Shipping JAR: {len(classes)} classes at Java 1.4 class-file version 48.0")
