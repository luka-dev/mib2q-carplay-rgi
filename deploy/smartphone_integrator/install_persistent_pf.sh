#!/bin/sh
# Add altScreen TCP 6031, buffered-audio TCP 6032, and buffered RTCP TCP 6033
# to the stock carplay0 rule
# PERMANENTLY, at the source.
#
# Why here and not at runtime:
#   /etc is a ramdisk copy of /mnt/system/etc, rebuilt every boot
#   (startup.sh move_etc_to_ramdisk: cp -r /mnt/system/etc /ramdisk/etc; ln -Ps).
#   A live `pfctl` edit dies on reboot; boot's `start_network` then reloads the
#   stock ruleset (which lacks 6031/6032/6033), racing HMI/CarPlay startup. Editing the
#   SOURCE means the ramdisk copy already carries both ports and CarPlay never touches PF.
#
# Why ALL the profiles (learned the hard way, 2026-08-28):
#   the firmware ships one ruleset per external interface — pf.conf, pf.ecm0.conf,
#   pf.mlan0.conf, pf.ppp0.conf — and the one actually loaded depends on which
#   external interface comes up. On a car with the LTE modem the live profile is
#   **pf.ecm0.conf**, NOT pf.conf. Patching only pf.conf silently does nothing.
#   All four profiles must contain 6030:6033 because the active profile can change
#   across boots/network state.
#
# Idempotent. Run once as root on the unit; survives reboots.

mount -uw /mnt/system 2>/dev/null

rc=1
for SRC in /mnt/system/etc/pf.conf /mnt/system/etc/pf.ecm0.conf \
           /mnt/system/etc/pf.mlan0.conf /mnt/system/etc/pf.ppp0.conf; do
    [ -f "$SRC" ] || continue
    if grep -q '6030:6033' "$SRC"; then
        echo "already patched: $SRC"; rc=0; continue
    fi
    if ! grep -q 'on carplay0 proto tcp .*6030' "$SRC"; then
        echo "SKIP (no stock carplay0 6030 rule): $SRC"; continue
    fi
    [ -f "$SRC.stock" ] || cp "$SRC" "$SRC.stock" 2>/dev/null
    # Upgrade a pristine stock profile and both previous deployments.
    if grep -q '6030:6032' "$SRC"; then
        sed -i 's/6030:6032/6030:6033/' "$SRC"
    elif grep -q '6030:6031' "$SRC"; then
        sed -i 's/6030:6031/6030:6033/' "$SRC"
    else
        sed -i 's/\(on carplay0 proto tcp .*\)6030\(,\)/\16030:6033\2/' "$SRC"
    fi
    if grep -q '6030:6033' "$SRC"; then
        echo "patched $SRC (backup at $SRC.stock)"; rc=0
    else
        echo "SED FAILED: $SRC"
    fi
done
exit $rc
