#!/bin/bash
# Compile GLSL shaders to SPIR-V (same output as the CMake build).
# Usage: shaders/compile.sh [output_dir]   (default: build/shaders)
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${1:-$SCRIPT_DIR/../build/shaders}"
mkdir -p "$OUT_DIR"

for shader in "$SCRIPT_DIR"/*.vert "$SCRIPT_DIR"/*.frag; do
    name="$(basename "$shader")"
    glslc "$shader" -o "$OUT_DIR/$name.spv"
    echo "Compiled $name -> $OUT_DIR/$name.spv"
done
