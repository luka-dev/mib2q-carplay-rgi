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
#   4. ordinary dio exit/replacement leaves both persistent renderers alive;
#      only an explicit supervisor stop tears them down.

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
        # signal the renderer actually owns: :19831 for altscreen_render; a live
        # PID for maneuver_render, which is a CLIENT of Java's optional :19800.
        # If that renderer-specific check fails, replace the wedged PID.
        # The 2s monitor loop keeps the cheap PID-only check (it restarts exits).
        if [ "$SR_REASON" = initial ] && ! cp_renderer_healthy "$SR_NAME"; then
            echo "[supervisor] $SR_NAME alive but not serving - restarting" >> "$WLOG"
            # This is pre-dio adoption inside SI's 10 s startup budget. One
            # second is enough for a responsive renderer to run its SIGTERM
            # handler; the explicit system-stop path still keeps the full 4 s
            # Qualcomm EGL/OMX grace in cp_stop_renderer_snapshot().
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
    # One Qualcomm EGL client at a time: altscreen_render binds :19831 only after its early
    # EGL/Screen output exists, and a dual eglInitialize stampede is what the settle avoids.
    start_renderer altscreen_render initial
    sleep 1
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
        start_renderer altscreen_render restart

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
        MR_ALTSCREEN_PIDS=`cp_renderer_pids altscreen_render`
        echo "[supervisor] explicit stop - stopping renderer snapshot" >> "$WLOG"
        cp_stop_renderer_snapshot "$WLOG" \
            "$MR_MANEUVER_PIDS" "$MR_ALTSCREEN_PIDS"
    else
        cp_usb_record_generation_result "$MR_DIO_PID" "$MR_LIVED_TICKS"
        echo "[supervisor] dio_manager pid=$MR_DIO_PID gone - persistent renderers left alive" >> "$WLOG"
    fi
}

rm -f "$STOP_FILE"
echo "$DIO_PID" > "$OWNER_FILE"
echo "===== carplay_startup.sh run pid=$$ ppid=$PPID =====" >> "$WLOG"

# altScreen TCP 6031 and buffered-audio TCP 6032 live in all four
# /mnt/system/etc/pf*.conf profiles (install_persistent_pf.sh, a one-time deploy
# step). start_network loads the selected profile at boot, so the wrapper does
# no firewall work at all.

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

# altScreen — cluster second display (stream 111). Plain flag, like SPATIAL_5_1 below.
# We advertise a fabricated second display; iOS 27+ (per-display capability model) accepts it,
# but iOS <= 26 has a single-zoom-for-the-whole-car model and can reject it, which then breaks the
# BASE CarPlay connection too (docs/reference/IOS26_VS_IOS27_BONJOUR.md).
# Comment BOTH lines out for a stock-compatible base CarPlay that connects on every iOS.
export ALTSCREEN_HANDLE_ALT=1
export ALTSCREEN_ADVERT_DISPLAYS=1

# CarPlay buffered Spatial policy. Boolean values accept 0/1/true/false
# (also yes/no/on/off, case-insensitive).
#
# AUDIO_SPATIAL=1 requests APAC 5.1 (format index 65) and keeps the existing
# type-100 PCM6 path as fallback. It requires the HU PCM/mixer patch for at
# least six channels.
#
# AUDIO_SPATIAL_ATMOS=1 requests the full APAC 5.1.2 master (format index 66).
# It implies Spatial even if AUDIO_SPATIAL=0.
#
# downmix    folds Ltm/Rtm into Ls/Rs at -6 dB and sends PCM6
# drop       discards Ltm/Rtm and sends PCM6
# passthrough sends PCM8 and requires BOTH an 8ch HU PCM patch and a patched amp;
#             the current six-channel Audi patch is not sufficient.
export AUDIO_SPATIAL=1
export AUDIO_SPATIAL_ATMOS=0
export AUDIO_SPATIAL_ATMOS_MODE=downmix
export AUDIO_SPATIAL_CODEC_LIB="$H/libcarplay_apac.so"
# Second advertised APAudioFormatIndex for the buffered stream.  22 = AAC-LC/44.1/stereo,
# which the sender's own buffered set carries unconditionally and reaches for first, so a
# non-spatial track has a format it can arrive in instead of being skipped.  Empty = off.
export SPATIAL_103_EXTRA_INDEX=
# --- the USB device stack must exist before dio starts ------------------------------
#
# A fast unplug/replug can leave io-usb-dcd dead.  Nothing restarts it: the OTG
# usblauncher keeps running against a stack that is gone, and /etc/usblauncher.lua only
# starts things when the media connector (0x0424:0x2530) is *inserted*, gated on a flag
# in /tmp that is cleared only when the hub is removed -- and the hub is soldered in.
# So the state survives every replug and only a reboot clears /tmp.
#
# The damage downstream is silent: startncm.sh mounts NCM on the missing
# /dev/io-usb/otg_dev, carplay0 comes up with no carrier, iAP2 never starts, dio misses
# its startup budget, and smartphone_integrator restarts it forever at TIMEOUT_STARTUP.
# Seen once as ~500 consecutive dumps, unrecoverable without a reboot.
#
# The platform already ships the recovery -- it writes start_stack::device,2 to the PPS
# object the OTG manager listens on -- so just call it.  Verified on the car: with
# otg_dev missing this brought io-usb-dcd back and recreated the node.
#
# Here rather than in carplay_cleanup.sh because this runs on *every* launch, including
# each retry of that loop, so the system heals itself on the next attempt; cleanup may
# not run at all when SI kills dio on a startup timeout.
if ! ls /dev/io-usb/otg_dev >/dev/null 2>&1; then
    echo "[supervisor] USB device stack gone - restarting it" >> "$WLOG"
    /etc/scripts/start_otg_stack_device_mode.sh >> "$WLOG" 2>&1

    # The node alone is not enough. /etc/usblauncher_otg.lua declares
    #     Device_Stack = { cmd = '/etc/scripts/start_usb_device_stack.sh';
    #                      path = '/dev/io-usb/otg_dev'; descriptors = {carlife, carplay, rndis} }
    # so the OTG usblauncher *owns* the device stack: it starts io-usb-dcd, picks the gadget
    # descriptor, and mounts NCM through startncm.sh when the gadget appears. Restoring the
    # node behind its back leaves it holding stale state -- which is why, the first time this
    # guard ran, otg_dev came back and CarPlay still did not connect.
    #
    # Restarting it makes it redo that bring-up. Its argv is read from pidin rather than
    # /proc: on QNX /proc/<pid>/cmdline is empty for this process, and an earlier version of
    # this guard scanned /proc, found nothing and silently did nothing at all.
    OTG_ARGV=$(pidin -F "%A" 2>/dev/null | grep usblauncher_otg.lua | grep -v grep | head -1)
    OTG_PID=$(pidin -F "%a %A" 2>/dev/null | grep usblauncher_otg.lua | grep -v grep | head -1 | cut -d' ' -f2)
    if [ -n "$OTG_ARGV" ] && [ -n "$OTG_PID" ]; then
        echo "[supervisor] restarting OTG usblauncher pid=$OTG_PID" >> "$WLOG"
        kill "$OTG_PID" 2>/dev/null
        # full path: usblauncher is not on PATH here
        ( cd / && eval "/mnt/app/armle/bin/${OTG_ARGV#usblauncher }" >/dev/null 2>&1 & ) 2>/dev/null
    fi
fi

exec "$DIODIR/dio_manager" "$@"
