#!/bin/sh
# Shared CarPlay renderer process helpers for QNX 6.5 /bin/sh.
# Sourced by carplay_startup.sh and carplay_cleanup.sh.

# Do not depend on smartphone_integrator preserving its parent's PATH. Append a
# safe subset of the stock MU1316 MMX search locations (startup.sh:395/443), while
# retaining the inherited head and its resolution order for dio_manager.
# These entries provide QNX sleep/rm/kill and /mnt/app/armle/usr/bin/tr.
CP_QNX_PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/bin:/mnt/app/armle/usr/sbin
PATH=${PATH:+$PATH:}$CP_QNX_PATH
export PATH
unset CP_QNX_PATH

# Return success when at least one exact renderer executable is present. Convert
# QNX's NUL-separated cmdline before putting it in a shell variable (the same
# /proc seam used by the existing on-unit deployment helper).
cp_renderer_running()
{
    CP_WANT=$1
    for CP_PATH in /proc/[0-9]*; do
        [ -r "$CP_PATH/cmdline" ] || continue
        CP_CMD=$(tr '\000' ' ' < "$CP_PATH/cmdline" 2>/dev/null)
        CP_EXE=${CP_CMD%% *}
        case "$CP_EXE" in
            "$CP_WANT"|*"/$CP_WANT") return 0 ;;
        esac
    done
    return 1
}

# Graceful first: both renderers handle SIGTERM and release EGL/OMX/Screen state.
# Only a process still alive after one second is force-killed.
cp_stop_renderers()
{
    CP_STOP_LOG=$1
    CP_SENT=0
    for CP_NAME in maneuver_render altscreen_render; do
        for CP_PATH in /proc/[0-9]*; do
            [ -r "$CP_PATH/cmdline" ] || continue
            CP_CMD=$(tr '\000' ' ' < "$CP_PATH/cmdline" 2>/dev/null)
            CP_EXE=${CP_CMD%% *}
            case "$CP_EXE" in
                "$CP_NAME"|*"/$CP_NAME")
                    CP_PID=${CP_PATH#/proc/}
                    echo "[carplay] SIGTERM $CP_NAME pid=$CP_PID" >> "$CP_STOP_LOG"
                    kill -TERM "$CP_PID" 2>/dev/null
                    CP_SENT=1
                    ;;
            esac
        done
    done

    [ "$CP_SENT" = 1 ] && sleep 1

    for CP_NAME in maneuver_render altscreen_render; do
        for CP_PATH in /proc/[0-9]*; do
            [ -r "$CP_PATH/cmdline" ] || continue
            CP_CMD=$(tr '\000' ' ' < "$CP_PATH/cmdline" 2>/dev/null)
            CP_EXE=${CP_CMD%% *}
            case "$CP_EXE" in
                "$CP_NAME"|*"/$CP_NAME")
                    CP_PID=${CP_PATH#/proc/}
                    echo "[carplay] SIGKILL residual $CP_NAME pid=$CP_PID" >> "$CP_STOP_LOG"
                    kill -KILL "$CP_PID" 2>/dev/null
                    ;;
            esac
        done
    done
}
