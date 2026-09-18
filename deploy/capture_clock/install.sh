#!/bin/sh
# Separate candidate installer, QNX 6.5 sh. No service restart or render monitors.
# Only the known service line is changed; unrelated framework content survives.
set -u
APP_ROOT=${APP_ROOT:-/mnt/app}
SYS_ROOT=${SYS_ROOT:-/mnt/system}
HOOKS=$APP_ROOT/root/hooks
ENCODER=$APP_ROOT/eso/bin/apps/videoencoderservice
CFG=$SYS_ROOT/etc/eso/production/framework.json
VC_CMP=${VC_CMP:-$APP_ROOT/armle/usr/bin/cmp}
case $0 in */*) SRC=${0%/*} ;; *) SRC=. ;; esac
SRC=${SRC_DIR:-$SRC}
MODE=${1:-install}
[ "$#" -le 1 ] || { echo 'usage: install.sh [install|disable|uninstall]' >&2; exit 2; }
case $MODE in install|disable|uninstall) ;; *) exit 2 ;; esac
fail() { echo "vc_capture_clock: $*" >&2; exit 1; }
if [ "$MODE" = disable ]; then
    rm -f "$HOOKS/vc_capture_clock.enabled" || fail 'cannot disable marker'
    echo 'Disabled for next encoder start; running process was not restarted.'
    exit 0
fi
[ -f "$CFG" ] && [ ! -L "$CFG" ] || fail "unsupported config path: $CFG"
[ -x "$VC_CMP" ] || fail "required QNX cmp utility unavailable: $VC_CMP"
if [ "$MODE" = install ]; then
    for file in libvc_capture_clock.so vc_capture_startup.sh; do
        [ -r "$SRC/bin/$file" ] || fail "missing payload: $file"
    done
    # The packager checks SHA256 + ARM ABI before including this reference.
    # Byte comparison on HU prevents enabling on a different executable.
    "$VC_CMP" "$ENCODER" "$SRC/reference/videoencoderservice" >/dev/null 2>&1 || fail 'encoder differs from verified MU1316 reference'
fi
# Exclude path characters which cannot be emitted literally inside this JSON string.
case "$HOOKS" in *'"'*|*'\'*|*'
'*) fail 'unsupported hooks path' ;; esac
STOCK='"exec": "./bin/apps/videoencoderservice"'
OURS='"exec": "'"$HOOKS"'/vc_capture_startup.sh"'
LOCK=$CFG.vc-capture.lock
mkdir "$LOCK" 2>/dev/null || fail 'another config operation is active'
STAGED=$CFG.vc-capture.$$
ORIGINAL=$LOCK/original
cleanup() {
    rm -f "$STAGED" "$HOOKS/libvc_capture_clock.so.new.$$" "$HOOKS/vc_capture_startup.sh.new.$$"
    rm -f "$ORIGINAL"
    rmdir "$LOCK"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
# Preserve original mode/owner on atomic replacement.
cp -p "$CFG" "$ORIGINAL" || fail 'cannot snapshot config'
cp -p "$ORIGINAL" "$STAGED" || fail 'cannot stage config; partitions must be writable'
: > "$STAGED" || fail 'cannot write staged config'
hits=0
while IFS= read -r line || [ -n "$line" ]; do
    # Exact supported MU1316 layout; reject unknown formatting, do not guess.
    trimmed=${line#"${line%%[! 	]*}"}
    case "$trimmed" in
        '"name":"videoencoderservice","exec": '*)
            hits=$((hits + 1))
            case "$line" in
                *"$STOCK"*) FROM=$STOCK ;;
                *"$OURS"*) FROM=$OURS ;;
                *) fail 'unexpected encoder executable; config unchanged' ;;
            esac
            if [ "$MODE" = install ]; then TO=$OURS; else TO=$STOCK; fi
            prefix=${line%%"$FROM"*}
            suffix=${line#*"$FROM"}
            line=$prefix$TO$suffix
            ;;
    esac
    printf '%s\n' "$line" >> "$STAGED" || fail 'cannot write staged config'
done < "$ORIGINAL"
[ "$hits" = 1 ] || fail "expected exactly one encoder service, found $hits; config unchanged"
# Config itself is never overwritten until the complete staged edit is valid.
if [ "$MODE" = install ]; then
    mkdir -p "$HOOKS" || fail 'cannot create hooks directory'
    for file in libvc_capture_clock.so vc_capture_startup.sh; do
        cp "$SRC/bin/$file" "$HOOKS/$file.new.$$" || fail "cannot stage $file"
        "$VC_CMP" "$SRC/bin/$file" "$HOOKS/$file.new.$$" >/dev/null 2>&1 || fail "copy verification failed: $file"
        chmod 755 "$HOOKS/$file.new.$$" || fail "cannot chmod $file"
    done
    for file in libvc_capture_clock.so vc_capture_startup.sh; do
        mv "$HOOKS/$file.new.$$" "$HOOKS/$file" || fail "cannot publish $file"
    done
else
    rm -f "$HOOKS/vc_capture_clock.enabled" || fail 'cannot remove enable marker'
fi
"$VC_CMP" "$CFG" "$ORIGINAL" >/dev/null 2>&1 || fail 'config changed during install; refusing to overwrite it'
if ! "$VC_CMP" "$CFG" "$STAGED" >/dev/null 2>&1; then
    mv "$STAGED" "$CFG" || fail 'cannot publish service config'
fi
if [ "$MODE" = install ]; then
    : > "$HOOKS/vc_capture_clock.enabled" || fail 'cannot enable marker'
    echo 'Capture candidate installed for next encoder start. No process restarted.'
else
    # Leave inert files in place: a running process may still use the library.
    echo 'Stock encoder exec restored. No process restarted.'
fi
