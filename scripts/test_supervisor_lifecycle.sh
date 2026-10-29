#!/bin/sh
# Exercise the actual monitor functions with harmless host-side fakes.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT HUP INT TERM
export TEST_DIR

# The real startup path must publish its own PID, launch the monitor without the
# hook, and exec (not fork) dio_manager with the hook and original arguments.
STARTUP_APP=$TEST_DIR/app
STARTUP_HOOKS=$TEST_DIR/hooks
mkdir -p "$STARTUP_APP" "$STARTUP_HOOKS"
cat > "$STARTUP_APP/dio_manager" <<'SH'
#!/bin/sh
echo "$$|${LD_PRELOAD:-}|$*" > "$STARTUP_TEST_DIO"
SH
cat > "$STARTUP_HOOKS/carplay_monitor.sh" <<'SH'
#!/bin/sh
echo "${LD_PRELOAD:-}|$1" > "$STARTUP_TEST_MONITOR"
SH
chmod +x "$STARTUP_APP/dio_manager" "$STARTUP_HOOKS/carplay_monitor.sh"
: > "$STARTUP_HOOKS/libcarplay_hook.so"
: > "$STARTUP_HOOKS/carplay_processes.sh"
STARTUP_TEST_DIO=$TEST_DIR/dio.env
STARTUP_TEST_MONITOR=$TEST_DIR/monitor.env
export STARTUP_TEST_DIO STARTUP_TEST_MONITOR
DIODIR=$STARTUP_APP H=$STARTUP_HOOKS WLOG=$TEST_DIR/startup.log \
OWNER_FILE=$TEST_DIR/startup.owner \
    sh "$ROOT/deploy/smartphone_integrator/carplay_startup.sh" alpha beta
sleep 1
STARTUP_OLD_IFS=$IFS
IFS='|' read STARTUP_DIO_PID STARTUP_DIO_PRELOAD STARTUP_DIO_ARGS < "$STARTUP_TEST_DIO"
IFS='|' read STARTUP_MON_PRELOAD STARTUP_MON_PID < "$STARTUP_TEST_MONITOR"
IFS=$STARTUP_OLD_IFS
[ "$(cat "$TEST_DIR/startup.owner")" = "$STARTUP_DIO_PID" ]
[ "$STARTUP_DIO_PRELOAD" = "$STARTUP_HOOKS/libcarplay_hook.so" ]
[ "$STARTUP_DIO_ARGS" = 'alpha beta' ]
[ -z "$STARTUP_MON_PRELOAD" ]
[ "$STARTUP_MON_PID" = "$STARTUP_DIO_PID" ]

# A stale QoS marker/helper must not start an updater in the Wi-Fi-only policy.
STARTUP_TEST_QOS=$TEST_DIR/qos.env
export STARTUP_TEST_QOS
cat > "$STARTUP_HOOKS/carplay_pf_watch" <<'SH'
#!/bin/sh
printf '%s' "${LD_PRELOAD:-}" > "$STARTUP_TEST_QOS"
SH
chmod +x "$STARTUP_HOOKS/carplay_pf_watch"
: > "$STARTUP_HOOKS/carplay_pf_dynamic.enabled"
DIODIR=$STARTUP_APP H=$STARTUP_HOOKS WLOG=$TEST_DIR/startup.log \
OWNER_FILE=$TEST_DIR/startup.owner \
    sh "$ROOT/deploy/smartphone_integrator/carplay_startup.sh" alpha beta
sleep 1
[ ! -e "$STARTUP_TEST_QOS" ]
rm "$STARTUP_HOOKS/carplay_pf_dynamic.enabled"
DIODIR=$STARTUP_APP H=$STARTUP_HOOKS WLOG=$TEST_DIR/startup.log \
OWNER_FILE=$TEST_DIR/startup.owner \
    sh "$ROOT/deploy/smartphone_integrator/carplay_startup.sh" alpha beta
sleep 1
[ ! -e "$STARTUP_TEST_QOS" ]

python3 - "$ROOT/deploy/smartphone_integrator/carplay_monitor.sh" "$TEST_DIR/functions.sh" <<'PY'
import re, sys
s = open(sys.argv[1]).read()
functions = re.findall(r'^\w+\(\)\n\{\n.*?^\}', s, re.M | re.S)
open(sys.argv[2], 'w').write('\n\n'.join(functions))
PY
. "$TEST_DIR/functions.sh"

H=$TEST_DIR
WLOG=$TEST_DIR/wrapper.log
OWNER_FILE=$TEST_DIR/owner
DIO_PID=100
printf '#!/bin/sh\nexit 0\n' > "$H/maneuver_render"
cp "$H/maneuver_render" "$H/altscreen_render"
chmod +x "$H/maneuver_render" "$H/altscreen_render"

monitor_dio_alive() { [ "$ALIVE" = 1 ]; }
cp_renderer_running() { [ "$RENDERER_LIVE" = 1 ]; }
cp_renderer_healthy() { return 0; }
cp_renderer_record_pid() { echo "$1" >> "$TEST_DIR/spawns"; }
cp_kill_renderer() { echo "$1" >> "$TEST_DIR/replaced"; RENDERER_LIVE=0; }
cp_seed_renderer_pid_files() { :; }
cp_cap_all_logs() { :; }
sleep() {
    case "$SCENARIO:$1" in
        replace:5) echo 200 > "$OWNER_FILE" ;;
        death:5) ALIVE=0 ;;
        adopted:5) RENDERER_LIVE=1 ;;
        *) : ;;
    esac
}

for RENDERER in maneuver_render altscreen_render; do
    for SCENARIO in replace death adopted; do
        ALIVE=1
        RENDERER_LIVE=0
        echo 100 > "$OWNER_FILE"
        rm -f "$TEST_DIR/spawns"
        start_renderer "$RENDERER" restart || :
        wait
        if [ -s "$TEST_DIR/spawns" ]; then
            echo "FAIL: stale/ceased monitor spawned $RENDERER after $SCENARIO" >&2
            exit 1
        fi
    done
done

# The current generation starts a genuinely missing renderer.
SCENARIO=current
ALIVE=1
RENDERER_LIVE=0
echo 100 > "$OWNER_FILE"
rm -f "$TEST_DIR/spawns"
start_renderer maneuver_render restart
wait
[ "$(cat "$TEST_DIR/spawns")" = maneuver_render ]

# Healthy adoption is preserved; an unhealthy initial adoption is replaced.
RENDERER_LIVE=1
rm -f "$TEST_DIR/spawns" "$TEST_DIR/replaced"
start_renderer altscreen_render initial
[ ! -e "$TEST_DIR/spawns" ] && [ ! -e "$TEST_DIR/replaced" ]
cp_renderer_healthy() { return 1; }
start_renderer altscreen_render initial
wait
[ "$(cat "$TEST_DIR/replaced")" = altscreen_render ]
[ "$(cat "$TEST_DIR/spawns")" = altscreen_render ]

# Runtime supervisor code must remain out of USB/OTG and dio ownership.
if grep -E 'media-con-ctrl|reset port|start_stack|kill .*DIO_PID' \
    "$ROOT/deploy/smartphone_integrator/carplay_startup.sh" \
    "$ROOT/deploy/smartphone_integrator/carplay_monitor.sh" \
    "$ROOT/deploy/smartphone_integrator/carplay_processes.sh" >/dev/null; then
    echo 'FAIL: supervisor contains USB/OTG or dio kill policy' >&2
    exit 1
fi

echo 'Supervisor lifecycle: generation handoff, death/backoff, adoption and USB ownership PASS'
sh "$ROOT/tests/renderer_pid_identity_test.sh"
