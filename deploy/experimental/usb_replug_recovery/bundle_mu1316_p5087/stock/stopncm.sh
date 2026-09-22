#!/bin/sh

CARLIFE_FLAG=/tmp/carlife_active


[ -f ${CARLIFE_FLAG} ] && IS_CARLIFE=1 || IS_CARLIFE=0
[ ${IS_CARLIFE} == 1 ] && INTERFACE="carlife0" || INTERFACE="carplay0"

echo "<STOPPING THE NCM DRIVER IS_CARLIFE=\"${IS_CARLIFE}\" INTERFACE=\"${INTERFACE}\">"

# To workaround race condition between removal/unmount
sleep 1

#if [ ${IS_CARLIFE} == 0 ]; then
#    export SOCK=/cplay
#fi

# Destroy NCM interface
ifconfig ${INTERFACE} destroy

# Delete CarLife indicator flag
if [ ${IS_CARLIFE} == 1 ]; then
    echo "Deleting ${CARLIFE_FLAG}"
    rm -f ${CARLIFE_FLAG}
fi
