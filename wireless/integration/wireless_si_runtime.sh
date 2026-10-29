#!/bin/sh
# Bootstrap/mDNS only. SI owns every media child and its watchdog.
H=${H:-/mnt/app/root/hooks}
export H
CP_ENV=${MHI2Q_CP_ENV_FILE:-${MHI2Q_CP_CONFIG:-$H/wireless_carplay.env}}
[ -r "$CP_ENV" ] || exit 78
. "$CP_ENV" || exit 78
unset LD_PRELOAD
IPL_CONFIG_DIR=${IPL_CONFIG_DIR:-/etc/eso/production}
IPL_CONFIG_DIR_DIO_MANAGER=${IPL_CONFIG_DIR_DIO_MANAGER:-$IPL_CONFIG_DIR}
LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/lib/dll:/mnt/app/armle/usr/lib}
export IPL_CONFIG_DIR IPL_CONFIG_DIR_DIO_MANAGER LD_LIBRARY_PATH
[ -r "$IPL_CONFIG_DIR/framework.json" ] &&
[ -r "$IPL_CONFIG_DIR_DIO_MANAGER/dio_manager.json" ] || exit 78
export MHI2Q_CP_SI_CONTROL=${MHI2Q_CP_SI_CONTROL:-/tmp/mhi2q_si_control.sock}
case "$MHI2Q_CP_SI_CONTROL" in /*) ;; *) exit 78 ;; esac
[ -x "$H/mhi2q-wireless-daemon" ] && [ -x "$H/wireless_runtime" ] || exit 78
"$H/mhi2q-wireless-daemon" --check-config || exit 78
exec "$H/wireless_runtime" "$H/mhi2q-wireless-daemon"
