#!/bin/sh
# Add altScreen TCP 6031 and buffered-audio TCP 6032 to the stock carplay0 rule
# PERMANENTLY, at the source.
#
# Why here and not at runtime:
#   /etc is a ramdisk copy of /mnt/system/etc, rebuilt every boot
#   (startup.sh move_etc_to_ramdisk: cp -r /mnt/system/etc /ramdisk/etc; ln -Ps).
#   A live `pfctl` edit dies on reboot; boot's `start_network` then reloads the
#   stock ruleset (which lacks 6031/6032), racing HMI/CarPlay startup. Editing the
#   SOURCE means the ramdisk copy already carries both ports and CarPlay never touches PF.
#
# Why ALL the profiles (learned the hard way, 2026-08-28):
#   the firmware ships one ruleset per external interface — pf.conf, pf.ecm0.conf,
#   pf.mlan0.conf, pf.ppp0.conf — and the one actually loaded depends on which
#   external interface comes up. On a car with the LTE modem the live profile is
#   **pf.ecm0.conf**, NOT pf.conf. Patching only pf.conf silently does nothing.
#   All four profiles must contain 6030:6032 because the active profile can change
#   across boots/network state.
#
# Idempotent. Run once as root on the unit; survives reboots.

# SYS_ROOT also lets the host installer tests use an isolated unit tree.
SYS_ROOT=${SYS_ROOT:-/mnt/system}
mount -uw "$SYS_ROOT" 2>/dev/null

# QNX has no sed. Rewrite only the known CarPlay TCP port-list rule using
# shell builtins; a matching range in a comment or another interface is not
# evidence that this profile is patched.
NEW=
trap '[ -z "$NEW" ] || rm -f "$NEW"' 0
patch_profile()
{
    SRC=$1
    NEW=$SRC.carplay.$$.new
    : > "$NEW" || return 1
    hits=0
    changed=0
    while IFS= read -r line || [ -n "$line" ]; do
        case "$line" in
            'pass in quick on carplay0 proto tcp from any to any port {'*)
                token=
                case "$line" in
                    *' 6030,'*) token=' 6030,' ;;
                    *' 6030:6031,'*) token=' 6030:6031,' ;;
                    *' 6030:6033,'*) token=' 6030:6033,' ;;
                    *' 6030:6032,'*) hits=$((hits + 1)) ;;
                esac
                if [ -n "$token" ]; then
                    line=${line%%"$token"*}' 6030:6032,'${line#*"$token"}
                    hits=$((hits + 1))
                    changed=1
                fi
                ;;
        esac
        printf '%s\n' "$line" >> "$NEW" || return 1
    done < "$SRC" || return 1
    [ "$hits" = 1 ] || {
        echo "ERROR: expected exactly one supported CarPlay TCP rule in $SRC, found $hits"
        return 1
    }
    if [ "$changed" = 0 ]; then
        echo "already patched: $SRC"
    else
        if [ ! -f "$SRC.stock" ]; then
            cp -p "$SRC" "$SRC.stock" || return 1
        fi
        # Keep the source file's owner and mode. Stop on any failed write.
        cat "$NEW" > "$SRC" || return 1
        echo "patched $SRC (backup at $SRC.stock)"
    fi
    rm -f "$NEW" || return 1
    NEW=
}

seen=0
for name in pf.conf pf.ecm0.conf pf.mlan0.conf pf.ppp0.conf; do
    [ -f "$SYS_ROOT/etc/$name" ] || continue
    seen=$((seen + 1))
    patch_profile "$SYS_ROOT/etc/$name" || exit 1
done
[ "$seen" -gt 0 ] || { echo "ERROR: no PF profiles found in $SYS_ROOT/etc"; exit 1; }
exit 0
