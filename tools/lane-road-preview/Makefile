ROOT=../..
OUT=$(ROOT)/build/lane-road-preview
CC=cc
CXX=c++
CPPFLAGS=-DPLATFORM_MACOS -I/opt/homebrew/include -I$(ROOT)/maneuver_render
CFLAGS=-O2 -std=c99 -Wall -Wextra
CXXFLAGS=-O2 -std=c++11 -Wall -Wextra -Wno-missing-field-initializers -fno-exceptions -fno-rtti
LDLIBS=-L/opt/homebrew/lib -lglfw -framework OpenGL -lm
CORE=render maneuver route_path platform_macos
HEADERS=$(wildcard $(ROOT)/maneuver_render/*.h $(ROOT)/maneuver_render/scene/*.h $(ROOT)/maneuver_render/scene/*.hpp) fixtures.h
OBJECTS=$(addprefix $(OUT)/,$(addsuffix .o,$(CORE))) $(OUT)/scene.o $(OUT)/geometry.o $(OUT)/layout.o
SAN=-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
SAN_OBJECTS=$(addprefix $(OUT)/san/,$(addsuffix .o,$(CORE))) $(OUT)/san/scene.o $(OUT)/san/geometry.o $(OUT)/san/layout.o
all: $(OUT)/preview $(OUT)/flag_atlas.rgba
$(OUT)/flag_atlas.rgba: $(ROOT)/maneuver_render/resources/flag_atlas.rgba
	@mkdir -p $(dir $@)
	cp $< $@
$(OUT)/preview: $(OUT)/preview.o $(OBJECTS)
	$(CC) $^ $(LDLIBS) -o $@
$(OUT)/preview.o: preview.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(OUT)/%.o: $(ROOT)/maneuver_render/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(OUT)/scene.o $(OUT)/geometry.o $(OUT)/layout.o: $(OUT)/%.o: $(ROOT)/maneuver_render/scene/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(OUT)/san/%.o: $(ROOT)/maneuver_render/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN) -c $< -o $@
$(OUT)/san/scene.o $(OUT)/san/geometry.o $(OUT)/san/layout.o: $(OUT)/san/%.o: $(ROOT)/maneuver_render/scene/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SAN) -c $< -o $@
$(OUT)/test_outline $(OUT)/test_scene: $(OUT)/test_%: test_%.cpp $(SAN_OBJECTS) $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SAN) $< $(SAN_OBJECTS) $(LDLIBS) -o $@
test: $(OUT)/test_outline $(OUT)/test_scene
	$(CC) $(CFLAGS) $(SAN) $(ROOT)/tests/lane_safety_test.c -o $(OUT)/test_safety
	$(OUT)/test_safety
	$(CC) $(CFLAGS) $(SAN) test_layout.c -o $(OUT)/test_layout
	$(OUT)/test_layout
	$(OUT)/test_outline
	$(OUT)/test_scene
.PHONY: all test
