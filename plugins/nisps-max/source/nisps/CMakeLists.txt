cmake_minimum_required(VERSION 3.19)

# The external is named after this directory: "nisps" -> nisps.mxo / nisps.mxe64
# (max-pretarget derives the target and output name from the folder).

# Path to https://github.com/Cycling74/max-sdk-base
if (NOT DEFINED MAX_SDK_BASE_PATH)
	set(MAX_SDK_BASE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../max-sdk-base")
endif ()
if (NOT EXISTS "${MAX_SDK_BASE_PATH}/script/max-pretarget.cmake")
	message(FATAL_ERROR "max-sdk-base not found at ${MAX_SDK_BASE_PATH}; set -DMAX_SDK_BASE_PATH=/path/to/max-sdk-base")
endif ()

# The engine is shared with the SuperCollider plugin: NISPSCore + the memlp
# subset it needs, compiled straight into the external (no separate library).
if (NOT DEFINED NISPS_SC_PATH)
	set(NISPS_SC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../nisps-sc")
endif ()
if (NOT EXISTS "${NISPS_SC_PATH}/src/NISPSCore.h")
	message(FATAL_ERROR "nisps-sc not found at ${NISPS_SC_PATH}; set -DNISPS_SC_PATH=/path/to/nisps-sc")
endif ()

# Where the built external goes (the package's externals/ folder). Guarded so a
# cross-compile can redirect it.
if (NOT DEFINED C74_LIBRARY_OUTPUT_DIRECTORY)
	set(C74_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../externals")
endif ()

include(${MAX_SDK_BASE_PATH}/script/max-pretarget.cmake)

include_directories(
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
	"${NISPS_SC_PATH}/src"
	"${NISPS_SC_PATH}/memlp"
)

add_library(${PROJECT_NAME} MODULE
	"${CMAKE_CURRENT_SOURCE_DIR}/nisps.cpp"
	"${NISPS_SC_PATH}/src/NISPSCore.cpp"
	"${NISPS_SC_PATH}/src/NISPSStateFlat.cpp"
	"${NISPS_SC_PATH}/memlp/MLP.cpp"
	"${NISPS_SC_PATH}/memlp/Utils.cpp"
)

include(${MAX_SDK_BASE_PATH}/script/max-posttarget.cmake)

# max-posttarget pins C++17; memlp/Layer.h uses std::span, so raise it after the include.
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
