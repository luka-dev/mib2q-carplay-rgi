# macOS host preview of the cluster startup loader (dev-only).
#   make run     # build + open the 1440x542 window (ESC = quit)
BREW ?= /opt/homebrew
CC    = cc
CFLAGS = -O2 -std=c99 -Wall -Wextra -I$(BREW)/include
LDFLAGS = -L$(BREW)/lib -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
BIN = loader_preview

$(BIN): loader_preview.c stbi.c apple_logo.png
	$(CC) -w -std=c99 -c stbi.c -o stbi.o
	$(CC) $(CFLAGS) -c loader_preview.c -o loader_preview.o
	$(CC) loader_preview.o stbi.o -o $@ $(LDFLAGS)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN) *.o

.PHONY: run clean
