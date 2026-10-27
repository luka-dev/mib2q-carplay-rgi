#!/bin/sh
# maneuver_render monitor for one smartphone_integrator/gal (Android Auto) generation.
# Port of luka-dev carplay_monitor.sh; started in the background by gal_startup.sh with
# LD_PRELOAD cleared, right before gal_startup.sh exec's gal (so $1 is gal's PID).
# It never controls USB, OTG, gal or the SI retry state machine, and never signals gal.
#
# Differences vs luka (CarPlay/dio_manager):
#   - generation = gal PID, owner file /tmp/sq5_luka_supervisor.owner;
#   - the renderer gets luka's tested CarPlay-child LD_LIBRARY_PATH explicitly (gal's own
#     path also carries /mnt/app/armle/graphics, which luka never ran the renderer with);
#   - when gal is gone for good the renderer is STOPPED: after gal exits the monitor waits
#     SQ5_LUKA_STOP_GRACE seconds (default 20); if SI started a new gal generation in that
#     window (owner file rewritten) the renderer is left alive for it to adopt (no EGL
#     re-init on quick reconnects / gal restarts), otherwise it is stopped with an
#     identity-checked TERM -> KILL.
#   - second process: sq5_mirror --display 99 (Android Auto cockpit mirror). Same PID-identity
#     rules, restart backoff and end-of-generation stop as maneuver_render. Skipped (and stopped if
#     running) while SD:/sq5_mirror_off exists; skipped when $H/sq5_mirror is not installed.
#     Extra arguments: SQ5_MIRROR_ARGS (e.g. "--size 1440x540"); defaults live in the binary.

H=${H:-/mnt/app/root/hooks}
WLOG=${WLOG:-/tmp/sq5_luka_wrapper.log}
OWNER_FILE=${OWNER_FILE:-/tmp/sq5_luka_supervisor.owner}
STOP_GRACE=${SQ5_LUKA_STOP_GRACE:-20}
RENDER_LD_LIBRARY_PATH=/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/lib/dll:/mnt/app/armle/usr/lib
GAL_PID=${1:-}
SD_ROOTS=${SQ5T_SD_ROOTS:-/fs/sda0 /net/mmx/fs/sda0}
MIRROR_ID=${SQ5_MIRROR_ID:-99}
MIRROR_STATE=

case "$GAL_PID" in
    ''|*[!0-9]*|0|1)
        echo "[monitor] invalid gal pid: $GAL_PID" >> "$WLOG"
        exit 2
        ;;
esac

if [ ! -r "$H/sq5_luka_processes.sh" ]; then
    echo "[monitor] missing $H/sq5_luka_processes.sh" >> "$WLOG"
    exit 127
fi
. "$H/sq5_luka_processes.sh"

monitor_owns_generation()
{
    MON_OWNER=
    [ -r "$OWNER_FILE" ] && read MON_OWNER < "$OWNER_FILE"
    [ "$MON_OWNER" = "$GAL_PID" ]
}

monitor_gal_alive()
{
    [ -d "/proc/$GAL_PID" ]
}

monitor_current()
{
    monitor_owns_generation && monitor_gal_alive
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
        # 5 s backoff: a renderer that dies at once (watchdog _exit 89, EGL failure)
        # must not be respawned in a tight loop.
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
    case "$MON_NAME" in
        sq5_mirror) MON_ARGS="--display $MIRROR_ID ${SQ5_MIRROR_ARGS:-}" ;;
        *) MON_ARGS= ;;
    esac
    echo "[monitor] starting $MON_NAME reason=$MON_REASON $MON_ARGS" >> "$WLOG"

    (
        trap - 1 2 15      # the renderer gets default dispositions (it installs its own TERM handler)
        cd "$H" || exit 1
        monitor_current || exit 0
        LD_PRELOAD= LD_LIBRARY_PATH=$RENDER_LD_LIBRARY_PATH GRAPHICS_ROOT=/proc/boot \
            exec "$H/$MON_NAME" $MON_ARGS </dev/null >>"/tmp/$MON_NAME.log" 2>&1
    ) &
    MON_NEW_PID=$!

    # A replacement generation can take ownership between fork and publish.
    # The child performs the same check before exec; never overwrite the new
    # generation's registry after ownership has changed.
    if monitor_current; then
        cp_renderer_record_pid "$MON_NAME" "$MON_NEW_PID" || :
    fi
}

sd_flag()
{
    for MON_ROOT in $SD_ROOTS; do [ -f "$MON_ROOT/$1" ] && return 0; done
    return 1
}

# Card independence (owner 2026-09-29): the independent cockpit map (aa-cluster-live) owns window 99 when it
# is installed on the car ($CL_ENABLE, written by its live.sh unit_sync) - then no mirror, card or not, unless
# a card off switch disables the cockpit map.
CL_ENABLE=${SQ5T_CL_ENABLE:-/mnt/app/root/sq5_cluster_live/enable}
cluster_live_owns()
{
    [ -f "$CL_ENABLE" ] || return 1
    sd_flag sq5_cluster_live_off && return 1
    sd_flag sq5_hook_off && return 1
    sd_flag sq5_luka_off && return 1
    return 0
}

# sq5_mirror policy for this tick: start/keep when wanted, stop when SD:/sq5_mirror_off appears.
# Logged on state change only (the loop runs every 2 s).
mirror_tick()
{
    MON_WHY=$1
    if [ ! -x "$H/sq5_mirror" ]; then
        MON_NEW=absent
    elif sd_flag sq5_mirror_off || cluster_live_owns; then
        MON_NEW=off
    else
        MON_NEW=on
    fi
    if [ "$MON_NEW" != "$MIRROR_STATE" ]; then
        case "$MON_NEW" in
            absent) echo "[monitor] sq5_mirror not installed; mirror skipped" >> "$WLOG" ;;
            off)    echo "[monitor] sq5_mirror_off present; mirror disabled" >> "$WLOG" ;;
            on)     [ -n "$MIRROR_STATE" ] && echo "[monitor] mirror enabled" >> "$WLOG" ;;
        esac
    fi
    if [ "$MON_NEW" = on ]; then
        [ "$MIRROR_STATE" = on ] || MON_WHY=initial
        MIRROR_STATE=on
        start_renderer sq5_mirror "$MON_WHY"
    else
        if [ "$MIRROR_STATE" = on ] || [ -z "$MIRROR_STATE" ]; then
            cp_stop_renderer sq5_mirror 2
        fi
        MIRROR_STATE=$MON_NEW
    fi
}

# gal has exited. Keep the renderer for a successor generation, else stop it.
monitor_finish()
{
    if ! monitor_owns_generation; then
        echo "[monitor] generation=$GAL_PID superseded; renderer left to the new generation" >> "$WLOG"
        return 0
    fi
    echo "[monitor] gal pid=$GAL_PID exited; waiting ${STOP_GRACE}s for a new gal generation" >> "$WLOG"
    MON_WAIT=0
    while [ "$MON_WAIT" -lt "$STOP_GRACE" ]; do
        sleep 1
        MON_WAIT=`expr "$MON_WAIT" + 1`
        if ! monitor_owns_generation; then
            echo "[monitor] new gal generation took over after ${MON_WAIT}s; renderer kept" >> "$WLOG"
            return 0
        fi
    done
    # Last ownership check right before the signal (a new gal may just have started).
    monitor_owns_generation || return 0
    echo "[monitor] no gal for ${STOP_GRACE}s; stopping maneuver_render + sq5_mirror" >> "$WLOG"
    cp_stop_renderer sq5_mirror 2
    cp_stop_renderer maneuver_render 2
    # Retire the owner token only if it is still ours (stage + mv keeps it atomic elsewhere).
    if monitor_owns_generation; then
        rm -f "$OWNER_FILE"
    fi
    echo "[monitor] generation=$GAL_PID closed" >> "$WLOG"
}

monitor_main()
{
    # Ignore HUP/INT/TERM: if SI signals gal's whole process group, the monitor must
    # survive to run monitor_finish (else the renderer would be orphaned). Maintenance
    # stops the monitor by removing the owner file (sq5_luka_cleanup.sh), not by signal.
    trap '' 1 2 15

    monitor_current || exit 0
    echo "[monitor] generation=$GAL_PID active" >> "$WLOG"

    cp_seed_renderer_pid_files
    start_renderer maneuver_render initial
    mirror_tick initial

    MON_TICKS=0
    while monitor_current; do
        start_renderer maneuver_render restart
        mirror_tick restart
        sleep 2
        MON_TICKS=`expr "$MON_TICKS" + 1`
        if [ `expr "$MON_TICKS" % 150` -eq 0 ]; then
            cp_cap_all_logs "$WLOG"
        fi
    done

    monitor_finish
    cp_cap_all_logs "$WLOG"
}

monitor_main
