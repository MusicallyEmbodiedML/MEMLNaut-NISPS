# MEMLNaut for VCV Rack. Build with the Rack 2 SDK:
#   make RACK_DIR=/path/to/Rack-SDK          # build plugin.so/.dylib/.dll
#   make RACK_DIR=/path/to/Rack-SDK install  # copy into the Rack user plugins folder
RACK_DIR ?= ../Rack-SDK

# The RL engine is compiled straight out of ../nisps-sc (one copy, shared with the
# SuperCollider and Max ports) through the thin wrappers in src/engine/.
ENGINE := ../nisps-sc
FLAGS += -I$(ENGINE)/src -I$(ENGINE)/memlp

SOURCES += $(wildcard src/*.cpp) $(wildcard src/engine/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

include $(RACK_DIR)/plugin.mk

# memlp needs C++20 (std::span); this comes after plugin.mk's -std=c++11, so it wins.
CXXFLAGS += -std=c++20

# Host test: drives the module through libRack without the Rack app (tests/module_test.cpp).
TEST_OBJECTS := $(filter-out build/src/plugin.cpp.o,$(OBJECTS))
build/module_test: tests/module_test.cpp $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ build/test_plugin_stub.o -L$(RACK_DIR) -lRack -Wl,-rpath,$(abspath $(RACK_DIR))

build/test_plugin_stub.o:
	@mkdir -p build
	printf '#include <rack.hpp>\nrack::plugin::Plugin* pluginInstance;\n' | $(CXX) $(CXXFLAGS) -x c++ -c - -o $@

test: build/test_plugin_stub.o build/module_test
	build/module_test
.PHONY: test
