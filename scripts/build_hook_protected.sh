#!/bin/bash
# Build a VIN-locked (protected) CarPlay hook. Produces stamped BINARIES only,
# in build/protected_<VIN>/. The install tree is staged by hand under
# install_MoreIncredibleBash/mod/carplay/root/ (see docs-obsidian/Sources/RELEASE_ARTIFACTS.md).
#
# The hook and APAC codec are bound to one car: on any other VIN the hook stays
# inert (stock CarPlay) and the codec fails closed. Protection is a compile-time
# gate (#ifdef CARPLAY_PROT) plus a post-build VIN stamp, so this is a real
# build, not a re-wrap - it shares the exact compile+verify with build_hook.sh
# via scripts/build_hook_common.sh. The stamp is part of producing the binary:
# unstamped, a CARPLAY_PROT .so is inert on every car.
#
#   ./scripts/build_hook_protected.sh WAUZZZF45HA149423
#   HOOK_AUDIO=0 LOG=0 ./scripts/build_hook_protected.sh <VIN>   # beta: no audio, no logs
#   PROT_SECRET=<hex> ./scripts/build_hook_protected.sh <VIN>    # pin a stable fleet secret
#
# PROT_SECRET is optional: unset -> a fresh random per-build secret, used for
# both the baked-in runtime value and the stamp, then thrown away (a one-off
# build is self-consistent). Set it, kept OUT of the tree, only to pin one fleet
# secret across rebuilds.
set -e

VIN="${1:-${PROT_VIN:-}}"
[ -n "$VIN" ] || { echo "usage: $0 <17-char-VIN>   (or PROT_VIN=... )" >&2; exit 2; }
[ ${#VIN} -eq 17 ] || { echo "error: VIN must be 17 chars, got ${#VIN}" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/build_hook_common.sh"

# Secret: reuse an explicit one, else generate a random per-build secret. The
# same value must reach both the compiled runtime and the stamp, so thread it
# through the environment into carplay_build_hook and protect.py below.
if [ -z "${PROT_SECRET:-}" ]; then
  PROT_SECRET="$(head -c 32 /dev/urandom | od -An -tx1 | tr -d '[:space:]')"
  echo "PROT_SECRET not set: generated a random per-build secret (not stored)"
fi
export PROT_SECRET

OUTDIR_REL="build/protected_$VIN"
echo "Protected build: VIN-locked to $VIN"

PROT_BUILD=1 OUTDIR_REL="$OUTDIR_REL" carplay_build_hook

BUILDD="$PROJECT_DIR/$OUTDIR_REL"

# Stamp the compiled .so in place - the stamp is what binds them to this VIN.
echo "=== Stamping the compiled .so for VIN $VIN ==="
printf '\x5a' > /tmp/prot_consts.bin
for so in "$BUILDD/libcarplay_hook.so" "$BUILDD/libcarplay_apac.so"; do
  [ -f "$so" ] && python3 "$PROJECT_DIR/protector/protect.py" "$so" \
      --vin "$VIN" --mode blind --payload-file /tmp/prot_consts.bin
done
rm -f /tmp/prot_consts.bin

echo ""
echo "Stamped protected binaries: $OUTDIR_REL/"
echo "Stage them by hand under mod/carplay/root/ (see docs-obsidian/Sources/RELEASE_ARTIFACTS.md)."
