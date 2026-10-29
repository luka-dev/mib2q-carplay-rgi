#!/bin/bash
# Assemble an M.I.B. install folder + zip from one hook build folder.
#
#   ./scripts/prepare_install.sh build/unprotected
#   SKIP_BUILD=1 ./scripts/prepare_install.sh build/unprotected   # reuse build/ jar + renderers
#
# Takes libcarplay_hook.so (+ libcarplay_apac.so if the build has audio) from the
# given folder, rebuilds the jar and renderers from the current tree, and stages
# everything at its on-unit path (docs/build-and-deploy/DEPLOY_AND_RELEASE.md, artifact map).
# No libcarplay_apac.so (HOOK_AUDIO=0) -> the upmix mask net is left out too.
# Output: build/install_<name>/install_MoreIncredibleBash/ and build/install_<name>.zip
set -e
[ "$#" -eq 1 ] || { [ "$#" -eq 3 ] && [ "$2" = --network-stage ]; } || {
  echo "usage: $0 <hook build dir> [--network-stage <verified network directory>]" >&2; exit 2;
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
IN="$(cd "$1" && pwd)"
[ -f "$IN/libcarplay_hook.so" ] || { echo "error: no libcarplay_hook.so in $IN" >&2; exit 1; }
NETWORK_STAGE=
if [ "$#" -eq 3 ]; then
  NETWORK_STAGE="$(cd "$3" && pwd)"
  python3 "$SCRIPT_DIR/network_patcher.py" verify "$NETWORK_STAGE" > /dev/null
fi

if [ -z "${SKIP_BUILD:-}" ]; then
  "$SCRIPT_DIR/build_java.sh"
  "$SCRIPT_DIR/build_renderers.sh"
fi

NAME="install_$(basename "$IN")"
OUT="$PROJECT_DIR/build/$NAME"
PKG="$OUT/install_MoreIncredibleBash"
MOD="$PKG/mod"
ROOT="$MOD/carplay/root"
HOOKS="$ROOT/mnt/app/root/hooks"
SI="$PROJECT_DIR/deploy/smartphone_integrator"

# Fail before replacing an existing output if any reviewed network resource changed.
python3 "$PROJECT_DIR/scripts/network_patcher.py" status > /dev/null
rm -rf "$OUT" "$OUT.zip"
mkdir -p "$HOOKS" "$ROOT/mnt/app/eso/hmi/lsd/jars" "$ROOT/mnt/app/eso/bin/apps" "$ROOT/mnt/app/armle/lib/dll"

cp "$PROJECT_DIR/install_MoreIncredibleBash/mod/custom.sh" "$PROJECT_DIR/install_MoreIncredibleBash/mod/command.sh" "$MOD/"
cp "$SI/carplay_child.json" "$MOD/carplay/"
cp "$PROJECT_DIR/deploy/lang/ldzip.py" "$PROJECT_DIR/deploy/lang/upmix_lang.py" "$MOD/carplay/"   # UpMix row texts (patch_lang)
cp "$PROJECT_DIR/deploy/binpatch/binpatch.py" "$MOD/carplay/"   # the unit's own SI re-arm (patch_si)

# Executable Wi-Fi runtime stays OUTSIDE root/manifest: only the verified helper may activate it.
cp "$PROJECT_DIR/deploy/binpatch/wifi_runtime.py" "$MOD/carplay/"
mkdir -p "$MOD/carplay/wifi"
cp -R "$PROJECT_DIR/deploy/wifi/runtime" "$MOD/carplay/wifi/"
python3 - "$MOD/carplay/wifi/runtime" <<'PYVERIFY'
import hashlib,json,sys
from pathlib import Path
p=Path(sys.argv[1]);m=json.loads((p/'manifest.json').read_text())
for name,key in [('driver.so','target_sha256'),('stock.so','stock_sha256')]:
    if hashlib.sha256((p/name).read_bytes()).hexdigest()!=m[key]: raise SystemExit('network resource SHA mismatch: '+name)
PYVERIFY
# Standalone uninstall has the same authenticated rollback path; unknown drivers are preserved.
cp -R "$PROJECT_DIR/uninstall_MoreIncredibleBash" "$OUT/"
UMOD="$OUT/uninstall_MoreIncredibleBash/mod"
mkdir -p "$UMOD/carplay/wifi"
cp "$PROJECT_DIR/deploy/binpatch/wifi_runtime.py" "$UMOD/carplay/"
cp -R "$PROJECT_DIR/deploy/wifi/runtime" "$UMOD/carplay/wifi/"
# the knob values recorded beside each model must be the ones the startup script ships
python3 "$PROJECT_DIR/scripts/check_model_settings.py" "$SI/carplay_startup.sh" "$PROJECT_DIR"/models/*/settings.json

cp "$IN/libcarplay_hook.so" "$HOOKS/"
if [ -f "$IN/libcarplay_apac.so" ]; then
  cp "$IN/libcarplay_apac.so" "$HOOKS/"
  # the scene renderer's fine mask net (docs/build-and-deploy/SCENE_BACKEND_DEPLOY.md), read
  # once at audio init from AUDIO_SCENE_MASK_BLOB (carplay_startup.sh)
  cp "$PROJECT_DIR/models/scene-masknet-c/scene_masknet_c.bin" "$HOOKS/"
  AUDIO=yes
else
  AUDIO=no
fi
cp "$PROJECT_DIR/build/maneuver_render" "$PROJECT_DIR/build/altscreen_render" "$HOOKS/"
cp "$PROJECT_DIR/maneuver_render/resources/flag_atlas.rgba" "$HOOKS/"
cp "$SI"/carplay_{startup,monitor,processes,cleanup,ncm_cleanup}.sh "$HOOKS/"
# NCM fence: the installer replaces the scripts only over the exact stock copy and patches the two
# command lines of the Lua in place (patch_ncm); the Lua pair is the uninstall copy and the test reference.
NCM=$PROJECT_DIR/deploy/experimental/usb_replug_recovery/bundle_mu1316_p5087
mkdir -p "$MOD/carplay/ncm/stock" "$MOD/carplay/ncm/payload"
for f in startncm.sh stopncm.sh usblauncher_otg.lua; do
  cp "$NCM/stock/$f" "$MOD/carplay/ncm/stock/"
  cp "$NCM/payload/$f" "$MOD/carplay/ncm/payload/"
done
# Both Java variants ship; custom.sh keeps the one for the E2P region and removes the other.
cp "$PROJECT_DIR/build/carplay_hook.jar" "$PROJECT_DIR/build/carplay_hook_cn.jar" "$ROOT/mnt/app/eso/hmi/lsd/jars/"

# custom.sh sets the on-unit modes; make the card copy match anyway.
find "$ROOT" -type f -exec chmod 644 {} +
chmod 755 "$MOD/custom.sh" "$MOD/command.sh" "$HOOKS"/*.so "$HOOKS"/*_render "$HOOKS"/*.sh

# The installer checks these files one by one instead of walking the card (QNX fs-dos
# fails to walk some FAT cards, and a partly copied card is caught by name).
( cd "$ROOT" && find . -type f | sort ) > "$MOD/carplay/manifest.txt"

( cd "$OUT" && set -- install_MoreIncredibleBash uninstall_MoreIncredibleBash
  COPYFILE_DISABLE=1 zip -qrX "$OUT.zip" "$@" )

echo "Built from $(git -C "$PROJECT_DIR" rev-parse --short HEAD)$(git -C "$PROJECT_DIR" diff --quiet HEAD || echo ' (dirty)'), audio: $AUDIO"
( cd "$PKG" && find . -type f | sort )
echo "Folder: build/$NAME/install_MoreIncredibleBash"
echo "Zip:    build/$NAME.zip  (copy mod/ onto the M.I.B. SD card)"
