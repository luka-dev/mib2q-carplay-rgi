#!/bin/bash
# Shipping US/EU/CN classes against real stock interfaces; seeded IPC and raw-delta fuzz.
# CARPLAY_FUZZ_ITERATIONS=100000 CARPLAY_FUZZ_SEEDS="32 47426351 0x8b338d86 0x4ccb85a6" bash scripts/test_route_context_fuzz.sh
# Replay a failure with the printed seed; complete output persists in build/route-context-fuzz/.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TEST_JDK="$PROJECT_DIR/../../Tools/jxe2jar/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
ITERATIONS=${CARPLAY_FUZZ_ITERATIONS:-10000}
SEEDS=${CARPLAY_FUZZ_SEEDS:-"32 47426351 0x8b338d86 0x4ccb85a6"}
case "$ITERATIONS" in ''|*[!0-9]*) echo "Invalid CARPLAY_FUZZ_ITERATIONS" >&2; exit 2 ;; esac
[ "$ITERATIONS" -ge 100 ] || { echo "Use at least 100 events for coverage" >&2; exit 2; }
bash "$PROJECT_DIR/scripts/build_java.sh"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
LOG_DIR="$PROJECT_DIR/build/route-context-fuzz"
mkdir -p "$LOG_DIR"
CP="$PROJECT_DIR/build/carplay_hook.jar:$PROJECT_DIR/java_stock/US_MU1316_lsd.jar"
"$TEST_JDK/bin/javac" -encoding UTF-8 -cp "$CP" -d "$TEST_DIR" \
    "$PROJECT_DIR/tests/AltScreenContextFuzzTest.java" \
    "$PROJECT_DIR/tests/RouteGenerationFuzzTest.java" \
    "$PROJECT_DIR/tests/RouteGuidanceDeltaTest.java"
for variant in us eu cn; do
    case "$variant" in
        us) jar=carplay_hook.jar; stock=US_MU1316_lsd.jar ;;
        eu) jar=carplay_hook.jar; stock=EU_MU1329_lsd.jar ;;
        cn) jar=carplay_hook_cn.jar; stock=CN_MU1003_lsd.jar ;;
    esac
    CP="$TEST_DIR:$PROJECT_DIR/build/$jar:$PROJECT_DIR/java_stock/$stock"
    for seed in $SEEDS; do
        case "$seed" in ''|*[!0-9a-fA-FxX-]*) echo "Invalid seed: $seed" >&2; exit 2 ;; esac
        echo "=== route/context fuzz: $variant seed=$seed events=$ITERATIONS ==="
        for suite in com.luka.carplay.core.AltScreenContextFuzzTest RouteGenerationFuzzTest; do
            "$TEST_JDK/bin/java" -Xmx512m -cp "$CP" "$suite" "$seed" "$ITERATIONS" \
                2>&1 | tee "$LOG_DIR/$variant-$seed-${suite##*.}.log"
        done
    done
done
