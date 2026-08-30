UNAME := $(shell uname)

BUILD_DIR = ../build

RENDERER_COMMON = main.c render.c maneuver.c route_path.c server.c
TARGET = $(BUILD_DIR)/maneuver_render
HARNESS = $(BUILD_DIR)/test_harness

ifeq ($(UNAME), Darwin)
  # macOS — GLFW + OpenGL 2.1
  CC = cc
  PLATFORM_SRC = platform_macos.c
  CFLAGS = -O2 -std=c99 -Wall -Wextra -DPLATFORM_MACOS
  LDFLAGS = -L/opt/homebrew/lib -lglfw -framework OpenGL -lm
  INCLUDES = -I/opt/homebrew/include
  HARNESS_LDFLAGS = -L/opt/homebrew/lib -lglfw -framework Cocoa -framework IOKit
  BUILD_HARNESS = 1
else
  # QNX ARM cross-compile. Canonical entry is ../scripts/build_renderers.sh (Docker
  # qnx65-armv7-toolchain, gcc on PATH); direct `make` works inside that image too.
  CC ?= arm-unknown-nto-qnx6.5.0eabi-gcc
  PLATFORM_SRC = platform_qnx.c ../common/cluster_surface.c
  CFLAGS = -O2 -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX
  LDFLAGS = -lEGL -lGLESv2 -lscreen -lsocket -lm
  INCLUDES = -I../common
  BUILD_HARNESS = 0
endif

RENDERER_SRC = $(RENDERER_COMMON) $(PLATFORM_SRC)
HARNESS_SRC = test_harness.c

ifeq ($(BUILD_HARNESS), 1)
all: $(TARGET) $(HARNESS)
else
all: $(TARGET)
endif

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(RENDERER_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(RENDERER_SRC) -o $@ $(LDFLAGS)

$(HARNESS): $(HARNESS_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(HARNESS_SRC) -o $@ $(HARNESS_LDFLAGS)

clean:
	rm -f $(TARGET) $(HARNESS)

run: $(TARGET)
	$(TARGET)

grid: CFLAGS += -DCR_DEBUG_GRID
grid: clean all

# Offline C/type-check of the QNX platform + cluster_surface against stub
# EGL/GLES2/screen headers (real EGL/screen are BSP-only).  Host compiler.
hostcheck:
	cc -fsyntax-only -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX \
	   -Ihostcheck -I../common -I. platform_qnx.c
	cc -fsyntax-only -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX \
	   -Ihostcheck -I../common ../common/cluster_surface.c

.PHONY: all clean run grid hostcheck
