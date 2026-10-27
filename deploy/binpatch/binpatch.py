# binpatch.py - patch a stock firmware binary of THIS unit by signature, not by offset.
# Runs under M.I.B.'s MicroPython on the head unit and under CPython on a host.
#
#   micropython binpatch.py <name> <stock file> <out file>
#
# Each patch site is found by the stock bytes with their surrounding code (context before
# and after).  Every site must match exactly once, so a build whose code differs is left
# alone instead of being patched in the wrong place.  The code around a site is the same
# across trains even where the offsets move (MU1316 P5087, MU1329 P5092, MU1003 K1004).
# Exit 0: <out> written with every site patched.  3: <stock> already carries the patch,
# nothing written.  1: a site did not match exactly once, or I/O failed; nothing written.
# Copyright (c) 2026 LuKa (@LuKa_dev)
import sys
try:
    import binascii
except ImportError:
    import ubinascii as binascii


def h(s):
    return binascii.unhexlify(s.replace(' ', ''))


# name -> [(context before, stock bytes, patched bytes, context after)]
PATCHES = {
    # Stock libairplay 210.81 (P5087/P5092): queue depths 4 -> 2, one imm8
    # byte each. QNX denies runtime mprotect on the library's shared text.
    # Both full contexts must match once; K1004's different library is refused.
    'airplay_decoder': [
        (h('03 30 84 e0 0c c0 84 e0 00 c0 8d e5 02 3c fe eb'), h('04'), h('02'),
         h('00 a0 e3 3a 38 fe eb 00 00 50 e3 0c 00 85 e5 20 00 00 0a 07 10 a0 e1 4f 33 fe eb')),
        (h('45 3b fe eb 00 20 a0 e1 c0 10 9f e5 07 00 a0 e1'), h('04'), h('02'),
         h('30 a0 e3 01 10 84 e0 bc 36 fe eb 00 00 50 e3 00 50 a0 13 20 00 00 0a 05 00 a0 e1')),
    ],
    # SI error re-arm (docs/connect-and-usb/USB_REPLUG_RECOVERY.md): the CarPlay device-error
    # path passes the real direction instead of 0, so a failed session re-arms on replug.
    # MOV R3, #0  ->  AND R3, R9, LR, LSR #6
    'smartphone_integrator': [
        (h('30 15 d6 e5 53 ce 86 e2 31 25 d6 e5 d8 00 84 e2'), h('00 30 a0 e3'),
         h('2e 33 09 e0'), h('08 c0 8c e2 04 e0 8d e5 08 b0 8d e5 00 c0 8d e5')),
    ],
    # mcd detector-thread CPU coalescing (mcd/README.md): the PATH_MEDIA_PROCMGR
    # directory-watch loop blocked on sigwaitinfo(signal 58), so every PATHSPACE pulse
    # fanned out to all 13 detector threads and each rescanned the tree at once. Replace
    # the blocking wait with a 200 ms poll delay - the pulse stays pending harmlessly and
    # scans cap at ~5 Hz/thread.  ADD R0,SP,#0x218 -> MOV R0,#200 ; BL sigwaitinfo -> BL delay.
    'mcd': [
        (h('22 1e 8d e2 21 2e 8d e2 00 30 a0 e3 2a ff ff eb'),
         h('86 0f 8d e2 00 10 a0 e3 73 eb ff eb'),
         h('c8 00 a0 e3 00 10 a0 e3 f7 eb ff eb'),
         h('c4 ff ff ea 7d eb ff eb 00 40 90 e5 10 02 9d e5')),
    ],
}


def find_once(data, needle):
    """Offset of the only occurrence of needle, or -1 for none or several."""
    first = data.find(needle)
    if first < 0 or data.find(needle, first + 1) >= 0:
        return -1
    return first


def patch(data, sites):
    """(sorted [(offset, bytes)], 0) | (None, 3) already patched | (None, 1) no unique match."""
    edits, done = [], 0
    for pre, old, new, post in sites:
        at = find_once(data, pre + old + post)
        if at >= 0:
            edits.append((at + len(pre), new))
        elif find_once(data, pre + new + post) >= 0:
            done += 1
        else:
            return None, 1
    if done == len(sites):
        return None, 3
    if done:
        return None, 1          # half patched: not ours to finish
    return sorted(edits), 0


def main(argv):
    if len(argv) != 4 or argv[1] not in PATCHES:
        print('usage: binpatch.py <%s> <stock file> <out file>' % '|'.join(PATCHES))
        return 1
    try:
        with open(argv[2], 'rb') as f:
            data = f.read()
    except OSError as e:
        print('binpatch: cannot read %s: %s' % (argv[2], e))
        return 1
    edits, rc = patch(data, PATCHES[argv[1]])
    if rc == 3:
        print('binpatch: %s already patched' % argv[2])
    elif rc:
        print('binpatch: %s does not match the %s patch sites - left unchanged' % (argv[2], argv[1]))
    else:
        try:
            # no second copy of the file: M.I.B.'s micropython heap is smaller than an 800 KB SI
            view, pos = memoryview(data), 0
            with open(argv[3], 'wb') as f:
                for at, new in edits:
                    f.write(view[pos:at]); f.write(new); pos = at + len(new)
                f.write(view[pos:])
        except OSError as e:
            print('binpatch: cannot write %s: %s' % (argv[3], e))
            return 1
        print('binpatch: %s patched (%d site(s))' % (argv[1], len(PATCHES[argv[1]])))
    return rc


if __name__ == '__main__':
    sys.exit(main(sys.argv))
