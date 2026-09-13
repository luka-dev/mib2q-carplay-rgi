.DEFAULT_GOAL := all
UNAME := $(shell uname)

BUILD_DIR = ../build

RENDERER_COMMON = main.c render.c maneuver.c route_path.c server.c
TARGET = $(BUILD_DIR)/maneuver_render
HARNESS = $(BUILD_DIR)/test_harness

ifeq ($(UNAME), Darwin)
  # macOS — GLFW + OpenGL 2.1
  CC = cc
  CXX = c++
  PLATFORM_SRC = platform_macos.c
  CFLAGS = -O2 -std=c99 -Wall -Wextra -DPLATFORM_MACOS
  LDFLAGS = -L/opt/homebrew/lib -lglfw -framework OpenGL -lm
  INCLUDES = -I/opt/homebrew/include
  HARNESS_LDFLAGS = -L/opt/homebrew/lib -lglfw -framework Cocoa -framework IOKit
  BUILD_HARNESS = 1
else
  # QNX ARM cross-compile
  CC = /usr/qnx650/host/qnx6/x86/usr/bin/ntoarmv7-gcc
  CXX = /usr/qnx650/host/qnx6/x86/usr/bin/ntoarmv7-g++
  PLATFORM_SRC = platform_qnx.c ../common/cluster_surface.c
  CFLAGS = -O2 -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX
  LDFLAGS = -lEGL -lGLESv2 -lscreen -lsocket -lm
  INCLUDES = -I../common
  BUILD_HARNESS = 0
endif

RENDERER_SRC = $(RENDERER_COMMON) $(PLATFORM_SRC)
HARNESS_SRC = test_harness.c
SCENE_SRC = scene/scene.cpp scene/geometry.cpp scene/layout.cpp scene/lane_panel.cpp
OBJ_DIR = $(BUILD_DIR)/maneuver-objects
C_OBJECTS = $(addprefix $(OBJ_DIR)/,$(RENDERER_SRC:.c=.o))
SCENE_OBJECTS = $(addprefix $(OBJ_DIR)/,$(SCENE_SRC:.cpp=.o))
CXXFLAGS = $(filter-out -std=c99 -std=gnu99,$(CFLAGS)) -std=c++11 -fno-exceptions -fno-rtti
HEADERS = $(wildcard *.h scene/*.h scene/*.hpp)
-include $(C_OBJECTS:.o=.d) $(SCENE_OBJECTS:.o=.d)

ifeq ($(BUILD_HARNESS), 1)
all: $(TARGET) $(HARNESS)
else
all: $(TARGET)
endif

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(C_OBJECTS) $(SCENE_OBJECTS) | $(BUILD_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: %.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(HARNESS): $(HARNESS_SRC) $(wildcard *.h) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(HARNESS_SRC) -o $@ $(HARNESS_LDFLAGS)

clean:
	rm -f $(TARGET) $(HARNESS) $(C_OBJECTS) $(SCENE_OBJECTS) $(C_OBJECTS:.o=.d) $(SCENE_OBJECTS:.o=.d)

run: $(TARGET)
	$(TARGET)

grid:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -DCR_DEBUG_GRID" all

# Offline C/type-check of the QNX platform + cluster_surface against stub
# EGL/GLES2/screen headers (real EGL/screen are BSP-only).  Host compiler.
hostcheck:
	cc -fsyntax-only -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX \
	   -Ihostcheck -I../altscreen_render/hostcheck -I../common -I. platform_qnx.c
	cc -fsyntax-only -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX \
	   -I../altscreen_render/hostcheck -I../common ../common/cluster_surface.c

.PHONY: all clean run grid hostcheck
