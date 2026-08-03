#!/bin/sh
# CarPlay child supervisor - configure as children.carplay.exec.
#
# The smartphone_integrator child environment deliberately contains NO LD_PRELOAD.
# This script applies libcarplay_hook.so only to dio_manager, so neither this shell
# nor either external renderer loads the hook.
#
# Lifecycle:
#   1. keep an already-running renderer untouched; start only a missing one;
#   2. launch dio_manager with the hook and altScreen gates;
#   3. while dio lives, restart a renderer if it exits;
#   4. when dio exits (or cleanup requests stop), gracefully stop both renderers
#      and exit with dio's status.

DIODIR=/mnt/app/eso/bin/apps
H=/mnt/app/root/hooks
WLOG=/tmp/carplay_wrapper.log
STOP_FILE=/tmp/carplay_supervisor.stop
DIO_PID=
MONITOR_PID=
STOPPING=0

if [ ! -r "$H/carplay_processes.sh" ]; then
    echo "[supervisor] missing $H/carplay_processes.sh" >> "$WLOG"
    exit 127
fi
. "$H/carplay_processes.sh"

start_renderer()
{
    SR_NAME=$1
    SR_REASON=$2
    if cp_renderer_running "$SR_NAME"; then
        [ "$SR_REASON" = initial ] &&
            echo "[supervisor] $SR_NAME already running - leave untouched" >> "$WLOG"
        return 0
    fi

    echo "[supervisor] starting $SR_NAME reason=$SR_REASON" >> "$WLOG"
    case "$SR_NAME" in
        maneuver_render)
            (
                cd "$H" || exit 1
                LD_PRELOAD= GRAPHICS_ROOT=/proc/boot exec "$H/maneuver_render" \
                    </dev/null >>/tmp/maneuver_render.log 2>&1
            ) &
            ;;
        altscreen_render)
            (
                cd "$H" || exit 1
                LD_PRELOAD= exec "$H/altscreen_render" \
                    </dev/null >>/tmp/altscreen_render.log 2>&1
            ) &
            ;;
        *) return 1 ;;
    esac
}

request_stop()
{
    STOPPING=1
    : > "$STOP_FILE"
    if [ -n "$DIO_PID" ] && [ -d "/proc/$DIO_PID" ]; then
        echo "[supervisor] stop requested - SIGTERM dio_manager pid=$DIO_PID" >> "$WLOG"
        kill -TERM "$DIO_PID" 2>/dev/null
    fi
}
trap 'request_stop' 1 2 15

monitor_renderers()
{
    while [ -d "/proc/$DIO_PID" ]; do
        if [ -e "$STOP_FILE" ]; then
            echo "[supervisor] monitor observed cleanup request" >> "$WLOG"
            kill -TERM "$DIO_PID" 2>/dev/null
            sleep 3
            if [ -d "/proc/$DIO_PID" ]; then
                echo "[supervisor] monitor SIGKILL stuck dio_manager pid=$DIO_PID" >> "$WLOG"
                kill -KILL "$DIO_PID" 2>/dev/null
            fi
            return
        fi
        start_renderer maneuver_render restart
        start_renderer altscreen_render restart
        sleep 2
    done
}

rm -f "$STOP_FILE"
echo "===== carplay_startup.sh run pid=$$ ppid=$PPID =====" >> "$WLOG"

start_renderer maneuver_render initial
start_renderer altscreen_render initial

cd "$DIODIR" 2>/dev/null || {
    echo "[supervisor] cannot cd to $DIODIR" >> "$WLOG"
    cp_stop_renderers "$WLOG"
    exit 127
}

echo "[supervisor] starting dio_manager with private LD_PRELOAD" >> "$WLOG"
LD_PRELOAD="$H/libcarplay_hook.so" \
ALTSCREEN_HANDLE_ALT=1 \
ALTSCREEN_ADVERT_DISPLAYS=1 \
"$DIODIR/dio_manager" &
DIO_PID=$!
echo "[supervisor] dio_manager pid=$DIO_PID" >> "$WLOG"

# Reap dio in the parent shell. A separate monitor supervises renderers while
# this wait blocks; polling /proc in the parent would risk waiting forever on a
# zombie that cannot disappear until its parent calls wait().
monitor_renderers &
MONITOR_PID=$!
wait "$DIO_PID" 2>/dev/null
DIO_STATUS=$?

# SIGTERM may interrupt wait before dio has exited. Bound the second wait with a
# three-second killer, then reap the real child exactly once.
if [ -d "/proc/$DIO_PID" ]; then
    (
        sleep 3
        if [ -d "/proc/$DIO_PID" ]; then
            echo "[supervisor] SIGKILL stuck dio_manager pid=$DIO_PID" >> "$WLOG"
            kill -KILL "$DIO_PID" 2>/dev/null
        fi
    ) &
    DIO_KILLER_PID=$!
    wait "$DIO_PID" 2>/dev/null
    DIO_STATUS=$?
    kill -TERM "$DIO_KILLER_PID" 2>/dev/null
    wait "$DIO_KILLER_PID" 2>/dev/null
fi

kill -TERM "$MONITOR_PID" 2>/dev/null
wait "$MONITOR_PID" 2>/dev/null
echo "[supervisor] dio_manager exited status=$DIO_STATUS - stopping renderers" >> "$WLOG"
cp_stop_renderers "$WLOG"

# A requested disconnect is a clean supervisor shutdown, not a crash that should
# consume smartphone_integrator's runtime-retry budget.
if [ "$STOPPING" = 1 ] || [ -e "$STOP_FILE" ]; then
    exit 0
fi
exit "$DIO_STATUS"
