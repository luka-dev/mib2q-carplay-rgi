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
    # SI error re-arm (docs/connect-and-usb/USB_REPLUG_RECOVERY.md): the CarPlay device-error
    # path passes the real direction instead of 0, so a failed session re-arms on replug.
    # MOV R3, #0  ->  AND R3, R9, LR, LSR #6
    'smartphone_integrator': [
        (h('30 15 d6 e5 53 ce 86 e2 31 25 d6 e5 d8 00 84 e2'), h('00 30 a0 e3'),
         h('2e 33 09 e0'), h('08 c0 8c e2 04 e0 8d e5 08 b0 8d e5 00 c0 8d e5')),
    ],
    # Marvell 88W8787 Wi-Fi driver RX-reorder + timer hardening (docs/wireless-carplay/WIFI_DRIVER_RE.md).
    # Three fragments: a 50 ms expiry that releases a held packet so a lost seqno cannot stall the
    # BA window, a wrapped-window fix, and a one-shot timer harden.  The driver is byte-identical on
    # P5087/P5092 (both get the fix); K1004/MU1003 ships a different driver build and keeps its stock.
    'wifi': [
        (h('01 50 a0 e1 04 20 8d e5 00 40 a0 e1 14 a0 91 e5'),
         h('02 00 5a e1 18 a0 91 a5 03 00 00 aa 02 a0 6a e0 18 30 91 e5 03 00 5a e1 03 a0 a0 a1'),
         h('0a a0 42 e0 5a a0 eb e7 18 30 91 e5 03 00 5a e1 03 a0 a0 a1 00 00 5a e3 03 a0 a0 01'),
         h('00 00 5a e3 1e 00 00 da 00 60 a0 e3 f4 87 01 e3')),
        (h('00 00 51 e3 0f 00 00 0a 24 30 d4 e5 00 00 53 e3'),
         h('05 00 00 0a'),
         h('00 f0 20 e3'),
         h('00 30 96 e5 ac 31 93 e5 00 00 95 e5 33 ff 2f e1')),
        (h('01 30 53 e2 fa ff ff 5a 08 00 00 ea 14 20 91 e5'),
         h('01 20 82 e2 0c 00 94 e5 02 2a a0 e1 22 2a a0 e1'),
         h('03 20 82 e0 01 20 82 e2 0c 00 94 e5 52 20 eb e7'),
         h('73 fe ff eb 0c 00 94 e5 08 10 94 e5 87 ff ff eb')),
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
