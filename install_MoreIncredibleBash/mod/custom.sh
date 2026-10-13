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
#       * NCM start/stop + usblauncher_otg.lua - only over the exact stock they replace
#       * SI usb.otgStackCleanupScript -> our generation-aware NCM cleanup
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

file_size() { set -- $(ls -l "$1" 2>/dev/null); echo "${5:-}"; }

# Firmware train from the RCC EEPROM, read like M.I.B. (config/GLOBALS: modifyE2P r 3A0 19),
# e.g. MHI2Q_CN_AUG22_K1004 = platform _ region _ brand(AU=Audi)+platform-generation _ version.
# modifyE2P prints "0x<addr> <hex>" lines (bytes spaced or not) and can block until the RCC
# E2P service is up, so it runs with a 10 s limit.  Sets TRAIN (empty if unreadable).
E2P_READ=${E2P_READ:-"on -f rcc /usr/apps/modifyE2P r 3A0 19"}
read_train() {
    TRAIN=; out=/tmp/carplay_train.$$
    $E2P_READ > "$out" 2>/dev/null &
    p=$!; i=0
    while kill -0 $p 2>/dev/null && [ $i -lt 10 ]; do sleep 1; i=$((i+1)); done
    kill $p 2>/dev/null
    while read -r addr rest; do
        case $addr in 0x*|0X*) ;; *) continue ;; esac
        for tok in $rest; do
            while [ -n "$tok" ]; do
                b=${tok%"${tok#??}"}; tok=${tok#??}
                case $b in [0-9a-fA-F][0-9a-fA-F]) ;; *) break ;; esac
                d=$((16#$b))
                [ $d -ge 32 ] && [ $d -lt 127 ] || continue
                TRAIN=$TRAIN$(printf "\\$((d/64))$((d/8%8))$((d%8))")
            done
        done
    done < "$out"
    rm -f "$out"
}
train_field() {   # $1 = field number (1 platform, 2 region, 3 brand+gen, 4 version)
    t=$TRAIN; i=1
    while [ $i -lt "$1" ]; do case $t in *_*) t=${t#*_} ;; *) t= ;; esac; i=$((i+1)); done
    echo "${t%%_*}"
}
# The Java patch replaces whole stock HMI classes, so each jar fits one firmware family:
# on any other the HMI fails at boot and RCC reboots MMX in a loop (issue #24, the US jar
# on MHI2Q_CN_AUG22_K1004).  US, ER and JP share every class we replace (byte-identical),
# CN differs in six (java_patch_cn/); scripts/audit_java_variants.sh proves each jar links
# cleanly on its firmware.  Chosen by the E2P region; any other region gets no jar.
java_jar() {
    case $(train_field 2) in
        US|ER|JP) echo carplay_hook.jar ;;
        CN)       echo carplay_hook_cn.jar ;;
    esac
}
# The firmware binaries we replace whole are patched MU1316 US P5087 files (same size as
# the stock they came from).  Other trains ship different builds (CN K1004: both differ),
# so replace only a file of that size - the P5087 stock or our own copy.
p5087_size() { case $1 in */smartphone_integrator) echo 802798 ;; */deva-ctrl-qc.so) echo 694904 ;; esac; }

backup_once() { [ -e "$2" ] || cp -p "$1" "$2" || { echo "FAILED backup $2"; return 1; }; }

# List every payload file into $2.  Normally from manifest.txt (written by
# prepare_install.sh), checking each file on its own: no walk of the card at all, and a
# partly copied card fails by name.  Without a manifest (hand-staged card) fall back to
# find.  QNX fs-dos cannot stat ".." inside the folders of some FAT cards: find then
# reports "./dir/..: Filename too long" and exits 1 although the list is complete.
# Tolerate exactly that error; anything else (or an empty list) still fails.
list_payload() {
    if [ -f "$RES/manifest.txt" ]; then
        : > "$2" || { echo "FAILED create $2"; return 1; }
        while IFS= read -r f || [ -n "$f" ]; do
            case $f in ''|*/._*|*/.DS_Store) continue ;; esac
            [ -f "$1/${f#./}" ] || { echo "FAILED missing payload file $f (copy mod/ again)"; rm -f "$2"; return 1; }
            printf '%s\n' "$f" >> "$2"
        done < "$RES/manifest.txt"
        [ -s "$2" ] || { echo "FAILED empty manifest"; rm -f "$2"; return 1; }
        return 0
    fi
    ( cd "$1" && find . -type f > "$2" ) 2> "$2.err"
    if [ ! -s "$2" ] || grep -v '/\.\.: Filename too long' "$2.err" | grep -q .; then
        echo "FAILED listing payload"; cat "$2.err" 2>/dev/null
        rm -f "$2" "$2.err"; return 1
    fi
    rm -f "$2.err"
}

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
    chmod 644 "$tmp"; mv -f "$tmp" "$CFG" && echo "  SI json patched" || return 1
    patch_otg_cleanup
}

# SI's usb.otgStackCleanupScript tears NCM down after every session with no identity;
# point it at our wrapper, which passes the ended session's NCM generation to stopncm.sh.
OTG_CLEANUP=/mnt/app/root/hooks/carplay_ncm_cleanup.sh
patch_otg_cleanup() {
    grep -q "\"otgStackCleanupScript\":\"$OTG_CLEANUP\"" "$CFG" && { echo "  SI otg cleanup already ours"; return 0; }
    [ "$(grep -c '"otgStackCleanupScript":"/etc/scripts/stopncm.sh"' "$CFG")" = 1 ] ||
        { echo "  WARN SI otgStackCleanupScript not the stock stopncm.sh - left as-is"; return 0; }
    tmp=$CFG.carplay-new.$$; : > "$tmp" || { echo "FAILED create $tmp"; return 1; }
    while IFS= read -r line || [ -n "$line" ]; do
        case $line in *'"otgStackCleanupScript":"/etc/scripts/stopncm.sh"'*)
            line=${line%%/etc/scripts/stopncm.sh*}$OTG_CLEANUP${line#*/etc/scripts/stopncm.sh} ;;
        esac
        printf '%s\n' "$line" >> "$tmp"
    done < "$CFG"
    grep -q "\"otgStackCleanupScript\":\"$OTG_CLEANUP\"" "$tmp" || { echo "  WARN SI otg cleanup edit failed"; rm -f "$tmp"; return 1; }
    chmod 644 "$tmp"; mv -f "$tmp" "$CFG" && echo "  SI otg cleanup -> $OTG_CLEANUP"
}

# ---- NCM fence: generation-aware startncm/stopncm + usblauncher_otg.lua ------
# Fixes the fast-replug race where a late teardown destroys the new carplay0
# (docs/connect-and-usb/USB_REPLUG_RECOVERY.md).  Whole files, so only over the exact
# stock they were written against (MU1316 P5087) or our own copy; anything else is left
# alone - the scripts and the Lua work independently, so a skip is never half-broken.
# The unit has no cmp: compare as text.  An older copy of our own scripts (marker line)
# counts as ours, so a script update still installs.  The stock copy for uninstall comes
# from the bundled original with the file's mode, so it is exact and executable even if an
# older install already replaced the file.
NCM_FILES="/mnt/system/etc/scripts/startncm.sh /mnt/system/etc/scripts/stopncm.sh /mnt/system/etc/usblauncher_otg.lua"
NCM_MARK='Generation-aware replacement for the stock MU1316'
same_text() { [ -f "$1" ] && [ -f "$2" ] && [ "$(cat "$1")" = "$(cat "$2")" ]; }
ncm_known() {   # $1 = on-unit file: stock, current payload, or an older copy of ours
    same_text "$1" "$RES/ncm/stock/${1##*/}" || same_text "$1" "$RES/ncm/payload/${1##*/}" ||
        { case $1 in *.sh) grep -q "$NCM_MARK" "$1" 2>/dev/null ;; *) false ;; esac; }
}
patch_ncm() {
    for dest in $NCM_FILES; do
        n=${dest##*/}
        [ -f "$RES/ncm/stock/$n" ] && [ -f "$RES/ncm/payload/$n" ] || { echo "  WARN no NCM resources - NCM fence skipped"; return 0; }
        case $n in *.sh) sh -n "$RES/ncm/payload/$n" || { echo "  WARN $n on the card is damaged - NCM fence skipped (copy mod/ again)"; return 0; } ;; esac
        ncm_known "$dest" || { echo "  WARN $dest is not the known stock - NCM fence skipped (firmware differs)"; return 0; }
    done
    for dest in $NCM_FILES; do
        n=${dest##*/}
        if [ ! -e "$dest.carplay-stock" ]; then
            cp "$RES/ncm/stock/$n" "$dest.carplay-stock" && chmod $(mode_for "$dest") "$dest.carplay-stock" ||
                { echo "FAILED backup $dest"; rm -f "$dest.carplay-stock"; return 1; }
        fi
        if same_text "$dest" "$RES/ncm/payload/$n"; then echo "  $dest already fenced"; continue; fi
        tmp=$dest.carplay-new.$$
        cp "$RES/ncm/payload/$n" "$tmp" && chmod $(mode_for "$dest") "$tmp" && mv -f "$tmp" "$dest" ||
            { echo "FAILED install $dest"; rm -f "$tmp"; return 1; }
        echo "  $dest"
    done
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
# (0x52xx), telephony (0x415B/0x415C/0x415D AcceptCall/EndCall/SwapCalls, 0x415F/0x4160
# Hold/MuteStatusUpdate, 0x4170/0x4171 ListUpdates) and vehicle status (0xA100/0xA102
# Start/StopVehicleStatusUpdates from the phone, 0xA101 VehicleStatusUpdate from the hook).
DIO_SENT_IDS="0x5200 0x5203 0x415B 0x415C 0x415D 0x415F 0x4160 0x4170 0xA101"
DIO_RECV_IDS="0x5201 0x5202 0x5204 0x4171 0xA100 0xA102"
# "nmea":"0xNNNN" is the NMEA bitmask dio advertises in Identify (LocationInformationComponent).
# Bit 0x08 = PASCD (VehicleSpeedData): the hook supplies the wheel-speed sentences.
add_nmea_speed() {   # $1 = line; prints the new line
    l=$1; pre=${l%%\"nmea\":\"*}; v=${l#*\"nmea\":\"}; rest=${v#*\"}; v=${v%%\"*}
    case $v in 0x[0-9A-Fa-f]*) printf '%s"nmea":"0x%04X"%s\n' "$pre" $((v | 8)) "$rest" ;; *) printf '%s\n' "$l" ;; esac
}
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
            *'"nmea":"0x'*)
                new=$(add_nmea_speed "$line") ;;
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
    chmod 644 "$tmp"; mv -f "$tmp" "$DIO" && echo "  dio_manager.json route-guidance, telephony and vehicle IDs registered (nmea +PASCD)"
}

# ---- lang_data.zip: the Sound effects "UpMix" row texts --------------------------
# carplay/upmix_lang.py (with our ldzip.py) appends the label and the five levels to
# every HMIToneEvoHigh locale; idempotent, atomic.  It needs M.I.B.'s micropython
# (apps/sbin/beta on the SD, or one installed by M.I.B. tools).  Without it the row
# shows the texts built into the jar, so a skip is never broken.  The zip is shared
# with other patches (e.g. luka-collection), so uninstall does not restore it: our
# appended strings stay, unused.  The first install keeps a copy (.carplay-stock).
LANG_ZIP=/mnt/app/eso/hmi/lsd/jars/lang_data.zip
patch_lang() {
    mp=
    for c in "$D/../apps/sbin/beta/micropython" "$(command -v micropython 2>/dev/null)"; do
        [ -n "$c" ] && [ -f "$c" ] && { mp=$c; break; }
    done
    [ -n "$mp" ] || { echo "  WARN no M.I.B. micropython - UpMix row keeps the jar's texts"; return 0; }
    [ -f "$LANG_ZIP" ] || { echo "  WARN no $LANG_ZIP - UpMix row keeps the jar's texts"; return 0; }
    backup_once "$LANG_ZIP" "$LANG_ZIP.carplay-stock" || return 0
    rm -f "$LANG_ZIP.carplay-new"
    # the zip is rebuilt in memory (~0.5 MB, 4 MB heap measured); default heaps are smaller
    "$mp" -X heapsize=16M "$RES/upmix_lang.py" "$LANG_ZIP" ||
        { rm -f "$LANG_ZIP.carplay-new"; echo "  WARN UpMix texts not added - the row keeps the jar's texts"; }
}

echo "custom.sh: CarPlay $ACTION"
echo "Remounting app and system read-write..."
mount -uw /mnt/app    2>/dev/null || true
mount -uw /mnt/system 2>/dev/null || true

LIST=/tmp/carplay_files.$$
list_payload "$ROOT" "$LIST" || exit 1

case $ACTION in
install)
    # Everything below depends on the firmware, so no E2P train = no install.
    read_train
    if [ -z "$TRAIN" ]; then
        echo "FAILED: could not read the firmware version from E2P - nothing was changed"
        rm -f "$LIST"; exit 1
    fi
    brand=$(train_field 3); brand=${brand%"${brand#??}"}
    echo "  Firmware: $TRAIN (platform $(train_field 1), region $(train_field 2), brand $brand)"
    if [ "$(train_field 1)" != MHI2Q ] || [ "$brand" != AU ]; then
        echo "FAILED: this patch is for Audi MHI2Q units only - nothing was changed"
        rm -f "$LIST"; exit 1
    fi
    JAVA_JAR=$(java_jar)
    if [ -n "$JAVA_JAR" ]; then echo "  Java: $JAVA_JAR"; else
        echo "  WARN region $(train_field 2) is not verified for the Java part:"
        echo "       no Java patch is installed - it could keep the HMI from booting."
        echo "       CarPlay runs without the cluster/HUD features. Report your firmware version."
    fi
    cd "$ROOT" || { rm -f "$LIST"; exit 1; }
    while IFS= read -r f; do
        dest=/${f#./}; dir=${dest%/*}
        # macOS junk from a Mac-written card: AppleDouble "._name" and .DS_Store.
        # Never install it, and drop copies an older install put on the unit
        # (a "._carplay_hook.jar" in lsd/jars would land on j9's bootclasspath).
        case $f in */._*|*/.DS_Store) rm -f "$dest"; continue ;; esac
        case $dest in */lsd/jars/carplay_hook.jar|*/lsd/jars/carplay_hook_cn.jar)
            # lsd.sh loads every jar in that folder: exactly one variant may stay, and the
            # other (or any jar on an unverified firmware) keeps the HMI from booting.
            [ "${dest##*/}" = "$JAVA_JAR" ] || { rm -f "$dest"; echo "  skipped $dest (removed any copy)"; continue; } ;;
        esac
        mkdir -p "$dir" || { echo "FAILED mkdir $dir"; rm -f "$LIST"; exit 1; }
        if is_replaced "$dest"; then
            want=$(p5087_size "$dest"); have=$(file_size "$dest")
            if [ -n "$want" ] && [ "$have" != "$want" ]; then
                echo "  WARN skipped $dest: not the P5087 build (size '$have') - stock kept"; continue
            fi
            backup_once "$dest" "$dest.carplay-stock" || { rm -f "$LIST"; exit 1; }
        fi
        tmp=$dest.carplay-new.$$
        cp -p "$f" "$tmp" || { echo "FAILED copy $tmp"; rm -f "$tmp" "$LIST"; exit 1; }
        chmod $(mode_for "$dest") "$tmp" || { echo "FAILED chmod $tmp"; rm -f "$tmp" "$LIST"; exit 1; }
        mv -f "$tmp" "$dest" || { echo "FAILED activate $dest"; rm -f "$tmp" "$LIST"; exit 1; }
        echo "  $dest"
    done < "$LIST"
    rm -f "$LIST"
    # M.I.B.'s "NavActiveIgnore" (navignore_audi/_vw.jar, installed as NavActiveIgnore.jar)
    # replaces org.dsi.ifc.carplay.AppState so getAppStateID()/getOwner() always return 0:
    # the HMI no longer sees which resources CarPlay owns, which our lifecycle relies on.
    # Not ours and not stock: removed, no backup.
    for j in NavActiveIgnore.jar navignore_audi.jar navignore_vw.jar; do
        j=/mnt/app/eso/hmi/lsd/jars/$j
        [ -e "$j" ] || continue
        if rm -f "$j"; then echo "  removed $j (M.I.B. NavActiveIgnore conflicts with CarPlay app state)"
        else echo "  WARN could not remove $j"; fi
    done
    patch_json
    patch_ncm
    patch_pf
    patch_dio
    patch_lang
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
    for f in $NCM_FILES; do
        [ -e "$f.carplay-stock" ] && mv -f "$f.carplay-stock" "$f"
    done
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
