# Host build for fast iteration. The QNX ARMv7 cross build comes later and is
# driven by scripts/build_hook.sh, not from here.
#
#   make            build the port
#   make check      build and run the tests
#
CXX      ?= c++
# -fno-exceptions/-fno-rtti are not taste: the target is an LD_PRELOADed .so on
# QNX 6.5 / GCC 4.9.4 and the decode path must not need either.
CXXFLAGS ?= -std=c++11 -O2 -g -Wall -Wextra -Werror -fno-exceptions -fno-rtti -Iinclude
BUILD    := .build

SRC  := $(wildcard src/*.cpp)
OBJ  := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRC))
TSRC := $(wildcard tests/*.cpp)
TBIN := $(patsubst tests/%.cpp,$(BUILD)/%,$(TSRC))

all: $(OBJ)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%: tests/%.cpp $(OBJ) | $(BUILD)
	$(CXX) $(CXXFLAGS) $< $(OBJ) -o $@

$(BUILD):
	@mkdir -p $(BUILD)

check: $(TBIN)
	@if [ -z "$(TBIN)" ]; then echo "no tests yet"; exit 1; fi
	@for t in $(TBIN); do printf '%-40s ' "$$(basename $$t)"; ./$$t || exit 1; done

clean:
	rm -rf $(BUILD)

.PHONY: all check clean
