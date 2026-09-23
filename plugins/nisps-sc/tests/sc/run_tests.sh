#!/usr/bin/env bash
# Runs the sclang/scsynth integration tests in this directory against a
# locally built NISPS.scx.
#
# Usage:
#   tests/sc/run_tests.sh [path-to-nisps-sc-build-dir] [path-to-sc-plugins-dir]
#
# Both arguments are optional and default to this repo's own build/ dir and
# /usr/local/lib/SuperCollider/plugins (the standard install location on
# Linux) respectively. SC_PLUGINS_PATH env var overrides the second default.
#
# scsynth's -U/ugenPluginsPath REPLACES the standard plugin search path
# rather than adding to it, so the standard plugins dir must always be passed
# alongside the build dir or basic UGens (Control, SendReply, ...) fail to
# load at all.
#
# On a system where the audio backend is JACK-via-PipeWire (as on typical
# desktop Linux with PipeWire active), run this under `pw-jack` so scsynth's
# audio driver can actually connect, e.g.:
#   pw-jack tests/sc/run_tests.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

NISPS_BUILD_DIR="${1:-$REPO_DIR/build}"
SC_PLUGINS_DIR="${2:-${SC_PLUGINS_PATH:-/usr/local/lib/SuperCollider/plugins}}"

if [ ! -e "$NISPS_BUILD_DIR/NISPS.scx" ] && [ ! -e "$NISPS_BUILD_DIR/NISPS.so" ]; then
    echo "error: no NISPS.scx (or .so) found in $NISPS_BUILD_DIR — build it first:" >&2
    echo "  cmake -S $REPO_DIR -B $NISPS_BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DSC_PATH=/path/to/supercollider" >&2
    echo "  cmake --build $NISPS_BUILD_DIR --target NISPS" >&2
    exit 1
fi

if [ ! -d "$SC_PLUGINS_DIR" ]; then
    echo "error: standard SC plugins dir not found at $SC_PLUGINS_DIR — pass it as the 2nd argument." >&2
    exit 1
fi

export NISPS_BUILD_DIR
export SC_PLUGINS_DIR="$SC_PLUGINS_DIR"

overall_status=0

for test_file in "$SCRIPT_DIR"/[0-9]*.scd; do
    echo "=================================================================="
    echo "Running $(basename "$test_file")"
    echo "=================================================================="
    output="$(sclang --include-path "$REPO_DIR/classes" "$test_file" 2>&1)" || true
    echo "$output"
    if ! echo "$output" | grep -q "SC_TEST_RESULT: PASS"; then
        echo ">>> $(basename "$test_file") FAILED"
        overall_status=1
    fi
done

echo "=================================================================="
if [ "$overall_status" -eq 0 ]; then
    echo "ALL SC INTEGRATION TESTS PASSED"
else
    echo "SOME SC INTEGRATION TESTS FAILED"
fi

exit $overall_status
