#!/bin/bash
# Lane guidance end to end: the real native route-guidance hook (iAP2 frames in,
# bus text out) feeding the real Java RouteGuidance/BAPBridge (HUD + renderer).
# tests/lane_guidance_e2e.c drives the hook like iOS CarKitNavigation does;
# tests/LaneGuidanceE2ETest.java replays its bus frames and checks every junction.
#   HOOK_SRC=<dir with routeguidance/ and framework/>  test another hook tree
#   SKIP_JAVA_BUILD=1                                   reuse build/carplay_hook.jar
#   KEEP_DIR=<dir>                                      keep the bus frames there
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
. "$PROJECT_DIR/scripts/java_env.sh"
HOOK=${HOOK_SRC:-$PROJECT_DIR/hook}
STOCK_JAR="$PROJECT_DIR/java_stock/US_MU1316_lsd.jar"
PATCH_JAR="$PROJECT_DIR/build/carplay_hook.jar"
if [ -n "${KEEP_DIR:-}" ]; then OUT=$KEEP_DIR; rm -rf "$OUT"; mkdir -p "$OUT"
else OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT; fi
mkdir "$OUT/frames" "$OUT/classes"

CFLAGS="-std=gnu99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DENABLE_LOGGING=0 -I$HOOK"
# The bus text builder is the real one; only the socket send is replaced (by the harness).
cc $CFLAGS -Dbus_send_text=bus_send_text_unused -c "$HOOK/framework/bus.c" -o "$OUT/bus.o"
cc $CFLAGS "$PROJECT_DIR/tests/lane_guidance_e2e.c" "$OUT/bus.o" \
    "$HOOK/routeguidance/rgd_tlv.c" "$HOOK/framework/iap2_protocol.c" "$HOOK/framework/signal_guard.c" \
    -o "$OUT/lane_e2e" -lpthread
"$OUT/lane_e2e" "$OUT/frames" > /dev/null

[ "${SKIP_JAVA_BUILD:-0}" = 1 ] && [ -f "$PATCH_JAR" ] || bash "$PROJECT_DIR/scripts/build_java.sh" > /dev/null
CP="$PATCH_JAR:$STOCK_JAR:$OSGI_CP"
"$JAVA8_HOME/bin/javac" -nowarn -encoding UTF-8 -cp "$CP" -d "$OUT/classes" \
    "$PROJECT_DIR/tests/LaneGuidanceLifecycleTest.java" "$PROJECT_DIR/tests/LaneGuidanceE2ETest.java" 2> /dev/null
"$JAVA8_HOME/bin/java" -Xverify:none -cp "$OUT/classes:$CP" com.luka.carplay.rgd.LaneGuidanceE2ETest "$OUT/frames"
