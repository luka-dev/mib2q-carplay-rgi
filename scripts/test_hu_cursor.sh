#!/bin/bash
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TEST_JDK="$PROJECT_DIR/../../Tools/jxe2jar/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
"$TEST_JDK/bin/javac" -encoding UTF-8 -source 1.4 -target 1.4 -Xlint:-options -d "$TEST_DIR" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/cursor/PointerState.java" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/cursor/PointerClick.java" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/cursor/CursorWire.java" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/cursor/CursorTransport.java" \
    "$PROJECT_DIR/tests/CursorTransportTest.java" \
    "$PROJECT_DIR/tests/HuPointerTest.java"
"$TEST_JDK/bin/java" -Djava.awt.headless=true -cp "$TEST_DIR" \
    com.luka.carplay.cursor.HuPointerTest "$PROJECT_DIR/build/hu-cursor-preview"
"$TEST_JDK/bin/java" -cp "$TEST_DIR" com.luka.carplay.cursor.CursorTransportTest
cc -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined \
    "$PROJECT_DIR/tests/cursor_engine_test.c" "$PROJECT_DIR/cursor_render/engine.c" \
    -o "$TEST_DIR/cursor_engine_test" -lm
"$TEST_DIR/cursor_engine_test"
cc -std=gnu99 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined \
    "$PROJECT_DIR/tests/cursor_plane_test.c" \
    "$PROJECT_DIR/cursor_render/session.c" "$PROJECT_DIR/cursor_render/engine.c" "$PROJECT_DIR/cursor_render/cursor_wire.c" \
    -o "$TEST_DIR/cursor_plane_test" -lm
"$TEST_DIR/cursor_plane_test" "$PROJECT_DIR/build/hu-cursor-preview/cursor-wire.bin"
cc -std=gnu99 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined \
    "$PROJECT_DIR/cursor_render/main.c" "$PROJECT_DIR/cursor_render/session.c" "$PROJECT_DIR/cursor_render/engine.c" \
    "$PROJECT_DIR/cursor_render/cursor_wire.c" "$PROJECT_DIR/tests/stubs/cursor_plane_stub.c" \
    -o "$TEST_DIR/cursor_process_test" -lm
python3 "$PROJECT_DIR/tests/cursor_process_test.py" "$TEST_DIR/cursor_process_test" \
    "$PROJECT_DIR/build/hu-cursor-preview/cursor-wire.bin"

# Hidden GPU checks use the same scene code as cursor_render and the host preview.
cc -std=c99 -Wall -Wextra -Werror -O2 -I/opt/homebrew/include \
    "$PROJECT_DIR/tests/cursor_gpu_test.c" "$PROJECT_DIR/cursor_render/cursor_gl.c" \
    "$PROJECT_DIR/cursor_render/cursor_wire.c" "$PROJECT_DIR/common/prism.c" \
    "$PROJECT_DIR/common/prism_gl.c" -L/opt/homebrew/lib -lglfw \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
    -o "$TEST_DIR/cursor_gpu_test"
"$TEST_DIR/cursor_gpu_test"

# Desktop input must never cut short the real native PRISM reveal.
make -C "$PROJECT_DIR/tools/cursor-preview" check
