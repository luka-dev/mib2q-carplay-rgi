#!/bin/sh
# Proof of the HOOK model: our hook is ACTIVE only with the right VIN; on
# wrong/absent VIN, a patch, or an unprovisioned image it is INERT and the head
# runs its stock path unchanged. The ACTIVE feature never appears without the
# right VIN. Usage: sh selftest.sh [secret]
set -e
SECRET="${1:-demo-build-secret-set-PROT_SECRET}"
CC="${CC:-cc}"
RIGHT="WAUC4CF55JA099137"
WRONG="WVWZZZ00Z00000000"

# stamped: sentinel 0x5A, gain 102, offset 7
printf '\x5a\x66\x07' > consts.bin
ACTIVE="hook=ACTIVE process=81656"   # 8*(100*102+7)
INERT="hook=inert process=800"       # stock_process = sum(8*100)

echo "== build generic app =="
$CC -O2 -Wall -Wextra -DPROT_SECRET="\"$SECRET\"" -DPROT_DEMO_ARGV -o app example_app.c prot_rt.c prot_vin.c

echo "== unprovisioned => inert (stock behavior) =="
cp app app_raw
OUT=$(./app_raw "$RIGHT"); echo "  $OUT"
[ "$OUT" = "$INERT" ] || { echo "FAIL: unprovisioned not inert"; exit 1; }

echo "== arm for $RIGHT =="
cp app app_prov
PROT_SECRET="$SECRET" python3 protect.py app_prov --vin "$RIGHT" --mode blind --payload-file consts.bin

echo "== right VIN => hook ACTIVE =="
OUT=$(./app_prov "$RIGHT"); echo "  $OUT"
[ "$OUT" = "$ACTIVE" ] || { echo "FAIL: right VIN not active"; exit 1; }

echo "== wrong VIN => hook INERT, stock path unchanged =="
OUT=$(./app_prov "$WRONG"); echo "  $OUT"
[ "$OUT" = "$INERT" ] || { echo "FAIL: wrong VIN not inert"; exit 1; }

echo "== absent VIN (bench/bus down) => INERT, head runs stock =="
OUT=$(./app_prov ""); echo "  $OUT"
[ "$OUT" = "$INERT" ] || { echo "FAIL: absent VIN not inert"; exit 1; }

echo "== patched image => INERT, hook cannot be forced ACTIVE =="
python3 - <<'PY'
b=bytearray(open("app_prov","rb").read())
i=b.find(b"PROT\x00SLT"); j=0x200 if i>0x400 else i+200
b[j]^=0x01
open("app_patched","wb").write(b)
PY
chmod +x app_patched
OUT=$(./app_patched "$RIGHT"); echo "  $OUT"
[ "$OUT" = "$INERT" ] || { echo "FAIL: patched image not inert"; exit 1; }

echo "== ACTIVE is unreachable without the right VIN =="
for t in app_raw app_patched; do
  if ./$t "$WRONG" 2>/dev/null | grep -q "ACTIVE"; then
    echo "FAIL: $t produced ACTIVE"; exit 1; fi
done
echo "  ok: no failure path went ACTIVE"

echo "== no plaintext constants leak =="
python3 - <<'PY'
import sys
if open("app_prov","rb").read().find(b"\x5a\x66\x07")!=-1:
    print("FAIL: raw constants visible"); sys.exit(1)
print("  ok: constants not present in cleartext")
PY

echo "== LIVE PATH: production auto-arm reading the real MMX persistence store =="
STORE=../advanced/MU1316-special/mmx/mnt/persist/var/fw/persistence.sqlite
if [ -f "$STORE" ]; then
  MMXVIN=$(grep -ao 'vin","d":"[A-HJ-NPR-Z0-9]\{17\}"' "$STORE" | head -1 | grep -o '[A-HJ-NPR-Z0-9]\{17\}')
  $CC -O2 -DPROT_SECRET="\"$SECRET\"" -DPROT_APP_SELF='"./app_live"' \
      -DPROT_MMX_STORE="\"$STORE\"" \
      -o app_live example_app.c prot_rt.c prot_vin.c
  PROT_SECRET="$SECRET" python3 protect.py app_live --vin "$MMXVIN" --mode blind --payload-file consts.bin >/dev/null
  OUT=$(./app_live); echo "  baked $MMXVIN, MMX store $MMXVIN -> $OUT"
  [ "$OUT" = "$ACTIVE" ] || { echo "FAIL: live path did not activate on matching VIN"; exit 1; }
  # mismatch: fresh build stamped for a DIFFERENT VIN, must stay inert here
  $CC -O2 -DPROT_SECRET="\"$SECRET\"" -DPROT_APP_SELF='"./app_live2"' \
      -DPROT_MMX_STORE="\"$STORE\"" \
      -o app_live2 example_app.c prot_rt.c prot_vin.c
  PROT_SECRET="$SECRET" python3 protect.py app_live2 --vin 1VWSA7A38LC000001 --mode blind --payload-file consts.bin >/dev/null
  OUT=$(./app_live2); echo "  baked 1VW..., MMX store $MMXVIN -> $OUT"
  [ "$OUT" = "$INERT" ] || { echo "FAIL: live path activated on wrong VIN"; exit 1; }
  echo "  ok: live MMX path activates only on the matching VIN"
else
  echo "  (skipped: MMX persistence store not present)"
fi

echo
echo "ALL PASS"
