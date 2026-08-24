#!/bin/sh
# Add altScreen TCP 6031 to the stock carplay0 rule PERMANENTLY, at the source.
#
# Why here and not at runtime:
#   /etc is a ramdisk copy of /mnt/system/etc, rebuilt every boot
#   (startup.sh move_etc_to_ramdisk: cp -r /mnt/system/etc /ramdisk/etc; ln -Ps).
#   A live `pfctl` edit dies on reboot; boot's `start_network` then reloads the
#   stock /etc/pf.conf (which lacks 6031), racing HMI/CarPlay startup. Editing
#   the SOURCE means the ramdisk copy already carries 6031, start_network loads
#   the right ruleset, and CarPlay never has to touch PF at all.
#
# Idempotent. Run once as root on the unit; survives reboots. No churn, no guard.

SRC=/mnt/system/etc/pf.conf
mount -uw /mnt/system 2>/dev/null

grep -q '6030:6031' "$SRC" && { echo "already patched"; exit 0; }
if ! grep -q 'on carplay0 proto tcp .* 6030' "$SRC"; then
    echo "ERROR: stock carplay0 6030 rule not found in $SRC - not patching"; exit 1
fi

cp "$SRC" "$SRC.stock" 2>/dev/null
# widen the single 6030 token to the range 6030:6031 in the carplay0 tcp rule only
sed -i 's/\(on carplay0 proto tcp .*\)6030\(,\)/\16030:6031\2/' "$SRC"

grep -q '6030:6031' "$SRC" && echo "patched $SRC (backup at $SRC.stock)" || { echo "sed failed"; exit 1; }
