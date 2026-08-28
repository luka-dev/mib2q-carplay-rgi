#!/bin/sh
# Stage the discrete-5.1 CarPlay bundle onto the HU. Two HU-side pieces:
#   1. libairplay hook — enabled by SPATIAL_5_1=1, a plain flag in carplay_startup.sh (deployed
#      separately). This script does not touch that.
#   2. patched deva-ctrl-qc.so (mpl5_dio_ent 2->6 voices) — a driver swap that needs io-audio/deva
#      to restart = REBOOT territory. This script COPIES it but NEVER restarts anything.
#
# Safe by default: dry-run unless --apply is passed. Never writes an on-device .bak (stock is
# recoverable from the firmware image). Never reboots / relaunches j9 / io-audio.
#
# Usage:
#   sh install_spatial_5_1.sh            # dry-run: show what it WOULD do
#   sh install_spatial_5_1.sh --apply    # copy the patched deva .so (still no restart)
set -e
HERE=$(dirname "$0")
APPLY=0; [ "$1" = "--apply" ] && APPLY=1

DEVA_SRC="$HERE/../deva-ctrl-qc/deva-ctrl-qc.so"
DEVA_DST=/mnt/app/armle/lib/dll/deva-ctrl-qc.so

say(){ echo "[spatial_5_1] $*"; }

[ -f "$DEVA_SRC" ] || { say "MISSING $DEVA_SRC (run deploy/deva-ctrl-qc/patch_deva.sh first)"; exit 1; }

# integrity gate: patched .so must have mpl5_dio_ent(idx4) max_voices=0x06 at file 0xA6E1C
b=$(xxd -s $((0xA6E1C)) -l 1 -p "$DEVA_SRC" 2>/dev/null || od -An -tx1 -j $((0xA6E1C)) -N1 "$DEVA_SRC" | tr -d ' ')
[ "$b" = "06" ] || { say "REFUSE: $DEVA_SRC max_voices byte=0x$b (expected 0x06 - not the patched build)"; exit 1; }
say "patched deva verified (mpl5_dio_ent max_voices=0x06)"

if [ "$APPLY" = 0 ]; then
    say "DRY-RUN. Would:"
    say "  cp $DEVA_SRC -> $DEVA_DST   (stock recoverable from firmware image; NO on-device .bak)"
    say "Re-run with --apply to do it. Neither this nor --apply restarts anything."
    say "Hook side: SPATIAL_5_1=1 is default-ON in carplay_startup.sh (deployed separately)."
    exit 0
fi

# deva driver swap (in-use lib; takes effect only after io-audio/deva restart = reboot)
cp "$DEVA_SRC" "$DEVA_DST"
say "copied deva-ctrl-qc.so -> $DEVA_DST"

say "DONE (staged). NOT ACTIVE YET:"
say "  - deva change needs io-audio/deva restart (audio subsystem) = REBOOT. Ask before rebooting."
say "  - hook side: SPATIAL_5_1=1 must be uncommented in carplay_startup.sh."
say "Rollback: restore stock deva-ctrl-qc.so from the firmware image; comment out SPATIAL_5_1."
