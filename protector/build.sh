#!/bin/sh
# build.sh - one-shot protected build. The VIN is provided PRE-BUILD.
#
#   PROT_SECRET=... ./build.sh <VIN> <output-binary> [consts-file] [extra cc args]
#
# Steps: compile your app generic, then stamp it for <VIN> in place. The runtime
# reads the LIVE VIN on the unit (FEC active+backup quorum) and unblinds; it only
# runs ACTIVE when the live VIN matches the one baked here.
#
# QNX (RCC target): CC='qcc -Vgcc_ntoarmv7le' PROT_SECRET=... ./build.sh <VIN> app.armle
set -e

VIN="$1"; OUT="$2"; CONSTS="${3:-consts.bin}"; shift 3 2>/dev/null || shift $#
EXTRA="$*"
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--O2 -Wall}"

[ -n "$VIN" ] && [ -n "$OUT" ] || { echo "usage: PROT_SECRET=... $0 <VIN> <out> [consts.bin]"; exit 1; }
[ -n "$PROT_SECRET" ] || { echo "PROT_SECRET must be set (build secret, keep out of the tree)"; exit 1; }
[ ${#VIN} -eq 17 ] || { echo "VIN must be 17 chars"; exit 1; }
[ -f "$CONSTS" ] || { echo "constants file '$CONSTS' not found (bytes: [sentinel, your constants...])"; exit 1; }

SRC="${PROT_APP:-example_app.c} prot_rt.c prot_vin.c"

echo "[1/2] compile ($CC)"
$CC $CFLAGS -DPROT_SECRET="\"$PROT_SECRET\"" $EXTRA -o "$OUT" $SRC

echo "[2/2] stamp for VIN=$VIN"
python3 protect.py "$OUT" --vin "$VIN" --mode blind --payload-file "$CONSTS"

echo "done: $OUT (runs ACTIVE only when the unit's live VIN == $VIN)"
