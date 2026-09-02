#!/bin/sh
#
# uninstall.sh - puts the unit back to stock CarPlay.
#
# Restores smartphone_integrator.json from the .orig install.sh made and deletes
# every file install.sh added. The PF widening is left in place: extra open ports
# on carplay0 change nothing when no CarPlay patch is listening on them, and
# rewriting four firmware profiles to remove two port numbers is the riskier act.
#
# Run it the same two ways install.sh runs - over ssh, or from the card by
# creating an empty /mod/carplay/UNINSTALL and running the custom script.
#
# Copyright (c) 2026 LuKa (@LuKa_dev)

set -u

APP_ROOT=${APP_ROOT:-/mnt/app}
SYS_ROOT=${SYS_ROOT:-/mnt/system}

HOOKS_DIR=$APP_ROOT/root/hooks
JARS_DIR=$APP_ROOT/eso/hmi/lsd/jars
CFG=$SYS_ROOT/etc/eso/production/smartphone_integrator.json
LOG=${LOG:-/tmp/carplay_uninstall.log}

HOOK_FILES="libcarplay_hook.so libcarplay_apac.so maneuver_render altscreen_render
            flag_atlas.rgba carplay_startup.sh carplay_processes.sh carplay_cleanup.sh"

say() {
    echo "$@"
    echo "$@" >> "$LOG" 2>/dev/null
}

: > "$LOG" 2>/dev/null
say "=== MHI2Q CarPlay - uninstall ==="

mount -uw "$APP_ROOT" 2>/dev/null
mount -uw "$SYS_ROOT" 2>/dev/null

say ""
say "--- restoring the stock carplay child ---"
if [ -f "$CFG.orig" ]; then
    # Write through the existing file so it keeps its mode and owner, and keep
    # the .orig: a half-finished restore must still have something to restore from.
    if cat "$CFG.orig" > "$CFG"; then
        say "ok: $CFG restored from $CFG.orig"
    else
        say "!! could not write $CFG - the stock copy is still at $CFG.orig"
        exit 1
    fi
else
    say "!! no $CFG.orig - config left alone."
    say "!! If CarPlay was patched, put the stock carplay child back by hand."
fi

say ""
say "--- removing installed files ---"
for f in $HOOK_FILES; do
    if [ -f "$HOOKS_DIR/$f" ]; then
        rm -f "$HOOKS_DIR/$f" && say "removed: $HOOKS_DIR/$f"
    fi
done
if [ -f "$JARS_DIR/carplay_hook.jar" ]; then
    rm -f "$JARS_DIR/carplay_hook.jar" && say "removed: $JARS_DIR/carplay_hook.jar"
fi

say ""
say "--- flushing writes ---"
sync
sleep 2
sync

say ""
say "=== DONE ==="
say "REBOOT the unit. Stock CarPlay comes back with it."
say "This log: $LOG"
