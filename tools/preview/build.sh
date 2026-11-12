#!/bin/sh
# Builds the desktop preview of the dashboard and renders the sample scenes as PNG files.
# Usage: tools/preview/build.sh [output directory]
set -e
cd "$(dirname "$0")/../.."
OUT="${1:-tools/preview/out}"
GFX="$(find .pio/libdeps -maxdepth 2 -type d -name 'Adafruit GFX Library' | head -n 1)"
if [ -z "$GFX" ]; then
  echo "Adafruit GFX Library not found, run 'pio pkg install' first" >&2
  exit 1
fi
mkdir -p "$OUT"
c++ -std=c++17 -O1 -Wall -Wno-unused-parameter -DARDUINO=100 \
  -I tools/preview/shim -I "$GFX" -I lib/Display -I lib/History -I lib/MaraXProtocol \
  tools/preview/preview.cpp lib/Display/DashboardPainter.cpp lib/Display/LogoScreen.cpp \
  lib/History/TemperatureHistory.cpp \
  lib/MaraXProtocol/MaraXProtocol.cpp "$GFX/Adafruit_GFX.cpp" \
  -o "$OUT/preview"
"$OUT/preview" "$OUT"
python3 tools/preview/pbm_to_png.py "$OUT"/*.pbm
