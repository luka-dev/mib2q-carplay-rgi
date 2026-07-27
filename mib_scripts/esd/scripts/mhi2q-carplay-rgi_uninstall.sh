#!/bin/sh
# esd mhi2q-carplay-rgi_uninstall.sh v0.1.0

trap '' 2

export PATH=.:/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/media/gracenote/bin:/mnt/app/armle/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/bin:/mnt/app/armle/usr/sbin
export LD_LIBRARY_PATH=/lib:/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/lib/dll:/mnt/app/armle/usr/lib
unset LD_PRELOAD

export GEM=1
echo -ne "M.I.B. - More Incredible Bash "
cat /net/mmx/fs/sda0/VERSION
echo ""
echo "MHI2Q CarPlay Route Guidance patch - UNINSTALL (restore originals from backup)"
echo ""
echo "NOTE: NEVER interrupt the process with -Back- button or removing SD Card!!!"
echo ""

/net/mmx/fs/sda0/apps/mhi2q-carplay-rgi -u

trap 2
exit 0
