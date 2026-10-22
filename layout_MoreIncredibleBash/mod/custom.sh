#!/bin/sh
# M.I.B. -> Advanced Settings -> Run Custom Script: CarPlay cluster layout options.
#
# Installs <card>/mod/carplay_layout.cfg as /mnt/app/carplay_layout.cfg.  If the card's file
# has no active line (everything commented, as shipped) or is missing, the installed file is
# removed and the built-in layout is back.  The previous installed file is saved on the card
# as mod/carplay_layout.cfg.previous.
#
# Keys and rules are explained in the card's carplay_layout.cfg.  This script refuses a file
# with an unknown key, a wrong field count, a non-number or an unknown word (nothing is
# changed then).  The safe-area limits are checked where they are used: the hook logs a
# rejected rectangle and keeps the built-in one.  Lower-case words, as in the template.
#
# Safe areas and the phone's cards apply at the next phone connection; compact and
# view_source within 2 seconds.  No reboot needed.
#
# QNX 6.5 /bin/sh is ksh (pdksh) - this script stays inside its portable subset.
# SPDX-License-Identifier: GPL-3.0-or-later
set -u
set -f          # the lines are split with set --: never expand a * from the file
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/usr/bin
export PATH
unset LD_PRELOAD 2>/dev/null

case $0 in */*) D=${0%/*} ;; *) D=. ;; esac
D=$(cd "$D" && pwd) || exit 1

if [ -z "${CP_LAYOUT_DST:-}" ] && [ ! -d /mnt/app/eso/hmi/lsd ] && [ -d /net/mmx/mnt/app/eso/hmi/lsd ]; then
    exec on -f mmx /bin/sh "$D/custom.sh" "$@"
fi

DST=${CP_LAYOUT_DST:-/mnt/app/carplay_layout.cfg}   # override only for a host test
SRC=$D/carplay_layout.cfg
echo "custom.sh: CarPlay cluster layout options"
CR=$(printf '\r')   # a file saved by a Windows editor ends every line with CR

# ---- check the card's file: known keys, field counts ----
active=0
bad=0
n=0
if [ -f "$SRC" ]; then
    while IFS= read -r line || [ -n "$line" ]; do
        n=$((n+1))
        line=${line%"$CR"}
        line=${line%%#*}
        set -- $line
        [ $# -eq 0 ] && continue
        key=$1
        case $key in
            full|sport|classic) want=5 ;;
            cards|eta|speed_limit|view_source|compact|car_profile) want=2 ;;
            *) want=0 ;;
        esac
        ok=1
        if [ $want -eq 0 ] || [ $# -ne $want ]; then
            ok=0
        elif [ $want -eq 5 ]; then
            for v in $2 $3 $4 $5; do
                case $v in ''|*[!0-9]*) ok=0 ;; esac
            done
        else
            case $key:$2 in
                cards:right|cards:left|cards:center|cards:centre|cards:top) ;;
                eta:yes|eta:no|speed_limit:yes|speed_limit:no) ;;
                view_source:auto|view_source:choice|view_source:vc) ;;
                compact:auto|compact:sport|compact:classic) ;;
                car_profile:auto|car_profile:off) ;;
                *) ok=0 ;;
            esac
        fi
        if [ $ok -eq 0 ]; then
            echo "  line $n: not understood: $line"
            bad=$((bad+1))
            continue
        fi
        active=$((active+1))
    done < "$SRC"
fi

if [ $bad -gt 0 ]; then
    echo "FAILED: $bad line(s) in $SRC not understood; nothing was changed."
    exit 1
fi

[ -n "${CP_LAYOUT_DST:-}" ] || mount -uw /mnt/app 2>/dev/null || echo "custom.sh: mount -uw /mnt/app failed"
if [ -f "$DST" ]; then
    cp "$DST" "$D/carplay_layout.cfg.previous" 2>/dev/null &&
        echo "  previous layout saved on the card as mod/carplay_layout.cfg.previous"
fi

if [ $active -eq 0 ]; then
    rm -f "$DST"
    if [ -e "$DST" ]; then
        echo "FAILED: could not remove $DST (is /mnt/app read-only?)"
        exit 1
    fi
    sync
    echo "DONE. Built-in layout (no layout file installed)."
    echo "Reconnect the phone to apply."
    exit 0
fi

cp "$SRC" "$DST.new" 2>/dev/null && mv -f "$DST.new" "$DST" 2>/dev/null
# the unit has no cmp: a complete copy has the card file's size
if [ ! -f "$DST" ] || [ $(($(wc -c < "$SRC"))) -ne $(($(wc -c < "$DST"))) ]; then
    rm -f "$DST.new"
    echo "FAILED: could not write $DST (is /mnt/app read-only?)"
    exit 1
fi
sync
echo "  installed $active setting(s):"
while IFS= read -r line || [ -n "$line" ]; do
    line=${line%"$CR"}
    line=${line%%#*}
    set -- $line
    [ $# -eq 0 ] || echo "    $*"
done < "$DST"
echo "DONE. compact / view_source apply within 2 seconds;"
echo "safe areas and cards apply when the phone connects next (reconnect it now)."
