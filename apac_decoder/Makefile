# Host build for fast iteration. The QNX ARMv7 cross build comes later and is
# driven by scripts/build_hook.sh, not from here.
#
#   make            build our decoder
#   make check      build and run the tests
#
CXX      ?= c++
# -fno-exceptions/-fno-rtti are not taste: the target is an LD_PRELOADed .so on
# QNX 6.5 / GCC 4.9.4 and the decode path must not need either.
CXXFLAGS ?= -std=c++11 -O2 -g -Wall -Wextra -Werror -fno-exceptions -fno-rtti -Iinclude
# -MMD/-MP: without header dependencies, changing a constant in include/apac
# recompiles only some translation units and the survivors disagree about a
# struct's size.  That fails as wrong numbers, not as a link error.
DEPFLAGS := -MMD -MP
BUILD    := .build

SRC  := $(wildcard src/*.cpp)
OBJ  := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRC))
TSRC := $(wildcard tests/*.cpp)
TBIN := $(patsubst tests/%.cpp,$(BUILD)/%,$(TSRC))

all: $(OBJ)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD)/%: tests/%.cpp $(OBJ) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $< $(OBJ) -o $@

$(BUILD):
	@mkdir -p $(BUILD)

# The whole-capture harness.  Not a test: it needs the capture, which is
# gitignored.  `make check` runs on committed fixtures alone.
$(BUILD)/apac_decode: tools/apac_decode.cpp $(OBJ) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $< $(OBJ) -o $@

tools: $(BUILD)/apac_decode

check: $(TBIN)
	@if [ -z "$(TBIN)" ]; then echo "no tests yet"; exit 1; fi
	@for t in $(TBIN); do printf '%-40s ' "$$(basename $$t)"; ./$$t || exit 1; done

clean:
	rm -rf $(BUILD)

-include $(BUILD)/*.d

.PHONY: all check clean tools
