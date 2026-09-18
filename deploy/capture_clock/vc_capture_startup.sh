#!/bin/sh
# Private environment of the native encoder only. exec preserves framework PID.
APP_ROOT=${APP_ROOT:-/mnt/app}
VC_HOOKS=$APP_ROOT/root/hooks
VC_ENCODER=$APP_ROOT/eso/bin/apps/videoencoderservice
if [ -f "$VC_HOOKS/vc_capture_clock.enabled" ] && [ -r "$VC_HOOKS/libvc_capture_clock.so" ]; then
    if [ -z "${LD_PRELOAD:-}" ]; then
        LD_PRELOAD=$VC_HOOKS/libvc_capture_clock.so
        export LD_PRELOAD
    else
        echo 'vc_capture_clock: existing preload preserved; capture correction skipped' >&2
    fi
fi
exec "$VC_ENCODER" "$@"
