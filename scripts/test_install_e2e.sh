#!/bin/bash
# The whole M.I.B. installer (the staged mod/custom.sh) on a fake unit built from the real
# MU1316 P5087 system files, with /mnt and /tmp redirected into a temp dir and the
# E2P train fed through E2P_READ.  Checks: the firmware gate (only Audi MHI2Q proceeds,
# nothing is touched otherwise, also when E2P cannot be read), exactly one Java variant
# stays per E2P region (the other is removed), and the 6-channel audio path stays on only
# when the amplifier coding (E2P 0xFC, fed through E2P_AMP_READ) says external.  Needs the staged install tree
# (scripts/prepare_install.sh) and the unpacked firmware; skips without them.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
MOD=${MOD:-$ROOT/build/install_unprotected/install_MoreIncredibleBash/mod}
FW=$ROOT/../../Tools/mhi2q-qemu/out/unpacked/MHI2Q_US_AUG22_P5087_MU1316/qnx6/MMX2QC/efs-sys/10/default/system
SH=$(command -v mksh || command -v ksh)
[ -f "$MOD/custom.sh" ] && [ -d "$FW" ] || { echo "SKIP (no staged install tree or unpacked P5087)"; exit 0; }
T=$(mktemp -d); trap '[ -n "${KEEP:-}" ] && echo "kept $T" || rm -rf "$T"' EXIT
JARS=mnt/app/eso/hmi/lsd/jars
fail() { echo "FAIL ($1): $2"; sed -n '1,60p' "$T/log"; exit 1; }

# E2P dumps as modifyE2P prints them (19 bytes, as M.I.B. reads 0x3A0)
e2p() { printf '0x000003a0 '; printf '%s' "${1:0:19}" | od -An -tx1 | tr -s ' \n' ' '; echo; }

unit() {   # unit [SI size] [deva size]: a fresh fake unit, BOTH jars left over from an older install
    U=$T/u; rm -rf "$U" "$T/mod"; mkdir -p "$U/$JARS" "$U/mnt/app/eso/bin/apps" "$U/mnt/app/armle/lib/dll" \
        "$U/mnt/app/root/hooks" "$U/mnt/system/etc/scripts" "$U/mnt/system/etc/eso/production" "$U/tmp"
    cp "$FW/etc/eso/production/smartphone_integrator.json" "$FW/etc/eso/production/dio_manager.json" "$U/mnt/system/etc/eso/production/"
    cp "$FW"/etc/pf.*conf "$FW/etc/usblauncher_otg.lua" "$U/mnt/system/etc/"
    cp "$FW/etc/scripts/startncm.sh" "$FW/etc/scripts/stopncm.sh" "$U/mnt/system/etc/scripts/"
    head -c "${1:-802798}" /dev/zero > "$U/mnt/app/eso/bin/apps/smartphone_integrator"
    head -c "${2:-694904}" /dev/zero > "$U/mnt/app/armle/lib/dll/deva-ctrl-qc.so"
    echo old > "$U/$JARS/carplay_hook.jar"; echo old > "$U/$JARS/carplay_hook_cn.jar"
    cp -R "$MOD" "$T/mod"
    # the installer and the startup are sources: test the checked-in ones, binaries come from the stage
    cp "$ROOT/deploy/smartphone_integrator/carplay_startup.sh" "$T/mod/carplay/root/mnt/app/root/hooks/carplay_startup.sh"
    sed -e "s#/mnt/#$U/mnt/#g" -e "s#dest=/\${f\#./}#dest=$U/\${f\#./}#" -e "s#/tmp/#$U/tmp/#g" "$ROOT/install_MoreIncredibleBash/mod/custom.sh" > "$T/mod/custom.sh"
    grep -q "dest=$U/" "$T/mod/custom.sh" || { echo "FAIL: harness could not redirect the install root"; exit 1; }
}
run() { E2P_READ="$1" E2P_AMP_READ="${2:-echo 0x000000fc 02}" "$SH" "$T/mod/custom.sh" install > "$T/log" 2>&1; }
spatial() { grep '^export AUDIO_SPATIAL=' "$U/mnt/app/root/hooks/carplay_startup.sh"; }
has() { [ -f "$U/$JARS/$1" ] && ! grep -qx old "$U/$JARS/$1"; }

e2p MHI2Q_US_AUG22_P5087 > "$T/us"; e2p MHI2Q_CN_AUG22_K1004 > "$T/cn"
e2p MHI2Q_JP_AUG22_P1017 > "$T/jp"; e2p MHI2Q_KR_AUG22_P5000 > "$T/kr"
e2p MHI2_ER_VWG13_P4521 > "$T/vw"; e2p MHI2Q_ER_POG24_P5000 > "$T/po"

unit; run "cat $T/us" || fail us "install failed"
grep -q 'Firmware: MHI2Q_US_AUG22_P508 (platform MHI2Q, region US, brand AU)' "$T/log" || fail us "train not read"
has carplay_hook.jar && [ ! -e "$U/$JARS/carplay_hook_cn.jar" ] || fail us "want only carplay_hook.jar"
grep -q DONE "$T/log" || fail us "no DONE"

# amplifier gate: only an external amp (E2P 0xFC = 02) keeps the 6-channel path
spatial | grep -qx 'export AUDIO_SPATIAL=1' && grep -q 'Amplifier: external' "$T/log" || fail amp-ext "external amp lost AUDIO_SPATIAL=1"
unit; run "cat $T/us" "echo 0x000000fc 01" || fail amp-int "install failed"
spatial | grep -q '^export AUDIO_SPATIAL=0 ' && grep -q '^export AUDIO_SPATIAL_ATMOS=0$' "$U/mnt/app/root/hooks/carplay_startup.sh" &&
    grep -q 'Amplifier: internal' "$T/log" || fail amp-int "internal amp kept the 6-channel path"
unit; run "cat $T/us" false || fail amp-none "install failed"
spatial | grep -q '^export AUDIO_SPATIAL=0 ' && grep -q 'amplifier coding unreadable' "$T/log" || fail amp-none "unreadable coding kept the 6-channel path"

unit 798808 694860; run "cat $T/cn" || fail cn "install failed"
grep -q 'region CN, brand AU' "$T/log" || fail cn "train not read"
grep -q 'WARN skipped .*smartphone_integrator' "$T/log" && grep -q 'WARN skipped .*deva-ctrl-qc.so' "$T/log" &&
    [ "$(wc -c < "$U/mnt/app/eso/bin/apps/smartphone_integrator")" -eq 798808 ] || fail cn "CN SI/deva were replaced"
has carplay_hook_cn.jar && [ ! -e "$U/$JARS/carplay_hook.jar" ] || fail cn "want only carplay_hook_cn.jar"

unit; run "cat $T/jp" || fail jp "install failed"
has carplay_hook.jar && [ ! -e "$U/$JARS/carplay_hook_cn.jar" ] || fail jp "JP: want carplay_hook.jar"

unit; run "cat $T/kr" || fail unknown "install failed"
[ ! -e "$U/$JARS/carplay_hook.jar" ] && [ ! -e "$U/$JARS/carplay_hook_cn.jar" ] || fail unknown "a jar stayed on an unverified region"
grep -q 'no Java patch is installed' "$T/log" || fail unknown "no warning"

for brand in vw po none; do
    unit; cp -R "$U" "$T/before"
    if [ $brand = none ]; then run false && fail none "installer did not stop without E2P"
        grep -q 'could not read the firmware version' "$T/log" || fail none "no refusal message"
    else run "cat $T/$brand" && fail $brand "installer did not stop"
        grep -q 'Audi MHI2Q units only' "$T/log" || fail $brand "no refusal message"; fi
    diff -r "$T/before" "$U" > /dev/null || fail $brand "the unit was changed"
    rm -rf "$T/before"
done

echo "installer end-to-end: US/JP -> carplay_hook.jar, CN -> carplay_hook_cn.jar (CN SI/deva kept), other jar removed, other region -> none, VW/Porsche/no E2P refused untouched, 5.1 only on an external amp ($SH) PASS"
