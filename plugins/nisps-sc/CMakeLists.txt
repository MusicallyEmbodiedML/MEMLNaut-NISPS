cmake_minimum_required(VERSION 3.16)
project(nisps-sc CXX)

set(CMAKE_CXX_STANDARD 20) # memlp/Layer.h uses std::span
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------------------
# NISPSCore: the portable RL engine (memlp + NISPSCore.cpp). No SuperCollider
# or Arduino dependency at all — this is what the host smoke test and the SC
# plugin both link against.
# ---------------------------------------------------------------------------
add_library(nisps_core STATIC
    src/NISPSCore.cpp
    src/NISPSStateFlat.cpp
    memlp/MLP.cpp
    memlp/Utils.cpp
)
target_include_directories(nisps_core PUBLIC src memlp)
# Needed so this static lib can also be linked into the NISPS shared module below.
set_property(TARGET nisps_core PROPERTY POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Host smoke test: proves the memlp subset + NISPSCore build and run cleanly
# on a desktop host, with no SC involved. Always built.
# ---------------------------------------------------------------------------
add_executable(nisps_host_test tests/host_smoke_test.cpp)
target_link_libraries(nisps_host_test PRIVATE nisps_core)

# ---------------------------------------------------------------------------
# SuperCollider plugin: only built when SC_PATH points at a SuperCollider
# source checkout (needs include/plugin_interface + include/common).
# ---------------------------------------------------------------------------
if(SC_PATH)
    include_directories(${SC_PATH}/include/plugin_interface)
    include_directories(${SC_PATH}/include/common)
    include_directories(${SC_PATH}/common)

    set(CMAKE_SHARED_MODULE_PREFIX "")
    set(CMAKE_SHARED_MODULE_SUFFIX ".scx") # .so is deprecated for SC plugins on Linux too

    add_library(NISPS MODULE src/NISPS.cpp)
    target_link_libraries(NISPS PRIVATE nisps_core)
    set_property(TARGET NISPS PROPERTY CXX_STANDARD 20)

    if(APPLE)
        target_link_options(NISPS PRIVATE -undefined dynamic_lookup)
    endif()
else()
    message(STATUS "SC_PATH not set — skipping the NISPS SuperCollider plugin target "
                    "(only nisps_host_test will be built). Pass -DSC_PATH=/path/to/supercollider "
                    "to also build the .scx plugin.")
endif()
