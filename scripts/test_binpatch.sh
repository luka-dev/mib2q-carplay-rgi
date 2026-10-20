#!/bin/bash
# deploy/binpatch/binpatch.py on the real stock smartphone_integrator of three trains:
# exactly the 4-byte re-arm on P5087, P5092 and K1004 (P5087 equals the shipped patched SI),
# "already patched" on a patched file and no output for a file without the site.  Runs under
# MicroPython when installed (as on the unit) and CPython.  Skips without the unpacked trees.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
U=$ROOT/../../Tools/mhi2q-qemu/out/unpacked
SI=qnx6/MMX2QC/app/10/default/app/eso/bin/apps/smartphone_integrator
WIFI=qnx6/MMX2QC/app/10/default/app/armle/lib/dll/devnp-mrvl_wlan-sdiorm.so
P=$ROOT/deploy/binpatch/binpatch.py
[ -f "$U/MHI2Q_US_AUG22_P5087_MU1316/$SI" ] || { echo "SKIP (no unpacked firmware)"; exit 0; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pys=python3; command -v micropython >/dev/null && pys="micropython python3"
for py in $pys; do
    for fw in MHI2Q_US_AUG22_P5087_MU1316 MHI2Q_ER_AUG22_P5092_MU1329 MHI2Q_CN_AUG22_K1004_MU1003_4M0906961EG; do
        "$py" "$P" smartphone_integrator "$U/$fw/$SI" "$T/out" > /dev/null
        [ "$(cmp -l "$U/$fw/$SI" "$T/out" | wc -l | tr -d ' ')" = 4 ] || { echo "FAIL ($py $fw): not exactly 4 bytes"; exit 1; }
        "$py" "$P" wifi "$U/$fw/$WIFI" "$T/wout" > /dev/null
        [ "$(cmp -l "$U/$fw/$WIFI" "$T/wout" | wc -l | tr -d ' ')" = 40 ] || { echo "FAIL ($py $fw): wifi not exactly 40 bytes"; exit 1; }
        rc=0; "$py" "$P" wifi "$T/wout" "$T/wagain" > /dev/null || rc=$?
        [ "$rc" = 3 ] && [ ! -e "$T/wagain" ] || { echo "FAIL ($py $fw): patched wifi not idempotent"; exit 1; }
    done
    "$py" "$P" smartphone_integrator "$U/MHI2Q_US_AUG22_P5087_MU1316/$SI" "$T/us" > /dev/null
    cmp -s "$T/us" "$ROOT/deploy/smartphone_integrator/smartphone_integrator" || { echo "FAIL ($py): P5087 differs from the shipped SI"; exit 1; }
    "$py" "$P" wifi "$U/MHI2Q_US_AUG22_P5087_MU1316/$WIFI" "$T/wus" > /dev/null
    cmp -s "$T/wus" "$ROOT/wifi/devnp-mrvl_wlan-sdiorm.so.patched" || { echo "FAIL ($py): P5087 wifi differs from the shipped patched driver"; exit 1; }
    rc=0; "$py" "$P" smartphone_integrator "$T/us" "$T/again" > /dev/null || rc=$?
    [ "$rc" = 3 ] && [ ! -e "$T/again" ] || { echo "FAIL ($py): patched file not reported as already patched"; exit 1; }
    rc=0; "$py" "$P" smartphone_integrator "$ROOT/deploy/deva-ctrl-qc/deva-ctrl-qc.so.stock" "$T/no" > /dev/null || rc=$?
    [ "$rc" = 1 ] && [ ! -e "$T/no" ] || { echo "FAIL ($py): a file without the site was written"; exit 1; }
done
echo "binpatch: SI re-arm (4B) + wifi RX-reorder (40B) on P5087/P5092/K1004, P5087 == shipped, idempotent, foreign file untouched ($pys) PASS"
