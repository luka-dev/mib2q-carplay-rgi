# PRISM: all runtime resources are compiled from checked-in generated headers.
BREW ?= /opt/homebrew
CC = cc
CFLAGS = -O2 -std=c99 -Wall -Wextra -I$(BREW)/include
# Bundle GLFW statically; only macOS system frameworks remain dynamic.
LDFLAGS = $(BREW)/lib/libglfw3.a -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore
BIN = prism_preview
COMMON = ../../common
HEADERS = $(COMMON)/prism.h $(COMMON)/prism_handoff.h $(COMMON)/prism_baked.h $(COMMON)/loader_layout.h prism_demo_baked.h
TESTS = .prism-test-handoff .prism-test-queue .prism-test-gpu

all: $(BIN)

$(BIN): prism_preview.c $(COMMON)/prism.c $(HEADERS)
	$(CC) $(CFLAGS) prism_preview.c $(COMMON)/prism.c -o $@ $(LDFLAGS)

.prism-test-handoff: test_handoff.c $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@

.prism-test-queue: test_prism_queue.c ../../altscreen_render/prism_queue.h $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@

.prism-test-gpu: test_prism_gpu.c $(COMMON)/prism.c $(HEADERS)
	$(CC) $(CFLAGS) test_prism_gpu.c $(COMMON)/prism.c -o $@ $(LDFLAGS)

check: $(TESTS)
	./.prism-test-handoff
	./.prism-test-queue
	./.prism-test-gpu

run: $(BIN)
	./$(BIN)

# Optional authoring step only. Building/running needs neither Python nor PNGs.
bake:
	python3 prepare_material.py
	python3 prepare_signature.py
	python3 prepare_stream.py
	python3 bake_prism.py

check-bake:
	python3 bake_prism.py --check

app: $(BIN)
	python3 bundle_preview.py

clean:
	rm -f $(BIN) $(TESTS) *.o

.PHONY: all run bake check check-bake app clean
