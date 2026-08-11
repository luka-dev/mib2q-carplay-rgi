#!/bin/sh
# Shared CarPlay renderer process helpers for QNX 6.5 /bin/sh.
# Sourced by carplay_startup.sh and carplay_cleanup.sh.

# Do not depend on smartphone_integrator preserving its parent's PATH. Append a
# safe subset of the stock MU1316 MMX search locations (startup.sh:395/443), while
# retaining the inherited head and its resolution order for dio_manager.
# These entries provide QNX pidin/sleep/rm/kill.
CP_QNX_PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/bin:/mnt/app/armle/usr/sbin
PATH=${PATH:+$PATH:}$CP_QNX_PATH
export PATH
unset CP_QNX_PATH

# Renderer PID registry. /tmp is reset on every HU boot, and QNX PIDs include a
# generation component, so a live /proc/<pid> check is enough for processes
# launched by this supervisor. A single pidin snapshot seeds the registry when
# upgrading over an already-running generation; recurring startup/monitor paths
# then use O(1) /proc checks instead of repeatedly scanning the whole system.
CP_MANEUVER_PID_FILE=/tmp/carplay_maneuver_render.pid
CP_ALTSCREEN_PID_FILE=/tmp/carplay_altscreen_render.pid
CP_RENDERER_SCAN_DONE=0

cp_renderer_pid_file()
{
    case "$1" in
        maneuver_render)  echo "$CP_MANEUVER_PID_FILE" ;;
        altscreen_render) echo "$CP_ALTSCREEN_PID_FILE" ;;
        *) return 1 ;;
    esac
}

cp_renderer_record_pid()
{
    CP_REC_FILE=`cp_renderer_pid_file "$1"` || return 1
    echo "$2" > "$CP_REC_FILE"
}

# TCP port each renderer LISTENs on (hook/Java connect to these). Used only for
# the initial-adoption health probe below.
cp_renderer_port()
{
    case "$1" in
        maneuver_render)  echo 19800 ;;
        altscreen_render) echo 19831 ;;
        *) return 1 ;;
    esac
}

# Run at most once in a startup wrapper. This preserves renderers inherited from
# an older script generation while avoiding two pidin scans before dio exec and
# two more scans immediately in the monitor.
cp_seed_renderer_pid_files()
{
    if [ ! -r "$CP_MANEUVER_PID_FILE" ] || [ ! -r "$CP_ALTSCREEN_PID_FILE" ]; then
        pidin ar 2>/dev/null | (
            while read CP_PID CP_EXE CP_REST; do
                case "$CP_EXE" in
                    maneuver_render|*"/maneuver_render")
                        [ -r "$CP_MANEUVER_PID_FILE" ] ||
                            echo "$CP_PID" > "$CP_MANEUVER_PID_FILE"
                        ;;
                    altscreen_render|*"/altscreen_render")
                        [ -r "$CP_ALTSCREEN_PID_FILE" ] ||
                            echo "$CP_PID" > "$CP_ALTSCREEN_PID_FILE"
                        ;;
                esac
            done
        )
    fi
    CP_RENDERER_SCAN_DONE=1
}

# Success only when the renderer's TCP port is actually LISTENing. A PID-alive
# check alone adopts a renderer that is alive but wedged (crashed listener thread
# / stuck before serving), leaving a dead cluster/overlay until the next reboot.
# netstat is a finite kernel-socket snapshot (unlike a blocking /proc walk) and is
# on the shared PATH. Ceiling: this catches "never started serving"; a renderer
# that bound the port then hung afterwards still reads healthy.
# ponytail: port-LISTEN probe; add a real ping/heartbeat only if post-bind hangs appear.
cp_renderer_healthy()
{
    CP_HP=`cp_renderer_port "$1"` || return 1
    CP_NS=`netstat -an 2>/dev/null`
    # Fail-OPEN if netstat produced nothing usable: a probe glitch must never kill a
    # live renderer (that would just re-add the pre-P3 "leave untouched" risk for a
    # healthy one). Only an affirmative "port is not LISTENing" counts as unhealthy.
    [ -z "$CP_NS" ] && return 0
    echo "$CP_NS" | grep -i listen | grep -q "[.:]${CP_HP} "
}

# Terminate a renderer's recorded generation and clear its pid file so the caller
# can start a fresh one. Used when initial adoption finds an unhealthy renderer.
cp_kill_renderer()
{
    CP_KP=`cp_renderer_pids "$1"`
    for CP_PID in $CP_KP; do
        echo "[carplay] SIGTERM wedged $1 pid=$CP_PID" >> "${WLOG:-/dev/null}"
        kill -15 "$CP_PID" 2>/dev/null
    done
    sleep 1
    for CP_PID in $CP_KP; do
        [ -d "/proc/$CP_PID" ] && kill -9 "$CP_PID" 2>/dev/null
    done
    CP_KF=`cp_renderer_pid_file "$1"` && rm -f "$CP_KF"
}

# Return success when at least one exact renderer executable is present. Use
# pidin's finite snapshot: on this unit, iterating every /proc/<pid>/cmdline can
# block forever on a live process and prevented dio_manager from starting.
cp_renderer_running()
{
    CP_WANT=$1
    CP_PID_FILE=`cp_renderer_pid_file "$CP_WANT"` || return 1
    if [ -r "$CP_PID_FILE" ]; then
        CP_KNOWN_PID=
        read CP_KNOWN_PID < "$CP_PID_FILE"
        [ -n "$CP_KNOWN_PID" ] && [ -d "/proc/$CP_KNOWN_PID" ]
        return $?
    fi
    [ "$CP_RENDERER_SCAN_DONE" = 1 ] && return 1

    pidin ar 2>/dev/null | (
        while read CP_PID CP_EXE CP_REST; do
            case "$CP_EXE" in
                "$CP_WANT"|*"/$CP_WANT")
                    echo "$CP_PID" > "$CP_PID_FILE"
                    exit 0
                    ;;
            esac
        done
        exit 1
    )
}

# Print the PIDs belonging to one exact renderer executable. Keeping this as a
# finite pidin snapshot is important on QNX: walking /proc/*/cmdline can block.
cp_renderer_pids()
{
    CP_WANT=$1
    CP_PID_FILE=`cp_renderer_pid_file "$CP_WANT"` || return 1
    if [ -r "$CP_PID_FILE" ]; then
        CP_KNOWN_PID=
        read CP_KNOWN_PID < "$CP_PID_FILE"
        if [ -n "$CP_KNOWN_PID" ] && [ -d "/proc/$CP_KNOWN_PID" ]; then
            echo "$CP_KNOWN_PID"
        fi
        return 0
    fi

    pidin ar 2>/dev/null | (
        while read CP_PID CP_EXE CP_REST; do
            case "$CP_EXE" in
                "$CP_WANT"|*"/$CP_WANT")
                    echo "$CP_PID" > "$CP_PID_FILE"
                    echo "$CP_PID"
                    exit 0
                    ;;
            esac
        done
    )
}

cp_stop_renderer_snapshot()
{
    CP_STOP_LOG=$1
    CP_STOP_MANEUVER_PIDS=$2
    CP_STOP_ALTSCREEN_PIDS=$3

    for CP_PID in $CP_STOP_MANEUVER_PIDS; do
        echo "[carplay] SIGTERM maneuver_render pid=$CP_PID" >> "$CP_STOP_LOG"
        kill -15 "$CP_PID" 2>/dev/null
    done
    for CP_PID in $CP_STOP_ALTSCREEN_PIDS; do
        echo "[carplay] SIGTERM altscreen_render pid=$CP_PID" >> "$CP_STOP_LOG"
        kill -15 "$CP_PID" 2>/dev/null
    done
    sleep 1
    for CP_PID in $CP_STOP_MANEUVER_PIDS; do
        if [ -d "/proc/$CP_PID" ]; then
            echo "[carplay] SIGKILL residual maneuver_render pid=$CP_PID" >> "$CP_STOP_LOG"
            kill -9 "$CP_PID" 2>/dev/null
        fi
    done
    for CP_PID in $CP_STOP_ALTSCREEN_PIDS; do
        if [ -d "/proc/$CP_PID" ]; then
            echo "[carplay] SIGKILL residual altscreen_render pid=$CP_PID" >> "$CP_STOP_LOG"
            kill -9 "$CP_PID" 2>/dev/null
        fi
    done
}

cp_stop_renderers()
{
    CP_STOP_LOG=$1

    # Snapshot the exact old generation once. A replacement dio_manager may
    # start new renderers during the one-second graceful shutdown window. The
    # previous implementation re-scanned all processes before SIGKILL and thus
    # killed those brand-new renderers, breaking the hook TCP link mid-GOP.
    CP_STOP_MANEUVER_PIDS=`cp_renderer_pids maneuver_render`
    CP_STOP_ALTSCREEN_PIDS=`cp_renderer_pids altscreen_render`
    cp_stop_renderer_snapshot "$CP_STOP_LOG" \
        "$CP_STOP_MANEUVER_PIDS" "$CP_STOP_ALTSCREEN_PIDS"
}
