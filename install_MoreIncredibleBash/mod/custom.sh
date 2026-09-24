#!/bin/sh
# M.I.B. -> Advanced Settings -> Run Custom Script.
# QNX 6.5 /bin/sh is ksh (pdksh) - this script stays inside its portable subset.
# Copy into /mod/ on the M.I.B. SD; resources live in /mod/carplay/.
#
# Hybrid install:
#   - whole-file rootfs tree (carplay/root -> "/"): our hooks/.so/jar and the
#     already-mask-patched firmware binaries (smartphone_integrator, deva-ctrl-qc.so).
#   - in-place runtime patches (per-unit / stock-dependent), done here:
#       * smartphone_integrator.json  - replace the "carplay" child by path
#       * pf profiles                 - widen the CarPlay TCP rule (MANDATORY)
#       * dio_manager.json            - register the iAP2 route-guidance message IDs
# No CRC gymnastics, no lock dir. Atomic renames. Upgrade-safe: a stock backup is
# kept once so uninstall restores the original.
# Copyright (c) 2026 LuKa (@LuKa_dev)
set -u
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/usr/bin
export PATH
unset LD_PRELOAD

case $0 in */*) D=${0%/*} ;; *) D=. ;; esac
D=$(cd "$D" && pwd) || exit 1

# GEM runs on MMX. Forward a manual RCC launch (the MMX-local check below is false
# once we are on MMX, so this never loops).
if [ ! -d /mnt/app/eso/hmi/lsd ] && [ -d /net/mmx/mnt/app/eso/hmi/lsd ]; then
    exec on -f mmx /bin/sh "$D/custom.sh" "$@"
fi

RES=$D/carplay                 # resources (carplay_child.json)
ROOT=$RES/root                 # the "/" tree
ACTION=${1:-install}
[ -d "$ROOT" ] || { echo "no payload tree at $ROOT"; exit 1; }

CFG=/mnt/system/etc/eso/production/smartphone_integrator.json
PROFILES="pf.conf pf.ecm0.conf pf.mlan0.conf pf.ppp0.conf"
DIO=/mnt/system/etc/eso/production/dio_manager.json

mode_for() {
    case $1 in
        *.so|*.so.*)                          echo 755 ;;
        */maneuver_render|*/altscreen_render) echo 755 ;;
        */eso/bin/apps/*)                     echo 755 ;;  # firmware exec (smartphone_integrator)
        *.sh)                                 echo 755 ;;
        *)                                    echo 644 ;;  # models, atlas, .jar
    esac
}
# STOCK replacement (back up once, restore on uninstall) vs OWNED file (remove).
is_replaced() { case $1 in */eso/bin/apps/*|*/armle/lib/dll/*) return 0 ;; *) return 1 ;; esac; }
# count occurrences of $2 in $1 (ksh-safe, no external tools)
count_char() { n=0; rest=$1; while :; do case $rest in *"$2"*) rest=${rest#*"$2"}; n=$((n+1)) ;; *) break ;; esac; done; echo "$n"; }

backup_once() { [ -e "$2" ] || cp -p "$1" "$2" || { echo "FAILED backup $2"; return 1; }; }

# ---- smartphone_integrator.json: replace the "carplay" child by path ----------
patch_json() {
    FRAG=$RES/carplay_child.json
    [ -f "$CFG" ]  || { echo "  WARN no SI config at $CFG - skipping json"; return 0; }
    [ -f "$FRAG" ] || { echo "  WARN no carplay_child.json resource - skipping json"; return 0; }
    tmp=$CFG.carplay-new.$$
    : > "$tmp" || { echo "FAILED create $tmp"; return 1; }
    state=copy; hits=0; depth=0
    while IFS= read -r line || [ -n "$line" ]; do
        if [ "$state" = copy ]; then
            case $line in
                *'"carplay"'*:*)
                    set -f; set -- $line; set +f
                    compact=; for part in "$@"; do compact=$compact$part; done
                    [ "$compact" = '"carplay":{' ] || { echo "  WARN unsupported carplay layout"; rm -f "$tmp"; return 1; }
                    hits=$((hits+1)); state=skip; depth=0 ;;
                *) printf '%s\n' "$line" >> "$tmp"; continue ;;
            esac
        fi
        opens=$(count_char "$line" '{'); closes=$(count_char "$line" '}')
        depth=$((depth + opens - closes))
        if [ "$depth" -le 0 ]; then
            printf '        "carplay": ' >> "$tmp"
            cat "$FRAG" >> "$tmp"
            printf '%s\n' "${line##*\}}" >> "$tmp"       # keep the trailing comma
            state=copy
        fi
    done < "$CFG"
    [ "$hits" = 1 ] && [ "$state" = copy ] || { echo "  WARN expected one carplay child (hits=$hits)"; rm -f "$tmp"; return 1; }
    o=$(grep -c '"exec"' "$CFG"); n=$(grep -c '"exec"' "$tmp")
    [ "$o" = "$n" ] || { echo "  WARN SI child count changed ($o->$n)"; rm -f "$tmp"; return 1; }
    grep -q 'carplay_startup.sh' "$CFG" || backup_once "$CFG" "$CFG.carplay-stock" || { rm -f "$tmp"; return 1; }
    chmod 644 "$tmp"; mv -f "$tmp" "$CFG" && echo "  SI json patched"
}

# ---- pf: widen the CarPlay TCP rule in every profile (MANDATORY) --------------
patch_pf() {
    seen=0
    for profile in $PROFILES; do
        src=/mnt/system/etc/$profile
        [ -f "$src" ] || continue
        seen=$((seen+1))
        if grep -q ' 6030:6032,' "$src"; then echo "  pf $profile already widened"; continue; fi
        tmp=$src.carplay-new.$$; : > "$tmp"; hits=0
        while IFS= read -r line || [ -n "$line" ]; do
            case $line in 'pass in quick on carplay0 proto tcp from any to any port {'*)
                for tok in ' 6030,' ' 6030:6031,' ' 6030:6033,'; do
                    case $line in *"$tok"*) line=${line%%"$tok"*}' 6030:6032,'${line#*"$tok"}; hits=$((hits+1)); break ;; esac
                done ;;
            esac
            printf '%s\n' "$line" >> "$tmp"
        done < "$src"
        if [ "$hits" != 1 ]; then echo "  WARN pf $profile: rule hits=$hits (expected 1); left as-is"; rm -f "$tmp"; continue; fi
        backup_once "$src" "$src.carplay-stock" || { rm -f "$tmp"; continue; }
        chmod 644 "$tmp"; mv -f "$tmp" "$src" && echo "  pf $profile widened"
    done
    [ "$seen" -gt 0 ] || echo "  WARN no pf profiles found"
}

# ---- dio_manager.json: register the route-guidance message IDs ----------------
# The Cinemo iAP2 SDK passes only listed messages. The hook's Identify patch makes
# iOS offer route guidance but does not touch these lists. The file carries "##"
# comment lines, so it is edited as text: IDs are appended before the list's "]".
add_ids() {   # $1 = line holding one list, rest = IDs; prints the new line
    l=$1; shift
    for id in "$@"; do
        case $l in *"\"$id\""*) continue ;; esac
        head=${l%%]*}; tail=${l#*]}
        case $head in *'[') l=$head'"'$id'"]'$tail ;; *) l=$head', "'$id'"]'$tail ;; esac
    done
    printf '%s\n' "$l"
}
# iAP2 messages the hook needs Cinemo to declare in Identify: route guidance
# (0x52xx) and telephony (0x415B/0x415C/0x415D AcceptCall/EndCall/SwapCalls, 0x415F/0x4160
# Hold/MuteStatusUpdate, 0x4170/0x4171 ListUpdates).
DIO_SENT_IDS="0x5200 0x5203 0x415B 0x415C 0x415D 0x415F 0x4160 0x4170"
DIO_RECV_IDS="0x5201 0x5202 0x5204 0x4171"
patch_dio() {
    [ -f "$DIO" ] || { echo "  WARN no $DIO - route guidance will not arrive"; return 0; }
    tmp=$DIO.carplay-new.$$; : > "$tmp" || { echo "FAILED create $tmp"; return 1; }
    sent=0; recv=0; changed=0
    while IFS= read -r line || [ -n "$line" ]; do
        case $line in
            *'##'*) new=$line ;;
            *'"MessagesSentByAccessory":['*']'*)
                sent=$((sent+1)); new=$(add_ids "$line" $DIO_SENT_IDS) ;;
            *'"MessagesReceivedFromDevice":['*']'*)
                recv=$((recv+1)); new=$(add_ids "$line" $DIO_RECV_IDS) ;;
            *) new=$line ;;
        esac
        [ "$new" = "$line" ] || { line=$new; changed=1; }
        printf '%s\n' "$line" >> "$tmp"
    done < "$DIO"
    if [ "$sent" != 1 ] || [ "$recv" != 1 ]; then
        echo "  WARN dio_manager.json: lists sent=$sent recv=$recv (expected 1/1); left as-is"; rm -f "$tmp"; return 1
    fi
    n=0; want=0
    for id in $DIO_SENT_IDS $DIO_RECV_IDS; do want=$((want+1)); grep -q "\"$id\"" "$tmp" && n=$((n+1)); done
    [ "$n" = "$want" ] || { echo "  WARN dio_manager.json: only $n/$want IDs after edit; left as-is"; rm -f "$tmp"; return 1; }
    if [ "$changed" = 0 ]; then echo "  dio_manager.json IDs already registered"; rm -f "$tmp"; return 0; fi
    backup_once "$DIO" "$DIO.carplay-stock" || { rm -f "$tmp"; return 1; }
    chmod 644 "$tmp"; mv -f "$tmp" "$DIO" && echo "  dio_manager.json route-guidance and telephony IDs registered"
}

echo "custom.sh: CarPlay $ACTION"
echo "Remounting app and system read-write..."
mount -uw /mnt/app    2>/dev/null || true
mount -uw /mnt/system 2>/dev/null || true

LIST=/tmp/carplay_files.$$
( cd "$ROOT" && find . -type f > "$LIST" ) 2>/dev/null || { echo "FAILED listing payload"; rm -f "$LIST"; exit 1; }

case $ACTION in
install)
    cd "$ROOT" || { rm -f "$LIST"; exit 1; }
    while IFS= read -r f; do
        dest=/${f#./}; dir=${dest%/*}
        # macOS junk from a Mac-written card: AppleDouble "._name" and .DS_Store.
        # Never install it, and drop copies an older install put on the unit
        # (a "._carplay_hook.jar" in lsd/jars would land on j9's bootclasspath).
        case $f in */._*|*/.DS_Store) rm -f "$dest"; continue ;; esac
        mkdir -p "$dir" || { echo "FAILED mkdir $dir"; rm -f "$LIST"; exit 1; }
        if is_replaced "$dest"; then backup_once "$dest" "$dest.carplay-stock" || { rm -f "$LIST"; exit 1; }; fi
        tmp=$dest.carplay-new.$$
        cp -p "$f" "$tmp" || { echo "FAILED copy $tmp"; rm -f "$tmp" "$LIST"; exit 1; }
        chmod $(mode_for "$dest") "$tmp" || { echo "FAILED chmod $tmp"; rm -f "$tmp" "$LIST"; exit 1; }
        mv -f "$tmp" "$dest" || { echo "FAILED activate $dest"; rm -f "$tmp" "$LIST"; exit 1; }
        echo "  $dest"
    done < "$LIST"
    rm -f "$LIST"
    patch_json
    patch_pf
    patch_dio
    sync
    echo "DONE (install). Reboot the HU to load."
    ;;
uninstall)
    cd "$ROOT" || { rm -f "$LIST"; exit 1; }
    while IFS= read -r f; do
        case $f in *.DS_Store) continue ;; esac
        dest=/${f#./}
        if is_replaced "$dest" && [ -e "$dest.carplay-stock" ]; then
            mv -f "$dest.carplay-stock" "$dest" || echo "WARN restore $dest"
        else
            rm -f "$dest" "$dest.carplay-stock"
        fi
    done < "$LIST"
    rm -f "$LIST"
    # restore in-place patched configs
    [ -e "$CFG.carplay-stock" ] && mv -f "$CFG.carplay-stock" "$CFG"
    [ -e "$DIO.carplay-stock" ] && mv -f "$DIO.carplay-stock" "$DIO"
    for profile in $PROFILES; do
        s=/mnt/system/etc/$profile
        [ -e "$s.carplay-stock" ] && mv -f "$s.carplay-stock" "$s"
    done
    # renderers' GL program-binary cache (common/gl_program_cache.h)
    rm -rf /mnt/persist/var/app/luka_carplay_altscreen /mnt/persist/var/app/luka_carplay_maneuver
    sync
    echo "DONE (uninstall). Reboot the HU."
    ;;
*)
    rm -f "$LIST"
    echo "usage: custom.sh [install|uninstall]"; exit 2
    ;;
esac
