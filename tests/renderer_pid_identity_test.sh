#!/bin/sh
# Exercise registry/adoption/identity helpers with finite fake QNX snapshots.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT HUP INT TERM
. "$ROOT/deploy/smartphone_integrator/carplay_processes.sh"

CP_MANEUVER_PID_FILE=$TEST_DIR/maneuver.pid
CP_ALTSCREEN_PID_FILE=$TEST_DIR/alt.pid
WLOG=$TEST_DIR/log

cp_pid_alive()
{
    case "$1" in ''|*[!0-9]*|0|1) return 1 ;; esac
    [ -f "$TEST_DIR/process-$1" ]
}

pidin()
{
    if [ "$#" = 1 ] && [ "$1" = ar ]; then
        for CP_TEST_FILE in "$TEST_DIR"/process-*; do
            [ -f "$CP_TEST_FILE" ] && cat "$CP_TEST_FILE"
        done
        return 0
    fi
    [ "$#" = 3 ] && [ "$1" = -p ] && [ "$3" = ar ] || return 99
    echo "$2" >> "$TEST_DIR/probes"
    [ ! -e "$TEST_DIR/unavailable" ] || return 1
    cat "$TEST_DIR/process-$2"
}

netstat() { echo 'tcp 0 0 127.0.0.1.19831 *.* LISTEN'; }
kill() { echo "$*" >> "$TEST_DIR/signals"; }
sleep()
{
    case "${GRACE_ACTION:-none}" in
        reuse)
            echo '222 /proc/boot/stock-service' > "$TEST_DIR/process-222"
            ;;
        replace_registry)
            echo '222 /proc/boot/stock-service' > "$TEST_DIR/process-222"
            echo '444 /mnt/app/root/hooks/altscreen_render' > "$TEST_DIR/process-444"
            echo 444 > "$CP_ALTSCREEN_PID_FILE"
            ;;
        unavailable)
            : > "$TEST_DIR/unavailable"
            ;;
    esac
}

# Recorded healthy processes use the cheap /proc path and never call pidin.
echo '333 /mnt/app/root/hooks/altscreen_render' > "$TEST_DIR/process-333"
cp_renderer_record_pid altscreen_render 333
for CP_TEST_I in 1 2 3 4 5; do cp_renderer_running altscreen_render; done
[ ! -e "$TEST_DIR/probes" ]
cp_renderer_healthy altscreen_render

# A stale registry falls back to a finite scan and adopts the real process,
# preventing a second renderer from being spawned beside it.
echo 777 > "$CP_ALTSCREEN_PID_FILE"
cp_renderer_running altscreen_render
[ "$(cat "$CP_ALTSCREEN_PID_FILE")" = 333 ]

# A reused PID is rejected by the initial identity check and never signalled.
echo '333 /proc/boot/dio_manager' > "$TEST_DIR/process-333"
if cp_renderer_healthy altscreen_render; then
    echo 'reused PID adopted' >&2
    exit 1
fi
[ ! -e "$CP_ALTSCREEN_PID_FILE" ]
cp_kill_renderer altscreen_render 1
[ ! -e "$TEST_DIR/signals" ]

# A verified wedged renderer gets TERM and residual KILL, then is forgotten.
echo '222 /mnt/app/root/hooks/altscreen_render' > "$TEST_DIR/process-222"
cp_renderer_record_pid altscreen_render 222
GRACE_ACTION=none
cp_kill_renderer altscreen_render 1
[ "$(cat "$TEST_DIR/signals")" = '-15 222
-9 222' ]
[ ! -e "$CP_ALTSCREEN_PID_FILE" ]

# PID reuse during grace prevents KILL. A newer registry entry is preserved.
: > "$TEST_DIR/signals"
echo '222 /mnt/app/root/hooks/altscreen_render' > "$TEST_DIR/process-222"
cp_renderer_record_pid altscreen_render 222
GRACE_ACTION=replace_registry
cp_kill_renderer altscreen_render 1
[ "$(cat "$TEST_DIR/signals")" = '-15 222' ]
[ "$(cat "$CP_ALTSCREEN_PID_FILE")" = 444 ]

# Failed inspection after TERM is conservative: no KILL and the PID remains.
: > "$TEST_DIR/signals"
rm -f "$TEST_DIR/unavailable" "$TEST_DIR/process-444"
echo '333 /mnt/app/root/hooks/altscreen_render' > "$TEST_DIR/process-333"
cp_renderer_record_pid altscreen_render 333
GRACE_ACTION=unavailable
cp_kill_renderer altscreen_render 1
[ "$(cat "$TEST_DIR/signals")" = '-15 333' ]
[ "$(cat "$CP_ALTSCREEN_PID_FILE")" = 333 ]

echo 'Renderer PID identity: cheap hot path, stale adoption, PID reuse and fail-open signalling PASS'
