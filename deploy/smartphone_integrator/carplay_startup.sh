#!/bin/sh
# CarPlay child supervisor - configure as children.carplay.exec.
#
# The smartphone_integrator child environment deliberately contains NO LD_PRELOAD.
# This script applies libcarplay_hook.so only to dio_manager, so neither this shell
# nor either external renderer loads the hook.
#
# Lifecycle:
#   1. keep an already-running renderer untouched; start only a missing one;
#   2. fork a renderer monitor, then EXEC dio_manager with the hook and altScreen
#      gates (exec is mandatory: smartphone_integrator tracks this exact PID);
#   3. while that PID lives, the monitor restarts a renderer if it exits;
#   4. when dio exits (or cleanup requests stop), the monitor stops both renderers.

DIODIR=/mnt/app/eso/bin/apps
H=/mnt/app/root/hooks
WLOG=/tmp/carplay_wrapper.log
STOP_FILE=/tmp/carplay_supervisor.stop
OWNER_FILE=/tmp/carplay_supervisor.owner
DIO_PID=$$

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
        # An adopted renderer from a previous generation may be alive but wedged
        # (see cp_renderer_healthy). On initial adoption, require its port to be
        # LISTENing; if not, kill the wedged PID and fall through to a fresh start.
        # The 2s monitor loop keeps the cheap PID-only check (it restarts exits).
        if [ "$SR_REASON" = initial ] && ! cp_renderer_healthy "$SR_NAME"; then
            echo "[supervisor] $SR_NAME alive but not serving - restarting" >> "$WLOG"
            cp_kill_renderer "$SR_NAME"
        else
            [ "$SR_REASON" = initial ] &&
                echo "[supervisor] $SR_NAME already running - leave untouched" >> "$WLOG"
            return 0
        fi
    fi

    echo "[supervisor] starting $SR_NAME reason=$SR_REASON" >> "$WLOG"
    case "$SR_NAME" in
        maneuver_render)
            (
                cd "$H" || exit 1
                LD_PRELOAD= GRAPHICS_ROOT=/proc/boot exec "$H/maneuver_render" \
                    </dev/null >>/tmp/maneuver_render.log 2>&1
            ) &
            cp_renderer_record_pid "$SR_NAME" "$!"
            ;;
        altscreen_render)
            (
                cd "$H" || exit 1
                LD_PRELOAD= GRAPHICS_ROOT=/proc/boot exec "$H/altscreen_render" \
                    </dev/null >>/tmp/altscreen_render.log 2>&1
            ) &
            cp_renderer_record_pid "$SR_NAME" "$!"
            ;;
        *) return 1 ;;
    esac
}

request_stop()
{
    : > "$STOP_FILE"
    echo "[supervisor] stop requested before dio exec" >> "$WLOG"
    cp_stop_renderers "$WLOG"
    exit 0
}
trap 'request_stop' 1 2 15

monitor_renderers()
{
    MR_DIO_PID=$1
    echo "[supervisor] renderer monitor watching dio pid=$MR_DIO_PID" >> "$WLOG"
    while [ -d "/proc/$MR_DIO_PID" ]; do
        if [ -e "$STOP_FILE" ]; then
            echo "[supervisor] monitor observed cleanup request" >> "$WLOG"
            kill -15 "$MR_DIO_PID" 2>/dev/null
            sleep 3
            if [ -d "/proc/$MR_DIO_PID" ]; then
                echo "[supervisor] monitor SIGKILL stuck dio_manager pid=$MR_DIO_PID" >> "$WLOG"
                kill -9 "$MR_DIO_PID" 2>/dev/null
            fi
            break
        fi
        start_renderer maneuver_render restart
        start_renderer altscreen_render restart
        sleep 2
    done

    # Capture this generation's renderer PIDs before a replacement wrapper can
    # start anything. If cleanup is ultimately required, only these exact PIDs
    # may be signalled; never re-enumerate and hit the replacement generation.
    MR_MANEUVER_PIDS=`cp_renderer_pids maneuver_render`
    MR_ALTSCREEN_PIDS=`cp_renderer_pids altscreen_render`

    # smartphone_integrator normally launches the replacement generation almost
    # immediately. Give its wrapper time to publish the new owner before deciding
    # to clean up shared renderers. Without this grace period, the old monitor can
    # win the race, decide it still owns cleanup, and tear down the new stream.
    sleep 2
    MR_OWNER=
    if [ -r "$OWNER_FILE" ]; then
        read MR_OWNER < "$OWNER_FILE"
    fi
    if [ "$MR_OWNER" = "$MR_DIO_PID" ]; then
        echo "[supervisor] dio_manager pid=$MR_DIO_PID gone - stopping renderers" >> "$WLOG"
        cp_stop_renderer_snapshot "$WLOG" \
            "$MR_MANEUVER_PIDS" "$MR_ALTSCREEN_PIDS"
    else
        echo "[supervisor] dio_manager pid=$MR_DIO_PID gone but superseded by pid=$MR_OWNER - leave new renderers untouched" >> "$WLOG"
    fi
}

rm -f "$STOP_FILE"
echo "$DIO_PID" > "$OWNER_FILE"
echo "===== carplay_startup.sh run pid=$$ ppid=$PPID =====" >> "$WLOG"

# One finite process snapshot at most. All following checks use the recorded PID
# and /proc directly, keeping expensive pidin enumeration out of dio startup.
cp_seed_renderer_pid_files
start_renderer maneuver_render initial
start_renderer altscreen_render initial

cd "$DIODIR" 2>/dev/null || {
    echo "[supervisor] cannot cd to $DIODIR" >> "$WLOG"
    cp_stop_renderers "$WLOG"
    exit 127
}

# The monitor is a separate child. This shell MUST exec dio_manager so the PID
# smartphone_integrator launched remains the PID that registers the CarPlay DSI
# service and receives SI stop/watchdog handling. Keeping this shell as dio's
# parent caused SI to kill/relaunch wrappers and the main CarPlay screen never rose.
monitor_renderers "$DIO_PID" &
echo "[supervisor] exec dio_manager pid=$DIO_PID with private LD_PRELOAD" >> "$WLOG"

export LD_PRELOAD="$H/libcarplay_hook.so"
export ALTSCREEN_HANDLE_ALT=1
export ALTSCREEN_ADVERT_DISPLAYS=1
exec "$DIODIR/dio_manager" "$@"
