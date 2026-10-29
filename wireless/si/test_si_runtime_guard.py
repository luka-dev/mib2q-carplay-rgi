"""Host-execute the SAME C guard linked into the target adapter."""
import ctypes
import struct
import subprocess
from pathlib import Path


def run_guard_tests(si_dir, cand, manifest, workdir):
    libpath = workdir / "si_guard_test.so"
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
        "-I", str(si_dir), "-I", str(workdir / "cand"),
        str(si_dir / "si_runtime_guard.c"), "-o", str(libpath)], check=True)
    lib = ctypes.CDLL(str(libpath))
    reader_type = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32,
                                  ctypes.c_void_p, ctypes.c_size_t)

    class Snapshot(ctypes.Structure):
        _fields_ = [("controller", ctypes.c_uint32), ("impl", ctypes.c_uint32),
                    ("listener", ctypes.c_uint32)]

    lib.si_guard_candidate.argtypes = [reader_type, ctypes.c_void_p]
    lib.si_guard_ready.argtypes = [reader_type, ctypes.c_void_p, ctypes.POINTER(Snapshot)]
    memory = bytearray(0x100000)
    # Stage1 addresses under test are within [0x100000,0x200000).
    memory[:len(cand)] = cand
    # Mirror runtime writable segment using program headers, not VA-base.
    phoff = manifest["elf"]["e_phoff"]
    for i in range(manifest["elf"]["e_phnum"]):
        p = struct.unpack_from("<8I", cand, phoff + i * 32)
        if p[0] == 1:
            off = p[2] - 0x100000
            memory[off:off + p[4]] = cand[p[1]:p[1] + p[4]]
            memory[off + p[4]:off + p[5]] = bytes(p[5] - p[4])
    heap = bytearray(0x4000)
    failed = False
    state_reads = 0
    revoke = False

    def store(va, value):
        data, base = (memory, 0x100000) if va < 0x200000 else (heap, 0x300000)
        struct.pack_into("<I", data, va - base, value)

    def read(_, va, out, size):
        nonlocal state_reads
        if failed: return 0
        if va == 0x302090:
            state_reads += 1
            if revoke and state_reads == 2: return 0
        for base, data in ((0x100000, memory), (0x300000, heap)):
            if base <= va and va + size <= base + len(data):
                ctypes.memmove(out, bytes(data[va - base:va - base + size]), size)
                return 1
        return 0

    reader = reader_type(read)
    total = 0

    def check(value):
        nonlocal total
        assert value
        total += 1

    check(lib.si_guard_candidate(reader, None) == 1)
    # Reject a mapped, writable, zero root in an unrelated executable.
    for va in (0x100000, 0x100034, 0x17fdb4, 0x1800b0, 0x17fc48,
               0x128dbc, 0x1293d8, 0x13f4f4, 0x128978, 0x180944,
               0x16635c, 0x1c6000, 0x1c6004, 0x1c60e8, 0x180704,
               0x129050, 0x1bf248, 0x13f2b4, 0x13f32c):
        off = va - 0x100000
        memory[off] ^= 1
        check(lib.si_guard_candidate(reader, None) == 0)
        memory[off] ^= 1
    store(0x1c5e00, 1)
    check(lib.si_guard_candidate(reader, None) == 0)
    store(0x1c5e00, 0)
    failed = True
    check(lib.si_guard_candidate(reader, None) == 0)
    failed = False
    snap = Snapshot()
    check(lib.si_guard_ready(reader, None, ctypes.byref(snap)) == 0)
    store(0x1c5928, 0x300000)
    store(0x300cc4, 0x302000)
    store(0x302090, 0)
    store(0x1c3c94, 0x3000d8)
    store(0x3000dc, 0x1c1000)
    store(0x1c1018, 0x1653c0)
    check(lib.si_guard_ready(reader, None, ctypes.byref(snap)) == 1)
    check((snap.controller, snap.impl, snap.listener) == (0x300000, 0x302000, 0x3000d8))
    for va, valid, invalid in ((0x1c5928, 0x300000, 0), (0x300cc4, 0x302000, 0),
        (0x302090, 0, 2), (0x302090, 0, 1), (0x302090, 0, 0xffffffff),
        (0x1c3c94, 0x3000d8, 0), (0x1c3c94, 0x3000d8, 0x3000e0),
        (0x3000dc, 0x1c1000, 0), (0x1c1018, 0x1653c0, 0x165390)):
        store(va, invalid)
        check(lib.si_guard_ready(reader, None, ctypes.byref(snap)) == 0)
        store(va, valid)
    state_reads, revoke = 0, True
    check(lib.si_guard_ready(reader, None, ctypes.byref(snap)) == 0)
    return total
