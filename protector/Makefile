# Protector build. The VIN is provided pre-build; stamping happens in the build.
#
#   make                         # generic host build (auto-arm from live FEC)
#   make build VIN=<17> SECRET=…  # compile + stamp for that VIN (uses build.sh)
#   make test                    # end-to-end selftest (argv-driven VIN)
#
# QNX cross build for the RCC (armle-v7):
#   make CC='qcc -Vgcc_ntoarmv7le'
#   CC='qcc -Vgcc_ntoarmv7le' PROT_SECRET=… ./build.sh <VIN> app.armle

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
SECRET  ?= $(PROT_SECRET)
VIN     ?= WAUC4CF55JA099137

SRC = example_app.c prot_rt.c prot_vin.c

ifeq ($(strip $(SECRET)),)
SECRET := demo-build-secret-set-PROT_SECRET
endif

# host build: production auto-arm path (reads live FEC on the unit at runtime)
app: $(SRC) prot_rt.h prot_vin.h prot_crypto.h
	$(CC) $(CFLAGS) -DPROT_SECRET='"$(SECRET)"' -o app $(SRC)

# one-shot protected build with a pre-provided VIN
build:
	printf '\x5a\x66\x07' > consts.bin
	PROT_SECRET='$(SECRET)' CC='$(CC)' ./build.sh $(VIN) app consts.bin

test:
	@sh ./selftest.sh '$(SECRET)'

clean:
	rm -f app app_* app_live app_live2 *.o consts.bin

.PHONY: build test clean
