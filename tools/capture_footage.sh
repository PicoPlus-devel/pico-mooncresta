#!/usr/bin/env bash
# Capture screenshots of the game for tools/mkartwork.py --footage.
#
#   tools/capture_footage.sh <romdir> <outdir>
#
# Plays the game in the host harness (attract mode, a coin, 1 player start,
# then scripted play) with Tate mode Off, and keeps one frame per second as
# outdir/frames/frame_NNNNN.ppm, with a .png beside it to look through. Five of
# them are copied to the names mkartwork.py reads (badge, left, middle, right,
# screensaver .ppm); copy another frame over a name to change what that part
# of the artwork shows.
set -euo pipefail
cd "$(dirname "$0")/.."

if [ $# -ne 2 ]; then
    sed -n 2,11p "$0"
    exit 1
fi
romdir=$1
out=$2

[ -x hosttest/mcr_host ] || hosttest/build.sh
mkdir -p "$out/frames"
./hosttest/mcr_host "$romdir" 3601 60 "$out/frames" --coin 1800 --start 1860 --play 1920 > /dev/null
for f in "$out"/frames/*.ppm; do
    python3 hosttest/ppm2png.py "$f" > /dev/null
done

pick() { cp "$out/frames/frame_$(printf %05d "$1").ppm" "$out/$2.ppm"; }
pick 2400 badge
pick 2400 screensaver
pick 2400 left
pick 2700 middle
pick 2880 right
echo "frames in $out/frames; badge, left, middle, right and screensaver.ppm in $out"
