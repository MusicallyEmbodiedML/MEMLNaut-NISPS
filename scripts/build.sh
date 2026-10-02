#!/usr/bin/env bash
# Build (and optionally upload) MEMLNaut-NISPS with arduino-cli, targeting the
# MEMLNaut board (rp2040:rp2040:memlnaut).
#
# This relies on arduino-cli already having:
#   - the rp2040:rp2040 core installed, patched with the MEMLNaut board
#     definition (see /home/ck84/Arduino/MEMLNaut_Board_Definition_Notes.md)
#   - the sketch's libraries installed as user libraries (TFT_eSPI with
#     Setup9999_MEMLNaut.h selected, ArduinoJson, Adafruit GFX/NeoPixel/
#     SleepyDog, MIDI Library, ETL, etc.)
#
# Usage:
#   scripts/build.sh                 # compile only
#   scripts/build.sh -u               # compile and upload (auto-detect port)
#   scripts/build.sh -u -p /dev/ttyACM0
#   scripts/build.sh -c               # clean build dir first
#   scripts/build.sh -v               # verbose arduino-cli output

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_DIR}/build"
FQBN="rp2040:rp2040:memlnaut"

UPLOAD=0
PORT=""
CLEAN=0
VERBOSE=0

usage() {
    grep -E '^#( |$)' "${BASH_SOURCE[0]}" | sed -E 's/^#( )?//' | sed -n '/^Usage:/,$p'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -u|--upload) UPLOAD=1; shift ;;
        -p|--port) PORT="$2"; shift 2 ;;
        -c|--clean) CLEAN=1; shift ;;
        -v|--verbose) VERBOSE=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

command -v arduino-cli >/dev/null 2>&1 || {
    echo "error: arduino-cli not found on PATH. Install it: https://arduino.github.io/arduino-cli/latest/installation/" >&2
    exit 1
}

if ! arduino-cli board details -b "${FQBN}" >/dev/null 2>&1; then
    echo "error: board ${FQBN} is not available to arduino-cli." >&2
    echo "See /home/ck84/Arduino/MEMLNaut_Board_Definition_Notes.md to install the MEMLNaut board definition into the rp2040 core." >&2
    exit 1
fi

# Pull in git submodules (src/memllib, src/memlp) if missing.
if git -C "${REPO_DIR}" submodule status 2>/dev/null | grep -q '^-'; then
    echo "==> Initializing git submodules"
    git -C "${REPO_DIR}" submodule update --init --recursive
fi

if [[ "${CLEAN}" -eq 1 ]]; then
    echo "==> Cleaning ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

VERBOSE_FLAG=()
[[ "${VERBOSE}" -eq 1 ]] && VERBOSE_FLAG=(--verbose)

echo "==> Compiling ${REPO_DIR} for ${FQBN}"
arduino-cli compile \
    --fqbn "${FQBN}" \
    --output-dir "${BUILD_DIR}" \
    --warnings default \
    "${VERBOSE_FLAG[@]}" \
    "${REPO_DIR}"

echo "==> Build output in ${BUILD_DIR}"

if [[ "${UPLOAD}" -eq 1 ]]; then
    FLASH_ARGS=()
    [[ -n "${PORT}" ]] && FLASH_ARGS+=(-p "${PORT}")
    [[ "${VERBOSE}" -eq 1 ]] && FLASH_ARGS+=(-v)
    "${SCRIPT_DIR}/flash.sh" "${FLASH_ARGS[@]}"
fi
