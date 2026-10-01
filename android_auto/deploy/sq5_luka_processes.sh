#!/bin/sh
# Small renderer-process helpers for QNX 6.5 /bin/sh (pdksh).
# Port of luka-dev carplay_processes.sh for the SQ5 Android Auto (gal) supervisor.
# Sourced by sq5_luka_monitor.sh and sq5_luka_cleanup.sh. Changes vs luka:
#   - PID file renamed to /tmp/sq5_luka_maneuver_render.pid (no clash with a CarPlay install);
#   - log cap covers /tmp/sq5_luka_wrapper.log;
#   - cp_stop_renderer: identity-checked TERM->KILL of the recorded renderer (used when gal
#     is gone for good and by the maintenance cleanup), because on Android Auto the renderer
#     is stopped with its session instead of living forever.
#   - a second supervised process, sq5_mirror (Android Auto cockpit mirror, displayable 99), with
#     its own PID file /tmp/sq5_luka_sq5_mirror.pid and the same identity rules.

# smartphone_integrator does not guarantee a useful PATH for its child. Keep the
# inherited order and append only stock MU1316 command locations.
CP_QNX_PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/bin:/mnt/app/armle/usr/sbin
PATH=${PATH:+$PATH:}$CP_QNX_PATH
export PATH
unset CP_QNX_PATH

CP_MANEUVER_PID_FILE=${CP_MANEUVER_PID_FILE:-/tmp/sq5_luka_maneuver_render.pid}
CP_MIRROR_PID_FILE=${CP_MIRROR_PID_FILE:-/tmp/sq5_luka_sq5_mirror.pid}
CP_MIRROR_READY=${CP_MIRROR_READY:-/tmp/sq5_mirror_ready}
CP_LOG_MAX_BYTES=${CP_LOG_MAX_BYTES:-524288}

cp_renderer_pid_file()
{
    case "$1" in
        maneuver_render)  echo "$CP_MANEUVER_PID_FILE" ;;
        sq5_mirror)       echo "$CP_MIRROR_PID_FILE" ;;
        *) return 1 ;;
    esac
}

cp_pid_alive()
{
    case "$1" in ''|*[!0-9]*|0|1) return 1 ;; esac
    [ -d "/proc/$1" ]
}

# Return 0 for the requested executable, 1 for a dead/different PID and 2 when
# pidin could not answer. Unknown identity is never safe to signal.
cp_renderer_identity()
{
    cp_pid_alive "$2" || return 1
    CP_ID_TEXT=`pidin -p "$2" ar 2>/dev/null` || return 2
    echo "$CP_ID_TEXT" | (
        while read CP_ID_PID CP_ID_EXE CP_ID_REST; do
            [ "$CP_ID_PID" = "$2" ] || continue
            case "$CP_ID_EXE" in
                "$1"|*"/$1") exit 0 ;;
                '') exit 2 ;;
                *) exit 1 ;;
            esac
        done
        exit 2
    )
}

cp_renderer_record_pid()
{
    cp_pid_alive "$2" || return 1
    CP_REC_FILE=`cp_renderer_pid_file "$1"` || return 1
    CP_REC_STAGE=${CP_REC_FILE}.$2
    echo "$2" > "$CP_REC_STAGE" || return 1
    mv "$CP_REC_STAGE" "$CP_REC_FILE"
}

cp_forget_renderer_pid()
{
    CP_FORGET_FILE=`cp_renderer_pid_file "$1"` || return 0
    CP_FORGET_PID=
    [ -r "$CP_FORGET_FILE" ] && read CP_FORGET_PID < "$CP_FORGET_FILE"
    [ "$CP_FORGET_PID" != "$2" ] || rm -f "$CP_FORGET_FILE"
}

# A full pidin scan is allowed only while adopting a process or recovering a
# stale/missing registry entry. The recurring healthy path is one /proc lookup.
cp_find_renderer_pid()
{
    CP_FIND_NAME=$1
    pidin ar 2>/dev/null | (
        while read CP_FIND_PID CP_FIND_EXE CP_FIND_REST; do
            case "$CP_FIND_EXE" in
                "$CP_FIND_NAME"|*"/$CP_FIND_NAME")
                    echo "$CP_FIND_PID"
                    exit 0
                    ;;
            esac
        done
        exit 1
    )
}

cp_renderer_adopt()
{
    CP_ADOPT_NAME=$1
    CP_ADOPT_PID=`cp_find_renderer_pid "$CP_ADOPT_NAME"` || return 1
    cp_renderer_record_pid "$CP_ADOPT_NAME" "$CP_ADOPT_PID"
}

cp_renderer_running()
{
    CP_RUN_NAME=$1
    CP_RUN_FILE=`cp_renderer_pid_file "$CP_RUN_NAME"` || return 1
    CP_RUN_PID=
    if [ -r "$CP_RUN_FILE" ]; then
        read CP_RUN_PID < "$CP_RUN_FILE"
        if cp_pid_alive "$CP_RUN_PID"; then
            return 0
        fi
        cp_forget_renderer_pid "$CP_RUN_NAME" "$CP_RUN_PID"
    fi
    cp_renderer_adopt "$CP_RUN_NAME"
}

cp_seed_renderer_pid_files()
{
    cp_renderer_running maneuver_render >/dev/null 2>&1 || :
    cp_renderer_running sq5_mirror >/dev/null 2>&1 || :
}

# maneuver_render is a client of Java's route-scoped :19800, so exact process
# identity is the only non-invasive health check.
cp_renderer_healthy()
{
    CP_HEALTH_NAME=$1
    CP_HEALTH_FILE=`cp_renderer_pid_file "$CP_HEALTH_NAME"` || return 1
    CP_HEALTH_PID=
    [ -r "$CP_HEALTH_FILE" ] || return 1
    read CP_HEALTH_PID < "$CP_HEALTH_FILE"

    CP_HEALTH_ID=0
    cp_renderer_identity "$CP_HEALTH_NAME" "$CP_HEALTH_PID" || CP_HEALTH_ID=$?
    if [ "$CP_HEALTH_ID" = 1 ]; then
        cp_forget_renderer_pid "$CP_HEALTH_NAME" "$CP_HEALTH_PID"
        return 1
    fi
    return 0
}

# Used only for an adopted renderer that is positively known to be wedged. Identity is rechecked before TERM and KILL; an unavailable query
# fails open and retains the process.
cp_kill_renderer()
{
    CP_KILL_NAME=$1
    CP_KILL_GRACE=${2:-1}
    CP_KILL_WHY=${3:-wedged}
    CP_KILL_FILE=`cp_renderer_pid_file "$CP_KILL_NAME"` || return 0
    CP_KILL_PID=
    [ -r "$CP_KILL_FILE" ] || return 0
    read CP_KILL_PID < "$CP_KILL_FILE"

    cp_renderer_identity "$CP_KILL_NAME" "$CP_KILL_PID" || return 0
    # sq5_mirror argument form "--snapshot" (card diagnostics) is not ours to kill
    if [ "$CP_KILL_NAME" = sq5_mirror ] && cp_mirror_is_snapshot "$CP_KILL_PID"; then return 0; fi
    echo "[monitor] SIGTERM $CP_KILL_WHY $CP_KILL_NAME pid=$CP_KILL_PID" >> "${WLOG:-/dev/null}"
    kill -15 "$CP_KILL_PID" 2>/dev/null
    sleep "$CP_KILL_GRACE"

    CP_KILL_ID=0
    cp_renderer_identity "$CP_KILL_NAME" "$CP_KILL_PID" || CP_KILL_ID=$?
    if [ "$CP_KILL_ID" = 0 ]; then
        echo "[monitor] SIGKILL $CP_KILL_WHY $CP_KILL_NAME pid=$CP_KILL_PID" >> "${WLOG:-/dev/null}"
        kill -9 "$CP_KILL_PID" 2>/dev/null
    fi
    [ "$CP_KILL_ID" = 2 ] || cp_forget_renderer_pid "$CP_KILL_NAME" "$CP_KILL_PID"
}

cp_log_size()
{
    set -- `ls -l "$1" 2>/dev/null`
    echo "${5:-0}"
}

cp_cap_log()
{
    CP_CAP_PATH=$1
    [ -f "$CP_CAP_PATH" ] || return 0
    CP_CAP_SIZE=`cp_log_size "$CP_CAP_PATH"`
    case "$CP_CAP_SIZE" in ''|*[!0-9]*) return 0 ;; esac
    [ "$CP_CAP_SIZE" -gt "$CP_LOG_MAX_BYTES" ] || return 0
    : > "$CP_CAP_PATH"
    echo "[monitor] truncated at $CP_CAP_SIZE bytes" >> "$CP_CAP_PATH"
}

cp_cap_all_logs()
{
    cp_cap_log /tmp/maneuver_render.log
    cp_cap_log /tmp/sq5_mirror.log
    cp_cap_log "${1:-/tmp/sq5_luka_wrapper.log}"
}

# Stop the recorded renderer for good (gal gone / maintenance). Adopts first, so a
# renderer hidden by a stale or missing PID file is found by one pidin scan.
# Identity is rechecked before every signal; an unknown answer keeps the process.
cp_stop_renderer()
{
    CP_STOP_NAME=${1:-maneuver_render}
    CP_STOP_FILE=`cp_renderer_pid_file "$CP_STOP_NAME"` || return 0
    if cp_renderer_running "$CP_STOP_NAME"; then
        CP_STOP_PID=
        [ -r "$CP_STOP_FILE" ] && read CP_STOP_PID < "$CP_STOP_FILE"
        cp_kill_renderer "$CP_STOP_NAME" "${2:-2}" stopping
        if [ -r "$CP_STOP_FILE" ]; then
            read CP_STOP_PID < "$CP_STOP_FILE"
            cp_pid_alive "$CP_STOP_PID" || rm -f "$CP_STOP_FILE"
        fi
        # still our live process (identity unknown / survived): leave its marker alone
        if [ "$CP_STOP_NAME" = sq5_mirror ] && [ -n "$CP_STOP_PID" ] &&
                cp_renderer_identity sq5_mirror "$CP_STOP_PID"; then
            return 0
        fi
    fi
    # the mirror deletes its own ready marker on TERM; after a KILL, a zombie or a crash the
    # supervisor does it, so the HMI never keeps a stale marker
    [ "$CP_STOP_NAME" = sq5_mirror ] && rm -f "$CP_MIRROR_READY"
    return 0
}

# 0 when the pid is a one-shot "sq5_mirror --snapshot" (card diagnostic run), never supervised.
cp_mirror_is_snapshot()
{
    CP_SNAP_TEXT=`pidin -p "$1" ar 2>/dev/null` || return 1
    case "$CP_SNAP_TEXT" in *--snapshot*) return 0 ;; esac
    return 1
}
