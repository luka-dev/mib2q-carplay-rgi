#!/bin/sh
# Real registry/adoption/stop helpers; fake finite QNX snapshots and harmless signals.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT HUP INT TERM
. "$ROOT/deploy/smartphone_integrator/carplay_processes.sh"
CP_MANEUVER_PID_FILE=$TEST_DIR/maneuver.pid
CP_ALTSCREEN_PID_FILE=$TEST_DIR/alt.pid
CP_RENDERER_SCAN_DONE=1
WLOG=$TEST_DIR/log
cp_pid_alive() {
    case "$1" in ''|*[!0-9]*|0|1) return 1 ;; esac
    [ -f "$TEST_DIR/process-$1" ]
}
pidin() {
    # The production hot path must never call this; slow paths target one PID.
    [ "$#" = 3 ] && [ "$1" = -p ] && [ "$3" = ar ] || return 99
    echo "$2" >> "$TEST_DIR/probes"
    [ ! -e "$TEST_DIR/unavailable" ] || return 1
    cat "$TEST_DIR/process-$2"
}
netstat() { echo 'tcp 0 0 127.0.0.1.19831 *.* LISTEN'; }
kill() { echo "$*" >> "$TEST_DIR/signals"; }
sleep() {
    if [ "${REUSE_DURING_GRACE:-0}" = 1 ]; then
        echo '222 /proc/boot/stock-service' > "$TEST_DIR/process-222"
        echo 444 > "$CP_ALTSCREEN_PID_FILE"
    fi
    [ "${UNAVAILABLE_DURING_GRACE:-0}" != 1 ] || : > "$TEST_DIR/unavailable"
}
echo '333 /mnt/app/root/hooks/altscreen_render' > "$TEST_DIR/process-333"
cp_renderer_record_pid altscreen_render 333
for i in 1 2 3 4 5; do cp_renderer_running altscreen_render; done
[ ! -e "$TEST_DIR/probes" ] # Existing cheap two-second monitor stays unchanged.
cp_renderer_healthy altscreen_render
[ "$(cp_renderer_pids altscreen_render)" = 333 ]

# A reused PID cannot be adopted or signalled as a renderer.
echo '333 /proc/boot/dio_manager' > "$TEST_DIR/process-333"
if cp_renderer_healthy altscreen_render; then echo 'reused PID adopted' >&2;exit 1;fi
[ ! -e "$CP_ALTSCREEN_PID_FILE" ]
cp_stop_renderer_snapshot "$WLOG" '' 333
[ ! -e "$TEST_DIR/signals" ]

# Check identity again after the SIGTERM grace: PID 222 changed owners.
echo '111 /mnt/app/root/hooks/maneuver_render' > "$TEST_DIR/process-111"
echo '222 /mnt/app/root/hooks/altscreen_render' > "$TEST_DIR/process-222"
echo '333 /mnt/app/root/hooks/altscreen_render' > "$TEST_DIR/process-333"
REUSE_DURING_GRACE=1
cp_stop_renderer_snapshot "$WLOG" 111 '222 333'
[ "$(cat "$TEST_DIR/signals")" = "-15 111
-15 222
-15 333
-9 111
-9 333" ]
cp_forget_renderer_pid altscreen_render 333
[ "$(cat "$CP_ALTSCREEN_PID_FILE")" = 444 ] # Preserve a newer registry generation.

# Failed inspection is conservative: retain on adoption, never signal.
REUSE_DURING_GRACE=0
cp_renderer_record_pid altscreen_render 333
: > "$TEST_DIR/unavailable"
cp_renderer_healthy altscreen_render
[ -z "$(cp_renderer_pids altscreen_render)" ]
rm "$TEST_DIR/signals"
cp_stop_renderer_snapshot "$WLOG" 111 '222 333'
[ ! -e "$TEST_DIR/signals" ]
cp_stop_renderer_snapshot "$WLOG" '' '-1 0 1'
[ ! -e "$TEST_DIR/signals" ]

# If inspection fails only after TERM, keep the PID registered. A skipped KILL
# must not let initial adoption spawn another renderer beside the old one.
rm "$TEST_DIR/unavailable"
UNAVAILABLE_DURING_GRACE=1
cp_kill_renderer altscreen_render 1
[ "$(cat "$TEST_DIR/signals")" = '-15 333' ]
[ "$(cat "$CP_ALTSCREEN_PID_FILE")" = 333 ]
echo 'Renderer PID identity: cheap monitor, bounded adoption, reuse before/after TERM, unavailable probe and newer registry PASS'
