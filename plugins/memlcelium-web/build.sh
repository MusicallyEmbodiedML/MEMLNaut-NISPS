#!/usr/bin/env bash
# Build MEMLCelium for the browser: web/memlcelium.wasm.
# Needs Emscripten (source ~/emsdk/emsdk_env.sh first, or set EMSDK).
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "${HERE}/../.." && pwd)"
SC="${REPO}/plugins/nisps-sc"
LIB="${REPO}/src/memllib"

if ! command -v em++ >/dev/null 2>&1; then
    # shellcheck disable=SC1091
    source "${EMSDK:-$HOME/emsdk}/emsdk_env.sh" >/dev/null 2>&1
fi

# Standalone wasm with no JS glue: the AudioWorklet instantiates it directly.
em++ -std=c++20 -O3 -DNDEBUG \
    -include "${HERE}/src/prelude.hpp" \
    -I "${HERE}/shim" -I "${SC}/src" -I "${SC}/memlp" \
    "${HERE}/src/celium.cpp" \
    "${SC}/src/NISPSCore.cpp" "${SC}/src/NISPSStateFlat.cpp" \
    "${SC}/memlp/MLP.cpp" "${SC}/memlp/Utils.cpp" \
    "${LIB}/synth/maximilian.cpp" \
    -sSTANDALONE_WASM --no-entry \
    -sINITIAL_MEMORY=16777216 -sALLOW_MEMORY_GROWTH=0 -sSTACK_SIZE=262144 \
    -fno-exceptions \
    -o "${HERE}/web/memlcelium.wasm"

echo "built ${HERE}/web/memlcelium.wasm ($(stat -c %s "${HERE}/web/memlcelium.wasm") bytes)"
