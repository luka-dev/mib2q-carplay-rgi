#!/bin/sh
# patch_ncm() + patch_otg_cleanup() from the M.I.B. installer, and the stopncm.sh path
# for SI's cleanup (NCM_EXPECT_GENERATION).  Installer: replaces the NCM scripts only over
# the exact stock and patches the Lua's two command lines, keeps a stock backup, is a no-op
# on re-run, leaves unknown scripts alone (the Lua is independent);
# repoints otgStackCleanupScript once.  stopncm: destroys only the expected generation.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/install_MoreIncredibleBash/mod/custom.sh
NCM=$ROOT/deploy/experimental/usb_replug_recovery/bundle_mu1316_p5087
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
fail() { echo "FAIL ($1): $2"; exit 1; }

{ echo 'set -u'
  sed -n '/^mode_for()/,/^}/p' "$SRC"
  sed -n "/^NCM_MARK=/p;/^same_text()/p" "$SRC"
  sed -n '/^ncm_known()/,/^}/p;/^ncm_lua()/,/^}/p' "$SRC"
  sed -n '/^patch_ncm()/,/^}/p;/^OTG_CLEANUP=/p;/^patch_otg_cleanup()/,/^}/p' "$SRC"
  echo 'RES=$1; CFG=$2; NCM_SCRIPTS="$3/startncm.sh $3/stopncm.sh"; NCM_LUA=$3/usblauncher_otg.lua; patch_ncm; patch_otg_cleanup'
} > "$T/h.sh"
mkdir -p "$T/res/ncm/stock" "$T/res/ncm/payload"
for f in startncm.sh stopncm.sh usblauncher_otg.lua; do
    cp "$NCM/stock/$f" "$T/res/ncm/stock/"; cp "$NCM/payload/$f" "$T/res/ncm/payload/"
done
printf '    "usb":{\n        "otgStackCleanupScript":"/etc/scripts/stopncm.sh",\n    },\n' > "$T/si.stock"

# every Lua the installer writes must still parse (when a luac is at hand)
LUAC=$(command -v luac 2>/dev/null || true)
lua_ok() { [ -z "$LUAC" ] || "$LUAC" -p "$1" || fail "$2" "patched Lua does not parse"; }
shells=
for s in /bin/ksh /bin/mksh "$(command -v mksh 2>/dev/null)" /bin/dash /bin/sh; do [ -x "$s" ] && shells="$shells $s"; done
for sh in $shells; do
    rm -rf "$T/u"; mkdir "$T/u"
    for f in startncm.sh stopncm.sh usblauncher_otg.lua; do cp "$NCM/stock/$f" "$T/u/"; done
    cp "$T/si.stock" "$T/si"
    "$sh" "$T/h.sh" "$T/res" "$T/si" "$T/u" > /dev/null || fail "$sh" "install returned non-zero"
    for f in startncm.sh stopncm.sh usblauncher_otg.lua; do
        cmp -s "$T/u/$f" "$NCM/payload/$f" || fail "$sh" "$f not installed"
        cmp -s "$T/u/$f.carplay-stock" "$NCM/stock/$f" || fail "$sh" "$f backup is not stock"
    done
    [ -x "$T/u/startncm.sh.carplay-stock" ] && [ -x "$T/u/stopncm.sh.carplay-stock" ] || fail "$sh" "stock script backup not executable"
    lua_ok "$T/u/usblauncher_otg.lua" "$sh"
    grep -q '"otgStackCleanupScript":"/mnt/app/root/hooks/carplay_ncm_cleanup.sh",' "$T/si" || fail "$sh" "otg cleanup not repointed"
    out=$("$sh" "$T/h.sh" "$T/res" "$T/si" "$T/u") || fail "$sh" "re-run returned non-zero"
    case $out in *'already fenced'*'already ours'*) ;; *) fail "$sh" "re-run not a no-op: $out" ;; esac

    # an older copy of our own stopncm.sh (marker line) is updated, backup still stock
    rm -rf "$T/u"; mkdir "$T/u"
    cp "$NCM/stock/startncm.sh" "$NCM/stock/usblauncher_otg.lua" "$T/u/"
    { cat "$NCM/payload/stopncm.sh"; echo '# older revision'; } > "$T/u/stopncm.sh"
    cp "$T/si.stock" "$T/si"
    "$sh" "$T/h.sh" "$T/res" "$T/si" "$T/u" > /dev/null || fail "$sh" "update returned non-zero"
    cmp -s "$T/u/stopncm.sh" "$NCM/payload/stopncm.sh" || fail "$sh" "older copy of ours not updated"
    cmp -s "$T/u/stopncm.sh.carplay-stock" "$NCM/stock/stopncm.sh" || fail "$sh" "update backup is not stock"

    # an older Lua fence (start line only): brought to the payload, parses, backup is the stock
    rm -rf "$T/u"; mkdir "$T/u"
    for f in startncm.sh stopncm.sh; do cp "$NCM/stock/$f" "$T/u/"; done
    sed 's#iopkt_instance)$#iopkt_instance .. " $(busno) $(devno) $(iface)")#' "$NCM/stock/usblauncher_otg.lua" > "$T/u/usblauncher_otg.lua"
    "$sh" "$T/h.sh" "$T/res" "$T/si" "$T/u" > /dev/null || fail "$sh" "older Lua returned non-zero"
    cmp -s "$T/u/usblauncher_otg.lua" "$NCM/payload/usblauncher_otg.lua" || fail "$sh" "older Lua fence not brought to the payload"
    cmp -s "$T/u/usblauncher_otg.lua.carplay-stock" "$NCM/stock/usblauncher_otg.lua" || fail "$sh" "older Lua backup is not stock"
    lua_ok "$T/u/usblauncher_otg.lua" "$sh"

    # the stop command inside a one-line if: not a whole statement line -> the Lua is left alone
    rm -rf "$T/u"; mkdir "$T/u"
    for f in startncm.sh stopncm.sh; do cp "$NCM/stock/$f" "$T/u/"; done
    sed 's#^\( *\)return "/etc/scripts/stopncm.sh"$#\1if true then return "/etc/scripts/stopncm.sh" end#' "$NCM/stock/usblauncher_otg.lua" > "$T/u/usblauncher_otg.lua"
    cp "$T/u/usblauncher_otg.lua" "$T/lua0"; lua_ok "$T/lua0" harness
    out=$("$sh" "$T/h.sh" "$T/res" "$T/si" "$T/u") || fail "$sh" "inline-if Lua returned non-zero"
    case $out in *'its fence skipped'*) ;; *) fail "$sh" "inline stop command edited: $out" ;; esac
    cmp -s "$T/u/usblauncher_otg.lua" "$T/lua0" && [ ! -e "$T/u/usblauncher_otg.lua.carplay-stock" ] || fail "$sh" "inline-if Lua touched"

    # damaged script on the card: nothing touched
    rm -rf "$T/u" "$T/res2"; mkdir "$T/u"; cp -R "$T/res" "$T/res2"
    for f in startncm.sh stopncm.sh usblauncher_otg.lua; do cp "$NCM/stock/$f" "$T/u/"; done
    head -c 3000 "$NCM/payload/startncm.sh" > "$T/res2/ncm/payload/startncm.sh"
    out=$("$sh" "$T/h.sh" "$T/res2" "$T/si" "$T/u" 2>/dev/null) || fail "$sh" "damaged card returned non-zero"
    case $out in *damaged*) ;; *) fail "$sh" "truncated payload not detected: $out" ;; esac
    cmp -s "$T/u/startncm.sh" "$NCM/stock/startncm.sh" || fail "$sh" "installed from a damaged card"

    # other firmware: a script differs -> no script touched or backed up; the stock Lua still fenced
    rm -rf "$T/u"; mkdir "$T/u"
    for f in startncm.sh stopncm.sh usblauncher_otg.lua; do cp "$NCM/stock/$f" "$T/u/"; done
    echo '# vendor change' >> "$T/u/stopncm.sh"
    printf '    "otgStackCleanupScript":"/etc/scripts/other.sh",\n' > "$T/si"; cp "$T/si" "$T/si0"
    out=$("$sh" "$T/h.sh" "$T/res" "$T/si" "$T/u") || fail "$sh" "skip returned non-zero"
    case $out in *'NCM scripts skipped'*) ;; *) fail "$sh" "no skip: $out" ;; esac
    cmp -s "$T/u/startncm.sh" "$NCM/stock/startncm.sh" || fail "$sh" "script touched on other firmware"
    ls "$T/u" | grep -q 'ncm.sh.carplay-stock' && fail "$sh" "script backup made on other firmware"
    cmp -s "$T/u/usblauncher_otg.lua" "$NCM/payload/usblauncher_otg.lua" || fail "$sh" "stock Lua not fenced beside unknown scripts"
    lua_ok "$T/u/usblauncher_otg.lua" "$sh"
    cmp -s "$T/si" "$T/si0" || fail "$sh" "unknown otg cleanup value modified"
done
ls -R "$T" | grep -q carplay-new && fail all "temporary file left behind"

# stopncm.sh with NCM_EXPECT_GENERATION (SI cleanup via carplay_ncm_cleanup.sh)
G=$T/g; mkdir "$G"
stop() { NCM_GENERATION_DIR=$G NCM_LOCK_BASE=$G NCM_LOG_FILE=$G/log NCM_IFCONFIG_BIN=true NCM_STOP_DELAY=${D:-0} \
         NCM_EXPECT_GENERATION=$1 sh "$NCM/payload/stopncm.sh" > /dev/null; }
echo 0:0x1:0x1:100 > "$G/ncm_generation_carplay0"
stop 0:0x1:0x1:99;  [ -f "$G/ncm_generation_carplay0" ] || fail stopncm "destroyed a newer generation"
grep -q 'skip session_mismatch' "$G/log" || fail stopncm "mismatch not logged"
stop 0:0x1:0x1:100; [ ! -f "$G/ncm_generation_carplay0" ] || fail stopncm "own generation not destroyed"
grep -q 'destroy rc=0 observed=0:0x1:0x1:100' "$G/log" || fail stopncm "destroy not logged"
echo 0:0x1:0x1:200 > "$G/ncm_generation_carplay0"
( sleep 0.3; echo 0:0x1:0x1:201 > "$G/ncm_generation_carplay0" ) &
D=1 stop 0:0x1:0x1:200; wait
grep -q 'skip replaced_after_sleep' "$G/log" || fail stopncm "replug during the delay not detected"
ls "$G" | grep -q ncm_lock && fail stopncm "lock left behind"
echo "NCM fence install (Lua line patch${LUAC:+, luac -p}) + SI cleanup generation:$shells PASS"
