#!/bin/sh
# CarPlay child supervisor - configure as children.carplay.exec.
#
# The smartphone_integrator child environment deliberately contains NO LD_PRELOAD.
# This script applies libcarplay_hook.so only to dio_manager, so neither this shell
# nor either external renderer loads the hook.
#
# Lifecycle:
#   1. keep an already-running renderer untouched; start only a missing one;
#   2. fork a renderer monitor, then EXEC dio_manager with the hook
#      (exec is mandatory: smartphone_integrator tracks this exact PID);
#   3. while that PID lives, the monitor restarts the renderer if it exits;
#   4. ordinary dio exit/replacement leaves the persistent renderer alive;
#      only an explicit supervisor stop tears it down.

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
        # (see cp_renderer_healthy). On initial adoption, probe only a readiness
        # signal the renderer actually owns: a live PID for maneuver_render,
        # which is a CLIENT of Java's optional :19800.
        # If that renderer-specific check fails, replace the wedged PID.
        # The 2s monitor loop keeps the cheap PID-only check (it restarts exits).
        if [ "$SR_REASON" = initial ] && ! cp_renderer_healthy "$SR_NAME"; then
            echo "[supervisor] $SR_NAME alive but not serving - restarting" >> "$WLOG"
            # This is pre-dio adoption inside SI's 10 s startup budget. One
            # second is enough for a responsive renderer to run its SIGTERM
            # handler; the explicit system-stop path still keeps the full 4 s
            # Qualcomm EGL grace in cp_stop_renderer_snapshot().
            cp_kill_renderer "$SR_NAME" 1
        else
            [ "$SR_REASON" = initial ] &&
                echo "[supervisor] $SR_NAME already running - leave untouched" >> "$WLOG"
            return 0
        fi
    fi

    if [ "$SR_REASON" = restart ]; then
        echo "[supervisor] $SR_NAME crash backoff 5s before restart" >> "$WLOG"
        sleep 5
    fi
    echo "[supervisor] starting $SR_NAME reason=$SR_REASON" >> "$WLOG"
    case "$SR_NAME" in
        maneuver_render)
            # On-car tuning overlay: `touch /mnt/app/carplay_crop_outline` draws a red border
            # around the rectangle the cluster layout crops out of the 328x180 canvas, so the
            # panel can be aligned by eye. Put `x,y,w,h` in that file to outline a different
            # rect (Classic in-tube is 59,27,210,153). `rm` it to go back to normal.
            SR_OUTLINE=
            SR_RECT=
            if [ -e /mnt/app/carplay_crop_outline ]; then
                SR_OUTLINE=1
                read SR_RECT < /mnt/app/carplay_crop_outline 2>/dev/null
                echo "[supervisor] maneuver_render crop outline ON rect=${SR_RECT:-default}" >> "$WLOG"
            fi
            (
                cd "$H" || exit 1
                LD_PRELOAD= GRAPHICS_ROOT=/proc/boot \
                CR_CROP_OUTLINE="$SR_OUTLINE" CR_CROP_RECT="$SR_RECT" \
                exec "$H/maneuver_render" \
                    </dev/null >>/tmp/maneuver_render.log 2>&1
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
    MR_EXPLICIT_STOP=0
    MR_LIVED_TICKS=0
    echo "[supervisor] renderer monitor watching dio pid=$MR_DIO_PID" >> "$WLOG"

    # Renderer adoption runs HERE, not before the exec, because smartphone_integrator's
    # children.carplay.startupTimeout (10 s) starts when SI spawns THIS SCRIPT — not when
    # dio_manager starts. Every millisecond spent before `exec` is taken out of dio's budget.
    # `pidin ar`, up to two `netstat -an` and a 1 s settle used to run in that window; if dio
    # then misses the deadline SI reports TIMEOUT_STARTUP, and per SI_STACK_RESTART_RE.md that
    # branch calls prepare(retry=1) -> RestartDeviceStack -> stopncm.sh -> `carplay0` destroyed,
    # which is exactly the "No Network Interface" the phone reports.
    # Renderers are persistent services and no dio startup step waits on them, so adopting them
    # a few hundred ms later costs nothing.
    cp_seed_renderer_pid_files
    start_renderer maneuver_render initial

    while [ -d "/proc/$MR_DIO_PID" ]; do
        if [ -e "$STOP_FILE" ]; then
            echo "[supervisor] monitor observed cleanup request" >> "$WLOG"
            MR_EXPLICIT_STOP=1
            kill -15 "$MR_DIO_PID" 2>/dev/null
            sleep 3
            if [ -d "/proc/$MR_DIO_PID" ]; then
                echo "[supervisor] monitor SIGKILL stuck dio_manager pid=$MR_DIO_PID" >> "$WLOG"
                kill -9 "$MR_DIO_PID" 2>/dev/null
            fi
            break
        fi
        start_renderer maneuver_render restart

        sleep 2
        MR_LIVED_TICKS=`expr "$MR_LIVED_TICKS" + 1`
        # ~2 s per tick: check the uncapped logs every ~5 minutes.
        if [ `expr "$MR_LIVED_TICKS" % 150` -eq 0 ]; then
            cp_cap_all_logs "$WLOG"
        fi
    done

    if [ "$MR_EXPLICIT_STOP" = 1 ]; then
        # Snapshot once so an explicit stop cannot hit a later replacement.
        MR_MANEUVER_PIDS=`cp_renderer_pids maneuver_render`
        echo "[supervisor] explicit stop - stopping renderer snapshot" >> "$WLOG"
        cp_stop_renderer_snapshot "$WLOG" "$MR_MANEUVER_PIDS"
    else
        cp_usb_record_generation_result "$MR_DIO_PID" "$MR_LIVED_TICKS"
        echo "[supervisor] dio_manager pid=$MR_DIO_PID gone - persistent renderer left alive" >> "$WLOG"
    fi
}

rm -f "$STOP_FILE"
echo "$DIO_PID" > "$OWNER_FILE"
echo "===== carplay_startup.sh run pid=$$ ppid=$PPID =====" >> "$WLOG"

# A previous monitor may have proved the independent pre-RTSP USB wedge. Consume
# its one-shot request before creating any graphics clients or exec'ing dio.
cp_usb_consume_pending_reset

# Nothing else may run before the exec: renderer adoption now happens inside the
# monitor (see monitor_renderers) so dio_manager gets the full SI startup budget.
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
exec "$DIODIR/dio_manager" "$@"
