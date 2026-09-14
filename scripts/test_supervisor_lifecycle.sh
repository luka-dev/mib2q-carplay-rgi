#!/bin/sh
# Exercise actual startup functions with fake renderers/process probes, no HU.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT HUP INT TERM
export TEST_DIR
python3 - "${SUPERVISOR_SOURCE:-$ROOT/deploy/smartphone_integrator/carplay_startup.sh}" "$TEST_DIR/functions.sh" <<'PY'
import re, sys
s = open(sys.argv[1]).read()
functions = re.findall(r'^\w+\(\)\n\{\n.*?^\}', s, re.M | re.S)
s = '\n\n'.join(functions)
s = re.sub(r'/tmp/(maneuver|altscreen)_render.log', '${TEST_DIR}/renderer.log', s)
open(sys.argv[2], 'w').write(s)
PY
. "$TEST_DIR/functions.sh"
H=$TEST_DIR
WLOG=$TEST_DIR/wrapper.log
OWNER_FILE=$TEST_DIR/owner
STOP_FILE=$TEST_DIR/stop
MR_DIO_PID=100
DIO_PID=100
printf '#!/bin/sh\nexit 0\n' > "$H/maneuver_render"
cp "$H/maneuver_render" "$H/altscreen_render"
chmod +x "$H/maneuver_render" "$H/altscreen_render"
supervisor_dio_alive() { [ "$ALIVE" = 1 ]; }
cp_renderer_running() { [ "$RENDERER_LIVE" = 1 ]; }
cp_renderer_healthy() { return 0; }
cp_renderer_record_pid() { echo "$1" >> "$TEST_DIR/spawns"; }
cp_seed_renderer_pid_files() { :; }
cp_usb_record_generation_result() { echo recovery >> "$TEST_DIR/recovery"; }
cp_cap_all_logs() { :; }
cp_renderer_pids() { echo "$RENDERER_TOKEN"; }
cp_stop_renderer_snapshot() { echo "$2 $3" >> "$TEST_DIR/stopped"; }
kill() { echo "$*" >> "$TEST_DIR/kills"; }
sleep() {
    case "$SCENARIO:$1" in
        replace:5) echo 200 > "$OWNER_FILE" ;;
        death:5) ALIVE=0 ;;
        stop:5) : > "$STOP_FILE" ;;
        adopted:5) RENDERER_LIVE=1 ;;
        explicit:3) echo 200 > "$OWNER_FILE"; RENDERER_TOKEN=222 ;;
        helpers:4) TEST_RENDERER_GONE=1 ;;
        *) : ;;
    esac
}
for RENDERER in maneuver_render altscreen_render; do
for SCENARIO in replace death stop adopted; do
    ALIVE=1; RENDERER_LIVE=0; echo 100 > "$OWNER_FILE"; rm -f "$STOP_FILE" "$TEST_DIR/spawns"
    start_renderer "$RENDERER" restart || :
    wait
    if [ -s "$TEST_DIR/spawns" ]; then
        echo "FAIL: stale/ceased monitor spawned renderer after $SCENARIO during backoff" >&2
        exit 1
    fi
done
done
# Current generation still starts a genuinely missing renderer.
SCENARIO=current; ALIVE=1; RENDERER_LIVE=0; echo 100 > "$OWNER_FILE"; rm -f "$STOP_FILE"
start_renderer maneuver_render restart
wait
[ "$(cat "$TEST_DIR/spawns")" = maneuver_render ]
rm -f "$TEST_DIR/spawns"
start_renderer altscreen_render restart
wait
[ "$(cat "$TEST_DIR/spawns")" = altscreen_render ]
# Explicit stop snapshots renderer identities before the DIO grace sleep. A
# replacement during that sleep must not change the killed renderer snapshot.
SCENARIO=explicit; ALIVE=1; RENDERER_LIVE=1; RENDERER_TOKEN=111
: > "$STOP_FILE"; echo 100 > "$OWNER_FILE"
monitor_renderers 100
[ "$(cat "$TEST_DIR/stopped")" = '111 111' ]
# An obsolete monitor must neither adopt renderers nor update USB failure state.
rm -f "$TEST_DIR/recovery" "$TEST_DIR/spawns"
SCENARIO=obsolete; ALIVE=1; RENDERER_LIVE=0; echo 200 > "$OWNER_FILE"
monitor_renderers 100
[ ! -e "$TEST_DIR/recovery" ] && [ ! -e "$TEST_DIR/spawns" ]
# Exercise the actual shared registry/cleanup helpers too. kill/sleep remain
# harmless test functions, so this never addresses a real process.
. "$ROOT/deploy/smartphone_integrator/carplay_processes.sh"
CP_ALTSCREEN_PID_FILE=$TEST_DIR/alt.pid
cp_renderer_record_pid altscreen_render 333
[ "$(cat "$CP_ALTSCREEN_PID_FILE")" = 333 ]
SCENARIO=helpers
cp_renderer_identity() { [ "${TEST_RENDERER_GONE:-0}" = 0 ]; } # Synthetic PIDs, exit during grace.
rm -f "$TEST_DIR/kills"
cp_stop_renderer_snapshot "$WLOG" 111 222
[ "$(cat "$TEST_DIR/kills")" = "-15 111
-15 222" ]
echo 'Supervisor lifecycle: owner/death/stop/adoption during backoff, stop snapshot and stale monitor PASS'
sh "$ROOT/tests/renderer_pid_identity_test.sh"
