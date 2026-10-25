#!/usr/bin/env bash
# Replicability script: reproduces the frames of the teaser figure (Fig. 1) of the paper
#   Dynamic Skinning: Kinematics-Driven Cartoon Effects for Articulated Characters (IEEE TVCG 2026)
#
# Usage (no parameter, from any directory):  bash replicability/teaser.sh
#
# Builds the complete application (code/), then runs it in capture mode on the three examples of the teaser,
# and saves the frames as PNG images (without the GUI and the skeleton) in replicability/output/teaser/:
#   giraffe/  the neck swings from left to right (t = 3.0 to 3.5 s), then the head lags and oscillates
#   clown/    the body swings from left to right (t = 3.0 to 3.5 s), then the arms and the hat oscillate
#   train/    the bumps of the engine propagate to the last wagon (delayed LBS)
# Each file is named <model>_<index>_t<animation time in s>.png. In the paper, several of these frames are
# superimposed with transparency to show the motion.
# A window opens during each capture, and closes by itself after the last frame.

set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT="$ROOT/replicability/output/teaser"

# ---- Build ----
cd "$ROOT/code"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4 --target complete

# ---- Captures ----
# capture <directory> <model> <animation times>
capture() {
    mkdir -p "$OUTPUT/$1"
    rm -f "$OUTPUT/$1"/*.png
    echo "Capturing $2 ..."
    DS_CAPTURE_DIR="$OUTPUT/$1/" DS_CAPTURE_MODEL="$2" DS_CAPTURE_TIMES="$3" DS_CAPTURE_SKELETON=0 \
        ./build/complete > "$OUTPUT/$1/log.txt" 2>&1
    echo "  $(ls "$OUTPUT/$1"/*.png | wc -l | tr -d ' ') frames in $OUTPUT/$1"
}

capture giraffe Giraffe "3.0,3.1,3.2,3.3,3.4,3.5,3.6,3.75,3.9,4.1,4.3,4.6,5.0"
capture clown   Clown   "3.0,3.1,3.2,3.3,3.4,3.5,3.6,3.75,3.9,4.1,4.3,4.6,5.0"
capture train   Train   "0.5,0.6,0.7,0.8,0.9,1.0,1.1,1.2,1.3,1.5,3.6,3.8,4.0,4.2,4.4"

echo "Done: frames of the teaser in $OUTPUT"
