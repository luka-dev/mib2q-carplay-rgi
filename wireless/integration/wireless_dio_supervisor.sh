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
exec "$H/wireless_dio_supervisor_bin"
