#!/bin/sh
# NCM bringup for carplay0 - our copy, replacing the stock /etc/scripts/startncm.sh.
#
# Fixes a real stock defect: the stock script's if_up flags are EN-DASHES (U+2013),
# not ASCII hyphens:
#     if if_up -p ${IF_NAME} <U+2013>s 2 <U+2013>r 1   # the s/r flags use U+2013, not '-'
# so if_up never receives -s/-r, its retry is not applied, and carplay0 comes up
# non-deterministically ("even stock was flaky"). Here the flags are ASCII and the
# retry is widened, and we bring the interface up even if if_up's probe is
# inconclusive - the mount already created it.
#
# Args (from usblauncher_otg.lua): $1 stack path, $2 interface, $3 iopkt instance.
# Everything else is byte-identical to stock (mount options, MACs, npkt).

STACK=$1
INTERFACE=$2
IOPKT_INSTANCE=$3
SOCKET=/cplay/dev/socket
IF_NAME=${INTERFACE}0

if [ "${IOPKT_INSTANCE}" = "io-pkt1" ]; then
    echo "Waiting for ${SOCKET}..."
    waitfor ${SOCKET} 2
    export SOCK=/cplay
fi

echo "<STARTING NCM DRIVER INTERFACE=\"${INTERFACE}\" STACK=\"${STACK}\" IOPKT_INSTANCE=\"${IOPKT_INSTANCE}\">"

mount -T ${IOPKT_INSTANCE} -o verbose=0,path=${STACK},protocol=ncm,usbdnet_mac=000022446688,mac=020022446688,name=${INTERFACE},npkt=10 /mnt/app/armle/lib/dll/devnp-usbdnet.so

# ASCII flags (stock had en-dashes): poll up to 10x at 1 s for the interface to appear.
if if_up -p ${IF_NAME} -s 1 -r 10; then
    ifconfig ${IF_NAME} up
    echo "${IF_NAME} up"
else
    # if_up was inconclusive, but the mount above created the interface. Bring it up
    # anyway rather than leaving it created-but-down (the stock 'not found' dead end).
    echo "${IF_NAME}: if_up inconclusive, forcing up"
    ifconfig ${IF_NAME} up 2>/dev/null || echo "${IF_NAME} not present"
fi
