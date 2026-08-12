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
# Renderers are process-persistent services and survive ordinary dio replacement;
# they already clear their session state and accept the next hook/Java connection.
# Here we only run Audi's stock mdnsd/PPS cleanup. An explicit supervisor stop uses
# the owner-monitor's exact renderer PID snapshot.

WLOG=/tmp/carplay_wrapper.log
echo "[cleanup] CarPlay cleanup requested (persistent renderers left alive)" >> "$WLOG"

# Must live under /mnt/app/root/hooks; the stock script below is a different file.
[ -x /etc/scripts/carplay_cleanup.sh ] && /etc/scripts/carplay_cleanup.sh
exit 0
