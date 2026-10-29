#!/bin/bash
# deploy/binpatch/binpatch.py on the real stock smartphone_integrator of three trains:
# exactly the 4-byte re-arm on P5087, P5092 and K1004 (P5087 equals the shipped patched SI),
# "already patched" on a patched file and no output for a file without the site.  Runs under
# MicroPython when installed (as on the unit) and CPython.  Skips without the unpacked trees.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
U=${CARPLAY_UNPACKED_ROOT:-$ROOT/../../Tools/mhi2q-qemu/out/unpacked}
SI=qnx6/MMX2QC/app/10/default/app/eso/bin/apps/smartphone_integrator
AIRPLAY=qnx6/MMX2QC/app/10/default/app/eso/lib/libairplay.so
MCD=qnx6/MMX2QC/app/10/default/app/armle/usr/sbin/mcd
DEVA=qnx6/MMX2QC/app/10/default/app/armle/lib/dll/deva-ctrl-qc.so
P=$ROOT/deploy/binpatch/binpatch.py

# mcd.conf priority lever (mcd/README.md): shipped custom == stock with active Priority
# 11,10 -> 11,3, device list (incl. cd) untouched, idempotent.  No firmware needed.
C=$(mktemp)
sed 's/^\(Priority[[:space:]]*=[[:space:]]*11\),10$/\1,3/' "$ROOT/mcd/mcd.conf.stock" > "$C"
cmp -s "$C" "$ROOT/mcd/mcd.conf.custom" || { echo "FAIL: mcd.conf.custom != sed(stock)"; rm -f "$C"; exit 1; }
[ "$(grep -c '^Priority[[:space:]]*=[[:space:]]*11,10$' "$C")" = 0 ] || { echo "FAIL: active 11,10 remains"; rm -f "$C"; exit 1; }
grep -q '^\[/dev/cd\*\]' "$ROOT/mcd/mcd.conf.custom" || { echo "FAIL: cd section dropped"; rm -f "$C"; exit 1; }
sed 's/^\(Priority[[:space:]]*=[[:space:]]*11\),10$/\1,3/' "$C" | cmp -s - "$C" || { echo "FAIL: conf sed not idempotent"; rm -f "$C"; exit 1; }
rm -f "$C"
echo "binpatch: mcd.conf priority lever == shipped, cd kept, idempotent PASS"
[ -f "$U/MHI2Q_US_AUG22_P5087_MU1316/$SI" ] || { echo "SKIP (no unpacked firmware)"; exit 0; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pys=python3; command -v micropython >/dev/null && pys="micropython python3"
for py in $pys; do
    for fw in MHI2Q_US_AUG22_P5087_MU1316 MHI2Q_ER_AUG22_P5092_MU1329 MHI2Q_CN_AUG22_K1004_MU1003_4M0906961EG; do
        "$py" "$P" smartphone_integrator "$U/$fw/$SI" "$T/out" > /dev/null
        [ "$(cmp -l "$U/$fw/$SI" "$T/out" | wc -l | tr -d ' ')" = 4 ] || { echo "FAIL ($py $fw): not exactly 4 bytes"; exit 1; }
        "$py" "$P" mcd "$U/$fw/$MCD" "$T/mout" > /dev/null
        [ "$(cmp -l "$U/$fw/$MCD" "$T/mout" | wc -l | tr -d ' ')" = 5 ] || { echo "FAIL ($py $fw): mcd not exactly 5 bytes"; exit 1; }
        "$py" "$P" deva "$U/$fw/$DEVA" "$T/dout" > /dev/null
        [ "$(cmp -l "$U/$fw/$DEVA" "$T/dout" | wc -l | tr -d ' ')" = 140 ] || { echo "FAIL ($py $fw): deva not 140 differing bytes"; exit 1; }
    done
    for fw in MHI2Q_US_AUG22_P5087_MU1316 MHI2Q_ER_AUG22_P5092_MU1329; do
        rm -f "$T/decoder" "$T/decoder-again" "$T/decoder-refused"
        "$py" "$P" airplay_decoder "$U/$fw/$AIRPLAY" "$T/decoder" > /dev/null
        python3 - "$U/$fw/$AIRPLAY" "$T/decoder" "$T/partial" "$T/corrupt" "$T/duplicate" <<'PY'
import sys
from pathlib import Path
stock, new = (Path(p).read_bytes() for p in sys.argv[1:3])
assert len(new) == len(stock)
assert [i for i, (a,b) in enumerate(zip(stock,new)) if a != b] == [0x8c694, 0x8c6f4]
assert all(stock[i] == 4 and new[i] == 2 for i in (0x8c694, 0x8c6f4))
partial = bytearray(stock); partial[0x8c694] = 2
Path(sys.argv[3]).write_bytes(partial)
corrupt = bytearray(new); corrupt[0x8c6f5] ^= 1
Path(sys.argv[4]).write_bytes(corrupt)
Path(sys.argv[5]).write_bytes(stock + stock[0x8c684:0x8c6b0])
PY
        rc=0; "$py" "$P" airplay_decoder "$T/decoder" "$T/decoder-again" > /dev/null || rc=$?
        [ "$rc" = 3 ] && [ ! -e "$T/decoder-again" ] || { echo "FAIL ($py): decoder idempotence"; exit 1; }
        for bad in "$T/partial" "$T/corrupt" "$T/duplicate" "$U/MHI2Q_CN_AUG22_K1004_MU1003_4M0906961EG/$AIRPLAY"; do
            rc=0; "$py" "$P" airplay_decoder "$bad" "$T/decoder-refused" > /dev/null || rc=$?
            [ "$rc" = 1 ] && [ ! -e "$T/decoder-refused" ] || { echo "FAIL ($py): decoder unsafe input accepted: $bad"; exit 1; }
        done
    done
    echo "binpatch: decoder two bytes only US/EU; already patched, partial, corrupt, duplicate, CN refusal ($py) PASS"
    "$py" "$P" smartphone_integrator "$U/MHI2Q_US_AUG22_P5087_MU1316/$SI" "$T/us" > /dev/null
    cmp -s "$T/us" "$ROOT/deploy/smartphone_integrator/smartphone_integrator" || { echo "FAIL ($py): P5087 differs from the shipped SI"; exit 1; }
    "$py" "$P" mcd "$U/MHI2Q_US_AUG22_P5087_MU1316/$MCD" "$T/mus" > /dev/null
    cmp -s "$T/mus" "$ROOT/mcd/mcd.custom" || { echo "FAIL ($py): P5087 mcd differs from the shipped mcd.custom"; exit 1; }
    "$py" "$P" deva "$U/MHI2Q_US_AUG22_P5087_MU1316/$DEVA" "$T/dus" > /dev/null
    cmp -s "$T/dus" "$ROOT/deploy/deva-ctrl-qc/deva-ctrl-qc.so.patched" || { echo "FAIL ($py): P5087 deva differs from the shipped deva-ctrl-qc.so.patched"; exit 1; }
    rc=0; "$py" "$P" deva "$T/dus" "$T/dagain" > /dev/null || rc=$?
    [ "$rc" = 3 ] && [ ! -e "$T/dagain" ] || { echo "FAIL ($py): patched deva not reported as already patched"; exit 1; }
    rc=0; "$py" "$P" mcd "$T/mus" "$T/magain" > /dev/null || rc=$?
    [ "$rc" = 3 ] && [ ! -e "$T/magain" ] || { echo "FAIL ($py): patched mcd not reported as already patched"; exit 1; }
    rc=0; "$py" "$P" smartphone_integrator "$T/us" "$T/again" > /dev/null || rc=$?
    [ "$rc" = 3 ] && [ ! -e "$T/again" ] || { echo "FAIL ($py): patched file not reported as already patched"; exit 1; }
    rc=0; "$py" "$P" smartphone_integrator "$ROOT/deploy/deva-ctrl-qc/deva-ctrl-qc.so.stock" "$T/no" > /dev/null || rc=$?
    [ "$rc" = 1 ] && [ ! -e "$T/no" ] || { echo "FAIL ($py): a file without the site was written"; exit 1; }
done
echo "binpatch: SI re-arm (4B) + mcd coalesce (5B) on P5087/P5092/K1004, P5087 == shipped, idempotent, foreign file untouched ($pys) PASS"
