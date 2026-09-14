#!/bin/sh
#
# install.sh - installs the MHI2Q CarPlay patches ON THE UNIT.
#
# Runs on the head unit itself (QNX 6.5 /bin/sh) - either over ssh after copying
# this directory across, or from the SD card via M.I.B. (Advanced Settings ->
# Run Custom Script; see custom.sh).
#
# What it does:
#   1. Remounts /mnt/app and /mnt/system read-write.
#   2. Backs up every file it changes, once, as <file>.orig.
#   3. Copies the runtime files into /mnt/app/root/hooks and the Java patch into
#      the LSD jar directory.
#   4. Replaces the carplay child in smartphone_integrator.json with our
#      supervisor - line-based, no sed (the unit has none).
#   5. Widens the PF carplay0 rule to 6030:6032 in all four profiles.
#   6. Tells you to reboot.
#
# Everything is idempotent: running it twice changes nothing the second time.
# POSIX sh only - no bashisms, and none of sed, awk, find or dirname: the unit
# has no such utilities.
#
# Copyright (c) 2026 LuKa (@LuKa_dev)

set -u

# Roots are overridable so the installer can be exercised against a fake unit
# tree (tests/card_install_test.sh). On the car they are never set.
APP_ROOT=${APP_ROOT:-/mnt/app}
SYS_ROOT=${SYS_ROOT:-/mnt/system}

HOOKS_DIR=$APP_ROOT/root/hooks
JARS_DIR=$APP_ROOT/eso/hmi/lsd/jars
CFG=$SYS_ROOT/etc/eso/production/smartphone_integrator.json
LOG=${LOG:-/tmp/carplay_install.log}

# Installed under $HOOKS_DIR. Executables/libraries use 755; the model uses
# 644. Order is irrelevant; the list matches the deployment README.
HOOK_FILES="libcarplay_hook.so libcarplay_apac.so hssr_model.bin maneuver_render altscreen_render
            flag_atlas.rgba carplay_startup.sh carplay_processes.sh carplay_cleanup.sh"

# No `dirname` either - see the note in custom.sh. ${0%/*} strips the last
# /component, but leaves $0 untouched when it has no slash at all, so the
# no-slash case has to pick "." explicitly.
if [ -z "${SRC_DIR:-}" ]; then
    case $0 in
        */*) SRC_DIR=${0%/*} ;;
        *)   SRC_DIR=. ;;
    esac
fi
BIN_DIR=$SRC_DIR/bin

say() {
    echo "$@"
    echo "$@" >> "$LOG" 2>/dev/null
}

die() {
    say "!! $*"
    say "!! ABORTED - nothing further was changed."
    exit 1
}

# Occurrences of $2 in $1. One loop iteration per hit, not per character: the
# config is a few hundred lines and this shell is slow.
count_char() {
    _n=0
    _rest=$1
    while :; do
        case $_rest in
            *"$2"*) _rest=${_rest#*"$2"}; _n=$((_n + 1)) ;;
            *) break ;;
        esac
    done
    echo "$_n"
}

: > "$LOG" 2>/dev/null
say "=== MHI2Q CarPlay - install ==="
say "payload: $BIN_DIR"

# ---------------------------------------------------------------- payload
for f in $HOOK_FILES carplay_hook.jar carplay_child.json install_persistent_pf.sh; do
    [ -f "$BIN_DIR/$f" ] || die "missing payload file: $BIN_DIR/$f"
done

# ---------------------------------------------------------------- unit check
[ -d "$APP_ROOT/eso" ] || die "$APP_ROOT/eso not found - this is not an MHI2 unit"
[ -f "$CFG" ] || die "$CFG not found - unexpected firmware layout, stopping"
[ -d "$JARS_DIR" ] || die "$JARS_DIR not found - unexpected firmware layout, stopping"

# ---------------------------------------------------------------- writable
say ""
say "--- remounting partitions read-write ---"
mount -uw "$APP_ROOT" 2>/dev/null
mount -uw "$SYS_ROOT" 2>/dev/null
touch "$APP_ROOT/.rwtest" 2>/dev/null || die "$APP_ROOT is not writable"
rm -f "$APP_ROOT/.rwtest"
touch "$SYS_ROOT/.rwtest" 2>/dev/null || die "$SYS_ROOT is not writable"
rm -f "$SYS_ROOT/.rwtest"
say "ok: $APP_ROOT and $SYS_ROOT are writable"

# ---------------------------------------------------------------- backup
backup_once() {
    if [ -f "$1" ] && [ ! -f "$1.orig" ]; then
        cp -p "$1" "$1.orig" || die "could not back up $1"
        say "backed up: $1 -> $1.orig"
    elif [ -f "$1.orig" ]; then
        say "backup already present: $1.orig (kept, not overwritten)"
    fi
}

say ""
say "--- backups ---"
backup_once "$CFG"
# Everything else we install is a new file. Nothing stock is replaced, so an
# uninstall is deleting them again plus restoring this one config.

# ---------------------------------------------------------------- copy
say ""
say "--- copying files ---"
[ -d "$HOOKS_DIR" ] || mkdir -p "$HOOKS_DIR" || die "could not create $HOOKS_DIR"

for f in $HOOK_FILES; do
    cp "$BIN_DIR/$f" "$HOOKS_DIR/$f" || die "copy $f failed"
    # Set the modes explicitly. cp gives whatever the umask and the source
    # filesystem happen to produce - and on the M.I.B. path the source is a
    # FAT32 card, which carries no Unix modes at all.
    if [ "$f" = hssr_model.bin ]; then
        chmod 644 "$HOOKS_DIR/$f" || die "chmod $f failed"
    else
        chmod 755 "$HOOKS_DIR/$f" || die "chmod $f failed"
    fi
    say "ok: $HOOKS_DIR/$f"
done

# Retired HU experiment. Never carry its executable into a new installation.
rm -f "$HOOKS_DIR/cursor_render" || die "could not remove retired HU renderer"

cp "$BIN_DIR/carplay_hook.jar" "$JARS_DIR/carplay_hook.jar" || die "copy carplay_hook.jar failed"
chmod 755 "$JARS_DIR/carplay_hook.jar" || die "chmod carplay_hook.jar failed"
say "ok: $JARS_DIR/carplay_hook.jar"

# ---------------------------------------------------------------- config
# The stock carplay child runs dio_manager directly. Ours runs carplay_startup.sh,
# which starts the renderers and then execs dio_manager with the hook - so the
# whole child object is replaced, not one key. Line-based, because the unit has
# no sed and no awk.
say ""
say "--- smartphone_integrator.json ---"

if grep "carplay_startup.sh" "$CFG" > /dev/null 2>&1; then
    say "our supervisor is already the carplay child - leaving the config alone"
else
    NEW=$CFG.new
    : > "$NEW" || die "cannot write $NEW"
    depth=0
    hits=0
    state=copy

    while IFS= read -r line || [ -n "$line" ]; do
        if [ "$state" = copy ]; then
            case "$line" in
                *'"carplay"'*:*)
                    hits=$((hits + 1))
                    state=skip
                    depth=0
                    ;;
                *)
                    printf '%s\n' "$line" >> "$NEW"
                    continue
                    ;;
            esac
        fi
        # state=skip: swallow the stock child, tracking brace depth.
        opens=`count_char "$line" '{'`
        closes=`count_char "$line" '}'`
        depth=$((depth + opens - closes))
        if [ "$depth" -le 0 ]; then
            # The child closed on this line. Whatever trails the last '}' - a
            # comma when another child follows - has to survive.
            printf '    "carplay": ' >> "$NEW"
            cat "$BIN_DIR/carplay_child.json" >> "$NEW"
            printf '%s\n' "${line##*\}}" >> "$NEW"
            state=copy
        fi
    done < "$CFG"

    if [ "$hits" != "1" ]; then
        rm -f "$NEW"
        die "expected exactly 1 carplay child, found $hits - config not touched"
    fi
    if [ "$state" != "copy" ]; then
        rm -f "$NEW"
        die "carplay child never closed - config not touched"
    fi
    # A child object that spans a line like '}, "usb": {' would make the brace
    # arithmetic swallow its neighbour. Every child has exactly one "exec", so
    # comparing the counts catches that before anything is written.
    old_execs=`grep -c '"exec"' "$CFG"`
    new_execs=`grep -c '"exec"' "$NEW"`
    if [ "$old_execs" != "$new_execs" ]; then
        rm -f "$NEW"
        die "child count changed ($old_execs -> $new_execs) - config not touched"
    fi
    grep "carplay_startup.sh" "$NEW" > /dev/null 2>&1 || {
        rm -f "$NEW"
        die "patched config does not name our supervisor - config not touched"
    }

    # Write through the existing file rather than replacing it, so the config
    # keeps its own mode and owner whatever they are. If this ever fails
    # half-way the stock file is still at $CFG.orig.
    cat "$NEW" > "$CFG" || die "could not write $CFG (backup is at $CFG.orig)"
    rm -f "$NEW"
    say "ok: carplay child now runs $HOOKS_DIR/carplay_startup.sh"
fi

# ---------------------------------------------------------------- firewall
# altScreen needs carplay0 TCP 6031 and buffered audio uses 6032 beside stock
# 6030. This edits the source profiles under /mnt/system/etc, not the live
# ramdisk ruleset, so it survives the reboot below. Idempotent on its own.
say ""
say "--- PF profiles ---"
sh "$BIN_DIR/install_persistent_pf.sh" 2>&1 | while IFS= read -r l; do say "$l"; done

# ---------------------------------------------------------------- done
say ""
say "--- flushing writes ---"
sync
sleep 2
sync

say ""
say "=== DONE ==="
say "REBOOT the unit for the patches to load."
say "Wait a few seconds first - the writes above must reach flash."
say ""
say "After the reboot, plug in an iPhone and check:"
say "  cat /tmp/carplay_wrapper.log     # supervisor: renderers and dio"
say "  cat /tmp/carplay_hook.log        # native hook"
say "  cat /tmp/carplay_java.log        # cluster/Java side"
say ""
say "This log: $LOG"
