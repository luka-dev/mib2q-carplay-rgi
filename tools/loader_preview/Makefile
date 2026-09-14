# PRISM: all runtime resources are compiled from checked-in generated headers.
BREW ?= /opt/homebrew
CC = cc
CFLAGS = -O2 -std=c99 -Wall -Wextra -I$(BREW)/include
# Bundle GLFW statically; only macOS system frameworks remain dynamic.
LDFLAGS = $(BREW)/lib/libglfw3.a -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore
BIN = prism_preview
LOADER = ../../altscreen_render/loader
HEADERS = $(LOADER)/prism.h $(LOADER)/prism_baked.h $(LOADER)/loader_layout.h $(LOADER)/prism_waiting.h
TESTS = .prism-test-waiting

all: $(BIN)

$(BIN): prism_preview.c $(LOADER)/prism.c $(HEADERS)
	$(CC) $(CFLAGS) prism_preview.c $(LOADER)/prism.c -o $@ $(LDFLAGS)

.prism-test-waiting: test_prism_waiting.c $(LOADER)/prism.c $(HEADERS)
	$(CC) $(CFLAGS) test_prism_waiting.c $(LOADER)/prism.c -o $@ $(LDFLAGS)

check: $(TESTS)
	./.prism-test-waiting

run: $(BIN)
	./$(BIN)

# Optional authoring step only. Building/running needs neither Python nor PNGs.
bake:
	python3 prepare_material.py
	python3 prepare_signature.py
	python3 bake_prism.py

check-bake:
	python3 bake_prism.py --check

app: $(BIN)
	python3 bundle_preview.py

clean:
	rm -f $(BIN) $(TESTS) *.o

.PHONY: all run bake check check-bake app clean
