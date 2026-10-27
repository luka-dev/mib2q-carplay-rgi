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
[ "$#" -eq 1 ] || { echo "usage: $0 <build dir with libcarplay_hook.so>" >&2; exit 2; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
IN="$(cd "$1" && pwd)"
[ -f "$IN/libcarplay_hook.so" ] || { echo "error: no libcarplay_hook.so in $IN" >&2; exit 1; }

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

rm -rf "$OUT" "$OUT.zip"
mkdir -p "$HOOKS" "$ROOT/mnt/app/eso/hmi/lsd/jars" "$ROOT/mnt/app/eso/bin/apps" "$ROOT/mnt/app/armle/lib/dll"

cp "$PROJECT_DIR/install_MoreIncredibleBash/mod/custom.sh" "$PROJECT_DIR/install_MoreIncredibleBash/mod/command.sh" "$MOD/"
cp "$SI/carplay_child.json" "$MOD/carplay/"
cp "$PROJECT_DIR/deploy/lang/ldzip.py" "$PROJECT_DIR/deploy/lang/upmix_lang.py" "$MOD/carplay/"   # UpMix row texts (patch_lang)
cp "$PROJECT_DIR/deploy/binpatch/binpatch.py" "$MOD/carplay/"   # the unit's own SI re-arm (patch_si)

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
cp "$PROJECT_DIR/deploy/deva-ctrl-qc/deva-ctrl-qc.so.patched" "$ROOT/mnt/app/armle/lib/dll/deva-ctrl-qc.so"

# custom.sh sets the on-unit modes; make the card copy match anyway.
find "$ROOT" -type f -exec chmod 644 {} +
chmod 755 "$MOD/custom.sh" "$MOD/command.sh" "$HOOKS"/*.so "$HOOKS"/*_render "$HOOKS"/*.sh \
  "$ROOT/mnt/app/armle/lib/dll/deva-ctrl-qc.so"

# The installer checks these files one by one instead of walking the card (QNX fs-dos
# fails to walk some FAT cards, and a partly copied card is caught by name).
( cd "$ROOT" && find . -type f | sort ) > "$MOD/carplay/manifest.txt"

( cd "$OUT" && COPYFILE_DISABLE=1 zip -qrX "$OUT.zip" install_MoreIncredibleBash )

echo "Built from $(git -C "$PROJECT_DIR" rev-parse --short HEAD)$(git -C "$PROJECT_DIR" diff --quiet HEAD || echo ' (dirty)'), audio: $AUDIO"
( cd "$PKG" && find . -type f | sort )
echo "Folder: build/$NAME/install_MoreIncredibleBash"
echo "Zip:    build/$NAME.zip  (copy mod/ onto the M.I.B. SD card)"
