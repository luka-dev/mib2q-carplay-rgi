#!/bin/bash
# Build a VIN-locked (protected) CarPlay hook: a thin entrypoint over
# build_hook.sh so the protected concern has its own script without forking the
# build (one source of truth stays in build_hook.sh; a copied build would drift).
#
# The hook and APAC codec are bound to one car: on any other VIN the hook stays
# inert (stock CarPlay) and the codec fails closed. Hand the result to a beta
# tester and copying it to another unit does nothing.
#
#   ./scripts/build_hook_protected.sh WAUZZZF45HA149423
#   HOOK_AUDIO=0 LOG=0 ./scripts/build_hook_protected.sh WAUZZZF45HA149423  # beta: no audio, no logs
#   PROT_SECRET=<hex> ./scripts/build_hook_protected.sh <VIN>               # stable fleet secret
#
# PROT_SECRET is optional: when unset build_hook.sh generates a fresh random
# per-build secret and throws it away (fine for a single build; the runtime
# value and the stamp are threaded from the same value). Set it only to pin one
# secret across rebuilds.
set -euo pipefail

VIN="${1:-${PROT_VIN:-}}"
[ -n "$VIN" ] || { echo "usage: $0 <17-char-VIN>   (or PROT_VIN=... )" >&2; exit 2; }
[ ${#VIN} -eq 17 ] || { echo "error: VIN must be 17 chars, got ${#VIN}" >&2; exit 1; }

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec env PROT_VIN="$VIN" "$HERE/build_hook.sh"
