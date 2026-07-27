#!/bin/sh
# esd mhi2q-carplay-rgi_check.sh v0.1.0

trap '' 2

export PATH=.:/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/media/gracenote/bin:/mnt/app/armle/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/bin:/mnt/app/armle/usr/sbin
export LD_LIBRARY_PATH=/lib:/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/lib/dll:/mnt/app/armle/usr/lib
unset LD_PRELOAD

export GEM=1
echo -ne "M.I.B. - More Incredible Bash "
cat /net/mmx/fs/sda0/VERSION
echo ""
echo "MHI2Q CarPlay Route Guidance patch - CHECK"
echo ""

/net/mmx/fs/sda0/apps/mhi2q-carplay-rgi -c

echo ""
echo "Now you can go back..."

trap 2
exit 0
