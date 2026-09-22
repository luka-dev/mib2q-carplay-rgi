#!/bin/bash
# Lay a protected build out as an on-unit rootfs ("/") for install via M.I.B.
# Takes the STAMPED binaries build_hook_protected.sh produced in
# build/protected_<VIN>/ and folds in the runtime assets from their canonical
# homes (models/, deploy/, maneuver_render/resources/). Renderers and the JAR
# are separate build outputs (build_renderers.sh, build_java.sh -> build/); if
# present they are included, otherwise the run notes them.
#
#   ./scripts/package_protected.sh WAUZZZF45HA149423
#
# Output: install_MoreIncredibleBash/mod/carplay/root/  (+ PROTECTED.txt,
# carplay_child.json). Install on the unit with mod/custom.sh; active only on
# the stamped VIN.
set -e

VIN="${1:?usage: $0 <17-char-VIN>}"
[ ${#VIN} -eq 17 ] || { echo "error: VIN must be 17 chars, got ${#VIN}" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILDD="$PROJECT_DIR/build/protected_$VIN"
[ -f "$BUILDD/libcarplay_hook.so" ] || {
  echo "error: no stamped hook at $BUILDD/libcarplay_hook.so" >&2
  echo "       run ./scripts/build_hook_protected.sh $VIN first" >&2
  exit 1
}

MOD="$PROJECT_DIR/install_MoreIncredibleBash/mod/carplay"
ROOTFS="$MOD/root"
echo "=== Assembling protected rootfs in mod/carplay/root/ (VIN $VIN) ==="
rm -rf "$ROOTFS"
HOOKS="$ROOTFS/mnt/app/root/hooks"
JARS="$ROOTFS/mnt/app/eso/hmi/lsd/jars"
APPS="$ROOTFS/mnt/app/eso/bin/apps"
DLL="$ROOTFS/mnt/app/armle/lib/dll"
mkdir -p "$HOOKS" "$JARS" "$APPS" "$DLL"

miss=""
copy() {  # copy() <dest-dir> <src> ; records a miss if src is absent
  if [ -f "$2" ]; then cp -p "$2" "$1/"; else miss="$miss $(basename "$2")"; fi
}

# /mnt/app/root/hooks/ - the stamped hook payload plus runtime assets from
# their canonical homes (models/ is where trained weights now live).
copy "$HOOKS" "$BUILDD/libcarplay_hook.so"
copy "$HOOKS" "$BUILDD/libcarplay_apac.so"   # absent for a HOOK_AUDIO=0 build
copy "$HOOKS" "$PROJECT_DIR/models/hssr-q6d-moises-a/hssr_model.bin"
copy "$HOOKS" "$PROJECT_DIR/models/hssr-selection-v11/hssr_guard.bin"
copy "$HOOKS" "$PROJECT_DIR/maneuver_render/resources/flag_atlas.rgba"
for s in carplay_startup.sh carplay_monitor.sh carplay_processes.sh carplay_cleanup.sh; do
  copy "$HOOKS" "$PROJECT_DIR/deploy/smartphone_integrator/$s"
done
# renderers are separate build outputs (build_renderers.sh -> build/)
copy "$HOOKS" "$PROJECT_DIR/build/maneuver_render"
copy "$HOOKS" "$PROJECT_DIR/build/altscreen_render"

# /mnt/app/eso/hmi/lsd/jars/ - the Java patch (build_java.sh -> build/)
copy "$JARS" "$PROJECT_DIR/build/carplay_hook.jar"

# patched firmware at their real paths (whole-file replacements, canonical homes)
copy "$APPS" "$PROJECT_DIR/deploy/usb_replug_recovery/bundle_mu1316_p5087/payload/smartphone_integrator"
copy "$DLL"  "$PROJECT_DIR/deploy/deva-ctrl-qc/deva-ctrl-qc-mirror.so"
# the DLL keeps its on-unit name
[ -f "$DLL/deva-ctrl-qc-mirror.so" ] && mv "$DLL/deva-ctrl-qc-mirror.so" "$DLL/deva-ctrl-qc.so"
rmdir "$APPS" "$DLL" 2>/dev/null || true

# installer resource (not a "/" file): the SI json child fragment custom.sh
# splices into smartphone_integrator.json at install time.
copy "$MOD" "$PROJECT_DIR/deploy/smartphone_integrator/carplay_child.json"
[ -n "$miss" ] && echo "  note: source not found (build separately):$miss"

find "$ROOTFS" -name .DS_Store -delete 2>/dev/null || true   # never ship macOS cruft
# PROTECTED.txt ships in the folder - keep it neutral, never describe the
# protection mechanism (which files are stamped / how the gate works).
{ echo "VIN=$VIN"; echo "built=$(date -u +%Y-%m-%dT%H:%M:%SZ)"; \
  echo "install=run mod/custom.sh"; } > "$MOD/PROTECTED.txt"
echo "Protected rootfs ready under mod/carplay/root/ :"
( cd "$ROOTFS" && find . -type f ! -name .DS_Store | sed 's|^\./|  /|' )
echo "Install on the unit: mod/custom.sh install   (ACTIVE only on $VIN)"
