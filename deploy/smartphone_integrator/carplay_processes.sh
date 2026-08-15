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

# Guarded recovery state for the one failure class which happens before RTSP:
# USB reports both iAP2 interfaces matched but only one running.  These files
# survive SI child replacement but live only until HU reboot (/tmp).
CP_USB_PPS=/ramdisk/pps/device/usb-1.0.1
CP_USB_FAIL_FILE=/tmp/carplay_usb_pre_setup_failures
CP_USB_RESET_PENDING=/tmp/carplay_usb_reset.pending
CP_USB_RESET_LATCH=/tmp/carplay_usb_reset.latched

# Stock MU1316 PF admits the single SDK-210 screen socket on TCP 6030 and then
# drops every other inbound carplay0 packet. Native altScreen uses the adjacent
# TCP 6031, so the second IPv6 screen connection never reaches accept() unless
# the stock rule is extended. Keep /etc immutable: once per HU boot, validate
# and load an otherwise-identical runtime ruleset with 6030:6031 enabled.
CP_PFCTL=/armle/sbin/pfctl
CP_PF_SED=/usr/bin/sed
CP_PF_GREP=/usr/bin/grep
CP_PF_SOURCE=/etc/pf.conf
CP_PF_RUNTIME=/tmp/pf.carplay_alt.conf
CP_PF_RULES=/tmp/pf.carplay_alt.rules
CP_PF_MARKER=/tmp/carplay_pf_6031.enabled

cp_altscreen_firewall_ready()
{
    [ -x "$CP_PFCTL" ] || return 1
    "$CP_PFCTL" -sr > "$CP_PF_RULES" 2>/dev/null || return 1
    "$CP_PF_GREP" -q 'carplay0.*6030:6031' "$CP_PF_RULES"
}

cp_enable_altscreen_firewall()
{
    if cp_altscreen_firewall_ready; then
        : > "$CP_PF_MARKER"
        return 0
    fi
    [ -e "$CP_PF_MARKER" ] && rm -f "$CP_PF_MARKER"
    if [ ! -r "$CP_PF_SOURCE" ] || [ ! -x "$CP_PF_SED" ] ||
       [ ! -x "$CP_PF_GREP" ] || [ ! -x "$CP_PFCTL" ]; then
        echo "[supervisor] altScreen PF setup unavailable; TCP 6031 remains blocked" >> "${WLOG:-/dev/null}"
        return 1
    fi

    # Match the exact stock CarPlay allow-list token. If a future firmware no
    # longer has it, fail open to normal CarPlay instead of loading a malformed
    # or incomplete firewall configuration.
    "$CP_PF_SED" 's/ 6030,/ 6030:6031,/' "$CP_PF_SOURCE" > "$CP_PF_RUNTIME" || return 1
    if ! "$CP_PF_GREP" -q ' 6030:6031,' "$CP_PF_RUNTIME"; then
        echo "[supervisor] altScreen PF stock rule not found; ruleset left untouched" >> "${WLOG:-/dev/null}"
        return 1
    fi
    if ! "$CP_PFCTL" -nf "$CP_PF_RUNTIME" >> "${WLOG:-/dev/null}" 2>&1; then
        echo "[supervisor] altScreen PF syntax check FAILED; ruleset left untouched" >> "${WLOG:-/dev/null}"
        return 1
    fi
    if ! "$CP_PFCTL" -f "$CP_PF_RUNTIME" >> "${WLOG:-/dev/null}" 2>&1; then
        echo "[supervisor] altScreen PF load FAILED; stock rules remain active" >> "${WLOG:-/dev/null}"
        return 1
    fi
    if ! cp_altscreen_firewall_ready; then
        echo "[supervisor] altScreen PF verification FAILED after load" >> "${WLOG:-/dev/null}"
        return 1
    fi
    : > "$CP_PF_MARKER"
    echo "[supervisor] altScreen PF enabled: carplay0 TCP 6030:6031" >> "${WLOG:-/dev/null}"
    return 0
}

cp_usb_stuck_pre_setup()
{
    [ -r "$CP_USB_PPS" ] || return 1
    grep -q 'drivers_matched::2' "$CP_USB_PPS" 2>/dev/null || return 1
    grep -q 'drivers_running::1' "$CP_USB_PPS" 2>/dev/null
}

cp_usb_clear_recovery_state()
{
    rm -f "$CP_USB_FAIL_FILE" "$CP_USB_RESET_PENDING" "$CP_USB_RESET_LATCH"
}

# Called once by the next SI-owned wrapper, before renderers and dio. The
# monitor only queues the request; it never resets the connector underneath a
# dying/starting CarPlay process. One latch permits at most one physical reset
# until a successful control SETUP, a detached/non-stuck PPS state, or reboot.
cp_usb_consume_pending_reset()
{
    if ! cp_usb_stuck_pre_setup; then
        cp_usb_clear_recovery_state
        return 0
    fi
    [ -e "$CP_USB_RESET_PENDING" ] || return 0
    [ -e "$CP_USB_RESET_LATCH" ] && return 0

    : > "$CP_USB_RESET_LATCH"
    rm -f "$CP_USB_RESET_PENDING" "$CP_USB_FAIL_FILE"
    if [ ! -w /dev/media-con-ctrl ]; then
        echo "[supervisor] guarded USB reset skipped: /dev/media-con-ctrl unavailable" >> "${WLOG:-/dev/null}"
        return 0
    fi
    echo "[supervisor] guarded USB reset: reset port 3 250 1" >> "${WLOG:-/dev/null}"
    if print -n 'reset port 3 250 1' > /dev/media-con-ctrl; then
        # Let the bridge re-enumerate before the ordinary SI launch continues.
        sleep 1
    else
        echo "[supervisor] guarded USB reset write FAILED (latched; no reset loop)" >> "${WLOG:-/dev/null}"
    fi
}

# Count only generations which lived long enough for normal control SETUP but
# never produced its PID marker and still have the exact bad USB PPS signature.
cp_usb_record_generation_result()
{
    CP_USB_DIO_PID=$1
    CP_USB_LIVED_TICKS=$2
    CP_USB_MARKER=/tmp/carplay_control_setup.$CP_USB_DIO_PID

    if [ -e "$CP_USB_MARKER" ]; then
        echo "[supervisor] control SETUP seen pid=$CP_USB_DIO_PID; USB recovery latch cleared" >> "${WLOG:-/dev/null}"
        cp_usb_clear_recovery_state
        rm -f "$CP_USB_MARKER"
        return 0
    fi
    rm -f "$CP_USB_MARKER"
    if [ "$CP_USB_LIVED_TICKS" -lt 4 ] || ! cp_usb_stuck_pre_setup; then
        # A short SI replacement or any post-USB state is not evidence for the
        # physical reset. A detached/non-stuck state also rearms the latch.
        if ! cp_usb_stuck_pre_setup; then cp_usb_clear_recovery_state; fi
        return 0
    fi
    [ -e "$CP_USB_RESET_LATCH" ] && return 0
    CP_USB_FAILS=0
    if [ -r "$CP_USB_FAIL_FILE" ]; then read CP_USB_FAILS < "$CP_USB_FAIL_FILE"; fi
    case "$CP_USB_FAILS" in ''|*[!0-9]*) CP_USB_FAILS=0 ;; esac
    CP_USB_FAILS=`expr "$CP_USB_FAILS" + 1`
    echo "$CP_USB_FAILS" > "$CP_USB_FAIL_FILE"
    echo "[supervisor] pre-control-SETUP USB failure $CP_USB_FAILS/3 pid=$CP_USB_DIO_PID" >> "${WLOG:-/dev/null}"
    if [ "$CP_USB_FAILS" -ge 3 ]; then
        : > "$CP_USB_RESET_PENDING"
        echo "[supervisor] guarded USB reset queued for next SI generation" >> "${WLOG:-/dev/null}"
    fi
}

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

# Initial-adoption health differs by renderer topology:
#   - altscreen_render OWNS the :19831 listener, so LISTEN is a valid readiness
#     signal and catches a process wedged before serving the hook;
#   - maneuver_render is a CLIENT of Java RendererServer's :19800 listener.
#     Java deliberately closes :19800 when RGI is inactive, while the persistent
#     renderer stays alive and retries. Treating that normal interval as an
#     unhealthy renderer killed/recreated its Qualcomm EGL context on every
#     CarPlay reconnect. For maneuver_render, a live recorded PID is therefore
#     the only non-invasive adoption check until a renderer-owned control/health
#     endpoint exists.
# netstat is a finite kernel-socket snapshot (unlike a blocking /proc walk).
cp_renderer_healthy()
{
    case "$1" in
        maneuver_render)  return 0 ;;
        altscreen_render) CP_HP=19831 ;;
        *) return 1 ;;
    esac
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
    CP_KILL_GRACE=${2:-4}
    for CP_PID in $CP_KP; do
        echo "[carplay] SIGTERM wedged $1 pid=$CP_PID" >> "${WLOG:-/dev/null}"
        kill -15 "$CP_PID" 2>/dev/null
    done
    # Qualcomm WFD/EGL may still hold native images for several vsyncs after
    # SIGTERM.  Give normal OMX/EGL teardown time to complete before the final
    # emergency SIGKILL; ordinary dio churn never enters this path anymore.
    sleep "$CP_KILL_GRACE"
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
    # The Qualcomm WFD/EGL teardown path may retain native images for several
    # vsyncs.  Match cp_kill_renderer's grace before the emergency SIGKILL.
    sleep 4
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
    # start new renderers during the graceful shutdown window. The
    # previous implementation re-scanned all processes before SIGKILL and thus
    # killed those brand-new renderers, breaking the hook TCP link mid-GOP.
    CP_STOP_MANEUVER_PIDS=`cp_renderer_pids maneuver_render`
    CP_STOP_ALTSCREEN_PIDS=`cp_renderer_pids altscreen_render`
    cp_stop_renderer_snapshot "$CP_STOP_LOG" \
        "$CP_STOP_MANEUVER_PIDS" "$CP_STOP_ALTSCREEN_PIDS"
}
