#!/bin/sh
# Build the Android Auto cockpit Java overlay against user-supplied firmware jars.
# Runs inside the pinned JDK 8 container from scripts/build_android_auto_java.sh.
# No Audi firmware or stock class is copied into the output.
set -e
cd "$(dirname "$0")"
ROOT=../..
LSD=${STOCK_JAR:?set STOCK_JAR to the firmware-derived compatibility jar}
LSDIC=${COMPILE_JAR:-$LSD}
DRIVE_LOG=${DRIVE_LOG:-}
JAR=build/mib2q_android_auto_cluster.jar
export LC_ALL=C

for f in "$LSD" "$LSDIC"; do [ -f "$f" ] || { echo "ERROR: $f not found"; exit 1; }; done

rm -rf build/classes build/gen build/test build/test-stubs build/check "$JAR"
mkdir -p build/classes build/gen build/test build/check

# 1. Firmware-specific Android Auto classes. For the validated MU0918 profile the reviewed
# generated sources are versioned. Porters can set FIRMWARE_SOURCE_DIR to regenerate the three
# seams from their own CFR output; the API/member gates below still decide whether the result is safe.
if [ -n "${FIRMWARE_SOURCE_DIR:-}" ]; then
    python3 gen_distributor.py
    python3 gen_navhandler.py
    python3 gen_connector.py
else
    cp -R generated-mu0918/. build/gen/
fi

# 2. luka's CarPlayApp with a build id (the source tree is never edited)
BUILD_ID=${AA_BUILD_ID:-android-auto-$(cat $(find src generated-mu0918 -name '*.java' | sort) gen_*.py | sha1sum | cut -c1-10)}
mkdir -p build/gen/com/luka/carplay/core
sed "s/@BUILD_ID@/$BUILD_ID/g" src/com/luka/carplay/core/CarPlayApp.java > build/gen/com/luka/carplay/core/CarPlayApp.java
find src -name '*.java' | grep -v '/com/luka/carplay/core/CarPlayApp.java$' > build/check/sources.txt
find build/gen -name '*.java' >> build/check/sources.txt
echo "Compiling $(wc -l < build/check/sources.txt | tr -d ' ') sources (Java 1.4 bytecode) build=$BUILD_ID"
# compile-only stand-ins for stock classes the rebuilt key controller references but lsd.jar lacks
# (present on the unit, e.g. TerminalModeUtils): first on -cp, never packaged
rm -rf build/cstubs; mkdir -p build/cstubs
javac -source 1.4 -target 1.4 -Xlint:-options -nowarn -encoding UTF-8 -cp "$LSDIC" \
	-d build/cstubs $(find compile-stubs -name '*.java')
javac -source 1.4 -target 1.4 -Xlint:-options -nowarn -encoding UTF-8 \
	-cp "build/cstubs:$LSDIC" -d build/classes @build/check/sources.txt
for s in de/audi/app/terminalmode/TerminalModeUtils de/audi/atip/utils/generics/Consumer \
	de/audi/atip/utils/reactive/observables/Observable de/audi/atip/utils/reactive/properties/LoggingPropertyFactory \
	de/audi/app/terminalmode/audio/IAudioManager; do
	if [ -f build/classes/$s.class ]; then echo "ERROR: compile stub $s leaked into build/classes"; exit 1; fi
done
cp -R resources/. build/classes/

# 3. Gates ---------------------------------------------------------------------------------
# 3a. Stock classes we ship on purpose (and their nested classes); anything else that also
#     exists in lsd_ic.jar is a classpath leak.
REPLACED="de.audi.tghu.navi.app.cluster.ClusterService
de.audi.tghu.fwhmi.DisplayManagerMIB2High
de.audi.tghu.fwhmi.DisplayManagerMIB2High\$DisplayManagerProvider
de.esolutions.hmi.widgets.audi.evo.high.widgets.CombiMapController
de.audi.app.terminalmode.smartphone.androidauto2.AndroidAuto2ListenerDistributor
de.audi.app.terminalmode.smartphone.androidauto2.nav.AndroidAuto2NavHandler
de.audi.app.terminalmode.smartphone.androidauto2.AndroidAuto2KeyEventsController
de.audi.app.combi.bap.app.audio.AppConnectorTerminalMode
de.audi.app.terminalmode.ExternalEventsListener
de.audi.app.earlyfunc.evo.parking.ParkingSystemControllerComponentEvo
de.audi.app.earlyfunc.core.parking.ParkingPartialPopupHandler
de.audi.audio.context.AudioDrawerContextImpl
de.esolutions.hmi.widgets.audi.evo.high.PartialPopupManagerEvoHigh"
echo "$REPLACED" > build/check/replaced.txt
jar tf "$LSDIC" | grep '\.class$' | sort -u > build/check/stock-classes.txt
find build/classes -name '*.class' | sed 's#^build/classes/##' | sort -u > build/check/our-classes.txt
sed 's#\.#/#g; s#\$.*##' build/check/replaced.txt | sort -u > build/check/allowed-outers.txt
: > build/check/gate-errors.txt
: > build/check/shipped-stock.txt
while read -r C; do
	[ -n "$C" ] || continue
	if grep -Fqx "$C" build/check/stock-classes.txt; then
		echo "$C" >> build/check/shipped-stock.txt
		OUTER=${C%.class}; OUTER=${OUTER%%\$*}
		grep -Fqx "$OUTER" build/check/allowed-outers.txt || echo "$C (unapproved stock replacement)" >> build/check/gate-errors.txt
	fi
	set -- $(od -An -tu1 -j6 -N2 "build/classes/$C")
	[ "$(( $1 * 256 + $2 ))" = 48 ] || echo "$C (not Java 1.4 bytecode)" >> build/check/gate-errors.txt
	case "$C" in
		com/sq5/aa/AaClusterBridge*|com/luka/carplay/pdc/*|com/luka/carplay/input/*|\
		com/luka/carplay/coverart/*|com/luka/carplay/core/SteeringWheelInputModule*|\
		de/audi/app/terminalmode/combi/*|de/audi/app/terminalmode/dsi/carplay/*)
			echo "$C (dropped/old-writer class in build)" >> build/check/gate-errors.txt ;;
	esac
done < build/check/our-classes.txt
if [ -s build/check/gate-errors.txt ]; then
	echo "ERROR: gate failed:"; sed 's/^/  /' build/check/gate-errors.txt; exit 1
fi
echo "leak gate OK: $(wc -l < build/check/our-classes.txt | tr -d ' ') classes, $(wc -l < build/check/shipped-stock.txt | tr -d ' ') replace stock classes"

# 3b. Replaced stock classes: every public/protected member of the MU0918 original must still be
#     there with the same signature (additions allowed); class header must match.
FAIL=0
while read -r C; do
	[ -n "$C" ] || continue
	N=$(echo "$C" | tr '.$' '__')
	javap -cp "$LSD" -p -s "$C" > build/check/$N.orig.txt
	javap -cp build/classes -p -s "$C" > build/check/$N.ours.txt
	grep -E '^  (public|protected) ' build/check/$N.orig.txt | sed 's/ *$//' | sort -u > build/check/$N.orig.members
	grep -E '^  (public|protected) ' build/check/$N.ours.txt | sed 's/ *$//' | sort -u > build/check/$N.ours.members
	MISSING=$(comm -23 build/check/$N.orig.members build/check/$N.ours.members)
	ADDED=$(comm -13 build/check/$N.orig.members build/check/$N.ours.members | wc -l | tr -d ' ')
	HO=$(grep -E '^(public |protected |final |abstract )*(class|interface) ' build/check/$N.orig.txt | head -1 | sed 's/ *{ *$//')
	HN=$(grep -E '^(public |protected |final |abstract )*(class|interface) ' build/check/$N.ours.txt | head -1 | sed 's/ *{ *$//')
	if [ -n "$MISSING" ]; then
		echo "ERROR: $C lost/changed stock-firmware members:"; echo "$MISSING"; FAIL=1
	elif [ "$HO" != "$HN" ]; then
		echo "ERROR: $C class header differs from stock firmware:"; echo "  stock: $HO"; echo "  ours:   $HN"; FAIL=1
	else
		echo "member gate OK: $C ($(wc -l < build/check/$N.orig.members | tr -d ' ') stock members kept, $ADDED added)"
	fi
done < build/check/replaced.txt
[ $FAIL -eq 0 ] || exit 1

# 3c. Steering-wheel roller hook (AaRollerInput): the BAP listener must override setMapScale(int) and
#     the rebuilt key controller must hand its DSI to the roller bridge.
javap -cp build/classes -p de.audi.tghu.navi.app.cluster.ScreenCombiBAPListener | grep -q 'public void setMapScale(int)' 	|| { echo "ERROR: ScreenCombiBAPListener.setMapScale(int) missing (roller hook)"; exit 1; }
javap -cp build/classes -c de.audi.app.terminalmode.smartphone.androidauto2.AndroidAuto2KeyEventsController 	| grep -q 'com/sq5/aa/input/AaRollerInput.setDsi' 	|| { echo "ERROR: AndroidAuto2KeyEventsController does not bind AaRollerInput"; exit 1; }
echo "roller hook gate OK: ScreenCombiBAPListener.setMapScale + AaRollerInput.setDsi"

# 4. Jar ------------------------------------------------------------------------------------
(cd build/classes && jar cf ../mib2q_android_auto_cluster.jar .)

# 5. Host test: replay the Android Auto drive log through adapter -> RouteGuidance -> BAPBridge
# test/stubs: host-only stand-ins for three MU0918 stock classes whose constructors cannot run on
# HotSpot (the J9 classes reconstructed from lsd.jxe recurse in <init>; jxe2jar also lost their
# float constants).  Their stub BAPDistanceFormatter is exact (metres*10, unit 0) so the replay can
# assert the FctID 18 value.  Compiled to build/test-stubs, first on the test classpath, never shipped.
mkdir -p build/test-stubs
javac -nowarn -encoding UTF-8 -d build/test-stubs $(find test/stubs -name '*.java')
javac -nowarn -encoding UTF-8 -cp "build/test-stubs:build/classes:$LSDIC" -d build/test test/*.java
# lsd_ic.jar holds IBM J9 classes reconstructed from the JXE; HotSpot's verifier rejects some of
# their bytecode, so (as luka's own host tests do) verification is off for this probe only, and the
# JIT is off (-Xint): C1 crashes compiling BAPBridge.update against the unverified J9 classes.
if [ -n "$DRIVE_LOG" ] && [ -f "$DRIVE_LOG" ]; then
    java -Xverify:none -Xint -cp "build/test:build/test-stubs:$JAR:$LSDIC" AaLukaReplayTest "$DRIVE_LOG"
else
    echo "replay test skipped (no private DRIVE_LOG supplied)"
fi
# single cluster-switch worker + mirror rule against a recording DisplayManager (real marker file)
java -Xverify:none -Xint -cp "build/test:build/test-stubs:$JAR:$LSDIC" ScreenModuleMirrorTest
# VC map-view gate: stock setGALState(true) (-> BAP InfoStates 6) suppressed only while ctx 81/82 is shown
java -Xverify:none -Xint -cp "build/test:build/test-stubs:$JAR:$LSDIC" VcMapViewGateTest
# cockpit turn card: layer rule (off/stock/card/prearm), 81 held across route edges, plane geometry per
# VC view against a recording DisplayManager, renderer viewport, sq5_mirror feed record
java -Xverify:none -Xint -cp "build/test:build/test-stubs:$JAR:$LSDIC" TurnCardTest
# cockpit options menu: open/close, roller selection/edit, DDS_SELECT consumption, record
java -Xverify:none -Xint -cp "build/test:build/test-stubs:$JAR:$LSDIC" AaClusterMenuTest

ls -la "$JAR"
echo "OK: $JAR (build $BUILD_ID)"
