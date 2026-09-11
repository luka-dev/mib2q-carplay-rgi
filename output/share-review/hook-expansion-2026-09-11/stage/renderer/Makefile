CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra

# Host-only receiver/capture build. No hardware backend is linked here.
video_receiver: main.c sink_capture.c sink.h ../altscreen/frame_io.c ../altscreen/frame_io.h
	$(CC) $(CFLAGS) main.c sink_capture.c ../altscreen/frame_io.c -pthread -o $@

.PHONY: clean
clean:
	rm -f video_receiver
