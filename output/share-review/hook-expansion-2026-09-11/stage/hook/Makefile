CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror -pedantic
OUT ?= build
BUILD := $(abspath $(OUT))
HOST_OS := $(shell uname -s)

# This target runs only the included synthetic caller and provider.
ifeq ($(HOST_OS),Darwin)
EXT := dylib
SHARED := -dynamiclib
DL_LIB :=
CALLER_FLAGS := -Wl,-flat_namespace
HOOK_PROVIDER = -Wl,-needed_library,"$(PROVIDER)"
RUN_HOOK = DYLD_INSERT_LIBRARIES="$(HOOK)" DYLD_FORCE_FLAT_NAMESPACE=1
else ifeq ($(HOST_OS),Linux)
EXT := so
SHARED := -shared
DL_LIB := -ldl
CALLER_FLAGS :=
HOOK_PROVIDER :=
RUN_HOOK = LD_PRELOAD="$(HOOK)"
else
$(error Host demo supports Darwin or Linux)
endif
PROVIDER := $(BUILD)/libdemo_provider.$(EXT)
HOOK := $(BUILD)/libdemo_hook.$(EXT)
DRIVER := $(BUILD)/demo_driver
VIEWS := $(BUILD)/view_demo

.PHONY: demo clean
demo: $(DRIVER) $(HOOK) $(VIEWS)
	$(RUN_HOOK) "$(DRIVER)"
	"$(VIEWS)"

$(BUILD):
	mkdir -p "$@"
$(PROVIDER): demo/provider.c interpose_example.h | $(BUILD)
	$(CC) $(CFLAGS) -fPIC $(SHARED) -I. demo/provider.c -o "$@"
$(HOOK): interpose_example.c interpose_example.h $(PROVIDER) | $(BUILD)
	$(CC) $(CFLAGS) -fPIC $(SHARED) -I. interpose_example.c $(HOOK_PROVIDER) -pthread $(DL_LIB) -o "$@"
$(DRIVER): demo/driver.c interpose_example.h $(PROVIDER) | $(BUILD)
	$(CC) $(CFLAGS) -I. $(CALLER_FLAGS) demo/driver.c "$(PROVIDER)" -pthread $(DL_LIB) -o "$@"
$(VIEWS): demo/view_demo.c view_area.c view_area.h view_switch.c view_switch.h | $(BUILD)
	$(CC) $(CFLAGS) -I. demo/view_demo.c view_area.c view_switch.c -o "$@"

clean:
	rm -f "$(PROVIDER)" "$(HOOK)" "$(DRIVER)" "$(VIEWS)"
