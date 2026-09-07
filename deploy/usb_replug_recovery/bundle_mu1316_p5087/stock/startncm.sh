#!/bin/sh

STACK=$1
INTERFACE=$2
IOPKT_INSTANCE=$3
SOCKET=/cplay/dev/socket

# Set interface name
IF_NAME=${INTERFACE}0

if [ ${IOPKT_INSTANCE} == "io-pkt1" ]; then
    echo "Waiting for ${SOCKET}..."
    waitfor ${SOCKET} 2
    export SOCK=/cplay
fi

echo "<STARTING NCM DRIVER INTERFACE=\"${INTERFACE}\" STACK=\"${STACK}\" IOPKT_INSTANCE=\"${IOPKT_INSTANCE}\">"

# Launch interface
mount -T ${IOPKT_INSTANCE} -o verbose=0,path=${STACK},protocol=ncm,usbdnet_mac=000022446688,mac=020022446688,name=${INTERFACE},npkt=10 /mnt/app/armle/lib/dll/devnp-usbdnet.so

# Wait until the interface is present
if if_up -p ${IF_NAME} –s 2 –r 1
then
    ifconfig ${IF_NAME} up
else
    echo "${IF_NAME} not found"
fi
