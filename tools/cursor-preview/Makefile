# Host viewer of the production GPU scene and native animation engine.
CC = cc
CFLAGS = -O2 -std=c99 -Wall -Wextra -Werror $(shell pkg-config --cflags sdl2)
LDLIBS = $(shell pkg-config --libs sdl2) -framework OpenGL -lm
SOURCES = preview.c ../../cursor_render/engine.c ../../cursor_render/cursor_wire.c ../../cursor_render/cursor_gl.c ../../common/prism.c ../../common/prism_gl.c
BIN = ../../build/cursor_preview

SCENE_HEADERS = ../../cursor_render/glass_shader.h ../../cursor_render/engine.h ../../common/prism_handoff.h ../../cursor_render/cursor_plane.h ../../common/prism.h ../../common/prism_gl.h ../../common/prism_baked.h ../../cursor_render/cursor_gl.h ../../hook/coverart/stb_image.h

$(BIN): $(SOURCES) $(SCENE_HEADERS)
	mkdir -p ../../build
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDLIBS)

run: $(BIN)
	$(BIN)

bundle: $(BIN)
	python3 bundle.py

.PHONY: run bundle

HANDOFF_TEST = ../../build/cursor_preview_handoff_test
$(HANDOFF_TEST): ../../tests/cursor_preview_handoff_test.c $(SOURCES) $(SCENE_HEADERS)
	$(CC) $(CFLAGS) ../../tests/cursor_preview_handoff_test.c $(filter-out preview.c,$(SOURCES)) -o $@ $(LDLIBS)

check: $(HANDOFF_TEST)
	$(HANDOFF_TEST)

.PHONY: check
