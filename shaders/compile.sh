#!/bin/bash
# Compile GLSL shaders to SPIR-V
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

glslc "$SCRIPT_DIR/shader.vert" -o "$SCRIPT_DIR/vert.spv"
glslc "$SCRIPT_DIR/shader.frag" -o "$SCRIPT_DIR/frag.spv"

echo "Shaders compiled successfully."
