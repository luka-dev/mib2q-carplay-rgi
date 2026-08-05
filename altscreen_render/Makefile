UNAME := $(shell uname)

BUILD_DIR = ../build
TARGET    = $(BUILD_DIR)/altscreen_render
TESTBIN   = $(BUILD_DIR)/altscreen_render_test

# OpenMAX IL API headers captured for the QNX 6.5 ABI.
OMX_INC = ../toolchain/qnx65-abi/include/OMX

# Shared managed-window primitive (same one maneuver_render uses).
COMMON_INC = ../common

COMMON = main.c frame_io.c altctl.c

ifeq ($(UNAME), Darwin)
  # macOS — file-capture sink (proves the wire; no OMX/GL deps).
  CC   = cc
  SINK = sink_file.c
  CFLAGS = -O2 -std=c99 -Wall -Wextra
  LDFLAGS =
  BUILD_TEST = 1
else
  # QNX ARM cross-compile — OMX AVC decode -> EGLImage/GLES tiled-to-RGBA.
  # OMX core + libscreen come from the MHI2Q BSP (unit/VM), NOT the generic SDP;
  # -lOmxCore is the expected OMX IL core name — CONFIRM on-unit if the link fails.
  CC   = /usr/qnx650/host/qnx6/x86/usr/bin/ntoarmv7-gcc
  SINK = sink_omx_qnx.c screen_egl_present.c $(COMMON_INC)/cluster_surface.c
  CFLAGS = -O2 -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX -fdata-sections -ffunction-sections -I$(OMX_INC) -I$(COMMON_INC)
  LDFLAGS = -Wl,--gc-sections -lscreen -lOmxCore -lEGL -lGLESv2 -lsocket
  BUILD_TEST = 0
endif

SRC = $(COMMON) $(SINK)

ifeq ($(BUILD_TEST), 1)
all: $(TARGET) $(TESTBIN)
else
all: $(TARGET)
endif

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

$(TESTBIN): test_feed.c frame_io.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) test_feed.c frame_io.c -o $@

test: $(TESTBIN)
	$(TESTBIN)

# Offline C/type-check of the QNX OMX sink against stub screen.h + OMX shims
# (the real screen/OMX libs are BSP-only).  Host compiler, syntax only.
hostcheck:
	$(CC) -fsyntax-only -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX \
	      -Ihostcheck -I$(OMX_INC) -I$(COMMON_INC) -I. sink_omx_qnx.c
	$(CC) -fsyntax-only -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX \
	      -Ihostcheck -I$(COMMON_INC) -I. screen_egl_present.c
	$(CC) -fsyntax-only -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX \
	      -Ihostcheck -I$(COMMON_INC) $(COMMON_INC)/cluster_surface.c
	# The Docker QNX build remains the authoritative Screen/OMX ABI link check.

clean:
	rm -f $(TARGET) $(TESTBIN)

.PHONY: all clean test hostcheck
