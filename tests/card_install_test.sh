#!/bin/bash
# Exercise deploy/card/install.sh + uninstall.sh against a fake unit tree.
#
# The interesting part is the smartphone_integrator.json edit: it replaces a
# whole JSON object with brace counting in POSIX sh, on a car where a wrong
# result means CarPlay does not come back. This asserts the output is still
# valid JSON, that the neighbouring children survive, and that a second run
# changes nothing.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

fail() { echo "FAIL: $*"; exit 1; }

APP="$TMP/app"
SYS="$TMP/sys"
CFG="$SYS/etc/eso/production/smartphone_integrator.json"
PAYLOAD="$TMP/carplay"

mkdir -p "$APP/eso/hmi/lsd/jars" "$SYS/etc/eso/production" "$PAYLOAD/bin"
mkdir -p "$TMP/host-tools"
printf '#!/bin/sh\nexit 0\n' > "$TMP/host-tools/mount"
chmod +x "$TMP/host-tools/mount"
export PATH="$TMP/host-tools:$PATH"

# Fake stock config: the carplay child between two others, so a swallowed
# neighbour is detectable.
cat > "$CFG" <<'EOF'
{
  "children": {
    "usb": {
      "exec": "usb_manager",
      "path": "/mnt/app/eso/bin/apps",
      "envs": ["LD_PRELOAD=/eso/lib/libsystemtime_hack.so"]
    },
    "carplay": {
      "exec": "dio_manager",
      "path": "/mnt/app/eso/bin/apps",
      "envs": [
        "LD_LIBRARY_PATH=/eso/lib",
        "IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production"
      ],
      "cleanupScript": "/etc/scripts/carplay_cleanup.sh",
      "restartDelay": 2000
    },
    "bt": {
      "exec": "bt_manager",
      "path": "/mnt/app/eso/bin/apps"
    }
  }
}
EOF
cp "$CFG" "$TMP/stock.json"

for f in libcarplay_hook.so libcarplay_apac.so hssr_model.bin maneuver_render altscreen_render \
         flag_atlas.rgba carplay_startup.sh carplay_processes.sh carplay_cleanup.sh \
         carplay_hook.jar; do
    echo "payload $f" > "$PAYLOAD/bin/$f"
done
cp "$ROOT/deploy/smartphone_integrator/carplay_child.json" "$PAYLOAD/bin/carplay_child.json"
cp "$ROOT/deploy/smartphone_integrator/install_persistent_pf.sh" "$PAYLOAD/bin/"
for profile in pf.conf pf.ecm0.conf pf.mlan0.conf pf.ppp0.conf; do
    cp "$ROOT/deploy/smartphone_integrator/pf_stock/$profile" "$SYS/etc/$profile"
done
cp "$ROOT/deploy/card/install.sh" "$ROOT/deploy/card/uninstall.sh" "$PAYLOAD/"

run() {
    APP_ROOT="$APP" SYS_ROOT="$SYS" LOG="$TMP/$1.log" SRC_DIR="$PAYLOAD" \
        sh "$PAYLOAD/$1.sh" > "$TMP/$1.out" 2>&1
}

# Upgrade from an old HU-overlay build must remove the obsolete executable.
mkdir -p "$APP/root/hooks"
echo legacy > "$APP/root/hooks/cursor_render"

# ---------------------------------------------------------------- install
run install || { cat "$TMP/install.out"; fail "install.sh exited $?"; }

python3 - "$CFG" <<'PY' || fail "patched config is not valid JSON"
import json, sys
json.load(open(sys.argv[1]))
PY

python3 - "$CFG" <<'PY' || exit 1
import json, sys
c = json.load(open(sys.argv[1]))["children"]
assert sorted(c) == ["bt", "carplay", "usb"], "children changed: %s" % sorted(c)
assert c["carplay"]["exec"] == "carplay_startup.sh", c["carplay"]["exec"]
assert c["carplay"]["path"] == "/mnt/app/root/hooks", c["carplay"]["path"]
assert c["carplay"]["cleanupScript"] == "/mnt/app/root/hooks/carplay_cleanup.sh"
assert c["usb"]["exec"] == "usb_manager", "neighbour above was damaged"
assert c["bt"]["exec"] == "bt_manager", "neighbour below was damaged"
PY

[ ! -e "$APP/root/hooks/cursor_render" ] || fail "install retained retired HU renderer"

[ -f "$CFG.orig" ] || fail "no .orig backup"
cmp -s "$CFG.orig" "$TMP/stock.json" || fail ".orig is not the stock config"

for f in libcarplay_hook.so hssr_model.bin maneuver_render altscreen_render carplay_startup.sh; do
    [ -f "$APP/root/hooks/$f" ] || fail "$f was not installed"
done
[ -f "$APP/eso/hmi/lsd/jars/carplay_hook.jar" ] || fail "jar was not installed"
for profile in pf.conf pf.ecm0.conf pf.mlan0.conf pf.ppp0.conf; do
    grep -q 'on carplay0 proto tcp .*6030:6032,' "$SYS/etc/$profile" \
        || fail "PF profile $profile was not patched"
    cmp -s "$SYS/etc/$profile.stock" "$ROOT/deploy/smartphone_integrator/pf_stock/$profile" \
        || fail "PF profile $profile backup changed"
done

# ---------------------------------------------------------------- idempotent
cp "$CFG" "$TMP/first.json"
run install || fail "second install.sh exited $?"
cmp -s "$CFG" "$TMP/first.json" || fail "second run rewrote the config"
grep -q "already the carplay child" "$TMP/install.out" \
    || fail "second run did not detect the existing patch"

# Also cover uninstall directly from a legacy installation.
echo legacy > "$APP/root/hooks/cursor_render"

# ---------------------------------------------------------------- uninstall
run uninstall || fail "uninstall.sh exited $?"
cmp -s "$CFG" "$TMP/stock.json" || fail "uninstall did not restore the stock config"
[ -f "$APP/root/hooks/libcarplay_hook.so" ] && fail "uninstall left the hook behind"
[ -f "$APP/root/hooks/cursor_render" ] && fail "uninstall left cursor_render behind"
[ -f "$APP/root/hooks/hssr_model.bin" ] && fail "uninstall left the HSSR model behind"
[ -f "$APP/eso/hmi/lsd/jars/carplay_hook.jar" ] && fail "uninstall left the jar behind"

# A failed helper used to disappear behind the installer's logging pipeline.
printf '#!/bin/sh\necho "injected PF failure"\nexit 23\n' > "$PAYLOAD/bin/install_persistent_pf.sh"
if run install; then fail "installer reported success after PF failure"; fi
grep -q 'PF profile update failed (exit 23)' "$TMP/install.log" \
    || fail "installer did not retain PF failure status"
grep -q 'injected PF failure' "$TMP/install.log" || fail "PF diagnostics were lost"
if grep -q '=== DONE ===' "$TMP/install.log"; then fail "installer logged false success"; fi

echo "PASS: card install/uninstall, real PF profiles and helper failure propagation"
