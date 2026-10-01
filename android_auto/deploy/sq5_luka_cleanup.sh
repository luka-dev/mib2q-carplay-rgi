#!/bin/sh
# Maintenance stop for the aa-luka cluster renderer (NOT an SI cleanupScript: children.gal keeps
# its stock empty cleanupScript). Used by the card uninstall (on -f mmx) and by hand.
#   1. retire the monitor: remove the owner token, so any running sq5_luka_monitor.sh sees it no
#      longer owns a generation and exits within 2 s without touching the renderer;
#   2. stop sq5_mirror and maneuver_render (identity-checked TERM, then KILL after 2 s);
#   3. remove the /tmp runtime files.
# Never touches gal, USB/OTG or smartphone_integrator. Safe to run when nothing is running.
H=${H:-/mnt/app/root/hooks}
WLOG=${WLOG:-/tmp/sq5_luka_wrapper.log}
OWNER_FILE=${OWNER_FILE:-/tmp/sq5_luka_supervisor.owner}
export WLOG

echo "[cleanup] maintenance stop requested" >> "$WLOG"
rm -f "$OWNER_FILE"
if [ -r "$H/sq5_luka_processes.sh" ]; then
    . "$H/sq5_luka_processes.sh"
    sleep 3     # let the monitor notice (it polls every 2 s) so it cannot respawn the renderer
    cp_stop_renderer sq5_mirror 2
    cp_stop_renderer maneuver_render 2
else
    echo "[cleanup] $H/sq5_luka_processes.sh missing; renderer not signalled" >> "$WLOG"
fi
rm -f /tmp/sq5_luka_maneuver_render.pid /tmp/sq5_luka_maneuver_render.pid.* "$OWNER_FILE".*
rm -f /tmp/sq5_luka_sq5_mirror.pid /tmp/sq5_luka_sq5_mirror.pid.* /tmp/sq5_mirror_ready
echo "[cleanup] done" >> "$WLOG"
exit 0
