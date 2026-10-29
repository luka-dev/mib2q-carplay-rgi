#!/bin/sh
# Offline candidate for the existing rc.local respawn() entry.
H=${H:-/mnt/app/root/hooks}
export H
CONFIG=${MHI2Q_CP_CONFIG:-$H/wireless_carplay.env}
[ -r "$CONFIG" ] || {
    echo "[wireless-dio] missing configuration: $CONFIG" >&2
    exit 78
}
. "$CONFIG" || exit 78
if [ "${MHI2Q_CP_RECEIVER:-}" = stock210 ]; then
    # SI normally inherits these from armle/scripts/env.sh and carplay_child.json.
    # A manual SSH launch has neither; the framework otherwise reads ./config.
    IPL_CONFIG_DIR=${IPL_CONFIG_DIR:-/etc/eso/production}
    IPL_CONFIG_DIR_DIO_MANAGER=${IPL_CONFIG_DIR_DIO_MANAGER:-$IPL_CONFIG_DIR}
    LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/lib/dll:/mnt/app/armle/usr/lib}
    export IPL_CONFIG_DIR IPL_CONFIG_DIR_DIO_MANAGER LD_LIBRARY_PATH
    [ -r "$IPL_CONFIG_DIR/framework.json" ] &&
    [ -r "$IPL_CONFIG_DIR_DIO_MANAGER/dio_manager.json" ] || {
        echo '[wireless-dio] factory framework/DIO configuration unavailable' >&2
        exit 78
    }
    exec "$H/wireless_runtime" "$H/wireless_dio_supervisor_bin"
fi
exec "$H/wireless_dio_supervisor_bin"
