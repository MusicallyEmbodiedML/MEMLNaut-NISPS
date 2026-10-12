#!/usr/bin/env bash
# Flash MEMLNaut-NISPS to a connected MEMLNaut board.
#
# Two ways this can work, tried in order:
#   1. arduino-cli upload over the board's serial port (normal case: the
#      sketch is running, arduino-cli does the 1200-baud touch + UF2 reset
#      dance for you).
#   2. BOOTSEL mass-storage copy: if the board is already sitting in
#      bootloader mode (held BOOTSEL while plugging in, or a crashed
#      sketch), it shows up as a "RPI-RP2" USB drive. This script copies
#      the .uf2 straight onto it.
#
# Usage:
#   scripts/flash.sh                  # build if needed, then flash (auto port)
#   scripts/flash.sh -p /dev/ttyACM0  # flash to a specific serial port
#   scripts/flash.sh -b               # force a rebuild before flashing
#   scripts/flash.sh -v               # verbose

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_DIR}/build"
# The board's default flash layout is "16MB (no FS)", which leaves LittleFS
# unmountable and silently drops every flash-persisted setting (input source,
# dislike mode, CC numbers...). Always build with a filesystem partition.
FQBN="rp2040:rp2040:memlnaut:flash=16777216_1048576"
UF2_FILE="${BUILD_DIR}/MEMLNaut-NISPS.ino.uf2"

PORT=""
FORCE_BUILD=0
VERBOSE=0

usage() {
    grep -E '^#( |$)' "${BASH_SOURCE[0]}" | sed -E 's/^#( )?//' | sed -n '/^Usage:/,$p'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port) PORT="$2"; shift 2 ;;
        -b|--build) FORCE_BUILD=1; shift ;;
        -v|--verbose) VERBOSE=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

command -v arduino-cli >/dev/null 2>&1 || {
    echo "error: arduino-cli not found on PATH." >&2
    exit 1
}

if [[ "${FORCE_BUILD}" -eq 1 || ! -f "${UF2_FILE}" ]]; then
    "${SCRIPT_DIR}/build.sh"
fi

VERBOSE_FLAG=()
[[ "${VERBOSE}" -eq 1 ]] && VERBOSE_FLAG=(--verbose)

find_serial_port() {
    arduino-cli board list --format json 2>/dev/null | python3 -c '
import json, sys
try:
    data = json.load(sys.stdin)
except Exception:
    sys.exit(0)
entries = data.get("detected_ports", []) if isinstance(data, dict) else data
for e in entries:
    port = e.get("port", {})
    matched = e.get("matching_boards") or []
    if any("memlnaut" in (m.get("fqbn") or "").lower() for m in matched):
        print(port.get("address", ""))
        sys.exit(0)
for e in entries:
    port = e.get("port", {})
    props = port.get("properties", {}) or {}
    if props.get("vid", "").lower() == "0x2e8a" and props.get("pid", "").lower() == "0xf00f":
        print(port.get("address", ""))
        sys.exit(0)
' || true
}

find_bootsel_drive() {
    for base in /media/"$USER" /run/media/"$USER" /media /mnt; do
        [[ -d "${base}" ]] || continue
        # RP2040 boards mount as RPI-RP2, RP2350 boards (MEMLNaut) as RP2350.
        find "${base}" -maxdepth 2 \( -iname "RPI-RP2" -o -iname "RP2350" \) -type d 2>/dev/null | head -1
    done | grep -v "^$" | head -1 || true
}

if [[ -z "${PORT}" ]]; then
    echo "==> Looking for a MEMLNaut serial port..."
    PORT="$(find_serial_port)"
fi

if [[ -n "${PORT}" ]]; then
    echo "==> Uploading to ${PORT} via arduino-cli"
    arduino-cli upload \
        --fqbn "${FQBN}" \
        --port "${PORT}" \
        --input-dir "${BUILD_DIR}" \
        "${VERBOSE_FLAG[@]}" \
        "${REPO_DIR}"
    echo "==> Done."
    exit 0
fi

echo "==> No serial port found, checking for a BOOTSEL (RPI-RP2) drive..."
BOOTSEL_DRIVE="$(find_bootsel_drive)"

if [[ -n "${BOOTSEL_DRIVE}" ]]; then
    echo "==> Found BOOTSEL drive at ${BOOTSEL_DRIVE}, copying UF2"
    cp -v "${UF2_FILE}" "${BOOTSEL_DRIVE}/"
    sync
    echo "==> Done. Board should reboot into the new firmware."
    exit 0
fi

cat >&2 <<EOF
error: could not find the MEMLNaut, either as a serial port or a mounted
RPI-RP2 BOOTSEL drive.

Try:
  - Check the board is plugged in ('arduino-cli board list').
  - Pass the port explicitly: scripts/flash.sh -p /dev/ttyACM0
  - Hold BOOTSEL while plugging in the board, then re-run this script
    (it will copy ${UF2_FILE##*/} onto the RPI-RP2 drive directly).
EOF
exit 1
