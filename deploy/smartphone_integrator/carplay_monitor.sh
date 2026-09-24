#!/bin/sh
# Persistent-renderer monitor for one smartphone_integrator/dio generation.
# It never controls USB, OTG, dio_manager or the SI retry state machine.

H=${H:-/mnt/app/root/hooks}
WLOG=${WLOG:-/tmp/carplay_wrapper.log}
OWNER_FILE=${OWNER_FILE:-/tmp/carplay_supervisor.owner}
DIO_PID=${1:-}

case "$DIO_PID" in
    ''|*[!0-9]*|0|1)
        echo "[monitor] invalid dio pid: $DIO_PID" >> "$WLOG"
        exit 2
        ;;
esac

if [ ! -r "$H/carplay_processes.sh" ]; then
    echo "[monitor] missing $H/carplay_processes.sh" >> "$WLOG"
    exit 127
fi
. "$H/carplay_processes.sh"

monitor_owns_generation()
{
    MON_OWNER=
    [ -r "$OWNER_FILE" ] && read MON_OWNER < "$OWNER_FILE"
    [ "$MON_OWNER" = "$DIO_PID" ]
}

monitor_dio_alive()
{
    [ -d "/proc/$DIO_PID" ]
}

monitor_current()
{
    monitor_owns_generation && monitor_dio_alive
}

monitor_renderer_log()
{
    case "$1" in
        maneuver_render) echo /tmp/maneuver_render.log ;;
        altscreen_render) echo /tmp/altscreen_render.log ;;
        *) return 1 ;;
    esac
}

start_renderer()
{
    MON_NAME=$1
    MON_REASON=$2
    monitor_current || return 0

    if cp_renderer_running "$MON_NAME"; then
        if [ "$MON_REASON" = initial ] && ! cp_renderer_healthy "$MON_NAME"; then
            echo "[monitor] $MON_NAME not ready; confirming in 2s" >> "$WLOG"
            sleep 2
            monitor_current || return 0
            if cp_renderer_running "$MON_NAME" && cp_renderer_healthy "$MON_NAME"; then
                echo "[monitor] $MON_NAME became ready" >> "$WLOG"
                return 0
            fi
            echo "[monitor] $MON_NAME still not ready; replacing" >> "$WLOG"
            cp_kill_renderer "$MON_NAME" 1
        else
            [ "$MON_REASON" = initial ] &&
                echo "[monitor] adopted $MON_NAME" >> "$WLOG"
            return 0
        fi
    fi

    if [ "$MON_REASON" = restart ]; then
        sleep 5
        monitor_current || return 0
    fi

    [ -x "$H/$MON_NAME" ] || {
        echo "[monitor] missing executable $H/$MON_NAME" >> "$WLOG"
        return 0
    }

    # Ownership may change during health probing or backoff. Re-scan before
    # spawning so a live renderer hidden by a stale PID file is adopted.
    monitor_current || return 0
    cp_renderer_running "$MON_NAME" && return 0
    MON_LOG=`monitor_renderer_log "$MON_NAME"` || return 1
    echo "[monitor] starting $MON_NAME reason=$MON_REASON" >> "$WLOG"

    (
        cd "$H" || exit 1
        monitor_current || exit 0
        LD_PRELOAD= GRAPHICS_ROOT=/proc/boot exec "$H/$MON_NAME" \
            </dev/null >>"$MON_LOG" 2>&1
    ) &
    MON_NEW_PID=$!

    # A replacement generation can take ownership between fork and publish.
    # The child performs the same check before exec; never overwrite the new
    # generation's registry after ownership has changed.
    if monitor_current; then
        cp_renderer_record_pid "$MON_NAME" "$MON_NEW_PID" || :
    fi
}

monitor_main()
{
    trap 'exit 0' 1 2 15

    # No settle sleeps: the renderers are separate processes and never gate dio.
    # SI's 10 s budget ends at the phone's control SETUP, which waits on USB/NCM/
    # mDNS, not CPU; the renderers must instead be ready before Java selects the
    # cluster context, so they start at once (GL programs load from the
    # persistent binary cache after the first boot).
    monitor_current || exit 0
    echo "[monitor] generation=$DIO_PID active" >> "$WLOG"

    cp_seed_renderer_pid_files
    start_renderer altscreen_render initial
    start_renderer maneuver_render initial

    MON_TICKS=0
    while monitor_current; do
        start_renderer altscreen_render restart
        start_renderer maneuver_render restart
        sleep 2
        MON_TICKS=`expr "$MON_TICKS" + 1`
        if [ `expr "$MON_TICKS" % 150` -eq 0 ]; then
            cp_cap_all_logs "$WLOG"
        fi
    done

    if monitor_owns_generation; then
        echo "[monitor] dio_manager pid=$DIO_PID exited; renderers remain persistent" >> "$WLOG"
    else
        echo "[monitor] generation=$DIO_PID superseded" >> "$WLOG"
    fi
}

monitor_main
