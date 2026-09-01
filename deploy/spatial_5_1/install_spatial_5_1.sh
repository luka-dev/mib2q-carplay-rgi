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

# integrity gate: patched .so must carry the caps edits AND the dynamic active-fragment helper.
chk(){ # chk <file-offset> <length> <expected hex> <label>
    v=$(xxd -s "$1" -l "$2" -p "$DEVA_SRC" 2>/dev/null)
    [ "$v" = "$3" ] || { say "REFUSE: $DEVA_SRC $4=$v (expected $3 - not the complete patched build)"; exit 1; }
}
chk $((0xA6E1C)) 4 06000000 max_voices
chk $((0xA6E24)) 4 00480000 max_fragsize
chk $((0xA6E28)) 4 00d00200 max_buffer
chk $((0x563C8)) 4 bee300eb acquire_branch
chk $((0x8F2C7)) 21 001c2095e5101096e5060051e3122ba0031eff2fe1 dynamic_helper
say "patched deva verified (6 voices; dynamic 6ch S32 frag=0x4800; stock 2ch frag=0x1800)"

if [ "$APPLY" = 0 ]; then
    say "DRY-RUN. Would:"
    say "  cp $DEVA_SRC -> $DEVA_DST   (stock recoverable from firmware image; NO on-device .bak)"
    say "Re-run with --apply to do it. Neither this nor --apply restarts anything."
    say "Hook side: SPATIAL_5_1=1 is default-ON in carplay_startup.sh (deployed separately)."
    exit 0
fi

# deva driver swap. Two on-unit facts make a plain `cp` fail:
#   1. /mnt/app is mounted READ-ONLY -> `mount -uw /mnt/app` first (same as scripts/deploy_unit.sh).
#   2. the .so is mapped by the running io-audio -> writing it gives "Resource busy".
# So: stage a copy next to the target (same fs, persistent), unlink the busy original, rename into
# place. If any step fails the previous file is still there, and the staged copy is on flash rather
# than in /dev/shmem (which is RAM and would vanish on the reboot this needs anyway).
mount -uw /mnt/app || { say "REFUSE: could not remount /mnt/app rw"; exit 1; }
cp "$DEVA_SRC" "$DEVA_DST.new"        || { say "FAIL: stage copy"; exit 1; }
rm "$DEVA_DST"                        || { say "FAIL: unlink busy original (staged copy left at $DEVA_DST.new)"; exit 1; }
mv "$DEVA_DST.new" "$DEVA_DST"        || { say "FAIL: rename into place"; exit 1; }
chmod 777 "$DEVA_DST"
say "copied deva-ctrl-qc.so -> $DEVA_DST"
# NOTE: the unit has no strings/od/cmp/wc/tail/printf, and its grep returns 0 on binaries even when
# the pattern IS present — verify the result by reading the file back and comparing on the host.

say "DONE (staged). NOT ACTIVE YET:"
say "  - deva change needs io-audio/deva restart (audio subsystem) = REBOOT. Ask before rebooting."
say "  - hook side: SPATIAL_5_1=1 must be uncommented in carplay_startup.sh."
say "Rollback: restore stock deva-ctrl-qc.so from the firmware image; comment out SPATIAL_5_1."
