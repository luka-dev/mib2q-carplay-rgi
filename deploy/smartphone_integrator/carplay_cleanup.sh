#!/bin/sh
# smartphone_integrator children.carplay cleanupScript.
#
# This script is identity-less: smartphone_integrator does not tell it WHICH
# dio_manager generation it is cleaning up for.  So it must NOT touch anything shared
# across generations, or it can damage a replacement generation that has already
# started:
#   - it does NOT create the global stop marker — a late write would make a freshly
#     started replacement monitor observe it and kill the NEW dio_manager;
#   - it does NOT stop renderers by the shared PID files — those may already name the
#     replacement generation's renderers.
# Renderer teardown belongs to the owner-monitor in carplay_startup.sh: it holds this
# exact generation's dio PID plus a renderer PID snapshot and stops only those when
# its own dio_manager exits.  Here we only run Audi's stock mdnsd/PPS cleanup.

WLOG=/tmp/carplay_wrapper.log
echo "[cleanup] CarPlay cleanup requested (renderers left to owner-monitor)" >> "$WLOG"

# Must live under /mnt/app/root/hooks; the stock script below is a different file.
[ -x /etc/scripts/carplay_cleanup.sh ] && /etc/scripts/carplay_cleanup.sh
exit 0
