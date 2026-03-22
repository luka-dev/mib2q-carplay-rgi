UNAME := $(shell uname)

RENDERER_COMMON = main.c render.c maneuver.c route_path.c server.c
TARGET = c_render
HARNESS = test_harness

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
  # QNX ARM cross-compile
  CC = /usr/qnx650/host/qnx6/x86/usr/bin/ntoarmv7-gcc
  PLATFORM_SRC = platform_qnx.c
  CFLAGS = -O2 -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX
  LDFLAGS = -lEGL -lGLESv2 -lsocket -lm
  INCLUDES =
  BUILD_HARNESS = 0
endif

RENDERER_SRC = $(RENDERER_COMMON) $(PLATFORM_SRC)
RENDERER_OBJ = $(RENDERER_SRC:.c=.o)

HARNESS_SRC = test_harness.c
HARNESS_OBJ = $(HARNESS_SRC:.c=.o)

ifeq ($(BUILD_HARNESS), 1)
all: $(TARGET) $(HARNESS)
else
all: $(TARGET)
endif

$(TARGET): $(RENDERER_OBJ)
	$(CC) $(RENDERER_OBJ) -o $@ $(LDFLAGS)

$(HARNESS): $(HARNESS_OBJ)
	$(CC) $(HARNESS_OBJ) -o $@ $(HARNESS_LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(RENDERER_OBJ) $(HARNESS_OBJ) $(TARGET) $(HARNESS)

run: $(TARGET)
	./$(TARGET)

grid: CFLAGS += -DCR_DEBUG_GRID
grid: clean all

.PHONY: all clean run grid
