ROOT=../..
OUT=$(ROOT)/build/lane-road-preview
CC=cc
CFLAGS=-O2 -std=c99 -Wall -Wextra -DPLATFORM_MACOS -I/opt/homebrew/include -I$(ROOT)/maneuver_render
SOURCES=preview.c $(ROOT)/maneuver_render/render.c $(ROOT)/maneuver_render/maneuver.c $(ROOT)/maneuver_render/route_path.c $(ROOT)/maneuver_render/platform_macos.c
all: $(OUT)/preview
$(OUT):
	mkdir -p $@
$(OUT)/preview: $(SOURCES) lane_layout.h $(ROOT)/maneuver_render/lane_safety.h $(ROOT)/maneuver_render/arrow_progress.h | $(OUT)
	$(CC) $(CFLAGS) $(SOURCES) -L/opt/homebrew/lib -lglfw -framework OpenGL -lm -o $@
test: | $(OUT)
	$(CC) -g -O1 -std=c99 -Wall -Wextra -fsanitize=address,undefined $(ROOT)/tests/lane_safety_test.c -o $(OUT)/test_safety
	$(OUT)/test_safety
	$(CC) -g -O1 -std=c99 -Wall -Wextra -fsanitize=address,undefined test_layout.c -o $(OUT)/test_layout
	$(OUT)/test_layout
	$(CC) $(CFLAGS) -g -O1 -fsanitize=address,undefined test_outline.c $(filter-out preview.c,$(SOURCES)) -L/opt/homebrew/lib -lglfw -framework OpenGL -lm -o $(OUT)/test_outline
	$(OUT)/test_outline
.PHONY: all test
