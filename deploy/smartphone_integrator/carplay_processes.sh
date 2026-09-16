#!/bin/sh
# Small renderer-process helpers for QNX 6.5 /bin/sh.
# Sourced only by carplay_monitor.sh.

# smartphone_integrator does not guarantee a useful PATH for its child. Keep the
# inherited order and append only stock MU1316 command locations.
CP_QNX_PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/bin:/mnt/app/armle/usr/sbin
PATH=${PATH:+$PATH:}$CP_QNX_PATH
export PATH
unset CP_QNX_PATH

CP_MANEUVER_PID_FILE=${CP_MANEUVER_PID_FILE:-/tmp/carplay_maneuver_render.pid}
CP_ALTSCREEN_PID_FILE=${CP_ALTSCREEN_PID_FILE:-/tmp/carplay_altscreen_render.pid}
CP_LOG_MAX_BYTES=${CP_LOG_MAX_BYTES:-524288}

cp_renderer_pid_file()
{
    case "$1" in
        maneuver_render)  echo "$CP_MANEUVER_PID_FILE" ;;
        altscreen_render) echo "$CP_ALTSCREEN_PID_FILE" ;;
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
    cp_renderer_running altscreen_render >/dev/null 2>&1 || :
    cp_renderer_running maneuver_render >/dev/null 2>&1 || :
}

# altscreen_render owns :19831, so its LISTEN socket is a useful adoption
# health check. maneuver_render is a client of Java's route-scoped :19800; for
# it, exact process identity is the only non-invasive check.
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
    [ "$CP_HEALTH_ID" != 2 ] || return 0

    case "$CP_HEALTH_NAME" in
        maneuver_render) return 0 ;;
        altscreen_render) ;;
        *) return 1 ;;
    esac

    CP_HEALTH_NETSTAT=`netstat -an 2>/dev/null`
    [ -n "$CP_HEALTH_NETSTAT" ] || return 0
    echo "$CP_HEALTH_NETSTAT" | grep -i listen | grep -q '[.:]19831 '
}

# Used only for an adopted altscreen process that is positively known to be
# wedged. Identity is rechecked before TERM and KILL; an unavailable query
# fails open and retains the process.
cp_kill_renderer()
{
    CP_KILL_NAME=$1
    CP_KILL_GRACE=${2:-1}
    CP_KILL_FILE=`cp_renderer_pid_file "$CP_KILL_NAME"` || return 0
    CP_KILL_PID=
    [ -r "$CP_KILL_FILE" ] || return 0
    read CP_KILL_PID < "$CP_KILL_FILE"

    cp_renderer_identity "$CP_KILL_NAME" "$CP_KILL_PID" || return 0
    echo "[monitor] SIGTERM wedged $CP_KILL_NAME pid=$CP_KILL_PID" >> "${WLOG:-/dev/null}"
    kill -15 "$CP_KILL_PID" 2>/dev/null
    sleep "$CP_KILL_GRACE"

    CP_KILL_ID=0
    cp_renderer_identity "$CP_KILL_NAME" "$CP_KILL_PID" || CP_KILL_ID=$?
    if [ "$CP_KILL_ID" = 0 ]; then
        echo "[monitor] SIGKILL wedged $CP_KILL_NAME pid=$CP_KILL_PID" >> "${WLOG:-/dev/null}"
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
    cp_cap_log /tmp/altscreen_render.log
    cp_cap_log "${1:-/tmp/carplay_wrapper.log}"
}
