UNAME := $(shell uname)

COMMON_SRC = main.c render.c maneuver.c route_path.c
TARGET = c_render

ifeq ($(UNAME), Darwin)
  # macOS — GLFW + OpenGL 2.1
  CC = cc
  PLATFORM_SRC = platform_macos.c
  CFLAGS = -O2 -std=c99 -Wall -Wextra -DPLATFORM_MACOS
  LDFLAGS = -L/opt/homebrew/lib -lglfw -framework OpenGL -lm
  INCLUDES = -I/opt/homebrew/include
else
  # QNX ARM cross-compile
  CC = /usr/qnx650/host/qnx6/x86/usr/bin/ntoarmv7-gcc
  PLATFORM_SRC = platform_qnx.c
  CFLAGS = -O2 -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX
  LDFLAGS = -lEGL -lGLESv2 -lsocket -lm
  INCLUDES =
endif

SRC = $(COMMON_SRC) $(PLATFORM_SRC)
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
