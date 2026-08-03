#!/bin/sh
# smartphone_integrator children.carplay cleanupScript.
# Idempotent with carplay_startup.sh: the stop marker prevents the supervisor from
# restarting a renderer while cleanup is in progress. Then release EGL/OMX/Screen
# gracefully and finally run Audi's stock mdnsd/PPS cleanup.

H=/mnt/app/root/hooks
WLOG=/tmp/carplay_wrapper.log
STOP_FILE=/tmp/carplay_supervisor.stop

: > "$STOP_FILE"
echo "[cleanup] CarPlay cleanup requested" >> "$WLOG"

if [ -r "$H/carplay_processes.sh" ]; then
    . "$H/carplay_processes.sh"
    cp_stop_renderers "$WLOG"
else
    echo "[cleanup] missing $H/carplay_processes.sh" >> "$WLOG"
fi

# This wrapper must live under /mnt/app/root/hooks. Do not overwrite the stock
# /etc script or this call would recurse.
[ -x /etc/scripts/carplay_cleanup.sh ] && /etc/scripts/carplay_cleanup.sh
exit 0
