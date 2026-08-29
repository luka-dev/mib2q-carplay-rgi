#!/bin/sh
# Add altScreen TCP 6031 to the stock carplay0 rule PERMANENTLY, at the source.
#
# Why here and not at runtime:
#   /etc is a ramdisk copy of /mnt/system/etc, rebuilt every boot
#   (startup.sh move_etc_to_ramdisk: cp -r /mnt/system/etc /ramdisk/etc; ln -Ps).
#   A live `pfctl` edit dies on reboot; boot's `start_network` then reloads the
#   stock ruleset (which lacks 6031), racing HMI/CarPlay startup. Editing the
#   SOURCE means the ramdisk copy already carries 6031 and CarPlay never touches PF.
#
# Why ALL the profiles (learned the hard way, 2026-08-28):
#   the firmware ships one ruleset per external interface — pf.conf, pf.ecm0.conf,
#   pf.mlan0.conf, pf.ppp0.conf — and the one actually loaded depends on which
#   external interface comes up. On a car with the LTE modem the live profile is
#   **pf.ecm0.conf**, NOT pf.conf. Patching only pf.conf silently does nothing:
#   the active ruleset still shows `port = 6030` and altScreen's 6031 stays blocked.
#
# Idempotent. Run once as root on the unit; survives reboots.

mount -uw /mnt/system 2>/dev/null

rc=1
for SRC in /mnt/system/etc/pf.conf /mnt/system/etc/pf.ecm0.conf \
           /mnt/system/etc/pf.mlan0.conf /mnt/system/etc/pf.ppp0.conf; do
    [ -f "$SRC" ] || continue
    if grep -q '6030:6031' "$SRC"; then
        echo "already patched: $SRC"; rc=0; continue
    fi
    if ! grep -q 'on carplay0 proto tcp .* 6030' "$SRC"; then
        echo "SKIP (no stock carplay0 6030 rule): $SRC"; continue
    fi
    [ -f "$SRC.stock" ] || cp "$SRC" "$SRC.stock" 2>/dev/null
    sed -i 's/\(on carplay0 proto tcp .*\)6030\(,\)/\16030:6031\2/' "$SRC"
    if grep -q '6030:6031' "$SRC"; then
        echo "patched $SRC (backup at $SRC.stock)"; rc=0
    else
        echo "SED FAILED: $SRC"
    fi
done
exit $rc
